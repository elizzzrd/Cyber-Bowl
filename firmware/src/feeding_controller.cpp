#include "feeding_controller.hpp"
#include "auger.hpp"
#include "pump.hpp"
#include "sensors.hpp"
#include "clock.hpp"
#include "config_store.hpp"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace 
{
    ProcessInfo process;

    char message[112] = "Готово";
    char nextText[112] = "Расписание выключено";

    float waterAfterFood = 0;
    float waterStartG = 0;

    uint32_t hopperEmptyMs = 0;
    bool hopperEmpty = false;

    bool reverseSegment = false;
    uint32_t finishedSegmentSteps = 0;

    uint32_t lastScheduleEpoch = 0;
    bool scheduleSeen = false;
    bool pendingSave = false;
    Settings::Data pendingConfig;

    void setMessage(const char* text)
    {
        snprintf(message, sizeof(message), "%s", text);
        process.message = message;
        Serial.println(text);
    }

    void fail(const char* text)
    {
        Pump::off(); 
        Auger::stop();
        
        waterAfterFood = 0;
        process.state = MachineState::ERROR;
        process.lastSuccess = false;
        
        setMessage(text);
    }

    void finish(const char* text, bool success = true)
    {
        Pump::off(); 
        Auger::stop();
        
        process.state = MachineState::IDLE;
        
        process.lastSuccess = success;
        setMessage(text);
    }

    bool checkFood(float amount)
    {
        const auto& s = Sensors::readings();
        
        if (!s.food.valid) {
            fail("Откалибруйте весы корма"); 
            return false; 
        }

        if (s.food.grams < -5) { 
            fail("Проверьте ноль весов корма"); 
            return false; 
        }

        if (!s.foodAvailable) { 
            fail("В бункере нет корма"); 
            return false; 
        }
        
        if (std::max(0.0f, s.food.grams) + amount > Config::get().foodBowlMaxG) {
            fail("Превышен лимит миски корма"); 
            return false;
        }

        return true;
    }

    bool checkTank()
    {
        if (Config::get().lowWaterProtection &&
            (!Sensors::readings().tankValid || 
            Sensors::readings().tankPercent <= Config::get().minTankPercent)) 
        {
            fail("В баке недостаточно воды"); 
            return false;
        }

        return true;
    }

    bool checkWater(float amount)
    {
        const auto& s = Sensors::readings();

        if (!s.water.valid) { 
            fail("Откалибруйте весы воды"); 
            return false; 
        }

        if (s.water.grams < -5) { 
            fail("Проверьте ноль весов воды"); 
            return false; 
        }

        if (std::max(0.0f, s.water.grams) + amount > Config::get().waterBowlMaxG) {
            fail("Превышен лимит миски воды"); 
            return false;
        }

        if (amount * Config::get().pumpMsPerMl > Config::get().waterTimeoutMs) {
            fail("Порция воды больше лимита времени"); 
            return false;
        }

        return checkTank();
    }

    void beginWater(float amount)
    {
        waterStartG = Sensors::readings().water.grams;
    
        process.requestedWaterMl = amount;
        process.durationMs = static_cast<uint32_t>(std::ceil(amount * Config::get().pumpMsPerMl));
        process.startedMs = millis();
        process.state = MachineState::WATERING;
    
        setMessage("Подача воды");
        Pump::on();
    }

    bool startSegment()
    {
        const auto& c = Config::get();
        const uint32_t elapsed = millis() - process.startedMs;
    
        if (elapsed >= c.feedTimeoutMs) 
            return false;
        
        return Auger::run(reverseSegment ? !c.stepperDirectionHigh : c.stepperDirectionHigh,
                          c.stepHalfPeriodUs, c.reverseSteps == 0 ? 0 :
                          (reverseSegment ? c.reverseSteps : c.forwardSteps),
                          c.feedTimeoutMs - elapsed);
    }

    uint32_t dayKey(const DateTime& date)
    {
        return date.year() * 10000u + date.month() * 100u + date.day();
    }

    void buildNextText()
    {
        if (!Clock::valid()) { 
            snprintf(nextText, sizeof(nextText), "Настройте часы RTC"); 
            return; 
        }
        
        const uint32_t epoch = Clock::now().unixtime();
        
        uint32_t earliest = UINT32_MAX;
        float grams = 0;
        float water = 0;
        
        const auto& c = Config::get();
        if (c.oneShot.enabled && c.oneShot.dueEpoch >= epoch) {
            earliest = c.oneShot.dueEpoch; 
            grams = c.oneShot.foodG; 
            water = c.oneShot.waterMl;
        }

        for (size_t i = 0; i < Settings::SCHEDULE_COUNT; ++i) {
            const auto& slot = c.schedule[i];
        
            if (!slot.enabled) 
                continue;
            
            const auto date = Clock::now();
            DateTime due(date.year(), date.month(), date.day(), slot.hour, slot.minute, 0);
            uint32_t when = due.unixtime();
        
            if (when < epoch || c.lastRunDay[i] == dayKey(date)) 
                when += 86400;
            if (when < earliest) { 
                earliest = when; 
                grams = slot.foodG; 
                water = slot.waterMl; 
            }
        }

        if (earliest == UINT32_MAX) 
            snprintf(nextText, sizeof(nextText), "Расписание выключено");
        else {
            DateTime due(earliest);
            snprintf(nextText, sizeof(nextText), "Далее %02u:%02u  %.0fг / %.0fмл", due.hour(), due.minute(), grams, water);
        }
    }
}


