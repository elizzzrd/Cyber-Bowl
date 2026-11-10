#include "settings.hpp"
#include <cmath>
#include <algorithm>


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

 
    bool validScales(const Settings::Data& d)
    {
        return validScale(d.foodScale) && validScale(d.waterScale);
    }

    bool validLimits(const Settings::Data& d)
    {
        return inRange(d.foodPortionG, 1, 200)                     && 
               inRange(d.waterPortionMl, 1, 200)                   &&
               inRange(d.foodBowlMaxG, 10, 2000)                   && 
               inRange(d.waterBowlMaxG, 10, 2000)                  &&
               inRange(d.pumpMsPerMl, 10, 20000)                   &&
               d.feedTimeoutMs >= 5000     && d.feedTimeoutMs <= 180000  &&
               d.waterTimeoutMs >= 1000    && d.waterTimeoutMs <= 300000 &&
               d.stepHalfPeriodUs >= 200   && d.stepHalfPeriodUs <= 10000 &&
               d.jogSteps >= 1             && d.jogSteps <= 20000  &&
               d.forwardSteps >= 1         && d.forwardSteps <= 20000 &&
               d.reverseSteps < d.forwardSteps                     &&
               d.brightnessPercent >= 10   && d.brightnessPercent <= 100;
    }

    bool validScheduleSlot(const Settings::ScheduleSlot& slot)
    {
        if (slot.hour > 23 || slot.minute > 59) return false;
        if (!validPortions(slot.foodG, slot.waterMl)) return false;
        if (slot.enabled && slot.foodG == 0 && slot.waterMl == 0) return false;
        return true;
    }

    bool validSchedule(const Settings::Data& d)
    {
        for (const auto& slot : d.schedule) {
            if (!validScheduleSlot(slot)) return false;
        }
        return true;
    }

    bool validOneShot(const Settings::Data& d)
    {
        if (!validPortions(d.oneShot.foodG, d.oneShot.waterMl)) return false;
        if (!d.oneShot.enabled) return true;
        
        return d.oneShot.dueEpoch > 0 && (d.oneShot.foodG > 0 || d.oneShot.waterMl > 0);
    }
}


Settings::Data Settings::defaults()
{
    Data d{};
    
    d.foodScale.factor = DEFAULT_FOOD_SCALE_FACTOR;
    d.foodScale.calibrated = true;
    d.foodScale.tared = true;
    
    d.waterScale.factor = DEFAULT_WATER_SCALE_FACTOR;
    d.waterScale.calibrated = true;
    d.waterScale.tared = true;

    d.schedule[1].hour = 20;
    d.schedule[2].hour = 12;
    d.schedule[3].hour = 16;
    
    return d;
}

bool Settings::valid(const Data& d)
{
    return validScales(d)   &&
           validLimits(d)   &&
           validSchedule(d) &&
           validOneShot(d);
}