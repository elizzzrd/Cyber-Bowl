#include "auger.hpp"
#include "pins.hpp"
#include "settings.hpp"

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <soc/soc.h>
#include <soc/gpio_reg.h>


// -----------------------------------------------------------------------------
// Auger — STEP pulse generator for A4988 driven by an ESP32 hardware timer.
//
// Invariants:
//   * active == true  -> timer is running, ISR toggles STEP.
//   * active == false -> STEP = LOW, ENABLE = HIGH (driver disabled).
//   * Motion stops on ANY of:
//       - stepsCompleted >= stepsRequested (when stepsRequested != 0),
//       - ticksElapsed   >= ticksUntilTimeout,
//       - external stop().
//   * All variables shared with the ISR are read/written under mux.
//   * One step = one full STEP period (HIGH -> LOW).
// -----------------------------------------------------------------------------

namespace
{
    namespace limits
    {
        constexpr uint32_t MIN_HALF_PERIOD_US = 200;      // max speed
        constexpr uint32_t MAX_HALF_PERIOD_US = 10000;    // min speed
        constexpr uint32_t MAX_TIMEOUT_MS     = 180000;   // 3 minutes
        constexpr uint32_t DIRECTION_SETUP_US = 20;
        constexpr uint32_t ENABLE_SETTLE_MS = 20;       // A4988 setup time
    }


    // ---- Shared state --------------------------------
    hw_timer_t*  timer = nullptr;
    portMUX_TYPE mux   = portMUX_INITIALIZER_UNLOCKED;

    volatile bool     active            = false;  
    volatile bool     stepLevelHigh     = false;  // current STEP pin level
    volatile bool     timeoutReached     = false;  // has the timeout fired
    volatile uint32_t stepsCompleted    = 0;      
    volatile uint32_t stepsRequested    = 0;      // steps needed (0 = unlimited)
    volatile uint32_t ticksElapsed      = 0;      // timer ticks elapsed
    volatile uint32_t ticksUntilTimeout = 0;      // ticks before timeout


    volatile uint32_t testTargetPeriodUs = 0;
    volatile uint32_t testTicksToEdge = 0;

    // ---- RAII critical sections --------------------------------------------
    struct CriticalSection
    {
        CriticalSection()  { portENTER_CRITICAL(&mux); }
        ~CriticalSection() { portEXIT_CRITICAL(&mux); }
        CriticalSection(const CriticalSection&)            = delete;
        CriticalSection& operator=(const CriticalSection&) = delete;
    };

    struct CriticalSectionISR
    {
        __attribute__((always_inline)) CriticalSectionISR()  { portENTER_CRITICAL_ISR(&mux); }
        __attribute__((always_inline)) ~CriticalSectionISR() { portEXIT_CRITICAL_ISR(&mux); }
        CriticalSectionISR(const CriticalSectionISR&)            = delete;
        CriticalSectionISR& operator=(const CriticalSectionISR&) = delete;
    };

    // ---- Direct GPIO write -----------------
    inline void IRAM_ATTR writePin(uint8_t pin, bool value)
    {
        if (pin < 32) {
            REG_WRITE(value ? GPIO_OUT_W1TS_REG  : GPIO_OUT_W1TC_REG,  1u << pin);
        } else {
            REG_WRITE(value ? GPIO_OUT1_W1TS_REG : GPIO_OUT1_W1TC_REG, 1u << (pin - 32));
        }
    }

    void IRAM_ATTR haltOutput()
    {
        writePin(Pins::STEPPER_STEP_PIN,   false);  // STEP idle = LOW
        writePin(Pins::STEPPER_ENABLE_PIN, true);   // driver disabled
        stepLevelHigh = false;
        active        = false;
    }

    // ---- One generator tick: toggle STEP and count the step ----------------
    void IRAM_ATTR emitStepEdge()
    {
        stepLevelHigh = !stepLevelHigh;
        writePin(Pins::STEPPER_STEP_PIN, stepLevelHigh);

        if (!stepLevelHigh) {                       // a full step has completed
            ++stepsCompleted;
            if (stepsRequested != 0 && stepsCompleted >= stepsRequested) {
                haltOutput();
            }
        }
    }

    void IRAM_ATTR emitTestEdge()
    {
        if (--testTicksToEdge != 0) return;

        emitStepEdge();
        if (!active) return;
        if (stepLevelHigh) {
            testTicksToEdge = 1;
            return;
        }

        const uint32_t stepIndex = stepsCompleted - 1;
        const uint32_t remaining = stepsRequested - 1 - stepIndex;
        uint32_t ramp = stepIndex < remaining ? stepIndex : remaining;
        if (ramp > Settings::AUGER_TEST_RAMP_STEPS)
            ramp = Settings::AUGER_TEST_RAMP_STEPS;
        const uint32_t startPeriod = Settings::AUGER_TEST_PERIOD_US[0];
        const uint32_t period = startPeriod -
            (startPeriod - testTargetPeriodUs) * ramp / Settings::AUGER_TEST_RAMP_STEPS;
        // Round to the nearest timer tick and subtract the preceding HIGH tick.
        testTicksToEdge = (period + Settings::AUGER_TEST_TICK_US / 2) /
                          Settings::AUGER_TEST_TICK_US - 1;
    }

