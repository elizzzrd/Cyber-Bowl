#include "auger.hpp"
#include "pins.hpp"

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
        constexpr uint32_t SETUP_DELAY_US     = 10;       // A4988 setup time
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

    // ---- Timer ISR ---------------------------------------------------------
    void IRAM_ATTR onTimer()
    {
        CriticalSectionISR lock;
        if (!active) return;

        if (++ticksElapsed >= ticksUntilTimeout) {
            timeoutReached = true;
            haltOutput();
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
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            timerStop(t);
        #else
            timerAlarmDisable(t);
        #endif
        }

        inline void start(hw_timer_t* t, uint32_t periodUs)
        {
            if (!t) return;
            timerWrite(t, 0);
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            timerAlarm(t, periodUs, true, 0);
            timerStart(t);
        #else
            timerAlarmWrite(t, periodUs, true);
            timerAlarmEnable(t);
        #endif
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
    digitalWrite(Pins::STEPPER_ENABLE_PIN, LOW);
    delayMicroseconds(limits::SETUP_DELAY_US);   // A4988 setup time
}


void Auger::prepareCounters(uint32_t pulses, uint32_t halfPeriodUs, uint32_t timeoutMs)
{
    CriticalSection lock;

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