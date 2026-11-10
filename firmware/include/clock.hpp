#pragma once
#include <RTClib.h>


namespace Clock 
{
    void begin();
    void update();
    bool connected() const;
    bool valid() const;
    DateTime now() const;
    bool set(const DateTime& date);
    bool retry();
}