    // ---- Timer ISR ---------------------------------------------------------
    void IRAM_ATTR onTimer()
    {
        CriticalSectionISR lock;
        if (!active) return;

        if (++ticksElapsed >= ticksUntilTimeout) {
            timeoutReached = true;
            haltOutput();
        } else if (testTargetPeriodUs != 0) {
            emitTestEdge();
        } else {
            emitStepEdge();
        }
    }

    // ---- Timer API adapter ----------------------------------
    namespace timer_api
    {
        inline hw_timer_t* create()
        {
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            hw_timer_t* t = timerBegin(1000000);            // 1 tick = 1 us
            if (t) timerAttachInterrupt(t, &onTimer);
        #else
            hw_timer_t* t = timerBegin(0, 80, true);        // 80 MHz / 80 = 1 us
            if (t) timerAttachInterrupt(t, &onTimer, true);
        #endif
            return t;
        }

        inline void stop(hw_timer_t* t)
        {
            if (!t) return;
            timerStop(t);
        #if ESP_ARDUINO_VERSION_MAJOR < 3
            timerAlarmDisable(t);
        #endif
        }

        inline void start(hw_timer_t* t, uint32_t periodUs)
        {
            if (!t) return;
            timerWrite(t, 0);
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            timerAlarm(t, periodUs, true, 0);
        #else
            timerAlarmWrite(t, periodUs, true);
            timerAlarmEnable(t);
        #endif
            timerStart(t);
        }
    } 

    void initPins()
    {
        digitalWrite(Pins::STEPPER_ENABLE_PIN, HIGH);
        pinMode(Pins::STEPPER_ENABLE_PIN, OUTPUT);
        pinMode(Pins::STEPPER_STEP_PIN,   OUTPUT);
        pinMode(Pins::STEPPER_DIR_PIN,    OUTPUT);
        digitalWrite(Pins::STEPPER_STEP_PIN, LOW);
    }

} 
// ------------------------------------------------------------------------------------


Auger auger;

bool Auger::begin()
{
    initPins();

    timer = timer_api::create();
    timer_api::stop(timer);             // created but wait until run()
    return timer != nullptr;
}


bool Auger::run(bool     directionHigh, 
                uint32_t halfPeriodUs,
                uint32_t pulses, 
                uint32_t timeoutMs)
{
    if (!isValidRequest(halfPeriodUs, timeoutMs) || running()) {
        return false;
    }

    stop();
    configureDriver(directionHigh);
    prepareCounters(pulses, halfPeriodUs, timeoutMs);
    timer_api::start(timer, halfPeriodUs);
    return true;
}


bool Auger::runTest(bool directionHigh, uint32_t periodUs)
{
    if (!timer || running() || periodUs < Settings::AUGER_TEST_PERIOD_US[3] ||
        periodUs > Settings::AUGER_TEST_PERIOD_US[0]) return false;

    stop();
    configureDriver(directionHigh);
    prepareCounters(Settings::AUGER_TEST_STEPS, Settings::AUGER_TEST_TICK_US,
                    Settings::AUGER_TEST_TIMEOUT_MS);
    {
        CriticalSection lock;
        testTargetPeriodUs = periodUs;
        testTicksToEdge = 1;
    }
    timer_api::start(timer, Settings::AUGER_TEST_TICK_US);
    return true;
}

void Auger::hold()
{
    stop();
    digitalWrite(Pins::STEPPER_ENABLE_PIN, LOW);
}


void Auger::stop()
{
    timer_api::stop(timer);
    
    // reset the outputs
    CriticalSection lock;
    haltOutput();
}


bool Auger::running()
{
    CriticalSection lock;
    return active;
}


bool Auger::timedOut()
{
    CriticalSection lock;
    return timeoutReached;
}


uint32_t Auger::steps()
{
    CriticalSection lock;
    return stepsCompleted;
}


// --------------------------------------------------------------------------------------------
bool Auger::isValidRequest(uint32_t halfPeriodUs, uint32_t timeoutMs) const
{
    return timer != nullptr
        && halfPeriodUs >= limits::MIN_HALF_PERIOD_US
        && halfPeriodUs <= limits::MAX_HALF_PERIOD_US
        && timeoutMs > 0
        && timeoutMs <= limits::MAX_TIMEOUT_MS;
}


void Auger::configureDriver(bool directionHigh)
{
    digitalWrite(Pins::STEPPER_DIR_PIN, directionHigh ? HIGH : LOW);
    delayMicroseconds(limits::DIRECTION_SETUP_US);
    digitalWrite(Pins::STEPPER_ENABLE_PIN, LOW);
    delay(limits::ENABLE_SETTLE_MS);
}


void Auger::prepareCounters(uint32_t pulses, uint32_t halfPeriodUs, uint32_t timeoutMs)
{
    CriticalSection lock;

    testTargetPeriodUs = 0;
    stepsCompleted = 0;
    stepsRequested = pulses;
    ticksElapsed   = 0;
    // uint64_t so that timeoutMs*1000 (up to 1.8e8) cannot overflow uint32_t.
    ticksUntilTimeout = static_cast<uint32_t>(
        static_cast<uint64_t>(timeoutMs) * 1000u / halfPeriodUs);

    timeoutReached = false;
    stepLevelHigh = false;
    active        = true;
}