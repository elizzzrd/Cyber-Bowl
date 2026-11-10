#include "ui.hpp"
#include "ui_font.hpp"
#include "button.hpp"
#include "pins.hpp"
#include "config_store.hpp"
#include "sensors.hpp"
#include "clock.hpp"
#include "feeding_controller.hpp"
#include "camera_info.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <esp_arduino_version.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace 
{
    constexpr uint16_t BACKGROUND = 0x08A3;
    constexpr uint16_t PANEL = 0x1105;
    constexpr uint16_t ACCENT = 0x34FD;
    constexpr uint16_t TEXT = 0xFFFF;
    constexpr uint16_t MUTED = 0x9CB4;
    constexpr uint16_t ORANGE = 0xFD20;
    constexpr uint16_t RED = 0xF9C7;
    constexpr uint16_t GREEN = 0x6EAE;
    constexpr int VISIBLE_ROWS = 6;
    constexpr int ROW_HEIGHT = 28;
    Adafruit_ST7789 tft(&SPI, Pins::TFT_CS_PIN, Pins::TFT_DC_PIN, Pins::TFT_RST_PIN);
    GFXcanvas16 canvas(304, ROW_HEIGHT);
    Button back(Pins::BUTTON_BACK_PIN), up(Pins::BUTTON_UP_PIN);
    Button down(Pins::BUTTON_DOWN_PIN), ok(Pins::BUTTON_OK_PIN);

    enum class Page : uint8_t {
        HOME, MAIN, MANUAL, SCHEDULE, SLOT, ONE_SHOT, SERVICE, AUGER, PUMP,
        SCALES, SCALE, SENSOR_SETTINGS, LIMITS, DISPLAY_SETTINGS, CLOCK, DIAGNOSTICS,
        EDIT, NOTICE, CONFIRM, CAL_EMPTY, CAL_CAPTURE, CAL_MASS, CAL_LOAD,
        CAL_RESULT, PUMP_SECONDS, PUMP_VOLUME, TANK_EMPTY, TANK_FULL, PROCESS, CAMERA
    };
    enum class Field : uint8_t {
        FOOD, WATER, SLOT_ON, SLOT_HOUR, SLOT_MINUTE, SLOT_FOOD, SLOT_WATER,
        ONE_DELAY, ONE_FOOD, ONE_WATER, STEP_PERIOD, DIRECTION, JOG_STEPS,
        FORWARD_STEPS, REVERSE_STEPS, PUMP_FACTOR, IR_LOW, TANK_PROTECTION,
        TANK_MIN, FOOD_LIMIT, WATER_LIMIT, FEED_TIMEOUT, WATER_TIMEOUT,
        BRIGHTNESS, INVERT, YEAR, MONTH, DAY, HOUR, MINUTE
    };
    enum class Action : uint8_t {
        NONE, MANUAL, SCHEDULE, DIAGNOSTICS, SERVICE, HOME, FOOD, WATER, BOTH,
        SLOT, ONE_SHOT, ONE_ARM, ONE_CANCEL, AUGER, PUMP, SCALES, SENSOR_SETTINGS,
        LIMITS, DISPLAY_SETTINGS, CLOCK, RESET, EDIT, JOG, PUMP_TEST, PUMP_CAL,
        FOOD_SCALE, WATER_SCALE, TARE, SCALE_CAL, TANK_CAL, CLOCK_SAVE, RTC_RETRY,
        BACK, CAMERA
    };
    struct Row {
        char label[112] = {};
        char value[112] = {};
        Action action = Action::NONE;
        Field field = Field::FOOD;
        int argument = 0;
    };
    Row rows[16];
    int rowCount = 0;
    int selection = 0;
    int firstVisible = 0;
    Page page = Page::HOME;
    Page previousRendered = Page::NOTICE;
    Page editReturn = Page::MAIN;
    int editSelection = 0;
    int editFirstVisible = 0;
    Page noticeReturn = Page::MAIN;
    Page processReturn = Page::HOME;
    Page confirmReturn = Page::HOME;
    char notice[192] = {};
    char editLabel[112] = {};
    char rowCache[VISIBLE_ROWS][256] = {};
    Field editField = Field::FOOD;
    double editValue = 0;
    double editMin = 0;
    double editMax = 0;
    double editStep = 1;
    int editingSlot = 0;
    uint32_t oneDelayMinutes = 60;
    float oneFoodG = 30;
    float oneWaterMl = 50;
    int clockYear = 2026, clockMonth = 1, clockDay = 1, clockHour = 12, clockMinute = 0;
    ScaleId scaleId = ScaleId::FOOD;
    bool tareOnly = false;
    int captureStage = 0; // 0 = tare/empty, 1 = loaded, 2 = empty tank, 3 = full tank.
    int32_t zeroRaw = 0;
    int32_t zeroSpan = 0;
    float knownMassG = 100;
    float proposedFactor = 400;
    uint32_t pumpSeconds = 10;
    float measuredMl = 10;
    uint32_t lastDrawMs = 0;
    bool wasBusy = false;
    bool forceDraw = true;
    bool displayAvailable = false;
    uint8_t cameraHelpPage = 0;
    int scrollbarFirst = -1;
    int scrollbarCount = -1;

    void go(Page next)
    {
        page = next;
        selection = firstVisible = 0;
        forceDraw = true;
    }
    void showNotice(const char* text, Page returnPage)
    {
        snprintf(notice, sizeof(notice), "%s", text);
        noticeReturn = returnPage;
        go(Page::NOTICE);
    }
    bool save(const Settings::Data& candidate, Page returnPage)
    {
        if (!Settings::valid(candidate)) {
            showNotice("Недопустимое значение. Обратные шаги должны быть меньше прямых. Защита бака требует калибровки.", returnPage);
            return false;
        }
        if (!Config::commit(candidate)) {
            showNotice("Не удалось сохранить настройки. Проверьте память устройства.", returnPage);
            return false;
        }
        Sensors::applySettings();
        Ui::applyDisplaySettings();
        return true;
    }
    void add(const char* label, const char* value, Action action = Action::NONE,
            Field field = Field::FOOD, int argument = 0)
    {
        if (rowCount >= 16) return;
        auto& row = rows[rowCount++];
        snprintf(row.label, sizeof(row.label), "%s", label);
        snprintf(row.value, sizeof(row.value), "%s", value);
        row.action = action; row.field = field; row.argument = argument;
    }
    void addField(const char* label, double value, const char* unit, Field field, int decimals = 0)
    {
        char text[64];
        snprintf(text, sizeof(text), "%.*f %s", decimals, value, unit);
        add(label, text, Action::EDIT, field);
    }
    void addBool(const char* label, bool value, Field field)
    {
        add(label, value ? "Да" : "Нет", Action::EDIT, field);
    }
    const char* scaleName() { return scaleId == ScaleId::FOOD ? "Весы корма" : "Весы воды"; }

    void buildRows()
    {
        rowCount = 0;
        const auto& c = Config::get();
        char text[112];
        switch (page) {
        case Page::MAIN:
            add("Подать сейчас", "", Action::MANUAL);
            add("Расписание", "", Action::SCHEDULE);
            add("Диагностика", "", Action::DIAGNOSTICS);
            add("Настройки", "", Action::SERVICE);
            add("Камера", "", Action::CAMERA);
            add("Главный экран", "", Action::HOME);
            break;
        case Page::MANUAL:
            addField("Порция корма", c.foodPortionG, "г", Field::FOOD);
            addField("Порция воды", c.waterPortionMl, "мл", Field::WATER);
            add("Выдать корм", "", Action::FOOD);
            add("Выдать воду", "", Action::WATER);
            add("Корм и вода", "", Action::BOTH);
            break;
        case Page::SCHEDULE:
            for (int i = 0; i < static_cast<int>(Settings::SCHEDULE_COUNT); ++i) {
                const auto& slot = c.schedule[i];
                char label[32]; snprintf(label, sizeof(label), "Таймер %d", i + 1);
                snprintf(text, sizeof(text), "%s %02u:%02u %.0fг/%.0fмл", slot.enabled ? "+" : "-",
                        slot.hour, slot.minute, slot.foodG, slot.waterMl);
                add(label, text, Action::SLOT, Field::FOOD, i);
            }
            add("Разовый таймер", c.oneShot.enabled ? "Включён" : "Выключен", Action::ONE_SHOT);
            break;
        case Page::SLOT: {
            const auto& slot = c.schedule[editingSlot];
            addBool("Включён", slot.enabled, Field::SLOT_ON);
            addField("Час", slot.hour, "", Field::SLOT_HOUR);
            addField("Минута", slot.minute, "", Field::SLOT_MINUTE);
            addField("Корм", slot.foodG, "г", Field::SLOT_FOOD);
            addField("Вода", slot.waterMl, "мл", Field::SLOT_WATER);
            add("Повтор", "Каждый день");
            break;
        }
        case Page::ONE_SHOT:
            addField("Через", oneDelayMinutes, "мин", Field::ONE_DELAY);
            addField("Корм", oneFoodG, "г", Field::ONE_FOOD);
            addField("Вода", oneWaterMl, "мл", Field::ONE_WATER);
            add("Поставить таймер", "", Action::ONE_ARM);
            add("Отменить таймер", "", Action::ONE_CANCEL);
            if (c.oneShot.enabled) {
                DateTime due(c.oneShot.dueEpoch);
                snprintf(text, sizeof(text), "%02u.%02u %02u:%02u", due.day(), due.month(), due.hour(), due.minute());
                add("Сейчас назначен", text);
            }
            break;
        case Page::SERVICE:
            add("Двигатель", "", Action::AUGER);
            add("Насос", "", Action::PUMP);
            add("Весы и калибровка", "", Action::SCALES);
            add("Датчики бака", "", Action::SENSOR_SETTINGS);
            add("Лимиты подачи", "", Action::LIMITS);
            add("Часы и дата", "", Action::CLOCK);
            add("Дисплей", "", Action::DISPLAY_SETTINGS);
            add("Сброс настроек", "", Action::RESET);
            break;
        case Page::AUGER:
            addField("Полупериод STEP", c.stepHalfPeriodUs, "мкс", Field::STEP_PERIOD);
            addBool("Направление HIGH", c.stepperDirectionHigh, Field::DIRECTION);
            addField("Шагов для теста", c.jogSteps, "", Field::JOG_STEPS);
            add("Запустить тест", "", Action::JOG);
            addField("Шагов вперёд", c.forwardSteps, "", Field::FORWARD_STEPS);
            addField("Шагов назад", c.reverseSteps, "0 = без реверса", Field::REVERSE_STEPS);
            snprintf(text, sizeof(text), "%.0f имп/с", 500000.0 / c.stepHalfPeriodUs);
            add("Частота STEP", text);
            break;
        case Page::PUMP:
            addField("Калибровка", c.pumpMsPerMl, "мс/мл", Field::PUMP_FACTOR, 1);
            add("Тест по времени", "", Action::PUMP_TEST);
            add("Измерить расход", "", Action::PUMP_CAL);
            add("Лимит по времени", "В разделе лимитов");
            break;
        case Page::SCALES:
            add("Весы корма", c.foodScale.calibrated ? "Калиброваны" : "Нужна калибровка", Action::FOOD_SCALE);
            add("Весы воды", c.waterScale.calibrated ? "Калиброваны" : "Нужна калибровка", Action::WATER_SCALE);
            break;
        case Page::SCALE: {
            const auto& value = Sensors::scale(scaleId);
            const auto& calibration = scaleId == ScaleId::FOOD ? c.foodScale : c.waterScale;
            add("Тарировать", "Пустая миска", Action::TARE);
            add("Калибровать", "Известный груз", Action::SCALE_CAL);
            snprintf(text, sizeof(text), "%.2f", calibration.factor); add("Коэффициент", text);
            snprintf(text, sizeof(text), "%ld", static_cast<long>(calibration.offset)); add("Ноль ADC", text);
            if (value.valid) snprintf(text, sizeof(text), "%.1f г", value.grams);
            else snprintf(text, sizeof(text), "%s", value.connected ? "Нужна калибровка" : "Нет данных");
            add("Показание", text);
            snprintf(text, sizeof(text), "%ld", static_cast<long>(value.raw)); add("Сырой ADC", text);
            break;
        }
        case Page::SENSOR_SETTINGS:
            addBool("Корм при LOW", c.foodPresentLow, Field::IR_LOW);
            add("Калибровка бака", "Пустой / полный", Action::TANK_CAL);
            addBool("Защита без воды", c.lowWaterProtection, Field::TANK_PROTECTION);
            addField("Минимум воды", c.minTankPercent, "%", Field::TANK_MIN);
            snprintf(text, sizeof(text), "%ld / %ld", static_cast<long>(c.tankRawEmpty), static_cast<long>(c.tankRawFull));
            add("ADC: пуст / полон", text);
            snprintf(text, sizeof(text), "%d", Sensors::readings().irRaw); add("ИК: сигнал GPIO", text);
            break;
        case Page::LIMITS:
            addField("Миска корма", c.foodBowlMaxG, "г", Field::FOOD_LIMIT);
            addField("Миска воды", c.waterBowlMaxG, "г", Field::WATER_LIMIT);
            addField("Подача корма", c.feedTimeoutMs / 1000, "с", Field::FEED_TIMEOUT);
            addField("Подача воды", c.waterTimeoutMs / 1000, "с", Field::WATER_TIMEOUT);
            break;
        case Page::DISPLAY_SETTINGS:
            addField("Яркость", c.brightnessPercent, "%", Field::BRIGHTNESS);
            addBool("Инверсия цветов", c.invertDisplay, Field::INVERT);
            add("Обновление полей", "При изменении");
            break;
        case Page::CLOCK:
            addField("Год", clockYear, "", Field::YEAR);
            addField("Месяц", clockMonth, "", Field::MONTH);
            addField("День", clockDay, "", Field::DAY);
            addField("Час", clockHour, "", Field::HOUR);
            addField("Минута", clockMinute, "", Field::MINUTE);
            add("Записать в RTC", "", Action::CLOCK_SAVE);
            add("Повторить поиск RTC", "", Action::RTC_RETRY);
            break;
        default: break;
        }
    }

    void startEdit(const Row& row)
    {
        editField = row.field;
        editReturn = page;
        editSelection = selection;
        editFirstVisible = firstVisible;
        snprintf(editLabel, sizeof(editLabel), "%s", row.label);
        editMin = 0; editMax = 200; editStep = 1;
        const auto& c = Config::get();
        const auto& slot = c.schedule[editingSlot];
        switch (editField) {
        case Field::FOOD: editValue = c.foodPortionG; editMin = 1; break;
        case Field::WATER: editValue = c.waterPortionMl; editMin = 1; break;
        case Field::SLOT_ON: editValue = slot.enabled; editMax = 1; break;
        case Field::SLOT_HOUR: editValue = slot.hour; editMax = 23; break;
        case Field::SLOT_MINUTE: editValue = slot.minute; editMax = 59; break;
        case Field::SLOT_FOOD: editValue = slot.foodG; break;
        case Field::SLOT_WATER: editValue = slot.waterMl; break;
        case Field::ONE_DELAY: editValue = oneDelayMinutes; editMin = 1; editMax = 10080; editStep = 5; break;
        case Field::ONE_FOOD: editValue = oneFoodG; break;
        case Field::ONE_WATER: editValue = oneWaterMl; break;
        case Field::STEP_PERIOD: editValue = c.stepHalfPeriodUs; editMin = 200; editMax = 10000; editStep = 50; break;
        case Field::DIRECTION: editValue = c.stepperDirectionHigh; editMax = 1; break;
        case Field::JOG_STEPS: editValue = c.jogSteps; editMin = 1; editMax = 20000; editStep = 10; break;
        case Field::FORWARD_STEPS: editValue = c.forwardSteps; editMin = 1; editMax = 20000; editStep = 10; break;
        case Field::REVERSE_STEPS: editValue = c.reverseSteps; editMax = c.forwardSteps - 1; editStep = 5; break;
        case Field::PUMP_FACTOR: editValue = c.pumpMsPerMl; editMin = 10; editMax = 20000; editStep = 10; break;
        case Field::IR_LOW: editValue = c.foodPresentLow; editMax = 1; break;
        case Field::TANK_PROTECTION: editValue = c.lowWaterProtection; editMax = 1; break;
        case Field::TANK_MIN: editValue = c.minTankPercent; editMax = 50; break;
        case Field::FOOD_LIMIT: editValue = c.foodBowlMaxG; editMin = 10; editMax = 2000; editStep = 10; break;
        case Field::WATER_LIMIT: editValue = c.waterBowlMaxG; editMin = 10; editMax = 2000; editStep = 10; break;
        case Field::FEED_TIMEOUT: editValue = c.feedTimeoutMs / 1000; editMin = 5; editMax = 180; break;
        case Field::WATER_TIMEOUT: editValue = c.waterTimeoutMs / 1000; editMin = 1; editMax = 300; break;
        case Field::BRIGHTNESS: editValue = c.brightnessPercent; editMin = 10; editMax = 100; editStep = 5; break;
        case Field::INVERT: editValue = c.invertDisplay; editMax = 1; break;
        case Field::YEAR: editValue = clockYear; editMin = 2024; editMax = 2099; break;
        case Field::MONTH: editValue = clockMonth; editMin = 1; editMax = 12; break;
        case Field::DAY: editValue = clockDay; editMin = 1; editMax = 31; break;
        case Field::HOUR: editValue = clockHour; editMax = 23; break;
        case Field::MINUTE: editValue = clockMinute; editMax = 59; break;
        }
        editValue = std::max(editMin, std::min(editMax, editValue));
        go(Page::EDIT);
    }

    void commitEdit()
    {
        auto c = Config::get();
        auto& slot = c.schedule[editingSlot];
        bool persistent = true;
        switch (editField) {
        case Field::FOOD: c.foodPortionG = editValue; break;
        case Field::WATER: c.waterPortionMl = editValue; break;
        case Field::SLOT_ON: slot.enabled = editValue != 0; break;
        case Field::SLOT_HOUR: slot.hour = editValue; break;
        case Field::SLOT_MINUTE: slot.minute = editValue; break;
        case Field::SLOT_FOOD: slot.foodG = editValue; break;
        case Field::SLOT_WATER: slot.waterMl = editValue; break;
        case Field::ONE_DELAY: oneDelayMinutes = editValue; persistent = false; break;
        case Field::ONE_FOOD: oneFoodG = editValue; persistent = false; break;
        case Field::ONE_WATER: oneWaterMl = editValue; persistent = false; break;
        case Field::STEP_PERIOD: c.stepHalfPeriodUs = editValue; break;
        case Field::DIRECTION: c.stepperDirectionHigh = editValue != 0; break;
        case Field::JOG_STEPS: c.jogSteps = editValue; break;
        case Field::FORWARD_STEPS: c.forwardSteps = editValue; break;
        case Field::REVERSE_STEPS: c.reverseSteps = editValue; break;
        case Field::PUMP_FACTOR: c.pumpMsPerMl = editValue; break;
        case Field::IR_LOW: c.foodPresentLow = editValue != 0; break;
        case Field::TANK_PROTECTION: c.lowWaterProtection = editValue != 0; break;
        case Field::TANK_MIN: c.minTankPercent = editValue; break;
        case Field::FOOD_LIMIT: c.foodBowlMaxG = editValue; break;
        case Field::WATER_LIMIT: c.waterBowlMaxG = editValue; break;
        case Field::FEED_TIMEOUT: c.feedTimeoutMs = editValue * 1000; break;
        case Field::WATER_TIMEOUT: c.waterTimeoutMs = editValue * 1000; break;
        case Field::BRIGHTNESS: c.brightnessPercent = editValue; break;
        case Field::INVERT: c.invertDisplay = editValue != 0; break;
        case Field::YEAR: clockYear = editValue; persistent = false; break;
        case Field::MONTH: clockMonth = editValue; persistent = false; break;
        case Field::DAY: clockDay = editValue; persistent = false; break;
        case Field::HOUR: clockHour = editValue; persistent = false; break;
        case Field::MINUTE: clockMinute = editValue; persistent = false; break;
        }
        if (!persistent || save(c, editReturn)) {
            go(editReturn);
            selection = editSelection;
            firstVisible = editFirstVisible;
        }
    }

    Page parent(Page p)
    {
        switch (p) {
        case Page::MAIN: return Page::HOME;
        case Page::MANUAL: case Page::SCHEDULE: case Page::SERVICE: case Page::DIAGNOSTICS: case Page::CAMERA: return Page::MAIN;
        case Page::SLOT: case Page::ONE_SHOT: return Page::SCHEDULE;
        case Page::SCALE: return Page::SCALES;
        case Page::AUGER: case Page::PUMP: case Page::SCALES: case Page::SENSOR_SETTINGS:
        case Page::LIMITS: case Page::DISPLAY_SETTINGS: case Page::CLOCK: return Page::SERVICE;
        default: return Page::HOME;
        }
    }

    void startProcess(Page returnPage)
    {
        processReturn = returnPage;
        wasBusy = FeedingController::busy();
        go(Page::PROCESS);
    }

    void activate(const Row& row)
    {
        const auto& c = Config::get();
        switch (row.action) {
        case Action::NONE: break;
        case Action::MANUAL: go(Page::MANUAL); break;
        case Action::SCHEDULE: go(Page::SCHEDULE); break;
        case Action::DIAGNOSTICS: go(Page::DIAGNOSTICS); break;
        case Action::SERVICE: go(Page::SERVICE); break;
        case Action::CAMERA: cameraHelpPage = 0; go(Page::CAMERA); break;
        case Action::HOME: go(Page::HOME); break;
        case Action::FOOD: FeedingController::startCycle(c.foodPortionG, 0); startProcess(Page::HOME); break;
        case Action::WATER: FeedingController::startCycle(0, c.waterPortionMl); startProcess(Page::HOME); break;
        case Action::BOTH: FeedingController::startCycle(c.foodPortionG, c.waterPortionMl); startProcess(Page::HOME); break;
        case Action::SLOT: editingSlot = row.argument; go(Page::SLOT); break;
        case Action::ONE_SHOT:
            oneFoodG = c.oneShot.foodG; oneWaterMl = c.oneShot.waterMl;
            oneDelayMinutes = 60;
            go(Page::ONE_SHOT); break;
        case Action::ONE_ARM: {
            if (!Clock::valid()) { showNotice("Сначала настройте часы RTC.", Page::ONE_SHOT); break; }
            auto candidate = c;
            candidate.oneShot.enabled = true;
            candidate.oneShot.dueEpoch = Clock::now().unixtime() + oneDelayMinutes * 60;
            candidate.oneShot.foodG = oneFoodG; candidate.oneShot.waterMl = oneWaterMl;
            if (save(candidate, Page::ONE_SHOT)) showNotice("Разовый таймер сохранён.", Page::SCHEDULE);
            break;
        }
        case Action::ONE_CANCEL: {
            auto candidate = c; candidate.oneShot.enabled = false;
            if (save(candidate, Page::ONE_SHOT)) showNotice("Разовый таймер отменён.", Page::SCHEDULE);
            break;
        }
        case Action::AUGER: go(Page::AUGER); break;
        case Action::PUMP: go(Page::PUMP); break;
        case Action::SCALES: go(Page::SCALES); break;
        case Action::SENSOR_SETTINGS: go(Page::SENSOR_SETTINGS); break;
        case Action::LIMITS: go(Page::LIMITS); break;
        case Action::DISPLAY_SETTINGS: go(Page::DISPLAY_SETTINGS); break;
        case Action::CLOCK: {
            const auto now = Clock::now();
            clockYear = now.year(); clockMonth = now.month(); clockDay = now.day();
            clockHour = now.hour(); clockMinute = now.minute(); go(Page::CLOCK); break;
        }
        case Action::RESET: confirmReturn = Page::SERVICE; go(Page::CONFIRM); break;
        case Action::EDIT: startEdit(row); break;
        case Action::JOG: FeedingController::startAugerTest(); startProcess(Page::AUGER); break;
        case Action::PUMP_TEST: processReturn = Page::PUMP; go(Page::PUMP_SECONDS); break;
        case Action::PUMP_CAL: processReturn = Page::PUMP_VOLUME; go(Page::PUMP_SECONDS); break;
        case Action::FOOD_SCALE: scaleId = ScaleId::FOOD; go(Page::SCALE); break;
        case Action::WATER_SCALE: scaleId = ScaleId::WATER; go(Page::SCALE); break;
        case Action::TARE: tareOnly = true; go(Page::CAL_EMPTY); break;
        case Action::SCALE_CAL: tareOnly = false; go(Page::CAL_EMPTY); break;
        case Action::TANK_CAL: go(Page::TANK_EMPTY); break;
        case Action::CLOCK_SAVE:
            if (Clock::set(DateTime(clockYear, clockMonth, clockDay, clockHour, clockMinute, 0)))
                showNotice("Дата и время записаны в RTC.", Page::CLOCK);
            else showNotice("RTC недоступен или дата неверна.", Page::CLOCK);
            break;
        case Action::RTC_RETRY:
            showNotice(Clock::retry() ? "RTC найден. Проверьте дату и время." : "RTC не найден. Проверьте подключение.", Page::CLOCK);
            break;
        case Action::BACK: go(parent(page)); break;
        }
    }

    void beginCapture(int stage)
    {
        captureStage = stage;
        const bool success = stage >= 2 ? Sensors::captureTank() : Sensors::capture(scaleId);
        if (success) go(Page::CAL_CAPTURE);
        else showNotice("Нет данных датчика. Проверьте подключение и повторите.", stage >= 2 ? Page::SENSOR_SETTINGS : Page::SCALE);
    }

    void updateCapture()
    {
        if (page != Page::CAL_CAPTURE) return;
        const auto state = Sensors::captureState();
        if (state == CaptureState::BUSY) return;
        if (state == CaptureState::FAILED) {
            Sensors::cancelCapture();
            showNotice("Датчик не ответил вовремя. Настройки не изменены.", captureStage >= 2 ? Page::SENSOR_SETTINGS : Page::SCALE);
            return;
        }
        if (state != CaptureState::DONE) return;
        const int32_t raw = Sensors::capturedRaw();
        const int32_t span = Sensors::capturedSpan();
        Sensors::cancelCapture();
        if (captureStage == 0) {
            zeroRaw = raw; zeroSpan = span;
            if (tareOnly) {
                auto c = Config::get();
                auto& calibration = scaleId == ScaleId::FOOD ? c.foodScale : c.waterScale;
                if (calibration.calibrated && span / std::fabs(calibration.factor) > 3) {
                    showNotice("Вес нестабилен. Уберите вибрацию и повторите тарирование.", Page::SCALE); return;
                }
                calibration.offset = raw; calibration.tared = true;
                if (save(c, Page::SCALE)) showNotice("Ноль весов сохранён. Автоматического тарирования при включении нет.", Page::SCALE);
            } else go(Page::CAL_MASS);
        } else if (captureStage == 1) {
            const int64_t delta = static_cast<int64_t>(raw) - zeroRaw;
            if (std::llabs(delta) < 100 || std::max(span, zeroSpan) > std::llabs(delta) * 0.03) {
                showNotice("Слишком маленький сигнал или нестабильный груз. Повторите калибровку.", Page::SCALE); return;
            }
            proposedFactor = static_cast<float>(delta) / knownMassG;
            go(Page::CAL_RESULT);
        } else if (captureStage == 2) {
            zeroRaw = raw;
            go(Page::TANK_FULL);
        } else {
            if (std::abs(raw - zeroRaw) < 50 || span > 100) {
                showNotice("Сигнал бака почти не изменился или нестабилен. Проверьте датчик.", Page::SENSOR_SETTINGS); return;
            }
            auto c = Config::get();
            c.tankRawEmpty = zeroRaw; c.tankRawFull = raw; c.tankCalibrated = true;
            if (save(c, Page::SENSOR_SETTINGS)) showNotice("Калибровка бака сохранена. Защиту без воды можно включить отдельно.", Page::SENSOR_SETTINGS);
        }
    }

    const char* title()
    {
        switch (page) {
        case Page::HOME: return "CYBER BOWL";
        case Page::MAIN: return "Главное меню";
        case Page::MANUAL: return "Подать сейчас";
        case Page::SCHEDULE: return "Расписание";
        case Page::SLOT: return "Ежедневный таймер";
        case Page::ONE_SHOT: return "Разовый таймер";
        case Page::SERVICE: return "Настройки";
        case Page::AUGER: return "Шаговый двигатель";
        case Page::PUMP: return "Насос";
        case Page::SCALES: return "Весы";
        case Page::SCALE: return scaleName();
        case Page::SENSOR_SETTINGS: return "Датчики";
        case Page::LIMITS: return "Лимиты подачи";
        case Page::DISPLAY_SETTINGS: return "Дисплей";
        case Page::CLOCK: return "Часы и дата";
        case Page::DIAGNOSTICS: return "Диагностика";
        case Page::CAMERA: return "Камера: как открыть видео";
        case Page::EDIT: return "Изменить значение";
        case Page::NOTICE: return "Сообщение";
        case Page::CONFIRM: return "Сброс настроек";
        case Page::CAL_EMPTY: return tareOnly ? "Тарирование весов" : "Калибровка: ноль";
        case Page::CAL_CAPTURE: return "Измерение";
        case Page::CAL_MASS: return "Масса груза";
        case Page::CAL_LOAD: return "Калибровка: груз";
        case Page::CAL_RESULT: return "Результат калибровки";
        case Page::PUMP_SECONDS: return "Время теста насоса";
        case Page::PUMP_VOLUME: return "Измеренный объём";
        case Page::TANK_EMPTY: return "Калибровка: пустой бак";
        case Page::TANK_FULL: return "Калибровка: полный бак";
        case Page::PROCESS: return FeedingController::info().state == MachineState::ERROR ? "Ошибка" : "Выполнение";
        }
        return "CYBER BOWL";
    }
    const char* footer()
    {
        if (FeedingController::busy()) return "BACK: остановить подачу";
        if (page == Page::HOME) return "OK: меню";
        if (page == Page::EDIT || page == Page::CAL_MASS || page == Page::PUMP_SECONDS || page == Page::PUMP_VOLUME)
            return "UP/DN:+/-  OK:принять  BACK:отмена";
        if (page == Page::CAL_CAPTURE) return "BACK: отменить измерение";
        if (page == Page::PROCESS || page == Page::NOTICE) return "OK / BACK: закрыть";
        if (page == Page::DIAGNOSTICS) return "BACK: меню";
        if (page == Page::CAMERA) return "UP/DN:страница  BACK:меню";
        if (page == Page::CAL_EMPTY || page == Page::CAL_LOAD || page == Page::CAL_RESULT ||
            page == Page::TANK_EMPTY || page == Page::TANK_FULL || page == Page::CONFIRM)
            return "OK: принять  BACK: отмена";
        return "UP/DN:выбор  OK:вход  BACK:назад";
    }

    void drawChrome()
    {
        tft.fillScreen(BACKGROUND); // Only when changing pages, never on the periodic refresh.
        tft.fillRect(0, 0, 320, 28, PANEL);
        UiFont::draw(tft, 10, 5, title(), TEXT, 1, Ui::serviceMode() ? 230 : 298);
        if (Ui::serviceMode()) UiFont::draw(tft, 251, 5, "ПАУЗА", ORANGE, 1, 64);
        tft.fillRect(0, 216, 320, 24, PANEL);
        UiFont::draw(tft, 7, 219, footer(), MUTED, 1, 306);
        for (auto& cache : rowCache) cache[0] = '\0';
        scrollbarFirst = scrollbarCount = -1;
    }
    void paintRow(int slot, const char* left, const char* right = "", bool selected = false,
                uint16_t color = TEXT, uint8_t scale = 1)
    {
        if (slot < 0 || slot >= VISIBLE_ROWS) return;
        char key[256];
        snprintf(key, sizeof(key), "%u/%u/%u/%s|%s", selected, color, scale, left, right);
        if (strcmp(rowCache[slot], key) == 0) return;
        snprintf(rowCache[slot], sizeof(rowCache[slot]), "%s", key);
        canvas.fillScreen(selected ? 0x1A0A : BACKGROUND);
        const int rightWidth = std::min(170, UiFont::width(right));
        if (selected) {
            canvas.fillRect(0, 1, 3, ROW_HEIGHT - 2, ACCENT);
            UiFont::draw(canvas, 7, 5, ">", ACCENT);
        }
        const int x = selected ? 22 : 7;
        const int available = right[0] ? 304 - rightWidth - x - 12 : 304 - x - 5;
        UiFont::draw(canvas, x, scale == 2 ? 0 : 5, left, color, scale, available);
        if (right[0]) UiFont::draw(canvas, 299 - rightWidth, 5, right, selected ? ACCENT : MUTED, 1, rightWidth);
        tft.drawRGBBitmap(8, 36 + slot * ROW_HEIGHT, canvas.getBuffer(), 304, ROW_HEIGHT);
    }

    void paragraph(const char* text, int startRow = 0, uint16_t color = TEXT)
    {
        char lines[6][160] = {};
        int line = 0;
        while (*text && line < VISIBLE_ROWS - startRow) {
            while (*text == ' ') ++text;
            if (!*text) break;
            char word[112] = {};
            size_t n = 0;
            while (*text && *text != ' ' && *text != '\n') {
                if (n + 1 < sizeof(word)) word[n++] = *text;
                ++text;
            }
            char candidate[272];
            snprintf(candidate, sizeof(candidate), "%s%s%s", lines[line], lines[line][0] ? " " : "", word);
            if (UiFont::width(candidate) > 286 && lines[line][0]) {
                if (++line >= VISIBLE_ROWS - startRow) break;
                snprintf(lines[line], sizeof(lines[line]), "%s", word);
            } else {
                const size_t length = std::min(strlen(candidate), sizeof(lines[line]) - 1);
                memcpy(lines[line], candidate, length);
                lines[line][length] = '\0';
            }
            if (*text == '\n') { ++line; ++text; }
        }
        for (int i = 0; i < VISIBLE_ROWS - startRow; ++i) paintRow(startRow + i, lines[i], "", false, color);
    }

    void drawHome()
    {
        char left[112], right[112];
        const auto& s = Sensors::readings();
        if (Clock::valid()) {
            const auto now = Clock::now();
            snprintf(left, sizeof(left), "%02u:%02u:%02u", now.hour(), now.minute(), now.second());
            snprintf(right, sizeof(right), "%02u.%02u.%04u", now.day(), now.month(), now.year());
            paintRow(0, left, right, false, ACCENT, 2);
        } else paintRow(0, Clock::connected() ? "Настройте часы" : "RTC не найден", "", false, ORANGE);
        if (s.food.valid) snprintf(right, sizeof(right), "%.1f г", s.food.grams);
        else snprintf(right, sizeof(right), "%s", s.food.connected ? "Калибровка" : "Нет данных");
        paintRow(1, "Миска корма", right);
        if (s.water.valid) snprintf(right, sizeof(right), "%.1f г", s.water.grams);
        else snprintf(right, sizeof(right), "%s", s.water.connected ? "Калибровка" : "Нет данных");
        paintRow(2, "Миска воды", right);
        if (s.tankValid) snprintf(right, sizeof(right), "Вода: %d%%", s.tankPercent);
        else snprintf(right, sizeof(right), "Бак: не калибр.");
        paintRow(3, s.foodAvailable ? "Корм в бункере" : "Бункер пуст", right, false, s.foodAvailable ? GREEN : ORANGE);
        paintRow(4, FeedingController::nextFeedingText(), "", false, ACCENT);
        paintRow(5, FeedingController::info().message, "", false, MUTED);
    }

    void drawDiagnostics()
    {
        const auto& s = Sensors::readings();
        char value[96];
        paintRow(0, "RTC", Clock::valid() ? "Работает" : (Clock::connected() ? "Нужно время" : "Нет связи"));
        snprintf(value, sizeof(value), "%s %ld", s.food.connected ? "OK" : "Нет", static_cast<long>(s.food.raw));
        paintRow(1, "HX711 корм", value);
        snprintf(value, sizeof(value), "%s %ld", s.water.connected ? "OK" : "Нет", static_cast<long>(s.water.raw));
        paintRow(2, "HX711 вода", value);
        snprintf(value, sizeof(value), "%d / %s", s.irRaw, s.foodAvailable ? "Корм есть" : "Пусто");
        paintRow(3, "ИК: GPIO", value);
        snprintf(value, sizeof(value), "%d", s.tankRaw); paintRow(4, "Бак: ADC", value);
        paintRow(5, "Память настроек", Config::storageReady() ? "Доступна" : "Ошибка");
    }

    void drawProcess()
    {
        const auto& p = FeedingController::info();
        const auto& s = Sensors::readings();
        char text[112];
        const uint32_t elapsed = millis() - p.startedMs;
        const uint32_t remaining = elapsed >= p.durationMs ? 0 : p.durationMs - elapsed;
        if (p.state == MachineState::ERROR) { paragraph(p.message, 0, RED); return; }
        if (!FeedingController::busy()) { paragraph(p.message, 0, p.lastSuccess ? GREEN : ORANGE); return; }
        paintRow(0, p.message, "", false, ACCENT);
        snprintf(text, sizeof(text), "%.1f с", remaining / 1000.0f); paintRow(1, "Осталось до лимита", text);
        if (p.state == MachineState::FEEDING) {
            snprintf(text, sizeof(text), "%.1f / %.1f г", s.food.grams, p.foodTargetG); paintRow(2, "Вес / цель", text);
            snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(p.completedSteps)); paintRow(3, "Импульсов STEP", text);
        } else if (p.state == MachineState::WATERING) {
            snprintf(text, sizeof(text), "%.0f мл", p.requestedWaterMl); paintRow(2, "Порция воды", text);
            snprintf(text, sizeof(text), "%.1f г", s.water.grams); paintRow(3, "Вес миски", text);
        } else if (p.state == MachineState::AUGER_TEST) {
            snprintf(text, sizeof(text), "%lu / %lu", static_cast<unsigned long>(p.completedSteps),
                    static_cast<unsigned long>(Config::get().jogSteps)); paintRow(2, "Импульсов STEP", text);
            paintRow(3, "Режим", "Тест без весов");
        } else {
            paintRow(2, "Режим", "Тест по времени");
            paintRow(3, "Подготовьте", "Мерную ёмкость");
        }
        paintRow(4, "BACK останавливает подачу", "", false, ORANGE);
        paintRow(5, "", "");
    }

    void drawPage()
    {
        buildRows();
        if (rowCount > 0) {
            selection = std::max(0, std::min(selection, rowCount - 1));
            if (selection < firstVisible) firstVisible = selection;
            if (selection >= firstVisible + VISIBLE_ROWS) firstVisible = selection - VISIBLE_ROWS + 1;
            if (rowCount > VISIBLE_ROWS && (scrollbarFirst != firstVisible || scrollbarCount != rowCount)) {
                const int height = ROW_HEIGHT * VISIBLE_ROWS;
                const int thumb = height * VISIBLE_ROWS / rowCount;
                const int y = 36 + (height - thumb) * firstVisible / (rowCount - VISIBLE_ROWS);
                tft.fillRect(316, 36, 2, height, PANEL);
                tft.fillRect(316, y, 2, thumb, ACCENT);
                scrollbarFirst = firstVisible; scrollbarCount = rowCount;
            }
            for (int i = 0; i < VISIBLE_ROWS; ++i) {
                const int index = firstVisible + i;
                if (index < rowCount) paintRow(i, rows[index].label, rows[index].value, index == selection);
                else paintRow(i, "");
            }
            return;
        }
        char value[112];
        switch (page) {
        case Page::HOME: drawHome(); break;
        case Page::DIAGNOSTICS: drawDiagnostics(); break;
        case Page::CAMERA:
            if (cameraHelpPage == 0) {
                paintRow(0, "Прямое подключение (1/2)", "", false, ACCENT);
                paintRow(1, "Подключите телефон/ПК к Wi-Fi:", "", false, MUTED);
                paintRow(2, CameraInfo::ACCESS_POINT);
                snprintf(value, sizeof(value), "Пароль: %s", CameraInfo::ACCESS_PASSWORD);
                paintRow(3, value);
                paintRow(4, CameraInfo::SETUP_URL, "", false, GREEN);
                paintRow(5, "Откройте адрес в браузере", "", false, MUTED);
            } else {
                paintRow(0, "Домашняя сеть (2/2)", "", false, ACCENT);
                paintRow(1, "Телефон/ПК и камера: один Wi-Fi");
                paintRow(2, "Сеть задаётся на сайте камеры", "", false, MUTED);
                paintRow(3, CameraInfo::HOME_URL, "", false, GREEN);
                paintRow(4, "Если имя не открывается:", "", false, MUTED);
                paintRow(5, "IP: сайт камеры > Состояние");
            }
            break;
        case Page::PROCESS: drawProcess(); break;
        case Page::NOTICE: paragraph(notice, 0, ORANGE); break;
        case Page::CONFIRM:
            paragraph("Вернуть заводские настройки? Расписание и калибровки будут удалены.\nOK: сбросить\nBACK: отменить", 0, ORANGE); break;
        case Page::EDIT:
            paintRow(0, editLabel, "", false, ACCENT);
            if (editMax == 1 && editMin == 0) snprintf(value, sizeof(value), "%s", editValue != 0 ? "Да" : "Нет");
            else snprintf(value, sizeof(value), "%.0f", editValue);
            paintRow(1, value, "", false, TEXT, 2);
            snprintf(value, sizeof(value), "От %.0f до %.0f", editMin, editMax); paintRow(2, value, "", false, MUTED);
            paintRow(3, "Удерживайте UP / DOWN", "", false, MUTED);
            paintRow(4, "OK сохраняет значение", "", false, GREEN);
            paintRow(5, "BACK отменяет изменение", "", false, MUTED);
            break;
        case Page::CAL_EMPTY:
            paintRow(0, scaleName(), "", false, ACCENT);
            paragraph("Уберите всё из выбранной миски. Дождитесь стабильности.\nOK: измерить ноль\nBACK: отменить", 1); break;
        case Page::CAL_CAPTURE:
            snprintf(value, sizeof(value), "%u / %u", Sensors::captureProgress(), Settings::CAPTURE_SAMPLES);
            paintRow(0, "Собираем показания", value, false, ACCENT);
            paragraph("Не трогайте миску и груз. Измерение можно отменить кнопкой BACK.", 1); break;
        case Page::CAL_MASS:
            paintRow(0, "Укажите массу груза", "", false, ACCENT);
            snprintf(value, sizeof(value), "%.0f г", knownMassG); paintRow(1, value, "", false, TEXT, 2);
            paragraph("Нужен груз с известной массой. UP / DOWN меняют массу.\nOK: дальше", 2); break;
        case Page::CAL_LOAD:
            snprintf(value, sizeof(value), "Положите груз %.0f г", knownMassG);
            paragraph(value, 0, ACCENT);
            paragraph("В выбранную миску. Дождитесь стабильности.\nOK: измерить груз\nBACK: отменить", 1); break;
        case Page::CAL_RESULT:
            paintRow(0, scaleName(), "", false, ACCENT);
            snprintf(value, sizeof(value), "%.3f", proposedFactor); paintRow(1, "Коэффициент", value);
            snprintf(value, sizeof(value), "%ld", static_cast<long>(zeroRaw)); paintRow(2, "Ноль ADC", value);
            paintRow(3, "OK: сохранить калибровку", "", false, GREEN);
            paintRow(4, "BACK: оставить прежнюю", "", false, MUTED);
            paintRow(5, "Уберите калибровочный груз", "", false, ORANGE); break;
        case Page::PUMP_SECONDS:
            paintRow(0, "Подготовьте мерную ёмкость", "", false, ORANGE);
            snprintf(value, sizeof(value), "%lu с", static_cast<unsigned long>(pumpSeconds));
            paintRow(1, value, "", false, TEXT, 2);
            paragraph("UP / DOWN: от 1 до 60 с.\nOK: запустить насос\nBACK: отменить", 2); break;
        case Page::PUMP_VOLUME:
            paintRow(0, "Сколько воды набралось?", "", false, ACCENT);
            snprintf(value, sizeof(value), "%.1f мл", measuredMl); paintRow(1, value, "", false, TEXT, 2);
            snprintf(value, sizeof(value), "Тест длился %lu с", static_cast<unsigned long>(pumpSeconds)); paintRow(2, value);
            paragraph("UP / DOWN: объём с шагом 0.5 мл.\nOK: сохранить расход\nBACK: отменить", 3); break;
        case Page::TANK_EMPTY:
            paragraph("Опустошите бак до рабочего нулевого уровня. Датчик должен быть на штатном месте.\nOK: измерить пустой бак\nBACK: отменить"); break;
        case Page::TANK_FULL:
            paragraph("Заполните бак до рабочего полного уровня.\nOK: измерить полный бак\nBACK: отменить"); break;
        default: break;
        }
    }
} // namespace