void FeedingController::begin()
{
    process.message = message;
    if (!Config::storageReady()) setMessage("Память настроек недоступна");
}

bool FeedingController::busy()
{
    return process.state != MachineState::IDLE && process.state != MachineState::ERROR;
}

const ProcessInfo& FeedingController::info() { return process; }
const char* FeedingController::nextFeedingText() { return nextText; }

bool FeedingController::startCycle(float foodG, float waterMl)
{
    if (process.state != MachineState::IDLE) 
        return false;
    
    if (!std::isfinite(foodG)        ||  !std::isfinite(waterMl)      || 
        foodG < 0 || foodG > 200     ||  waterMl < 0 || waterMl > 200 || 
        (foodG == 0 && waterMl == 0)) 
    {
        fail("Неверная порция"); 
        return false;
    }

    if (foodG > 0 && !checkFood(foodG)) 
        return false;
    
    if (waterMl > 0 && !checkWater(waterMl)) 
        return false;
    
    process.lastSuccess = false;
    process.completedSteps = 0;
    waterAfterFood = waterMl;

    if (foodG == 0) { 
        waterAfterFood = 0; 
        beginWater(waterMl); 
        return true; 
    }
    
    process.foodTargetG = Sensors::readings().food.grams + foodG;
    process.startedMs = millis();
    process.durationMs = Config::get().feedTimeoutMs;
    
    hopperEmpty = false;
    reverseSegment = false;
    finishedSegmentSteps = 0;
    
    if (!startSegment()) { 
        fail("Таймер двигателя недоступен"); 
        return false; 
    }
    
    process.state = MachineState::FEEDING;
    setMessage("Подача корма");
    
    return true;
}

bool FeedingController::startAugerTest()
{
    if (process.state != MachineState::IDLE) 
        return false;
    
    const auto& c = Config::get();
    const uint64_t duration = static_cast<uint64_t>(c.jogSteps) * 2 * c.stepHalfPeriodUs;

    if (duration > 15000000) { 
        fail("Тест двигателя должен быть до 15 с"); 
        return false; 
    }

    if (!Auger::run(c.stepperDirectionHigh, c.stepHalfPeriodUs, c.jogSteps, 16000)) {
        fail("Не удалось запустить двигатель"); 
        return false;
    }

    process.state = MachineState::AUGER_TEST;
    process.startedMs = millis();
    process.durationMs = static_cast<uint32_t>(duration / 1000);
    process.lastSuccess = false;
    
    setMessage("Тест двигателя");
    return true;
}


bool FeedingController::startPumpTest(uint32_t durationMs)
{
    if (process.state != MachineState::IDLE) 
        return false;
    
    if (durationMs < 1000 || durationMs > 60000 || !checkTank()) 
        return false;
    
    if (Sensors::readings().water.valid && Sensors::readings().water.grams >= Config::get().waterBowlMaxG) {
        fail("Миска воды заполнена"); return false;
    }

    process.state = MachineState::PUMP_TEST;
    process.startedMs = millis();
    process.durationMs = durationMs;
    process.lastSuccess = false;
    
    setMessage("Тест насоса");
    Pump::on();
    return true;
}


void FeedingController::stop()
{
    waterAfterFood = 0;
    finish("Остановлено кнопкой BACK", false);
}


void FeedingController::acknowledge()
{
    if (process.state == MachineState::ERROR) finish("Готово", false);
}

