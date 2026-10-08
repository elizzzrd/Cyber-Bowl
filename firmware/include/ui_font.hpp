#pragma once
#include <Adafruit_GFX.h>
#include <cstdint>
namespace UiFont 
{
    int width(const char* text, uint8_t scale = 1);
    void draw(Adafruit_GFX& surface, int x, int y, const char* text, uint16_t color,
              uint8_t scale = 1, int maxWidth = 320);
}
