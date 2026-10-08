#pragma once
#include <cstdint>
namespace Auger {
    bool begin();
    bool run(bool directionHigh, uint32_t halfPeriodUs, uint32_t pulses, uint32_t timeoutMs);
    void stop();
    bool running();
    bool timedOut();
    uint32_t steps();
}
