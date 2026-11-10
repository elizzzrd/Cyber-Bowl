#pragma once
#include <cstddef>
#include <cstdint>


namespace Settings 
{
    constexpr uint32_t BUTTON_DEBOUNCE_MS         = 30;
    constexpr uint32_t RTC_INTERVAL_MS            = 1000;
    constexpr uint32_t SENSOR_STALE_MS            = 2000;

    constexpr uint8_t  CAPTURE_SAMPLES            = 10;
    constexpr uint32_t CAPTURE_TIMEOUT_MS         = 3000;

    constexpr float    DEFAULT_FOOD_SCALE_FACTOR  = 194.0f;
    constexpr float    DEFAULT_WATER_SCALE_FACTOR = 178.0f;

    constexpr int32_t  WATER_LEVEL_THRESHOLD      = 1650;

    constexpr int32_t  HX711_RAW_MAX              = 0x7FFFFF;
    constexpr int32_t  HX711_RAW_MIN              = -0x800000;
    constexpr uint32_t SLOW_SENSOR_INTERVAL_MS    = 100;
    constexpr float    MIN_VALID_FACTOR           = 0.01f;

    constexpr size_t SCHEDULE_COUNT = 4;
    constexpr uint32_t DISPLAY_INTERVAL_MS = 100;
    constexpr float MAX_FOOD_PORTION_G = 200.0f;
    constexpr float MAX_WATER_PORTION_ML = 200.0f;
    constexpr float SCALE_ZERO_TOLERANCE_G = 5.0f;
    constexpr uint32_t HOPPER_EMPTY_TIMEOUT_MS = 2000;
    constexpr float WATER_DELIVERY_MIN_FRACTION = 0.9f;
    constexpr uint32_t US_PER_MS = 1000;
    constexpr uint32_t MS_PER_SECOND = 1000;
    constexpr uint32_t STEPPER_HALVES_PER_PULSE = 2;
    constexpr uint32_t AUGER_TEST_MAX_MS = 15000;
    constexpr uint32_t AUGER_TEST_TIMEOUT_MS = 16000;
    constexpr uint32_t PUMP_TEST_MIN_MS = 1000;
    constexpr uint32_t PUMP_TEST_MAX_MS = 60000;
    constexpr uint32_t ONE_SHOT_WINDOW_SECONDS = 60;
    constexpr uint32_t SECONDS_PER_DAY = 86400;
    constexpr uint32_t DATE_KEY_YEAR_MULTIPLIER = 10000;
    constexpr uint32_t DATE_KEY_MONTH_MULTIPLIER = 100;
    constexpr size_t CONTROLLER_TEXT_CAPACITY = 112;

    struct ScaleCalibration {
        float factor = 0.0f;
        int32_t offset = 0;
        bool calibrated = false;
        bool tared = false;
    };

    struct ScheduleSlot {
        uint8_t hour = 0;
        uint8_t minute = 0;
        float foodG = 0;
        float waterMl = 0;
        bool enabled = false;
    };

    struct OneShot {
        float foodG = 0;
        float waterMl = 0;
        uint32_t dueEpoch = 0;
        bool enabled = false;
    };

    struct Data {
        ScaleCalibration foodScale;
        ScaleCalibration waterScale;
        
        float foodPortionG        = 20.0f;
        float waterPortionMl      = 50.0f;
        float foodBowlMaxG        = 100.0f;
        float waterBowlMaxG       = 200.0f;
        float pumpMsPerMl         = 100.0f;
        
        uint32_t feedTimeoutMs    = 30000;
        uint32_t waterTimeoutMs   = 30000;
        uint32_t stepHalfPeriodUs = 1000;
        
        uint32_t jogSteps         = 100;
        uint32_t forwardSteps     = 200;
        uint32_t reverseSteps     = 100;
        
        uint8_t brightnessPercent = 100;
        
        bool stepperDirectionHigh = true;
        bool invertDisplay = false;
        bool foodPresentLow = true;
        
        ScheduleSlot schedule[SCHEDULE_COUNT];
        OneShot oneShot;
        uint32_t lastRunDay[SCHEDULE_COUNT] = {};
    };

    Data defaults();
    bool valid(const Data& d);
}