bool Ui::begin()
{
    back.begin(); up.begin(); down.begin(); ok.begin();
    SPI.begin(Pins::TFT_SCK_PIN, -1, Pins::TFT_MOSI_PIN, Pins::TFT_CS_PIN);
    tft.init(240, 320);
    tft.setRotation(1);
    displayAvailable = canvas.getBuffer() != nullptr;
    pinMode(Pins::TFT_BL_PIN, OUTPUT);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(Pins::TFT_BL_PIN, 5000, 8);
#else
    ledcSetup(0, 5000, 8);
    ledcAttachPin(Pins::TFT_BL_PIN, 0);
#endif
    applyDisplaySettings();
    return displayAvailable;
}
void Ui::applyDisplaySettings()
{
    if (!displayAvailable) return;
    const auto& c = Config::get();
    tft.invertDisplay(c.invertDisplay);
    const uint32_t duty = static_cast<uint32_t>(c.brightnessPercent) * 255 / 100;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(Pins::TFT_BL_PIN, duty);
#else
    ledcWrite(0, duty);
#endif
    previousRendered = Page::NOTICE;
    forceDraw = true;
}
bool Ui::serviceMode()
{
    return page != Page::HOME && page != Page::MAIN && page != Page::MANUAL &&
           page != Page::PROCESS && page != Page::DIAGNOSTICS && page != Page::CAMERA;
}

