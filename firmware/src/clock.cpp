#include "clock.hpp"
#include "pins.hpp"
#include "settings.hpp"
#include <Arduino.h>
#include <Wire.h>


namespace 
{
    RTC_DS3231 rtc;
    DateTime cached(2026, 1, 1, 0, 0, 0);
    bool available = false;
    bool timeValid = false;
    uint32_t lastMs = 0;

    bool validDate(const DateTime& date)
    {
        return date.isValid() && date.year() >= 2024 && date.year() <= 2099;
    }
}


void Clock::begin()
{
    Wire.begin(Pins::RTC_SDA_PIN, Pins::RTC_SCL_PIN);
    Wire.setClock(100000);
    Wire.setTimeOut(20);
    retry();
}


bool Clock::retry()
{
    available = rtc.begin();
    timeValid = false;
    if (available) {
        cached = rtc.now();
        timeValid = !rtc.lostPower() && validDate(cached);
    }
    lastMs = millis();
    return available;
}


void Clock::update()
{
    if (!available || millis() - lastMs < Settings::RTC_INTERVAL_MS) return;
    lastMs = millis();
    Wire.beginTransmission(0x68);
    if (Wire.endTransmission() != 0) {
        available = timeValid = false;
        return;
    }
    cached = rtc.now();
    if (!validDate(cached)) timeValid = false;
}


bool Clock::connected()     { return available; }
bool Clock::valid()         { return available && timeValid; }
DateTime Clock::now()       { return cached; }


bool Clock::set(const DateTime& date)
{
    if (!available || !validDate(date)) return false;
    rtc.adjust(date);
    cached = rtc.now();
    timeValid = validDate(cached) && cached.unixtime() == date.unixtime();
    lastMs = millis();
    return timeValid;
}
