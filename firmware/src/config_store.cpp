#include "config_store.hpp"
#include "config_legacy.hpp"
#include <Preferences.h>
#include <type_traits>

namespace
{
    using ConfigStorage::LegacyData;

    constexpr uint32_t CONFIG_MAGIC = 0x4342574C;
    constexpr uint32_t CURRENT_FORMAT_VERSION = 3;
    constexpr uint32_t LEGACY_FORMAT_VERSION = 1;
    constexpr const char* NVS_NAMESPACE = "cyber-bowl";
    constexpr const char* NVS_CONFIG_KEY = "config";

    template <typename Data> struct StoredRecord
    {
        uint32_t magic;
        uint32_t version;
        uint32_t size;
        uint32_t checksum;
        Data data;
    };

    static_assert(std::is_trivially_copyable<Settings::Data>::value,
                  "NVS settings must be trivially copyable");

    Preferences preferences;
    Settings::Data currentSettings = Settings::defaults();
    bool storageAvailable = false;
    bool settingsLoaded = false;

    template <typename Data> uint32_t calculateChecksum(const Data& settings)
    {
        uint32_t hash = 2166136261u;
        const auto* bytes = reinterpret_cast<const uint8_t*>(&settings);
        for (size_t i = 0; i < sizeof(settings); ++i)
        {
            hash = (hash ^ bytes[i]) * 16777619u;
        }
        return hash;
    }

    template <typename Data>
    bool recordMetadataValid(const StoredRecord<Data>& record, uint32_t expectedVersion)
    {
        return record.magic == CONFIG_MAGIC && record.version == expectedVersion &&
               record.size == sizeof(Data) && record.checksum == calculateChecksum(record.data);
    }

    template <typename Data>
    bool readStoredRecord(StoredRecord<Data>& record, uint32_t expectedVersion)
    {
        if (preferences.getBytesLength(NVS_CONFIG_KEY) != sizeof(record))
        {
            return false;
        }
        if (preferences.getBytes(NVS_CONFIG_KEY, &record, sizeof(record)) != sizeof(record))
        {
            return false;
        }
        return recordMetadataValid(record, expectedVersion);
    }

    Settings::Data migrateLegacySettings(const LegacyData& old)
    {
        Settings::Data current{};
        current.foodScale = {old.foodScale.factor, old.foodScale.offset, old.foodScale.calibrated,
                             old.foodScale.tared};
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
        current.oneShot = {old.oneShot.foodG, old.oneShot.waterMl, old.oneShot.dueEpoch,
                           old.oneShot.enabled};
        for (size_t i = 0; i < Settings::SCHEDULE_COUNT; ++i)
        {
            const auto& slot = old.schedule[i];
            current.schedule[i] = {slot.hour, slot.minute, slot.foodG, slot.waterMl, slot.enabled};
            current.lastRunDay[i] = old.lastRunDay[i];
        }
        return current;
    }

    bool loadCurrentSettings()
    {
        StoredRecord<Settings::Data> record{};
        if (!readStoredRecord(record, CURRENT_FORMAT_VERSION) || !Settings::valid(record.data))
        {
            return false;
        }

        currentSettings = record.data;
        return true;
    }

    bool loadLegacySettings()
    {
        StoredRecord<LegacyData> record{};
        if (!readStoredRecord(record, LEGACY_FORMAT_VERSION))
        {
            return false;
        }

        const auto migrated = migrateLegacySettings(record.data);
        if (!Settings::valid(migrated))
        {
            return false;
        }

        currentSettings = migrated;
        return true;
    }

    StoredRecord<Settings::Data> makeStoredRecord(const Settings::Data& settings)
    {
        StoredRecord<Settings::Data> record{};
        record.magic = CONFIG_MAGIC;
        record.version = CURRENT_FORMAT_VERSION;
        record.size = sizeof(Settings::Data);
        record.data = settings;
        record.checksum = calculateChecksum(record.data);
        return record;
    }
} // namespace

bool Config::begin()
{
    currentSettings = Settings::defaults();
    settingsLoaded = false;
    storageAvailable = preferences.begin(NVS_NAMESPACE, false);
    if (!storageAvailable)
    {
        return false;
    }

    // Try the current format first. Import v1 into RAM if needed.
    // The next successful commit writes the current format.
    settingsLoaded = loadCurrentSettings() || loadLegacySettings();
    return true;
}

const Settings::Data& Config::get()
{
    return currentSettings;
}

bool Config::storageReady()
{
    return storageAvailable;
}

bool Config::loadedSavedSettings()
{
    return settingsLoaded;
}

bool Config::commit(const Settings::Data& candidate)
{
    if (!storageAvailable || !Settings::valid(candidate))
    {
        return false;
    }

    const auto record = makeStoredRecord(candidate);
    if (preferences.putBytes(NVS_CONFIG_KEY, &record, sizeof(record)) != sizeof(record))
    {
        return false;
    }

    // Publish only after NVS accepted the complete record.
    currentSettings = candidate;
    return true;
}