void Ui::handleButtons()
{
    const uint32_t now = millis();
    back.update(now); up.update(now); down.update(now); ok.update(now);
    if (FeedingController::busy()) {
        if (page != Page::PROCESS) { processReturn = Page::HOME; go(Page::PROCESS); }
        wasBusy = true;
        if (back.pressed()) {
            FeedingController::stop();
            processReturn = processReturn == Page::PUMP_VOLUME ? Page::PUMP : processReturn;
            previousRendered = Page::NOTICE;
        }
        return;
    }
    if (FeedingController::info().state == MachineState::ERROR && page != Page::PROCESS) {
        processReturn = Page::HOME; go(Page::PROCESS);
    }
    if (page == Page::PROCESS) {
        if (wasBusy) {
            wasBusy = false;
            previousRendered = Page::NOTICE;
            if (processReturn == Page::PUMP_VOLUME && FeedingController::info().lastSuccess) {
                measuredMl = std::max(0.5f, std::min(1000.0f,
                    std::round(pumpSeconds * 1000 / Config::get().pumpMsPerMl * 2) / 2));
                go(Page::PUMP_VOLUME); return;
            }
        }
        if (ok.pressed() || back.pressed()) {
            FeedingController::acknowledge();
            go(processReturn == Page::PUMP_VOLUME ? Page::PUMP : processReturn);
        }
        return;
    }
    if (page == Page::NOTICE) { if (ok.pressed() || back.pressed()) go(noticeReturn); return; }
    if (page == Page::CONFIRM) {
        if (back.pressed()) go(confirmReturn);
        else if (ok.pressed() && save(Settings::defaults(), confirmReturn)) showNotice("Заводские настройки восстановлены.", Page::HOME);
        return;
    }
    if (page == Page::EDIT) {
        if (back.pressed()) {
            go(editReturn); selection = editSelection; firstVisible = editFirstVisible; return;
        }
        if (up.repeated()) editValue = std::min(editMax, editValue + editStep);
        if (down.repeated()) editValue = std::max(editMin, editValue - editStep);
        if (ok.pressed()) commitEdit();
        return;
    }
    if (page == Page::CAL_EMPTY || page == Page::CAL_LOAD) {
        if (back.pressed()) go(Page::SCALE);
        else if (ok.pressed()) beginCapture(page == Page::CAL_EMPTY ? 0 : 1);
        return;
    }
    if (page == Page::CAL_CAPTURE) {
        if (back.pressed()) { Sensors::cancelCapture(); go(captureStage >= 2 ? Page::SENSOR_SETTINGS : Page::SCALE); }
        return;
    }
    if (page == Page::CAL_MASS) {
        if (back.pressed()) { go(Page::SCALE); return; }
        if (up.repeated()) knownMassG = std::min(2000.0f, knownMassG + 5);
        if (down.repeated()) knownMassG = std::max(5.0f, knownMassG - 5);
        if (ok.pressed()) go(Page::CAL_LOAD);
        return;
    }
    if (page == Page::CAL_RESULT) {
        if (back.pressed()) { go(Page::SCALE); return; }
        if (ok.pressed()) {
            auto c = Config::get();
            auto& calibration = scaleId == ScaleId::FOOD ? c.foodScale : c.waterScale;
            calibration.offset = zeroRaw; calibration.factor = proposedFactor;
            calibration.tared = calibration.calibrated = true;
            if (save(c, Page::SCALE)) showNotice("Калибровка сохранена. Уберите груз и проверьте ноль.", Page::SCALE);
        }
        return;
    }
    if (page == Page::TANK_EMPTY || page == Page::TANK_FULL) {
        if (back.pressed()) go(Page::SENSOR_SETTINGS);
        else if (ok.pressed()) beginCapture(page == Page::TANK_EMPTY ? 2 : 3);
        return;
    }
    if (page == Page::PUMP_SECONDS) {
        if (back.pressed()) { go(Page::PUMP); return; }
        if (up.repeated()) pumpSeconds = std::min(60u, pumpSeconds + 1);
        if (down.repeated()) pumpSeconds = std::max(1u, pumpSeconds - 1);
        if (ok.pressed()) { FeedingController::startPumpTest(pumpSeconds * 1000); startProcess(processReturn); }
        return;
    }
    if (page == Page::PUMP_VOLUME) {
        if (back.pressed()) { go(Page::PUMP); return; }
        if (up.repeated()) measuredMl = std::min(1000.0f, measuredMl + 0.5f);
        if (down.repeated()) measuredMl = std::max(0.5f, measuredMl - 0.5f);
        if (ok.pressed()) {
            auto c = Config::get(); c.pumpMsPerMl = pumpSeconds * 1000.0f / measuredMl;
            if (save(c, Page::PUMP)) showNotice("Расход насоса сохранён.", Page::PUMP);
        }
        return;
    }
    if (page == Page::CAMERA) {
        if (back.pressed()) { go(Page::MAIN); return; }
        const bool previous = up.pressed();
        const bool next = down.pressed();
        const bool advance = ok.pressed();
        if (previous || next || advance) { cameraHelpPage ^= 1; forceDraw = true; }
        return;
    }
    if (page == Page::HOME) { if (ok.pressed()) go(Page::MAIN); return; }
    if (back.pressed()) { go(parent(page)); return; }
    buildRows();
    if (rowCount == 0) return;
    if (up.repeated()) selection = (selection + rowCount - 1) % rowCount;
    if (down.repeated()) selection = (selection + 1) % rowCount;
    if (ok.pressed()) activate(rows[selection]);
}

void Ui::update()
{
    if (!displayAvailable) return;
    updateCapture();
    if (FeedingController::busy() && page != Page::PROCESS) {
        processReturn = Page::HOME; wasBusy = true; go(Page::PROCESS);
    }
    if (FeedingController::info().state == MachineState::ERROR && page != Page::PROCESS) {
        processReturn = Page::HOME; go(Page::PROCESS);
    }
    const bool pageChanged = page != previousRendered;
    if (!pageChanged && !forceDraw && millis() - lastDrawMs < Settings::DISPLAY_INTERVAL_MS) return;
    lastDrawMs = millis();
    forceDraw = false;
    if (pageChanged) { drawChrome(); previousRendered = page; }
    drawPage();
}
