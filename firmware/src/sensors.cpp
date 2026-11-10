#include "sensors.hpp"
#include "settings.hpp"
#include "config_store.hpp"
#include "pins.hpp"
#include <Arduino.h>
#include <HX711.h>
#include <algorithm>
#include <cmath>
#include <climits>

namespace 
{
    HX711 food;
    HX711 water;

    SensorReadings data;
    uint32_t lastOtherMs = 0;

    bool foodSeen = false;
    bool waterSeen = false;

    CaptureState captureStatus = CaptureState::IDLE;
    ScaleId captureId = ScaleId::FOOD;

    bool tankCapture = false;

    uint32_t captureStartedMs = 0;
    uint8_t captureCount = 0;
    int64_t rawSum = 0;
    int32_t rawMin = 0;
    int32_t rawMax = 0;
    int32_t resultRaw = 0;

    void readScale(HX711& hx, ScaleReading& value, bool& seen, ScaleId id, uint32_t now)
    {
        if (hx.is_ready()) {
            const int32_t raw = hx.read(); // Exactly one ready sample, never read_average/tare.
            if (raw != 0x7FFFFF && raw != -0x800000) {
                value.raw = raw;
                value.sampledMs = now;
                seen = true;

                if (captureStatus == CaptureState::BUSY && !tankCapture && id == captureId) {
                    rawSum += raw;
                    rawMin = std::min(rawMin, raw);
                    rawMax = std::max(rawMax, raw);
                    if (++captureCount == Settings::CAPTURE_SAMPLES) {
                        resultRaw = static_cast<int32_t>(rawSum / captureCount);
                        captureStatus = CaptureState::DONE;
                    }
                }
            }
        }

        value.connected = seen && now - value.sampledMs <= Settings::SENSOR_STALE_MS;
        const auto& c = id == ScaleId::FOOD ? Config::get().foodScale : Config::get().waterScale;
        value.grams = static_cast<float>(static_cast<int64_t>(value.raw) - c.offset) / c.factor;
        value.valid = value.connected && c.calibrated && c.tared && std::isfinite(value.grams);
    }
}

void Sensors::begin()
{
    pinMode(Pins::IR_SENSOR_PIN, INPUT_PULLUP);
    pinMode(Pins::WATER_LEVEL_PIN, INPUT);
    
    analogReadResolution(12);
    
    food.begin(Pins::FOOD_HX_DT_PIN, Pins::FOOD_HX_SCK_PIN);
    water.begin(Pins::WATER_HX_DT_PIN, Pins::WATER_HX_SCK_PIN);
    
    // A disconnected DOUT must not float LOW and look like a connected HX711.
    pinMode(Pins::FOOD_HX_DT_PIN, INPUT_PULLUP);
    pinMode(Pins::WATER_HX_DT_PIN, INPUT_PULLUP);
}


void Sensors::update()
{
    const uint32_t now = millis();

    readScale(food, data.food, foodSeen, ScaleId::FOOD, now);
    readScale(water, data.water, waterSeen, ScaleId::WATER, now);

    if (captureStatus == CaptureState::BUSY && now - captureStartedMs > Settings::CAPTURE_TIMEOUT_MS)
        captureStatus = CaptureState::FAILED;
    
    if (now - lastOtherMs >= 100) 
    {
        lastOtherMs = now;
        
        data.irRaw = digitalRead(Pins::IR_SENSOR_PIN);
        data.foodAvailable = data.irRaw == (Config::get().foodPresentLow ? LOW : HIGH);
        data.tankRaw = analogRead(Pins::WATER_LEVEL_PIN);
        
        if (captureStatus == CaptureState::BUSY && tankCapture) {
            rawSum += data.tankRaw;
            rawMin = std::min(rawMin, static_cast<int32_t>(data.tankRaw));
            rawMax = std::max(rawMax, static_cast<int32_t>(data.tankRaw));
        
            if (++captureCount == Settings::CAPTURE_SAMPLES) {
                resultRaw = static_cast<int32_t>(rawSum / captureCount);
                captureStatus = CaptureState::DONE;
            }
        }
        
        const auto& c = Config::get();
        data.tankValid = c.tankCalibrated && std::abs(c.tankRawFull - c.tankRawEmpty) >= 50;
        
        if (data.tankValid) {
            const float percent = 100.0f * (data.tankRaw - c.tankRawEmpty) /
                                  (c.tankRawFull - c.tankRawEmpty);
            data.tankPercent = std::max(0, std::min(100, static_cast<int>(percent)));
        }
    }
}


const SensorReadings& Sensors::readings()      { return data; }
const ScaleReading& Sensors::scale(ScaleId id) { return id == ScaleId::FOOD ? data.food : data.water; }

bool Sensors::capture(ScaleId id)
{
    if (captureStatus == CaptureState::BUSY || !scale(id).connected) 
        return false;
    
    captureId = id;
    tankCapture = false;
    captureStatus = CaptureState::BUSY;
    captureStartedMs = millis();
    captureCount = 0;
    rawSum = 0;
    rawMin = INT32_MAX;
    rawMax = INT32_MIN;

    return true;
}


bool Sensors::captureTank()
{
    if (captureStatus == CaptureState::BUSY) 
        return false;
    
    captureStatus = CaptureState::BUSY;
    tankCapture = true;
    captureStartedMs = millis();
    captureCount = 0;
    rawSum = 0;
    rawMin = INT32_MAX;
    rawMax = INT32_MIN;

    return true;
}


void Sensors::cancelCapture()           { captureStatus = CaptureState::IDLE; }
CaptureState Sensors::captureState()    { return captureStatus; }
uint8_t Sensors::captureProgress()      { return captureCount; }
int32_t Sensors::capturedRaw()          { return resultRaw; }
int32_t Sensors::capturedSpan()         { return rawMax - rawMin; }


void Sensors::applySettings()
{
    // Recompute from the cached raw sample immediately after tare/calibration.
    const auto& c = Config::get();
    data.food.grams = static_cast<float>(static_cast<int64_t>(data.food.raw) - c.foodScale.offset) / c.foodScale.factor;
    data.water.grams = static_cast<float>(static_cast<int64_t>(data.water.raw) - c.waterScale.offset) / c.waterScale.factor;
    data.food.valid = data.food.connected && c.foodScale.calibrated && c.foodScale.tared;
    data.water.valid = data.water.connected && c.waterScale.calibrated && c.waterScale.tared;
}
