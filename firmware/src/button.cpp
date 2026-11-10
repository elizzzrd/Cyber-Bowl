#include "button.hpp"
#include "settings.hpp"
#include <Arduino.h>

void Button::begin()
{
    pinMode(pin_, INPUT_PULLUP);
    raw_ = stable_ = digitalRead(pin_);
    changedMs_ = millis();
}

void Button::update(uint32_t nowMs)
{
    // Events live for ONE loop iteration; unused presses cannot leak into another page.
    pressed_ = false;
    repeated_ = false;
    const bool reading = digitalRead(pin_);
    if (reading != raw_) {
        raw_ = reading;
        changedMs_ = nowMs;
    }
    if (nowMs - changedMs_ >= Settings::BUTTON_DEBOUNCE_MS && raw_ != stable_) {
        stable_ = raw_;
        pressed_ = !stable_;
        if (pressed_) { repeated_ = true; repeatAtMs_ = nowMs + 500; }
    }
    if (!stable_ && !pressed_ && static_cast<int32_t>(nowMs - repeatAtMs_) >= 0) {
        repeated_ = true;
        repeatAtMs_ = nowMs + 100;
    }
}
