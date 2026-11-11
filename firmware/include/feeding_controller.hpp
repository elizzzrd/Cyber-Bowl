#pragma once
#include <cstdint>

enum class MachineState : uint8_t 
{ 
    IDLE, 
    FEEDING, 
    WATERING, 
    AUGER_TEST, 
    AUGER_HOLD, 
    PUMP_TEST, 
    ERROR 
};

struct ProcessInfo 
{
    MachineState state = MachineState::IDLE;
    const char* message = "Готово";
    
    float foodTargetG = 0;
    float requestedWaterMl = 0;

    uint32_t startedMs = 0;
    uint32_t durationMs = 0;
    uint32_t completedSteps = 0;
    
    bool lastSuccess = false;
};

namespace FeedingController 
{
    void begin();
    
    bool startCycle(float foodG, float waterMl);
    
    bool startAugerTest(bool directionHigh, uint32_t periodUs);
    bool startAugerHold();
    
    bool startPumpTest(uint32_t durationMs);
    
    void update();
    void updateSchedule(bool serviceMode);
    
    void stop();
    void acknowledge();
    bool busy();
    
    const ProcessInfo& info();
    const char* nextFeedingText();
}
