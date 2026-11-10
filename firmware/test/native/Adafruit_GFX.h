#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
class Adafruit_GFX {
public:
    Adafruit_GFX(int w, int h) : w_(w), h_(h), pixels_(w * h) {}
    virtual ~Adafruit_GFX() = default;
    int width() const { return w_; }
    int height() const { return h_; }
    virtual void fillScreen(uint16_t color) { std::fill(pixels_.begin(), pixels_.end(), color); }
    void fillRect(int x, int y, int w, int h, uint16_t color) {
        for (int yy = std::max(0, y); yy < std::min(h_, y + h); ++yy)
            for (int xx = std::max(0, x); xx < std::min(w_, x + w); ++xx) pixels_[yy * w_ + xx] = color;
    }
    uint16_t* getBuffer() { return pixels_.data(); }
    const std::vector<uint16_t>& pixels() const { return pixels_; }
protected:
    int w_, h_;
    std::vector<uint16_t> pixels_;
};
class GFXcanvas16 : public Adafruit_GFX {
public:
    GFXcanvas16(int w, int h) : Adafruit_GFX(w, h) {}
};
