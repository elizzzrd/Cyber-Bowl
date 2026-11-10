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
    
    // Time of the last poll of "slow" sensors (IR and water level)
    uint32_t lastSlowPollMs = 0;

    // Flags: whether we have seen at least one valid sample since startup.
    bool foodEverSampled = false;
    bool waterEverSampled = false;

    CaptureState captureStatus = CaptureState::IDLE;
    ScaleId captureTarget = ScaleId::FOOD;
    bool isCapturingTank = false;
    
    uint32_t captureStartedMs = 0;
    uint8_t  captureSamplesCount = 0;
    int64_t  captureSum = 0;
    int32_t  captureMin = 0;
    int32_t  captureMax = 0;
    int32_t  captureResult = 0;


    bool isRawValid(int32_t raw)
    {
        return raw != Settings::HX711_RAW_MAX && raw != Settings::HX711_RAW_MIN;
    }

    bool isSensorConnected(const ScaleReading& value, uint32_t now, bool seen)
    {
        return seen && (now - value.sampledMs <= Settings::SENSOR_STALE_MS);
    }

    float getFactor(ScaleId id, const Settings::ScaleCalibration& c)
    {
        if (std::isfinite(c.factor) && std::fabs(c.factor) >= Settings::MIN_VALID_FACTOR) {
            return c.factor;
        }
        return (id == ScaleId::FOOD) ? Settings::DEFAULT_FOOD_SCALE_FACTOR 
                                     : Settings::DEFAULT_WATER_SCALE_FACTOR;
    }

    void addSampleToCapture(int32_t raw)
    {
        captureSum += raw;
        captureMin = std::min(captureMin, raw);
        captureMax = std::max(captureMax, raw);

        if (++captureSamplesCount == Settings::CAPTURE_SAMPLES) {
            captureResult = static_cast<int32_t>(captureSum / captureSamplesCount);
            captureStatus = CaptureState::DONE;
        }
    }

    bool isCapturingThisScale(ScaleId id)
    {
        return captureStatus == CaptureState::BUSY && !isCapturingTank && id == captureTarget;
    }

    bool isCapturingTankNow()
    {
        return captureStatus == CaptureState::BUSY && isCapturingTank;
    }

    void recalculateScale(ScaleId id, ScaleReading& value, const Settings::ScaleCalibration& c)
    {
        const float factor = getFactor(id, c);
        value.grams = static_cast<float>(static_cast<int64_t>(value.raw) - c.offset) / factor;
        value.valid = value.connected && c.calibrated && c.tared && std::isfinite(value.grams);
    }

    void pollScale(HX711& hx, ScaleReading& value, bool& everSampled, ScaleId id, uint32_t now)
    {
        if (hx.is_ready()) {
            const int32_t raw = hx.read();
            if (isRawValid(raw)) {
                value.raw = raw;
                value.sampledMs = now;
                everSampled = true;

                if (isCapturingThisScale(id)) {
                    addSampleToCapture(raw);
                }
            }
        }

        value.connected = isSensorConnected(value, now, everSampled);
        const auto& c = (id == ScaleId::FOOD) ? Config::get().foodScale : Config::get().waterScale;
        recalculateScale(id, value, c);
    }

    void pollSlowSensors(uint32_t now)
    {
        if (now - lastSlowPollMs < Settings::SLOW_SENSOR_INTERVAL_MS) 
            return;

        lastSlowPollMs = now;

        // IR food sensor
        data.irRaw = digitalRead(Pins::IR_SENSOR_PIN);
        data.foodAvailable = (data.irRaw == (Config::get().foodPresentLow ? LOW : HIGH));

        // Analog water level sensor
        data.tankRaw = analogRead(Pins::WATER_LEVEL_PIN);
        data.waterPresent = (data.tankRaw > Settings::WATER_LEVEL_THRESHOLD);

        // Averaging for water level calibration
        if (isCapturingTankNow()) {
            addSampleToCapture(data.tankRaw);
        }
    }

    bool startCapture(bool forTank)
    {
        if (captureStatus == CaptureState::BUSY)
            return false;

        captureStatus = CaptureState::BUSY;
        isCapturingTank = forTank;
        captureStartedMs = millis();
        captureSamplesCount = 0;
        captureSum = 0;
        captureMin = INT32_MAX;
        captureMax = INT32_MIN;
        return true;
    }
}



void Sensors::begin()
{
    pinMode(Pins::IR_SENSOR_PIN, INPUT_PULLUP);
    pinMode(Pins::WATER_LEVEL_PIN, INPUT);
    
    analogReadResolution(12);
    
    food.begin(Pins::FOOD_HX_DT_PIN, Pins::FOOD_HX_SCK_PIN);
    water.begin(Pins::WATER_HX_DT_PIN, Pins::WATER_HX_SCK_PIN);
    
    // Pull-up for disconnected HX711 DOUT to avoid false readiness
    pinMode(Pins::FOOD_HX_DT_PIN, INPUT_PULLUP);
    pinMode(Pins::WATER_HX_DT_PIN, INPUT_PULLUP);
}

void Sensors::update()
{
    const uint32_t now = millis();

    pollScale(food, data.food, foodEverSampled, ScaleId::FOOD, now);
    pollScale(water, data.water, waterEverSampled, ScaleId::WATER, now);

    if (captureStatus == CaptureState::BUSY && now - captureStartedMs > Settings::CAPTURE_TIMEOUT_MS) {
        captureStatus = CaptureState::FAILED;
    }
    
    pollSlowSensors(now);
}

const SensorReadings& Sensors::readings()      { return data; }
const ScaleReading& Sensors::scale(ScaleId id) { return id == ScaleId::FOOD ? data.food : data.water; }

bool Sensors::capture(ScaleId id)
{
    if (!scale(id).connected) 
        return false;
        
    captureTarget = id;
    return startCapture(false);
}

bool Sensors::captureTank()
{
    return startCapture(true);
}

void Sensors::cancelCapture()           { captureStatus = CaptureState::IDLE; }
CaptureState Sensors::captureState()    { return captureStatus; }
uint8_t Sensors::captureProgress()      { return captureSamplesCount; }
int32_t Sensors::capturedRaw()          { return captureResult; }
int32_t Sensors::capturedSpan()         { return captureSamplesCount ? captureMax - captureMin : 0; }

void Sensors::applySettings()
{
    const auto& c = Config::get();
    
    recalculateScale(ScaleId::FOOD, data.food, c.foodScale);
    recalculateScale(ScaleId::WATER, data.water, c.waterScale);
}