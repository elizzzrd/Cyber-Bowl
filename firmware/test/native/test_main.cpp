#include "fakes.hpp"
#include "config_store.hpp"
#include "feeding_controller.hpp"
#include "pins.hpp"
#include "ui.hpp"
#include "button.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
int checks = 0;
void check(bool condition) { ++checks; assert(condition); }
void state(MachineState expected) { check(FeedingController::info().state == expected); }
void stopAndReset() { Fake::reset(); }
void draw() { Fake::advance(200); Fake::frame(); }
void press(uint8_t pin) { Fake::press(pin); draw(); }
}
int main()
{
    Config::begin();
    stopAndReset();
    // Configuration validation, negative calibration and failed persistence.
    auto c = Settings::defaults();
    c.foodScale.factor = -400; check(Settings::valid(c));
    c.reverseSteps = c.forwardSteps; check(!Settings::valid(c));
    const float oldPortion = Config::get().foodPortionG;
    c = Config::get(); c.foodPortionG = 45; Fake::nvsFail = true;
    check(!Config::commit(c)); check(Config::get().foodPortionG == oldPortion); Fake::nvsFail = false;

    // Food -> water uses the requested water amount, not a fixed automatic default.
    check(FeedingController::startCycle(20, 35)); state(MachineState::FEEDING);
    Fake::sensors.food.grams = 20; FeedingController::update(); state(MachineState::WATERING);
    check(FeedingController::info().requestedWaterMl == 35);
    Fake::sensors.water.grams = 35; FeedingController::update(); state(MachineState::IDLE);
    check(!Fake::pump && !Fake::auger);

    // The threshold-based tank sensor blocks water and stops an active pump.
    stopAndReset(); Fake::sensors.waterPresent = false;
    check(!FeedingController::startCycle(0, 10)); state(MachineState::ERROR); check(!Fake::pump);
    stopAndReset(); check(FeedingController::startCycle(0, 10));
    Fake::sensors.waterPresent = false; FeedingController::update(); state(MachineState::ERROR); check(!Fake::pump);

    // Stale/disconnected scale aborts an active feed.
    stopAndReset(); check(FeedingController::startCycle(20, 0));
    Fake::sensors.food.valid = false; FeedingController::update(); state(MachineState::ERROR); check(!Fake::auger);

    // A timed water run with no measured delivery is a fault, not success.
    stopAndReset(); check(FeedingController::startCycle(0, 10));
    Fake::advance(10000); FeedingController::update(); state(MachineState::ERROR); check(!Fake::pump);

    // BACK cancels the whole combined cycle; it cannot start water afterwards.
    stopAndReset(); check(FeedingController::startCycle(20, 50)); FeedingController::stop();
    Fake::sensors.food.grams = 100; FeedingController::update(); state(MachineState::IDLE); check(!Fake::pump);

    // Hopper fluctuations are debounced; persistent empty hopper aborts.
    stopAndReset(); check(FeedingController::startCycle(20, 0));
    Fake::sensors.foodAvailable = false; FeedingController::update();
    Fake::advance(1900); FeedingController::update(); state(MachineState::FEEDING);
    Fake::sensors.foodAvailable = true; FeedingController::update();
    Fake::sensors.foodAvailable = false; FeedingController::update();
    Fake::advance(2100); FeedingController::update(); state(MachineState::ERROR);

    // Unsigned elapsed time remains correct through millis() wraparound.
    stopAndReset(); Fake::ms = UINT32_MAX - 500;
    check(FeedingController::startCycle(20, 0)); Fake::advance(1000);
    FeedingController::update(); state(MachineState::FEEDING);
    Fake::advance(30000); FeedingController::update(); state(MachineState::ERROR);

    // Persist daily consumption before starting. No duplicate in the same minute or after reload.
    stopAndReset(); Fake::date = DateTime(2026, 10, 10, 8, 0, 0);
    c = Config::get(); c.schedule[0].enabled = true; c.schedule[0].foodG = 15; c.schedule[0].waterMl = 0;
    check(Config::commit(c)); FeedingController::updateSchedule(false); state(MachineState::FEEDING);
    check(Config::get().lastRunDay[0] == 20261010);
    Fake::sensors.food.grams = 15; FeedingController::update(); state(MachineState::IDLE);
    Fake::date = DateTime(2026, 10, 10, 8, 0, 2); FeedingController::updateSchedule(false); state(MachineState::IDLE);
    Config::begin(); Fake::date = DateTime(2026, 10, 10, 8, 0, 3);
    FeedingController::updateSchedule(false); state(MachineState::IDLE);
    Fake::date = DateTime(2026, 10, 11, 8, 0, 0); FeedingController::updateSchedule(false); state(MachineState::FEEDING);

    // A due event during manual operation is skipped, with NVS writes deferred until stopped.
    stopAndReset(); Fake::date = DateTime(2026, 10, 12, 8, 0, 0);
    c = Config::get(); c.schedule[0].enabled = true; c.schedule[0].foodG = 15; c.schedule[0].waterMl = 0; check(Config::commit(c));
    check(FeedingController::startCycle(10, 0));
    const auto writes = Fake::nvsWrites;
    FeedingController::updateSchedule(false); check(Fake::nvsWrites == writes);
    FeedingController::stop(); Fake::date = DateTime(2026, 10, 12, 8, 0, 1);
    FeedingController::updateSchedule(false); state(MachineState::IDLE); check(Config::get().lastRunDay[0] == 20261012);

    // Calibration/service mode does not dispense and consumes that occurrence.
    stopAndReset(); Fake::date = DateTime(2026, 10, 13, 8, 0, 0);
    c = Config::get(); c.schedule[0].enabled = true; c.schedule[0].foodG = 15; check(Config::commit(c));
    FeedingController::updateSchedule(true); state(MachineState::IDLE);
    Fake::date = DateTime(2026, 10, 13, 8, 0, 1); FeedingController::updateSchedule(false); state(MachineState::IDLE);

    // Invalid clock never starts an event; expired one-shot does not catch up.
    stopAndReset(); c = Config::get(); c.schedule[0].enabled = true; c.schedule[0].foodG = 15; check(Config::commit(c));
    Fake::date = DateTime(2026, 10, 14, 8, 0, 0); Fake::rtcValid = false;
    FeedingController::updateSchedule(false); state(MachineState::IDLE);
    Fake::rtcValid = true; c = Settings::defaults(); c.oneShot.enabled = true;
    c.oneShot.foodG = 15; c.oneShot.dueEpoch = Fake::date.unixtime() - 120; check(Config::commit(c));
    FeedingController::updateSchedule(false); state(MachineState::IDLE); check(!Config::get().oneShot.enabled);

    // Coincident slots cannot cause a second unexpected meal.
    stopAndReset(); Fake::date = DateTime(2026, 10, 15, 8, 0, 0);
    c = Config::get(); c.schedule[0].enabled = true; c.schedule[0].foodG = 15; c.schedule[0].waterMl = 0;
    c.schedule[1] = c.schedule[0]; check(Config::commit(c));
    FeedingController::updateSchedule(false); state(MachineState::FEEDING);
    check(Config::get().lastRunDay[0] == 20261015 && Config::get().lastRunDay[1] == 20261015);
    FeedingController::stop(); Fake::date = DateTime(2026, 10, 15, 8, 0, 3);
    FeedingController::updateSchedule(false); state(MachineState::IDLE);

    // On-time one-shot fires once and disables itself persistently.
    stopAndReset(); Fake::date = DateTime(2026, 10, 16, 9, 0, 0);
    c = Config::get(); c.oneShot.enabled = true; c.oneShot.foodG = 0; c.oneShot.waterMl = 12;
    c.oneShot.dueEpoch = Fake::date.unixtime() + 600; check(Config::commit(c));
    Fake::date = DateTime(c.oneShot.dueEpoch + 5); FeedingController::updateSchedule(false);
    state(MachineState::WATERING); check(!Config::get().oneShot.enabled);
    check(FeedingController::info().requestedWaterMl == 12);
    Fake::sensors.water.grams = 12; FeedingController::update(); state(MachineState::IDLE);
    Config::begin(); Fake::date = DateTime(c.oneShot.dueEpoch + 10);
    FeedingController::updateSchedule(false); state(MachineState::IDLE);

    // No false pending key event after release / screen changes.
    stopAndReset(); Button button(Pins::BUTTON_UP_PIN); button.begin();
    Fake::pins[Pins::BUTTON_UP_PIN] = LOW; button.update(Fake::ms); Fake::advance(31); button.update(Fake::ms);
    check(button.pressed()); button.update(Fake::ms); check(!button.pressed());
    Fake::pins[Pins::BUTTON_UP_PIN] = HIGH; button.update(Fake::ms); Fake::advance(31); button.update(Fake::ms);
    check(!button.pressed());

    // Render the REAL ui.cpp with a framebuffer display fake.
    stopAndReset(); Fake::date = DateTime(2026, 10, 8, 16, 51, 3);
    c = Config::get(); c.schedule[0].enabled = true; c.schedule[0].foodG = 15; c.schedule[0].hour = 20; check(Config::commit(c));
    Fake::sensors.food.grams = 12.4f; Fake::sensors.water.grams = 85.2f;
    check(Ui::begin()); draw(); Fake::screenshot("docs/home.ppm");
    const auto clears = Fake::fullScreenClears;
    const auto transfers = Fake::rowTransfers;
    draw(); check(Fake::fullScreenClears == clears); check(Fake::rowTransfers == transfers);
    Fake::date = DateTime(2026, 10, 8, 16, 51, 4); draw();
    check(Fake::fullScreenClears == clears); check(Fake::rowTransfers == transfers + 1);
    press(Pins::BUTTON_OK_PIN); Fake::screenshot("docs/menu.ppm");
    press(Pins::BUTTON_DOWN_PIN); press(Pins::BUTTON_OK_PIN); Fake::screenshot("docs/timers.ppm");
    press(Pins::BUTTON_BACK_PIN);
    press(Pins::BUTTON_DOWN_PIN); press(Pins::BUTTON_DOWN_PIN); press(Pins::BUTTON_DOWN_PIN);
    press(Pins::BUTTON_OK_PIN); Fake::screenshot("docs/settings.ppm");
    press(Pins::BUTTON_OK_PIN); Fake::screenshot("docs/auger.ppm");
    press(Pins::BUTTON_BACK_PIN); press(Pins::BUTTON_DOWN_PIN); press(Pins::BUTTON_DOWN_PIN);
    press(Pins::BUTTON_OK_PIN); press(Pins::BUTTON_OK_PIN); press(Pins::BUTTON_DOWN_PIN);
    press(Pins::BUTTON_OK_PIN); Fake::screenshot("docs/calibration.ppm");
    // Execute the actual UI calibration wizard: empty bowl -> known mass -> save.
    press(Pins::BUTTON_OK_PIN); Fake::completeCapture(200000); draw();
    check(Config::get().foodScale.offset == 0); // No settings changed after the zero stage.
    press(Pins::BUTTON_OK_PIN); press(Pins::BUTTON_OK_PIN);
    Fake::completeCapture(240000); draw();
    check(Config::get().foodScale.factor == Settings::DEFAULT_FOOD_SCALE_FACTOR); // Proposal is still uncommitted.
    press(Pins::BUTTON_OK_PIN);
    check(Config::get().foodScale.calibrated && Config::get().foodScale.tared);
    check(Config::get().foodScale.offset == 200000);
    check(Config::get().foodScale.factor == 400);
    // Re-tare preserves calibration and only changes the offset.
    press(Pins::BUTTON_BACK_PIN); press(Pins::BUTTON_OK_PIN); press(Pins::BUTTON_OK_PIN);
    Fake::completeCapture(200100); draw();
    check(Config::get().foodScale.offset == 200100 && Config::get().foodScale.factor == 400);
    // Execute the actual pump calibration UI: 10 s test -> measured 10 ml -> 1000 ms/ml.
    c = Config::get(); c.pumpMsPerMl = 952; check(Config::commit(c));
    press(Pins::BUTTON_BACK_PIN); press(Pins::BUTTON_BACK_PIN); press(Pins::BUTTON_BACK_PIN);
    press(Pins::BUTTON_DOWN_PIN); press(Pins::BUTTON_OK_PIN);
    press(Pins::BUTTON_DOWN_PIN); press(Pins::BUTTON_DOWN_PIN); press(Pins::BUTTON_OK_PIN);
    press(Pins::BUTTON_OK_PIN); state(MachineState::PUMP_TEST);
    Fake::advance(10000); Fake::frame(); draw(); state(MachineState::IDLE);
    press(Pins::BUTTON_DOWN_PIN); press(Pins::BUTTON_OK_PIN);
    check(std::fabs(Config::get().pumpMsPerMl - 1000) < 0.01f);
    std::cout << "PASS: " << checks << " assertions; real controller/config/buttons/UI, mocked hardware.\n";
}
