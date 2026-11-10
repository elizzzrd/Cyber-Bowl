#pragma once
#include <cstddef>
#include <cstdint>

namespace CameraConfig 
{
    struct ImageSettings 
    {
        uint8_t size = 0; // 0=320x240, 1=640x480, 2=800x600.
        uint8_t quality = 12; // Smaller JPEG quality number means a better image.
        int8_t brightness = 0;
        int8_t contrast = 0;
        int8_t saturation = 0;
        bool mirror = false;
        bool flip = false;
        uint8_t fps = 10;
    };

    struct Data 
    {
        ImageSettings image;
        char ssid[33] = {};
        char password[65] = {};
        char apPassword[64] = "bowl-camera";
    };

    bool valid(const ImageSettings& value);
    bool valid(const Data& value);
    bool begin();
    bool ready();
    Data get();
    bool save(const Data& candidate);
}