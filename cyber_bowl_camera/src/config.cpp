#include "config.hpp"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace 
{
    constexpr uint32_t MAGIC = 0x43424341;
    constexpr uint32_t VERSION = 1;

    struct Saved 
    {
        uint32_t magic;
        uint32_t version;
        uint32_t checksum;
        CameraConfig::Data data;
    };

    Preferences preferences;
    SemaphoreHandle_t mutex = nullptr;
    CameraConfig::Data data;
    bool storageReady = false;
    
    
    uint32_t checksum(const CameraConfig::Data& candidate)
    {
        const auto* bytes = reinterpret_cast<const uint8_t*>(&candidate);
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < sizeof(candidate); ++i) hash = (hash ^ bytes[i]) * 16777619u;
        return hash;
    }
}


bool CameraConfig::begin()
{
    data = Data{};

    mutex = xSemaphoreCreateMutex();
    if (!mutex) 
        return false;
    
    storageReady = preferences.begin("bowl-camera", false);
    Saved stored{};

    if (storageReady && preferences.getBytesLength("config") == sizeof(stored) &&
        preferences.getBytes("config", &stored, sizeof(stored)) == sizeof(stored) &&
        stored.magic == MAGIC && stored.version == VERSION &&
        checksum(stored.data) == stored.checksum && valid(stored.data)) 
        { data = stored.data; }

    return storageReady;
}


bool CameraConfig::ready()          { return storageReady; }

CameraConfig::Data CameraConfig::get()
{
    Data copy{};
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) {
        copy = data; xSemaphoreGive(mutex);
    }
    return copy;
}


bool CameraConfig::save(const Data& candidate)
{
    if (!storageReady || !mutex || 
        !valid(candidate)       || 
        !xSemaphoreTake(mutex, pdMS_TO_TICKS(1000))) 
        { return false; }

    Saved stored{};
    stored.magic = MAGIC; stored.version = VERSION; stored.data = candidate;
    stored.checksum = checksum(stored.data);
    
    const bool success = preferences.putBytes("config", &stored, sizeof(stored)) == sizeof(stored);
    
    if (success) 
        data = candidate;
    
    xSemaphoreGive(mutex);
    return success;
}