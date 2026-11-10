#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
constexpr int HIGH = 1, LOW = 0, INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2;
#define DISPLAY 0x1
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void pinMode(uint8_t pin, int mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, int value);
int analogRead(uint8_t pin);
void analogReadResolution(int bits);
double ledcSetup(int channel, int frequency, int bits);
void ledcAttachPin(uint8_t pin, int channel);
void ledcWrite(int channel, uint32_t duty);
struct FakeSerial {
    void begin(int) {}
    void println(const char*) {}
    template<class... A> void printf(const char*, A...) {}
};
extern FakeSerial Serial;
