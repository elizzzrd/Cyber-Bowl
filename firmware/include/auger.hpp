#pragma once
#include <cstdint>


class Auger
{
    public:
        bool     begin();
        bool     run(bool     directionHigh, 
                     uint32_t halfPeriodUs,
                     uint32_t pulses, 
                     uint32_t timeoutMs);
        bool runTest(bool directionHigh, uint32_t periodUs);
        void hold();
        void     stop();
        bool     running();
        bool     timedOut();
        uint32_t steps();

    private:
        bool isValidRequest(uint32_t halfPeriodUs, uint32_t timeoutMs) const;
        void configureDriver(bool directionHigh);
        void prepareCounters(uint32_t pulses, uint32_t halfPeriodUs, uint32_t timeoutMs);
};

extern Auger auger;
