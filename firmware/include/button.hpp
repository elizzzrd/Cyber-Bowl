#pragma once
#include <cstdint>

class Button 
{
    public:
        explicit Button(uint8_t pin) : pin_(pin) {}
        
        void begin();
        void update(uint32_t nowMs);
        bool pressed() const    { return pressed_; }
        bool repeated() const   { return repeated_; }
        
    private:
        uint8_t pin_;

        bool raw_ = true;           // last level read from GPIO
        bool stable_ = true;        // level stayed constant at least 30ms
        bool pressed_ = false;      
        bool repeated_ = false;

        uint32_t changedMs_ = 0;    // time of the last raw change
        uint32_t repeatAtMs_ = 0;   // 
};
