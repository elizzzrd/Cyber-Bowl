#include <Arduino.h>
#include "auger.hpp"
#include "pump.hpp"
#include "config_store.hpp"
#include "clock.hpp"
#include "sensors.hpp"
#include "feeding_controller.hpp"
#include "ui.hpp"

namespace
{
    constexpr uint32_t SERIAL_BAUD_RATE = 115200;
    constexpr uint32_t LOOP_YIELD_MS = 1;

    void printStartupStatus(bool motorReady, bool uiReady)
    {
        Serial.printf("Cyber Bowl: motor timer=%s, UI buffer=%s, saved settings=%s\n",
                      motorReady ? "OK" : "ERROR", uiReady ? "OK" : "ERROR",
                      Config::loadedSavedSettings() ? "YES" : "DEFAULTS");
    }
} // namespace

void setup()
{
    // Initialize actuator outputs before NVS, I2C or display initialization.
    Pump::begin();
    const bool motorReady = auger.begin();

    Serial.begin(SERIAL_BAUD_RATE);
    Config::begin();
    Sensors::begin();
    Clock::begin();
    FeedingController::begin();

    const bool uiReady = Ui::begin();
    printStartupStatus(motorReady, uiReady);
}

void loop()
{
    // Read user commands, then refresh the inputs used by the controller.
    Ui::handleButtons();
    Sensors::update();
    Clock::update();

    // Advance the active operation before considering a scheduled operation.
    FeedingController::update();
    FeedingController::updateSchedule(Ui::serviceMode());
    Ui::update();

    // Yield to system tasks. The hardware timer generates STEP independently.
    delay(LOOP_YIELD_MS);
}
