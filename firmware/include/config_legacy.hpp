#pragma once
#include "settings.hpp"

// Binary layout of NVS format v1. Field order and types must stay unchanged.
namespace ConfigStorage
{
    // Layout written by the last working version before the sensor refactor.
    struct LegacyScale
    {
        float factor;
        int32_t offset;
        bool tared;
        bool calibrated;
    };

    struct LegacySlot
    {
        bool enabled;
        uint8_t hour;
        uint8_t minute;
        float foodG;
        float waterMl;
    };

    struct LegacyOneShot
    {
        bool enabled;
        uint32_t dueEpoch;
        float foodG;
        float waterMl;
    };

    struct LegacyData
    {
        LegacyScale foodScale;
        LegacyScale waterScale;
        float foodPortionG;
        float waterPortionMl;
        float foodBowlMaxG;
        float waterBowlMaxG;
        float pumpMsPerMl;
        uint32_t feedTimeoutMs;
        uint32_t waterTimeoutMs;
        uint32_t stepHalfPeriodUs;
        uint32_t jogSteps;
        uint32_t forwardSteps;
        uint32_t reverseSteps;
        bool stepperDirectionHigh;
        bool foodPresentLow;
        bool lowWaterProtection;
        bool tankCalibrated;
        int32_t tankRawEmpty;
        int32_t tankRawFull;
        uint8_t minTankPercent;
        uint8_t brightnessPercent;
        bool invertDisplay;
        LegacySlot schedule[Settings::SCHEDULE_COUNT];
        LegacyOneShot oneShot;
        uint32_t lastRunDay[Settings::SCHEDULE_COUNT];
    };

} // namespace ConfigStorage
