#pragma once
#include "settings.hpp"

namespace Config {
    bool begin();
    const Settings::Data& get();
    bool commit(const Settings::Data& candidate);
    bool storageReady();
    bool loadedSavedSettings();
}
