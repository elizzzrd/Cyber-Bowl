#pragma once
#include <cstdint>

class Button {
    public:
        explicit Button(uint8_t pin) : pin_(pin) {}
        void begin();
        void update(uint32_t nowMs);
        bool pressed() const { return pressed_; }
        bool repeated() const { return repeated_; }
        
    private:
        uint8_t pin_;
        bool raw_ = true;
        bool stable_ = true;
        bool pressed_ = false;
        bool repeated_ = false;
        uint32_t changedMs_ = 0;
        uint32_t repeatAtMs_ = 0;
};
