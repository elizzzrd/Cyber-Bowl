#include "config_store.hpp"
#include <Preferences.h>
#include <type_traits>

namespace 
{
    constexpr uint32_t MAGIC = 0x4342574C;
    constexpr uint32_t VERSION = 3;
 
    // Layout written by the last working version before the sensor refactor.
    struct LegacyScale {
        float factor;
        int32_t offset;
        bool tared;
        bool calibrated;
    };

    struct LegacySlot {
        bool enabled;
        uint8_t hour;
        uint8_t minute;
        float foodG;
        float waterMl;
    };

    struct LegacyOneShot {
        bool enabled;
        uint32_t dueEpoch;
        float foodG;
        float waterMl;
    };

    struct LegacyData {
        LegacyScale foodScale;
        LegacyScale waterScale;
        float foodPortionG;
        float waterPortionMl;
        float foodBowlMaxG;
        float waterBowlMaxG;
        float pumpMsPerMl;
        uint32_t feedTimeoutMs;
        uint32_t waterTimeoutMs;
        uint32_t stepHalfPeriodUs;
        uint32_t jogSteps;
        uint32_t forwardSteps;
        uint32_t reverseSteps;
        bool stepperDirectionHigh;
        bool foodPresentLow;
        bool lowWaterProtection;
        bool tankCalibrated;
        int32_t tankRawEmpty;
        int32_t tankRawFull;
        uint8_t minTankPercent;
        uint8_t brightnessPercent;
        bool invertDisplay;
        LegacySlot schedule[Settings::SCHEDULE_COUNT];
        LegacyOneShot oneShot;
        uint32_t lastRunDay[Settings::SCHEDULE_COUNT];
    };

    template<typename Data>
    struct Stored {
        uint32_t magic;
        uint32_t version;
        uint32_t size;
        uint32_t checksum;
        Data data;
    };

    static_assert(std::is_trivially_copyable<Settings::Data>::value, "NVS data must be plain");
    Preferences preferences;
    Settings::Data data = Settings::defaults();
    bool ready = false;
    bool loaded = false;
 
    template<typename Data>
    uint32_t checksum(const Data& d)
    {
        uint32_t value = 2166136261u;
        const auto* bytes = reinterpret_cast<const uint8_t*>(&d);
        for (size_t i = 0; i < sizeof(d); ++i) value = (value ^ bytes[i]) * 16777619u;
        return value;
    }

    template<typename Data>
    bool load(Stored<Data>& saved, uint32_t version)
    {
        return preferences.getBytesLength("config") == sizeof(saved) &&
               preferences.getBytes("config", &saved, sizeof(saved)) == sizeof(saved) &&
               saved.magic == MAGIC && saved.version == version &&
               saved.size == sizeof(Data) && saved.checksum == checksum(saved.data);
    }

    Settings::Data migrate(const LegacyData& old)
    {
        Settings::Data current{};
        current.foodScale = {old.foodScale.factor, old.foodScale.offset,
                             old.foodScale.calibrated, old.foodScale.tared};
        current.waterScale = {old.waterScale.factor, old.waterScale.offset,
                              old.waterScale.calibrated, old.waterScale.tared};
        current.foodPortionG = old.foodPortionG;
        current.waterPortionMl = old.waterPortionMl;
        current.foodBowlMaxG = old.foodBowlMaxG;
        current.waterBowlMaxG = old.waterBowlMaxG;
        current.pumpMsPerMl = old.pumpMsPerMl;
        current.feedTimeoutMs = old.feedTimeoutMs;
        current.waterTimeoutMs = old.waterTimeoutMs;
        current.stepHalfPeriodUs = old.stepHalfPeriodUs;
        current.jogSteps = old.jogSteps;
        current.forwardSteps = old.forwardSteps;
        current.reverseSteps = old.reverseSteps;
        current.stepperDirectionHigh = old.stepperDirectionHigh;
        current.foodPresentLow = old.foodPresentLow;
        current.brightnessPercent = old.brightnessPercent;
        current.invertDisplay = old.invertDisplay;
        current.oneShot = {old.oneShot.foodG, old.oneShot.waterMl,
                           old.oneShot.dueEpoch, old.oneShot.enabled};
        for (size_t i = 0; i < Settings::SCHEDULE_COUNT; ++i) {
            const auto& slot = old.schedule[i];
            current.schedule[i] = {slot.hour, slot.minute, slot.foodG, slot.waterMl, slot.enabled};
            current.lastRunDay[i] = old.lastRunDay[i];
        }
        return current;
    }

}


bool Config::begin()
{
    loaded = false;
    data = Settings::defaults();
    ready = preferences.begin("cyber-bowl", false);
    if (!ready) return false;

    Stored<Settings::Data> saved{};
    if (load(saved, VERSION) && Settings::valid(saved.data)) {
        data = saved.data;
        loaded = true;
        return true;
    }

    Stored<LegacyData> legacy{};
    if (load(legacy, 1)) {
        const auto migrated = migrate(legacy.data);
        if (Settings::valid(migrated)) {
            data = migrated;
            loaded = true;
        }
    }
    return true;
}


const Settings::Data& Config::get()     { return data; }
bool Config::storageReady()             { return ready; }
bool Config::loadedSavedSettings()      { return loaded; }


bool Config::commit(const Settings::Data& candidate)
{
    if (!ready || !Settings::valid(candidate)) 
        return false;
    
    Stored<Settings::Data> saved{};
    saved.magic = MAGIC;
    saved.version = VERSION;
    saved.size = sizeof(Settings::Data);
    saved.data = candidate;
    saved.checksum = checksum(saved.data);

    if (preferences.putBytes("config", &saved, sizeof(saved)) != sizeof(saved)) 
        return false;
    
    data = candidate;
    return true;
}
