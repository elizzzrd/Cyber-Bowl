#pragma once
#include "config.hpp"
#include <esp_camera.h>

namespace Camera 
{
    bool begin();
    bool ready();
    bool hasPsram();
    const char* error();
    bool apply(const CameraConfig::ImageSettings& candidate);

    CameraConfig::ImageSettings activeSettings();
    // A successfully acquired frame owns the camera mutex until release().
    camera_fb_t* acquire(uint32_t timeoutMs = 500);

    void release(camera_fb_t* frame);
    void setLight(bool enabled);
    bool lightOn();
    void update();
}
