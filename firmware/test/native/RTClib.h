#pragma once
#include <cstdint>
#include <ctime>
class DateTime {
public:
    DateTime(uint16_t y = 2026, uint8_t m = 1, uint8_t d = 1, uint8_t h = 0, uint8_t n = 0, uint8_t s = 0)
        : y_(y), m_(m), d_(d), h_(h), n_(n), s_(s) {}
    explicit DateTime(uint32_t epoch) {
        std::time_t value = epoch;
        const auto date = *std::gmtime(&value);
        y_ = date.tm_year + 1900; m_ = date.tm_mon + 1; d_ = date.tm_mday;
        h_ = date.tm_hour; n_ = date.tm_min; s_ = date.tm_sec;
    }
    uint16_t year() const { return y_; }
    uint8_t month() const { return m_; }
    uint8_t day() const { return d_; }
    uint8_t hour() const { return h_; }
    uint8_t minute() const { return n_; }
    uint8_t second() const { return s_; }
    uint32_t unixtime() const {
        std::tm t{};
        t.tm_year = y_ - 1900; t.tm_mon = m_ - 1; t.tm_mday = d_;
        t.tm_hour = h_; t.tm_min = n_; t.tm_sec = s_;
        return static_cast<uint32_t>(timegm(&t));
    }
    bool isValid() const {
        if (m_ < 1 || m_ > 12 || d_ < 1 || d_ > 31 || h_ > 23 || n_ > 59 || s_ > 59) return false;
        DateTime roundTrip(unixtime());
        return roundTrip.year() == y_ && roundTrip.month() == m_ && roundTrip.day() == d_;
    }
private:
    uint16_t y_;
    uint8_t m_, d_, h_, n_, s_;
};
