#include "config_store.hpp"
#include <Preferences.h>
#include <cstring>
#include <type_traits>

namespace 
{
    constexpr uint32_t MAGIC = 0x4342574C;
    constexpr uint32_t VERSION = 2;
 
    struct Stored {
        uint32_t magic;
        uint32_t version;
        uint32_t size;
        uint32_t checksum;
        Settings::Data data;
    };
 
    static_assert(std::is_trivially_copyable<Settings::Data>::value, "NVS data must be plain");
    Preferences preferences;
    Settings::Data data = Settings::defaults();
    bool ready = false;
    bool loaded = false;
 
    uint32_t checksum(const Settings::Data& d)
    {
        uint32_t value = 2166136261u;
        const auto* bytes = reinterpret_cast<const uint8_t*>(&d);
        for (size_t i = 0; i < sizeof(d); ++i) value = (value ^ bytes[i]) * 16777619u;
        return value;
    }
}


bool Config::begin()
{
    ready = preferences.begin("cyber-bowl", false);
    if (!ready) return false;
    Stored saved{};

    if (preferences.getBytesLength("config") == sizeof(saved) &&
        preferences.getBytes("config", &saved, sizeof(saved)) == sizeof(saved) &&
        saved.magic == MAGIC && saved.version == VERSION &&
        saved.size == sizeof(Settings::Data) && saved.checksum == checksum(saved.data) &&
        Settings::valid(saved.data)) {
        data = saved.data;
        loaded = true;
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
    
    Stored saved{};
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
