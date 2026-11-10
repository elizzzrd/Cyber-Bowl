#include "fakes.hpp"
#include "config_store.hpp"
#include "pins.hpp"
#include "HX711.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    int checks = 0;
    auto check = [&](bool value) { ++checks; assert(value); };
    Config::begin(); Fake::reset(); Sensors::begin();
    Fake::hxSamples[Pins::FOOD_HX_DT_PIN].push_back(240000);
    Sensors::update();
    check(Sensors::readings().food.connected);
    check(!Sensors::readings().food.valid); // Uncalibrated counts cannot masquerade as grams.
    auto c = Config::get();
    c.foodScale.offset = 200000; c.foodScale.factor = 400;
    c.foodScale.tared = c.foodScale.calibrated = true;
    check(Config::commit(c)); Sensors::applySettings();
    check(Sensors::readings().food.grams == 100 && Sensors::readings().food.valid);
    Fake::advance(Settings::SENSOR_STALE_MS + 1); Sensors::update();
    check(!Sensors::readings().food.connected && !Sensors::readings().food.valid);
    check(!Sensors::capture(ScaleId::FOOD));
    Fake::hxSamples[Pins::FOOD_HX_DT_PIN].push_back(200000); Sensors::update();
    check(Sensors::capture(ScaleId::FOOD));
    for (int i = 0; i < Settings::CAPTURE_SAMPLES; ++i) {
        Fake::advance(100); Fake::hxSamples[Pins::FOOD_HX_DT_PIN].push_back(200000 + i); Sensors::update();
    }
    check(Sensors::captureState() == CaptureState::DONE);
    check(Sensors::capturedRaw() == 200005 && Sensors::capturedSpan() == 11);
    Sensors::cancelCapture(); check(Sensors::capture(ScaleId::FOOD));
    Fake::advance(Settings::CAPTURE_TIMEOUT_MS + 1); Sensors::update();
    check(Sensors::captureState() == CaptureState::FAILED);
    Sensors::cancelCapture();
    c.foodScale.factor = -400; check(Config::commit(c));
    Fake::hxSamples[Pins::FOOD_HX_DT_PIN].push_back(160000); Sensors::update();
    check(Sensors::readings().food.grams == 100 && Sensors::readings().food.valid);
    Fake::adc = 1234; check(Sensors::captureTank());
    for (int i = 0; i < Settings::CAPTURE_SAMPLES; ++i) { Fake::advance(100); Sensors::update(); }
    check(Sensors::captureState() == CaptureState::DONE && Sensors::capturedRaw() == 1234);
    c.tankCalibrated = true; c.tankRawEmpty = 3000; c.tankRawFull = 1000;
    check(Config::commit(c)); Fake::adc = 2000; Fake::advance(100); Sensors::update();
    check(Sensors::readings().tankValid && Sensors::readings().tankPercent == 50);
    Fake::adc = 4000; Fake::advance(100); Sensors::update(); check(Sensors::readings().tankPercent == 0);
    Fake::adc = 0; Fake::advance(100); Sensors::update(); check(Sensors::readings().tankPercent == 100);
    std::cout << "PASS: " << checks << " assertions; real sensor module with queued HX711/ADC input.\n";
}
