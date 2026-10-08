#pragma once
#include <cstdint>

enum class ScaleId : uint8_t { FOOD, WATER };
struct ScaleReading {
    int32_t raw = 0;
    float grams = 0;
    uint32_t sampledMs = 0;
    bool connected = false;
    bool valid = false;
};
struct SensorReadings {
    ScaleReading food;
    ScaleReading water;
    bool foodAvailable = false;
    int irRaw = 1;
    int tankRaw = 0;
    int tankPercent = 0;
    bool tankValid = false;
};
enum class CaptureState : uint8_t { IDLE, BUSY, DONE, FAILED };

namespace Sensors {
    void begin();
    void update();
    const SensorReadings& readings();
    const ScaleReading& scale(ScaleId id);
    bool capture(ScaleId id);
    bool captureTank();
    void cancelCapture();
    CaptureState captureState();
    uint8_t captureProgress();
    int32_t capturedRaw();
    int32_t capturedSpan();
    void applySettings();
}
