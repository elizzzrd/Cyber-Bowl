#include "pump.hpp"
#include "pins.hpp"
#include <Arduino.h>


namespace { bool active = false; }

void Pump::begin()
{
    digitalWrite(Pins::PUMP_PIN, LOW);
    pinMode(Pins::PUMP_PIN, OUTPUT);
    off();
}

void Pump::on() { 
    digitalWrite(Pins::PUMP_PIN, HIGH); 
    active = true; 
}

void Pump::off() { 
    digitalWrite(Pins::PUMP_PIN, LOW); 
    active = false; 
}

bool Pump::running() { return active; }
