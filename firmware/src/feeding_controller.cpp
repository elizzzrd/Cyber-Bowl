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

namespace
{
    ProcessInfo process;

    char message[Settings::CONTROLLER_TEXT_CAPACITY] = "Готово";
    char nextText[Settings::CONTROLLER_TEXT_CAPACITY] = "Расписание выключено";

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
        auger.stop();

        waterAfterFood = 0;
        process.state = MachineState::ERROR;
        process.lastSuccess = false;

        setMessage(text);
    }

    void finish(const char* text, bool success = true)
    {
        Pump::off();
        auger.stop();

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

        if (s.food.grams < -Settings::SCALE_ZERO_TOLERANCE_G) {
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
        if (!Sensors::readings().waterPresent) {
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

        if (s.water.grams < -Settings::SCALE_ZERO_TOLERANCE_G) {
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

        const bool direction = reverseSegment ? !c.stepperDirectionHigh : c.stepperDirectionHigh;
        uint32_t pulses = 0;
        if (c.reverseSteps != 0)
            pulses = reverseSegment ? c.reverseSteps : c.forwardSteps;

        return auger.run(direction, c.stepHalfPeriodUs, pulses, c.feedTimeoutMs - elapsed);
    }

    void updateFeeding(const Settings::Data& c, const SensorReadings& s, uint32_t elapsed)
    {
        process.completedSteps = finishedSegmentSteps + auger.steps();

        if (!s.food.valid)
        {
            fail("Потеряны показания весов корма");
            return;
        }

        if (s.food.grams >= process.foodTargetG)
        {
            auger.stop();
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

            if (millis() - hopperEmptyMs >= Settings::HOPPER_EMPTY_TIMEOUT_MS) {
                fail("Корм закончился");
                return;
            }
        }
        else
            hopperEmpty = false;

        if (elapsed >= c.feedTimeoutMs || auger.timedOut()) {
            fail("Время подачи корма истекло");
            return;
        }

        if (!auger.running()) {
            if (c.reverseSteps == 0) {
                fail("Двигатель остановился");
                return;
            }

            finishedSegmentSteps += auger.steps();
            reverseSegment = !reverseSegment;

            if (!startSegment())
                fail("Не удалось продолжить подачу");
        }
    }

    void updateWatering(const Settings::Data& c, const SensorReadings& s, uint32_t elapsed)
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
            if (delivered >= process.requestedWaterMl * Settings::WATER_DELIVERY_MIN_FRACTION)
                finish("Вода выдана");
            else
                fail("Мало воды: проверьте насос");
        }
    }

    void updateAugerTest()
    {
        process.completedSteps = auger.steps();

        if (auger.timedOut())
            fail("Тест двигателя: таймаут");
        else if (!auger.running())
            finish("Тест двигателя завершён");
    }

    void updatePumpTest(const Settings::Data& c, const SensorReadings& s, uint32_t elapsed)
    {
        if (!checkTank())
            return;

        if (s.water.valid && s.water.grams >= c.waterBowlMaxG) {
            fail("Тест: миска воды заполнена");
            return;
        }

        if (elapsed >= process.durationMs)
            finish("Тест насоса завершён");
    }

    uint32_t dayKey(const DateTime& date)
    {
        return date.year() * Settings::DATE_KEY_YEAR_MULTIPLIER +
               date.month() * Settings::DATE_KEY_MONTH_MULTIPLIER + date.day();
    }

    struct ScheduleEvent {
        bool changed = false;
        bool due = false;
        bool collision = false;
        float food = 0;
        float water = 0;
    };

    ScheduleEvent collectScheduleEvent(Settings::Data& candidate, const DateTime& date)
    {
        ScheduleEvent event;
        const uint32_t epoch = date.unixtime();
        const uint32_t today = dayKey(date);

        if (candidate.oneShot.enabled && epoch >= candidate.oneShot.dueEpoch) {
            candidate.oneShot.enabled = false;
            event.changed = true;
            if (epoch - candidate.oneShot.dueEpoch < Settings::ONE_SHOT_WINDOW_SECONDS) {
                event.due = true;
                event.food = candidate.oneShot.foodG;
                event.water = candidate.oneShot.waterMl;
            }
            else
                Serial.println("One-shot expired: no catch-up dispensing");
        }
        for (size_t i = 0; i < Settings::SCHEDULE_COUNT; ++i) {
            const auto& slot = candidate.schedule[i];

            if (!slot.enabled || slot.hour != date.hour() ||
                slot.minute != date.minute() ||
                candidate.lastRunDay[i] == today)
                continue;

            candidate.lastRunDay[i] = today;
            event.changed = true;
            if (event.due)
                event.collision = true;
            else {
                event.due = true;
                event.food = slot.foodG;
                event.water = slot.waterMl;
            }
        }
        return event;
    }

    void buildNextText()
    {
        if (!Clock::valid()) {
            snprintf(nextText, sizeof(nextText), "Настройте часы RTC");
            return;
        }

        const auto date = Clock::now();
        const uint32_t epoch = date.unixtime();

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

            DateTime due(date.year(), date.month(), date.day(), slot.hour, slot.minute, 0);
            uint32_t when = due.unixtime();

            if (when < epoch || c.lastRunDay[i] == dayKey(date))
                when += Settings::SCHEDULE_DAY_SECONDS;
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

    if (!std::isfinite(foodG) || !std::isfinite(waterMl) ||
        foodG < 0 || foodG > Settings::MAX_FOOD_PORTION_G ||
        waterMl < 0 || waterMl > Settings::MAX_WATER_PORTION_ML ||
        (foodG == 0 && waterMl == 0)) {
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

bool FeedingController::startAugerTest(bool directionHigh, uint32_t periodUs)
{
    if (process.state != MachineState::IDLE) return false;

    if (!auger.runTest(directionHigh, periodUs)) {
        fail("Не удалось запустить двигатель");
        return false;
    }

    process.state = MachineState::AUGER_TEST;
    process.startedMs = millis();
    process.durationMs = Settings::AUGER_TEST_TIMEOUT_MS;
    process.completedSteps = 0;
    process.lastSuccess = false;
    setMessage("Тест двигателя");
    return true;
}

bool FeedingController::startAugerHold()
{
    if (process.state != MachineState::IDLE) return false;

    auger.hold();
    process.state = MachineState::AUGER_HOLD;
    process.startedMs = millis();
    process.durationMs = Settings::AUGER_HOLD_MS;
    process.lastSuccess = false;
    setMessage("Удержание вала");
    return true;
}

bool FeedingController::startPumpTest(uint32_t durationMs)
{
    if (process.state != MachineState::IDLE)
        return false;

    if (durationMs < Settings::PUMP_TEST_MIN_MS || durationMs > Settings::PUMP_TEST_MAX_MS || !checkTank())
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
    const auto& config = Config::get();
    const auto& readings = Sensors::readings();
    const uint32_t elapsed = millis() - process.startedMs;

    switch (process.state) {
    case MachineState::FEEDING:
        updateFeeding(config, readings, elapsed);
        break;
    case MachineState::WATERING:
        updateWatering(config, readings, elapsed);
        break;
    case MachineState::AUGER_TEST:
        updateAugerTest();
        break;
    case MachineState::AUGER_HOLD:
        if (elapsed >= process.durationMs) finish("Удержание завершено");
        break;
    case MachineState::PUMP_TEST:
        updatePumpTest(config, readings, elapsed);
        break;
    case MachineState::IDLE:
    case MachineState::ERROR:
        break;
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
    const auto event = collectScheduleEvent(candidate, date);

    if (!event.changed)
        return;

    // Events during a manual/automatic run are consumed but do not queue another meal.
    if (busy()) {
        pendingConfig = candidate;
        pendingSave = true;
        Serial.println("Scheduled event skipped: busy");
        return;
    }

    if (!Config::commit(candidate)) {
        fail("Не удалось сохранить событие таймера");
        return;
    }

    if (!event.due)
        return;

    if (serviceMode || process.state != MachineState::IDLE) {
        Serial.println("Scheduled event skipped: busy/service/error");
        return;
    }

    if (event.collision)
        Serial.println("Coincident timers: only the first event is executed");

    startCycle(event.food, event.water);
}
