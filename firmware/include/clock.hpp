#pragma once
#include <RTClib.h>
namespace Clock {
    void begin();
    void update();
    bool connected();
    bool valid();
    DateTime now();
    bool set(const DateTime& date);
    bool retry();
}
