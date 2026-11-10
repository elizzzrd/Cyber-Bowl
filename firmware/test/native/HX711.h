#pragma once
#include <map>
#include <deque>
#include <cstdint>
#include <cassert>
namespace Fake {
inline std::map<uint8_t, std::deque<int32_t>> hxSamples;
}
class HX711 {
public:
    void begin(uint8_t dout, uint8_t) { dout_ = dout; }
    bool is_ready() { return !Fake::hxSamples[dout_].empty(); }
    int32_t read() { assert(is_ready()); const auto v = Fake::hxSamples[dout_].front(); Fake::hxSamples[dout_].pop_front(); return v; }
private:
    uint8_t dout_ = 0;
};
