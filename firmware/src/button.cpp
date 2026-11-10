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
    pressed_ = false;
    repeated_ = false;
    const bool reading = digitalRead(pin_);

    if (reading != raw_) 
    {
        raw_ = reading;
        changedMs_ = nowMs;
    }

    // fixing a click
    if ((nowMs - changedMs_) >= Settings::BUTTON_DEBOUNCE_MS && raw_ != stable_) 
    {
        stable_ = raw_;
        pressed_ = !stable_;            // LOW (pressed)
        
        if (pressed_) 
        { 
            repeated_ = true; 
            repeatAtMs_ = nowMs + 500; 
        }
    }

    // the button is being held down    
    if (!stable_ && !pressed_ && static_cast<int32_t>(nowMs - repeatAtMs_) >= 0) 
    {
        repeated_ = true;
        repeatAtMs_ = nowMs + 100;
    }
}
