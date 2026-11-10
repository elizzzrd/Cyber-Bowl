#pragma once
#include <Arduino.h>


namespace Network 
{
    void begin();
    void update();
    void reconnectSoon();
    String apName();
    String apAddress();
    String stationAddress();
    bool connected();
    int signalDbm();
}
