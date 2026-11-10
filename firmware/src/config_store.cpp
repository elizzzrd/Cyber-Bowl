#include "config_store.hpp"
#include <Preferences.h>
#include <cstring>
#include <type_traits>

namespace 
{
    // Magic signature "CBWL" (Cyber Bowl)
    constexpr uint32_t MAGIC = 0x4342574C;
    
    // Increment this if Settings::Data structure changes.
    // It forces a fallback to defaults instead of loading incompatible binary data.
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
    bool isStorageReady = false;
    bool isLoadedFromStorage = false;
 
    // FNV-1a hash function
    uint32_t calculateChecksum(const Settings::Data& d)
    {
        uint32_t value = 2166136261u;
        const auto* bytes = reinterpret_cast<const uint8_t*>(&d);
        for (size_t i = 0; i < sizeof(d); ++i) {
            value = (value ^ bytes[i]) * 16777619u;
        }
        return value;
    }

    // Wraps raw settings into a Stored structure with metadata
    Stored makeStored(const Settings::Data& candidate)
    {
        Stored saved{};
        saved.magic = MAGIC;
        saved.version = VERSION;
        saved.size = sizeof(Settings::Data);
        saved.data = candidate;
        saved.checksum = calculateChecksum(saved.data);
        return saved;
    }

    // Checks if the loaded binary structure is valid and belongs to the current version
    bool isValidStored(const Stored& saved)
    {
        return saved.magic == MAGIC 
            && saved.version == VERSION
            && saved.size == sizeof(Settings::Data) 
            && saved.checksum == calculateChecksum(saved.data) 
            && Settings::valid(saved.data);
    }
}

bool Config::begin()
{
    isStorageReady = preferences.begin("cyber-bowl", false);
    if (!isStorageReady) return false;

    Stored saved{};
    const size_t storedLength = preferences.getBytesLength("config");
    
    if (storedLength == sizeof(saved) && 
        preferences.getBytes("config", &saved, sizeof(saved)) == sizeof(saved) &&
        isValidStored(saved)) 
    {
        data = saved.data;
        isLoadedFromStorage = true;
    }
    
    return true;
}

const Settings::Data& Config::get()     { return data; }
bool Config::storageReady()             { return isStorageReady; }
bool Config::loadedSavedSettings()      { return isLoadedFromStorage; }

bool Config::commit(const Settings::Data& candidate)
{
    if (!isStorageReady || !Settings::valid(candidate)) {
        return false;
    }
    
    const Stored saved = makeStored(candidate);

    if (preferences.putBytes("config", &saved, sizeof(saved)) != sizeof(saved)) {
        return false;
    }
    
    data = candidate;
    return true;
}