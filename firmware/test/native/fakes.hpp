#pragma once
#include "Arduino.h"
#include "RTClib.h"
#include "sensors.hpp"
#include <map>
#include <string>
#include <vector>
namespace Fake {
extern uint32_t ms;
extern int pins[64];
extern int adc;
extern SensorReadings sensors;
extern DateTime date;
extern bool rtcValid;
extern bool pump;
extern bool auger;
extern bool augerTimeout;
extern uint32_t augerSteps;
extern bool nvsFail;
extern unsigned nvsWrites;
extern unsigned fullScreenClears;
extern unsigned rowTransfers;
extern std::map<std::string, std::vector<uint8_t>> nvs;
void reset();
void advance(uint32_t amount);
void frame();
void press(uint8_t pin);
void screenshot(const char* filename);
void completeCapture(int32_t raw, int32_t span = 0);
}
