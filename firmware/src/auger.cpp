#include "auger.hpp"
#include "pins.hpp"
#include <Arduino.h>
#include <esp_arduino_version.h>
#include <soc/soc.h>
#include <soc/gpio_reg.h>

namespace {
    hw_timer_t* timer = nullptr;
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    volatile bool active = false;
    volatile bool high = false;
    volatile bool timeout = false;
    volatile uint32_t pulsesDone = 0;
    volatile uint32_t pulseLimit = 0;
    volatile uint32_t ticksDone = 0;
    volatile uint32_t tickLimit = 0;

    inline void IRAM_ATTR writePin(uint8_t pin, bool value)
    {
        if (pin < 32) REG_WRITE(value ? GPIO_OUT_W1TS_REG : GPIO_OUT_W1TC_REG, 1u << pin);
        else REG_WRITE(value ? GPIO_OUT1_W1TS_REG : GPIO_OUT1_W1TC_REG, 1u << (pin - 32));
    }

    void IRAM_ATTR haltOutput()
    {
        writePin(Pins::STEPPER_STEP_PIN, false);
        writePin(Pins::STEPPER_ENABLE_PIN, true);
        high = false;
        active = false;
    }

    void IRAM_ATTR onTimer()
    {
        portENTER_CRITICAL_ISR(&mux);
        if (active) {
            ++ticksDone;
            if (ticksDone >= tickLimit) {
                timeout = true;
                haltOutput();
            } else {
                high = !high;
                writePin(Pins::STEPPER_STEP_PIN, high);
                if (!high) {
                    ++pulsesDone;
                    if (pulseLimit != 0 && pulsesDone >= pulseLimit) haltOutput();
                }
            }
        }
        portEXIT_CRITICAL_ISR(&mux);
    }

    void disableTimer()
    {
        if (!timer) return;
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
        timerStop(timer);
    #else
        timerAlarmDisable(timer);
    #endif
    }
}


bool Auger::begin()
{
    digitalWrite(Pins::STEPPER_ENABLE_PIN, HIGH);
    pinMode(Pins::STEPPER_ENABLE_PIN, OUTPUT);
    pinMode(Pins::STEPPER_STEP_PIN, OUTPUT);
    pinMode(Pins::STEPPER_DIR_PIN, OUTPUT);
    digitalWrite(Pins::STEPPER_STEP_PIN, LOW);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    timer = timerBegin(1000000);
    if (timer) timerAttachInterrupt(timer, &onTimer);
#else
    timer = timerBegin(0, 80, true);
    if (timer) timerAttachInterrupt(timer, &onTimer, true);
#endif
    disableTimer();
    return timer != nullptr;
}


bool Auger::run(bool directionHigh, uint32_t halfPeriodUs, uint32_t pulses, uint32_t timeoutMs)
{
    if (!timer || running() || halfPeriodUs < 200 || halfPeriodUs > 10000 ||
        timeoutMs == 0 || timeoutMs > 180000) return false;
    stop();
    digitalWrite(Pins::STEPPER_DIR_PIN, directionHigh ? HIGH : LOW);
    digitalWrite(Pins::STEPPER_ENABLE_PIN, LOW);
    delayMicroseconds(10); // A4988 enable/direction setup, not a loop delay.
    portENTER_CRITICAL(&mux);
    pulsesDone = 0;
    pulseLimit = pulses;
    ticksDone = 0;
    tickLimit = static_cast<uint32_t>(static_cast<uint64_t>(timeoutMs) * 1000 / halfPeriodUs);
    timeout = false;
    high = false;
    active = true;
    portEXIT_CRITICAL(&mux);
    timerWrite(timer, 0);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    timerAlarm(timer, halfPeriodUs, true, 0);
    timerStart(timer);
#else
    timerAlarmWrite(timer, halfPeriodUs, true);
    timerAlarmEnable(timer);
#endif
    return true;
}


void Auger::stop()
{
    disableTimer();
    portENTER_CRITICAL(&mux);
    haltOutput();
    portEXIT_CRITICAL(&mux);
}


bool Auger::running()
{
    portENTER_CRITICAL(&mux); const bool result = active; portEXIT_CRITICAL(&mux); return result;
}


bool Auger::timedOut()
{
    portENTER_CRITICAL(&mux); const bool result = timeout; portEXIT_CRITICAL(&mux); return result;
}


uint32_t Auger::steps()
{
    portENTER_CRITICAL(&mux); const uint32_t result = pulsesDone; portEXIT_CRITICAL(&mux); return result;
}