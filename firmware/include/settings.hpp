#pragma once
#include <cstddef>
#include <cstdint>

namespace Settings {
    constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
    constexpr uint32_t DISPLAY_INTERVAL_MS = 100;
    constexpr uint32_t SENSOR_STALE_MS = 1500;
    constexpr uint32_t CAPTURE_TIMEOUT_MS = 5000;
    constexpr uint8_t CAPTURE_SAMPLES = 12;
    constexpr uint32_t RTC_INTERVAL_MS = 1000;
    constexpr size_t SCHEDULE_COUNT = 4;

    struct ScaleCalibration {
        float factor = 400.0f;
        int32_t offset = 0;
        bool tared = false;
        bool calibrated = false;
    };

    struct FeedingSlot {
        bool enabled = false;
        uint8_t hour = 8;
        uint8_t minute = 0;
        float foodG = 30.0f;
        float waterMl = 50.0f;  // Zero disables this part of the cycle.
    };

    struct OneShot {
        bool enabled = false;
        uint32_t dueEpoch = 0;
        float foodG = 30.0f;
        float waterMl = 50.0f;
    };

    struct Data {
        ScaleCalibration foodScale;
        ScaleCalibration waterScale;
        float foodPortionG = 30.0f;
        float waterPortionMl = 50.0f;
        float foodBowlMaxG = 300.0f;
        float waterBowlMaxG = 300.0f;
        float pumpMsPerMl = 952.0f;
        uint32_t feedTimeoutMs = 30000;
        uint32_t waterTimeoutMs = 180000;
        uint32_t stepHalfPeriodUs = 800;
        uint32_t jogSteps = 200;
        uint32_t forwardSteps = 200;
        uint32_t reverseSteps = 0;
        bool stepperDirectionHigh = true;
        bool foodPresentLow = true;
        bool lowWaterProtection = false;
        bool tankCalibrated = false;
        int32_t tankRawEmpty = 0;
        int32_t tankRawFull = 2000;
        uint8_t minTankPercent = 5;
        uint8_t brightnessPercent = 80;
        bool invertDisplay = false;
        FeedingSlot schedule[SCHEDULE_COUNT];
        OneShot oneShot;
        uint32_t lastRunDay[SCHEDULE_COUNT] = {};
    };

    Data defaults();
    bool valid(const Data& data);
}