void FeedingController::update()
{
    const auto& c = Config::get();
    const auto& s = Sensors::readings();
    const uint32_t elapsed = millis() - process.startedMs;
    
    if (process.state == MachineState::FEEDING) 
    {
        process.completedSteps = finishedSegmentSteps + Auger::steps();
        
        if (!s.food.valid) 
        { 
            fail("Потеряны показания весов корма"); 
            return; 
        }

        if (s.food.grams >= process.foodTargetG) 
        {
            Auger::stop();
            const float amount = waterAfterFood;
            waterAfterFood = 0;
        
            if (amount > 0) {
                if (checkWater(amount)) beginWater(amount);
            } else 
                finish("Корм выдан");
            
            return;
        }

        if (!s.foodAvailable) 
        {
            if (!hopperEmpty) { 
                hopperEmpty = true; 
                hopperEmptyMs = millis(); 
            }

            if (millis() - hopperEmptyMs >= 2000) { 
                fail("Корм закончился"); 
                return; 
            }
        } 
        else 
            hopperEmpty = false;
        
        if (elapsed >= c.feedTimeoutMs || Auger::timedOut()) { 
            fail("Время подачи корма истекло"); 
            return; 
        }

        if (!Auger::running()) {
            if (c.reverseSteps == 0) { 
                fail("Двигатель остановился"); 
                return; 
            }

            finishedSegmentSteps += Auger::steps();
            reverseSegment = !reverseSegment;
            
            if (!startSegment()) 
                fail("Не удалось продолжить подачу");
        }
    } 
    else if (process.state == MachineState::WATERING) 
    {
        if (!s.water.valid) { 
            fail("Потеряны показания весов воды"); 
            return; 
        }

        if (!checkTank()) 
            return;

        if (s.water.grams >= c.waterBowlMaxG) { 
            finish("Миска воды заполнена"); 
            return; 
        }

        if (s.water.grams - waterStartG >= process.requestedWaterMl) { 
            finish("Вода выдана"); 
            return; 
        }

        // Time is a safety limit. Insufficient measured water is reported as a fault.
        if (elapsed >= process.durationMs || elapsed >= c.waterTimeoutMs) {
            const float delivered = s.water.grams - waterStartG;
            if (delivered >= process.requestedWaterMl * 0.9f) 
                finish("Вода выдана");
            else 
                fail("Мало воды: проверьте насос");
        }
    } 
    else if (process.state == MachineState::AUGER_TEST) {
        process.completedSteps = Auger::steps();
        
        if (Auger::timedOut()) 
            fail("Тест двигателя: таймаут");
        else if (!Auger::running()) 
            finish("Тест двигателя завершён");
    } 
    else if (process.state == MachineState::PUMP_TEST) {
        if (!checkTank()) 
            return;
        
            if (s.water.valid && s.water.grams >= c.waterBowlMaxG) { 
            fail("Тест: миска воды заполнена"); 
            return; 
        }

        if (elapsed >= process.durationMs) 
            finish("Тест насоса завершён");
    }
}


void FeedingController::updateSchedule(bool serviceMode)
{
    if (pendingSave && !busy()) {
        if (!Config::commit(pendingConfig)) {
            pendingSave = false;
            fail("Не удалось сохранить пропуск таймера");
            return;
        }
        pendingSave = false;
    }

    buildNextText();
    if (!Clock::valid()) 
        return;
    const auto date = Clock::now();
    const uint32_t epoch = date.unixtime();
    
    if (scheduleSeen && epoch == lastScheduleEpoch) return;
    scheduleSeen = true;
    lastScheduleEpoch = epoch;
    // Mark every due event before any motor starts, so a reset cannot duplicate a meal.
    
    auto candidate = pendingSave ? pendingConfig : Config::get();
    bool changed = false;
    bool due = false;
    bool collision = false;
    float food = 0, water = 0;
    
    if (candidate.oneShot.enabled && epoch >= candidate.oneShot.dueEpoch) {
        candidate.oneShot.enabled = false;
        changed = true;
        if (epoch - candidate.oneShot.dueEpoch < 60) {
            due = true; 
            food = candidate.oneShot.foodG; 
            water = candidate.oneShot.waterMl;
        } 
        else 
            Serial.println("One-shot expired: no catch-up dispensing");
    }
    for (size_t i = 0; i < Settings::SCHEDULE_COUNT; ++i) {
        const auto& slot = candidate.schedule[i];
        
        if (!slot.enabled || slot.hour != date.hour() || 
            slot.minute != date.minute() ||
            candidate.lastRunDay[i] == dayKey(date)) 
            { continue; }

        candidate.lastRunDay[i] = dayKey(date);
        changed = true;
        if (due) 
            collision = true;
        else { 
            due = true; 
            food = slot.foodG; 
            water = slot.waterMl; 
        }
    }

    if (!changed) 
        return;
    
        // Events during a manual/automatic run are consumed but do not queue another meal.
    if (busy()) {
        pendingConfig = candidate;
        pendingSave = true;
        Serial.println("Scheduled event skipped: busy");
        return;
    }
    
    if (!Config::commit(candidate)) {
        if (!busy()) fail("Не удалось сохранить событие таймера");
        return;
    }
    
    if (!due)
        return;
    
    if (serviceMode || process.state != MachineState::IDLE) {
        Serial.println("Scheduled event skipped: busy/service/error");
        return;
    }

    if (collision) 
        Serial.println("Coincident timers: only the first event is executed");
    
    startCycle(food, water);
}
