#include "settings.hpp"
#include <cmath>

namespace 
{
    bool inRange(float value, float low, float high)
    {
        return std::isfinite(value) && value >= low && value <= high;
    }

    bool validScale(const Settings::ScaleCalibration& c)
    {
        return std::isfinite(c.factor) && std::fabs(c.factor) >= 0.01f &&
            std::fabs(c.factor) <= 1000000.0f && (!c.calibrated || c.tared);
    }

    bool validPortions(float food, float water)
    {
        return inRange(food, 0, 200) && inRange(water, 0, 200);
    }
}


Settings::Data Settings::defaults()
{
    Data d{};
    d.schedule[1].hour = 20;
    d.schedule[2].hour = 12;
    d.schedule[3].hour = 16;
    return d;
}

bool Settings::valid(const Data& d)
{
    if (!validScale(d.foodScale)                                            || 
        !validScale(d.waterScale)                                           ||
        !inRange(d.foodPortionG, 1, 200)                                    || 
        !inRange(d.waterPortionMl, 1, 200)                                  ||
        !inRange(d.foodBowlMaxG, 10, 2000)                                  || 
        !inRange(d.waterBowlMaxG, 10, 2000)                                 ||
        !inRange(d.pumpMsPerMl, 10, 20000)                                  ||
        d.feedTimeoutMs < 5000              || d.feedTimeoutMs > 180000     ||
        d.waterTimeoutMs < 1000             || d.waterTimeoutMs > 300000    ||
        d.stepHalfPeriodUs < 200            || d.stepHalfPeriodUs > 10000   ||
        d.jogSteps < 1                      || d.jogSteps > 20000           ||
        d.forwardSteps < 1                  || d.forwardSteps > 20000       ||
        d.reverseSteps >= d.forwardSteps    || d.minTankPercent > 50        ||
        d.brightnessPercent < 10            || d.brightnessPercent > 100    ||
        d.tankRawEmpty < 0                  || d.tankRawEmpty > 4095        ||
        d.tankRawFull < 0                   || d.tankRawFull > 4095         ||
        (d.tankCalibrated && std::abs(d.tankRawFull - d.tankRawEmpty) < 50) ||
        (d.lowWaterProtection && !d.tankCalibrated)) 
        { return false; }

    for (const auto& slot : d.schedule) {
        if (slot.hour > 23                              || 
            slot.minute > 59                            || 
            !validPortions(slot.foodG, slot.waterMl)    ||
            (slot.enabled && slot.foodG == 0 && slot.waterMl == 0)) 
        { return false; }
    }

    return validPortions(d.oneShot.foodG, d.oneShot.waterMl)            &&
                        (!d.oneShot.enabled  || (d.oneShot.dueEpoch > 0 &&
                        (d.oneShot.foodG > 0 || d.oneShot.waterMl > 0)));
}
