#include "fakes.hpp"
#include "config_store.hpp"
#include "feeding_controller.hpp"
#include "pump.hpp"
#include "auger.hpp"
#include "clock.hpp"
#include "ui.hpp"
#include "Adafruit_ST7789.h"
#include <fstream>
#include <algorithm>

FakeSerial Serial;
SPIClass SPI;
namespace Fake {
uint32_t ms = 2000;
int pins[64];
int adc = 1000;
SensorReadings sensors;
DateTime date(2026, 10, 8, 12, 0, 0);
bool rtcValid = true;
bool pump = false;
bool auger = false;
bool augerTimeout = false;
uint32_t augerSteps = 0;
bool nvsFail = false;
unsigned nvsWrites = 0;
unsigned fullScreenClears = 0;
unsigned rowTransfers = 0;
std::map<std::string, std::vector<uint8_t>> nvs;
Adafruit_ST7789*& display() { static Adafruit_ST7789* instance = nullptr; return instance; }
void reset()
{
    std::fill(std::begin(pins), std::end(pins), HIGH);
    FeedingController::stop();
    nvsFail = false;
    Config::commit(Settings::defaults());
    sensors = {};
    sensors.food.connected = sensors.food.valid = true;
    sensors.water.connected = sensors.water.valid = true;
    sensors.foodAvailable = true;
    sensors.tankValid = true;
    sensors.tankPercent = 75;
    rtcValid = true;
    augerTimeout = false;
    date = DateTime(date.unixtime() + 86401);
    advance(100);
}
void advance(uint32_t amount) { ms += amount; }
void frame() {
    Ui::handleButtons(); Clock::update(); Sensors::update();
    FeedingController::update(); FeedingController::updateSchedule(Ui::serviceMode()); Ui::update();
}
void press(uint8_t pin) {
    pins[pin] = LOW; frame(); advance(40); frame();
    pins[pin] = HIGH; frame(); advance(40); frame();
}
void screenshot(const char* filename)
{
    std::ofstream out(filename, std::ios::binary);
    out << "P6\n320 240\n255\n";
    for (uint16_t p : display()->pixels()) {
        const char rgb[] = {static_cast<char>(((p >> 11) & 31) * 255 / 31),
                            static_cast<char>(((p >> 5) & 63) * 255 / 63),
                            static_cast<char>((p & 31) * 255 / 31)};
        out.write(rgb, 3);
    }
}
}
uint32_t millis() { return Fake::ms; }
uint32_t micros() { return Fake::ms * 1000; }
void delay(uint32_t ms) { Fake::advance(ms); }
void delayMicroseconds(uint32_t) {}
void pinMode(uint8_t, int) {}
int digitalRead(uint8_t pin) { return Fake::pins[pin]; }
void digitalWrite(uint8_t pin, int value) { Fake::pins[pin] = value; }
int analogRead(uint8_t) { return Fake::adc; }
void analogReadResolution(int) {}
double ledcSetup(int, int, int) { return 5000; }
void ledcAttachPin(uint8_t, int) {}
void ledcWrite(int, uint32_t) {}
void Pump::begin() { off(); }
void Pump::on() { Fake::pump = true; }
void Pump::off() { Fake::pump = false; }
bool Pump::running() { return Fake::pump; }
bool Auger::begin() { return true; }
bool Auger::run(bool, uint32_t, uint32_t, uint32_t) { Fake::auger = true; Fake::augerSteps = 0; return true; }
void Auger::stop() { Fake::auger = false; }
bool Auger::running() { return Fake::auger; }
bool Auger::timedOut() { return Fake::augerTimeout; }
uint32_t Auger::steps() { return Fake::augerSteps; }
void Clock::begin() {}
void Clock::update() {}
bool Clock::connected() { return true; }
bool Clock::valid() { return Fake::rtcValid; }
DateTime Clock::now() { return Fake::date; }
bool Clock::set(const DateTime& date) { if (!date.isValid()) return false; Fake::date = date; return true; }
bool Clock::retry() { return true; }
#ifndef CYBER_TEST_REAL_SENSORS
void Sensors::begin() {}
void Sensors::update() {}
const SensorReadings& Sensors::readings() { return Fake::sensors; }
const ScaleReading& Sensors::scale(ScaleId id) { return id == ScaleId::FOOD ? Fake::sensors.food : Fake::sensors.water; }
namespace { CaptureState captureStatus = CaptureState::IDLE; int32_t captureRaw = 0; int32_t captureSpan = 0; }
bool Sensors::capture(ScaleId) { captureStatus = CaptureState::BUSY; return true; }
bool Sensors::captureTank() { captureStatus = CaptureState::BUSY; return true; }
void Sensors::cancelCapture() { captureStatus = CaptureState::IDLE; }
CaptureState Sensors::captureState() { return captureStatus; }
uint8_t Sensors::captureProgress() { return captureStatus == CaptureState::DONE ? 12 : 0; }
int32_t Sensors::capturedRaw() { return captureRaw; }
int32_t Sensors::capturedSpan() { return captureSpan; }
void Sensors::applySettings() {}
void Fake::completeCapture(int32_t raw, int32_t span) {
    captureRaw = raw; captureSpan = span; captureStatus = CaptureState::DONE;
}
#endif
