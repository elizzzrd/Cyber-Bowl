#pragma once
#include "Adafruit_GFX.h"
#include "SPI.h"
class Adafruit_ST7789;
namespace Fake {
Adafruit_ST7789*& display();
extern unsigned fullScreenClears;
extern unsigned rowTransfers;
}
class Adafruit_ST7789 : public Adafruit_GFX {
public:
    Adafruit_ST7789(SPIClass*, int, int, int) : Adafruit_GFX(320, 240) { Fake::display() = this; }
    void init(int, int) {}
    void setRotation(int) {}
    void invertDisplay(bool) {}
    void fillScreen(uint16_t color) override { ++Fake::fullScreenClears; Adafruit_GFX::fillScreen(color); }
    void drawRGBBitmap(int x, int y, const uint16_t* data, int w, int h) {
        ++Fake::rowTransfers;
        for (int yy = 0; yy < h; ++yy)
            for (int xx = 0; xx < w; ++xx)
                if (x + xx >= 0 && x + xx < w_ && y + yy >= 0 && y + yy < h_)
                    pixels_[(y + yy) * w_ + x + xx] = data[yy * w + xx];
    }
};
