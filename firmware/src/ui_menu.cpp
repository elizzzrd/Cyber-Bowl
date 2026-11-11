#include "ui_internal.hpp"

namespace UiInternal
{
    void addMenuRow(const char* label, const char* value, Action action = Action::NONE,
                    Field field = Field::FOOD, int argument = 0)
    {
        if (state.rowCount >= 16)
            return;
        auto& row = state.rows[state.rowCount++];
        snprintf(row.label, sizeof(row.label), "%s", label);
        snprintf(row.value, sizeof(row.value), "%s", value);
        row.action = action;
        row.field = field;
        row.argument = argument;
    }

    void addEditableRow(const char* label, double value, const char* unit, Field field,
                        int decimals = 0)
    {
        char text[64];
        snprintf(text, sizeof(text), "%.*f %s", decimals, value, unit);
        addMenuRow(label, text, Action::EDIT, field);
    }

    void addBooleanRow(const char* label, bool value, Field field)
    {
        addMenuRow(label, value ? "Да" : "Нет", Action::EDIT, field);
    }

    void buildMainRows()
    {
        addMenuRow("Подать сейчас", "", Action::MANUAL);
        addMenuRow("Расписание", "", Action::SCHEDULE);
        addMenuRow("Диагностика", "", Action::DIAGNOSTICS);
        addMenuRow("Настройки", "", Action::SERVICE);
        addMenuRow("Камера", "", Action::CAMERA);
        addMenuRow("Главный экран", "", Action::HOME);
    }

    void buildManualRows()
    {
        const auto& settings = Config::get();
        addEditableRow("Порция корма", settings.foodPortionG, "г", Field::FOOD);
        addEditableRow("Порция воды", settings.waterPortionMl, "мл", Field::WATER);
        addMenuRow("Выдать корм", "", Action::FOOD);
        addMenuRow("Выдать воду", "", Action::WATER);
        addMenuRow("Корм и вода", "", Action::BOTH);
    }

    void buildScheduleRows()
    {
        const auto& settings = Config::get();
        char text[112];
        for (int i = 0; i < static_cast<int>(Settings::SCHEDULE_COUNT); ++i)
        {
            const auto& slot = settings.schedule[i];
            char label[32];
            snprintf(label, sizeof(label), "Таймер %d", i + 1);
            snprintf(text, sizeof(text), "%s %02u:%02u %.0fг/%.0fмл", slot.enabled ? "+" : "-",
                     slot.hour, slot.minute, slot.foodG, slot.waterMl);
            addMenuRow(label, text, Action::SLOT, Field::FOOD, i);
        }
        addMenuRow("Разовый таймер", settings.oneShot.enabled ? "Включён" : "Выключен",
                   Action::ONE_SHOT);
    }

    void buildSlotRows()
    {
        const auto& settings = Config::get();
        const auto& slot = settings.schedule[state.editingSlot];
        addBooleanRow("Включён", slot.enabled, Field::SLOT_ON);
        addEditableRow("Час", slot.hour, "", Field::SLOT_HOUR);
        addEditableRow("Минута", slot.minute, "", Field::SLOT_MINUTE);
        addEditableRow("Корм", slot.foodG, "г", Field::SLOT_FOOD);
        addEditableRow("Вода", slot.waterMl, "мл", Field::SLOT_WATER);
        addMenuRow("Повтор", "Каждый день");
    }

    void buildOneShotRows()
    {
        const auto& settings = Config::get();
        char text[112];
        addEditableRow("Через", state.oneDelayMinutes, "мин", Field::ONE_DELAY);
        addEditableRow("Корм", state.oneFoodG, "г", Field::ONE_FOOD);
        addEditableRow("Вода", state.oneWaterMl, "мл", Field::ONE_WATER);
        addMenuRow("Поставить таймер", "", Action::ONE_ARM);
        addMenuRow("Отменить таймер", "", Action::ONE_CANCEL);
        if (settings.oneShot.enabled)
        {
            DateTime due(settings.oneShot.dueEpoch);
            snprintf(text, sizeof(text), "%02u.%02u %02u:%02u", due.day(), due.month(), due.hour(),
                     due.minute());
            addMenuRow("Сейчас назначен", text);
        }
    }

    void buildServiceRows()
    {
        addMenuRow("Двигатель", "", Action::AUGER);
        addMenuRow("Насос", "", Action::PUMP);
        addMenuRow("Весы", "", Action::SCALES);
        addMenuRow("Датчики бака", "", Action::SENSOR_SETTINGS);
        addMenuRow("Лимиты подачи", "", Action::LIMITS);
        addMenuRow("Часы и дата", "", Action::CLOCK);
        addMenuRow("Дисплей", "", Action::DISPLAY_SETTINGS);
        addMenuRow("Сброс настроек", "", Action::RESET);
    }

    void buildAugerRows()
    {
        char text[112];
        snprintf(text, sizeof(text), "%lu шаг/с",
                 static_cast<unsigned long>((1000000 + Settings::AUGER_TEST_PERIOD_US[state.augerSpeed] / 2) /
                 Settings::AUGER_TEST_PERIOD_US[state.augerSpeed]));
        
        addMenuRow("Скорость теста", text, Action::AUGER_SPEED);
        addMenuRow("3 оборота вперёд", "", Action::JOG_FORWARD);
        addMenuRow("3 оборота назад", "", Action::JOG_BACKWARD);
        addMenuRow("Удержать вал 3 с", "", Action::AUGER_HOLD);
    }

    void buildPumpRows()
    {
        const auto& settings = Config::get();
        addEditableRow("Калибровка", settings.pumpMsPerMl, "мс/мл", Field::PUMP_FACTOR, 1);
        addMenuRow("Тест по времени", "", Action::PUMP_TEST);
        addMenuRow("Измерить расход", "", Action::PUMP_CAL);
        addMenuRow("Лимит по времени", "В разделе лимитов");
    }

    void buildScalesRows()
    {
        const auto& settings = Config::get();
        addMenuRow("Весы корма", settings.foodScale.calibrated ? "Калиброваны" : "Нужна калибровка",
                   Action::FOOD_SCALE);
        addMenuRow("Весы воды", settings.waterScale.calibrated ? "Калиброваны" : "Нужна калибровка",
                   Action::WATER_SCALE);
    }

    void buildScaleRows()
    {
        const auto& settings = Config::get();
        char text[112];
        const auto& value = Sensors::scale(state.scaleId);
        const auto& calibration =
            state.scaleId == ScaleId::FOOD ? settings.foodScale : settings.waterScale;
        addMenuRow("Тарировать", "Пустая миска", Action::TARE);
        snprintf(text, sizeof(text), "%.2f", calibration.factor);
        addMenuRow("Коэффициент", text);
        if (value.valid)
            snprintf(text, sizeof(text), "%.1f г", value.grams);
        else
            snprintf(text, sizeof(text), "%s", value.connected ? "Нужна калибровка" : "Нет данных");
        addMenuRow("Показание", text);
        snprintf(text, sizeof(text), "%ld", static_cast<long>(value.raw));
        addMenuRow("Сырой ADC", text);
    }

    void buildSensorSettingsRows()
    {
        char text[112];
        addMenuRow("Датчик в воде", Sensors::readings().waterPresent ? "Да" : "Нет");
        snprintf(text, sizeof(text), "%d", Sensors::readings().tankRaw);
        addMenuRow("Бак: ADC", text);
    }

    void buildLimitsRows()
    {
        const auto& settings = Config::get();
        addEditableRow("Миска корма", settings.foodBowlMaxG, "г", Field::FOOD_LIMIT);
        addEditableRow("Миска воды", settings.waterBowlMaxG, "г", Field::WATER_LIMIT);
        addEditableRow("Подача корма", settings.feedTimeoutMs / 1000, "с", Field::FEED_TIMEOUT);
        addEditableRow("Подача воды", settings.waterTimeoutMs / 1000, "с", Field::WATER_TIMEOUT);
    }

    void buildDisplaySettingsRows()
    {
        const auto& settings = Config::get();
        addEditableRow("Яркость", settings.brightnessPercent, "%", Field::BRIGHTNESS);
        addBooleanRow("Инверсия цветов", settings.invertDisplay, Field::INVERT);
        addMenuRow("Обновление полей", "При изменении");
    }

    void buildClockRows()
    {
        addEditableRow("Год", state.clockYear, "", Field::YEAR);
        addEditableRow("Месяц", state.clockMonth, "", Field::MONTH);
        addEditableRow("День", state.clockDay, "", Field::DAY);
        addEditableRow("Час", state.clockHour, "", Field::HOUR);
        addEditableRow("Минута", state.clockMinute, "", Field::MINUTE);
        addMenuRow("Записать в RTC", "", Action::CLOCK_SAVE);
        addMenuRow("Повторить поиск RTC", "", Action::RTC_RETRY);
    }

    void buildRows()
    {
        state.rowCount = 0;
        switch (state.currentPage)
        {
        case Page::MAIN:                { buildMainRows();              break;}
        case Page::MANUAL:              { buildManualRows();            break; }
        case Page::SCHEDULE:            { buildScheduleRows();          break; }
        case Page::SLOT:                { buildSlotRows();              break; }
        case Page::ONE_SHOT:            { buildOneShotRows();           break; }
        case Page::SERVICE:             { buildServiceRows();           break; }
        case Page::AUGER:               { buildAugerRows();             break; }
        case Page::PUMP:                { buildPumpRows();              break; }
        case Page::SCALES:              { buildScalesRows();            break; }
        case Page::SCALE:               { buildScaleRows();             break; }
        case Page::SENSOR_SETTINGS:     { buildSensorSettingsRows();    break; }
        case Page::LIMITS:              { buildLimitsRows();            break; }
        case Page::DISPLAY_SETTINGS:    { buildDisplaySettingsRows();   break; }
        case Page::CLOCK:               { buildClockRows();             break; }
        default:                                                        break;
        }
    }

    Page parentPage(Page childPage)
    {
        switch (childPage)
        {
        case Page::MAIN:                { return Page::HOME;     }
        case Page::MANUAL:
        case Page::SCHEDULE:
        case Page::SERVICE:
        case Page::DIAGNOSTICS:
        case Page::CAMERA:              { return Page::MAIN;     }
        case Page::SLOT:
        case Page::ONE_SHOT:            { return Page::SCHEDULE; }
        case Page::SCALE:               { return Page::SCALES;   }
        case Page::AUGER:
        case Page::PUMP:
        case Page::SCALES:
        case Page::SENSOR_SETTINGS:
        case Page::LIMITS:
        case Page::DISPLAY_SETTINGS:
        case Page::CLOCK:               { return Page::SERVICE;  }
        default:                        { return Page::HOME;     }
        }
    }

    void startProcess(Page returnPage)
    {
        state.processReturn = returnPage;
        state.operationWasRunning = FeedingController::busy();
        openPage(Page::PROCESS);
    }

    void openClockSettings()
    {
        const auto now = Clock::now();

        state.clockYear = now.year();
        state.clockMonth = now.month();
        state.clockDay = now.day();
        state.clockHour = now.hour();
        state.clockMinute = now.minute();
        
        openPage(Page::CLOCK);
    }

    void openOneShotSettings()
    {
        const auto& settings = Config::get();
        
        state.oneFoodG = settings.oneShot.foodG;
        state.oneWaterMl = settings.oneShot.waterMl;
        state.oneDelayMinutes = 60;
        
        openPage(Page::ONE_SHOT);
    }

    void armOneShot()
    {
        if (!Clock::valid())
        {
            showNotice("Сначала настройте часы RTC.", Page::ONE_SHOT);
            return;
        }

        auto settings = Config::get();
        
        settings.oneShot.enabled = true;
        settings.oneShot.dueEpoch = Clock::now().unixtime() + state.oneDelayMinutes * 60;
        settings.oneShot.foodG = state.oneFoodG;
        settings.oneShot.waterMl = state.oneWaterMl;
        
        if (saveSettings(settings, Page::ONE_SHOT))
        {
            showNotice("Разовый таймер сохранён.", Page::SCHEDULE);
        }
    }

    void cancelOneShot()
    {
        auto settings = Config::get();
        
        settings.oneShot.enabled = false;
        
        if (saveSettings(settings, Page::ONE_SHOT))
        {
            showNotice("Разовый таймер отменён.", Page::SCHEDULE);
        }
    }

    void saveClockSettings()
    {
        const DateTime date(state.clockYear, state.clockMonth, 
                            state.clockDay, state.clockHour,
                            state.clockMinute, 0);
        
        if (Clock::set(date))
        {
            showNotice("Дата и время записаны в RTC.", Page::CLOCK);
        }
        else
        {
            showNotice("RTC недоступен или дата неверна.", Page::CLOCK);
        }
    }

    void activateMenuRow(const Row& row)
    {
        const auto& settings = Config::get();
        switch (row.action)
        {
        case Action::NONE:                      { break; }
        case Action::MANUAL:                    { openPage(Page::MANUAL); break; }                   
        
        case Action::SCHEDULE:                  { openPage(Page::SCHEDULE); break; }
        
        case Action::DIAGNOSTICS:               { openPage(Page::DIAGNOSTICS); break; }
        
        case Action::SERVICE:                   { openPage(Page::SERVICE); break; }
        
        case Action::CAMERA:                    { state.cameraHelpPage = 0;
                                                openPage(Page::CAMERA);
                                                break; }
        
        case Action::HOME:                      { openPage(Page::HOME); break; }
        
        case Action::FOOD:                      { FeedingController::startCycle(settings.foodPortionG, 0);
                                                startProcess(Page::HOME);
                                                break; }
        
        case Action::WATER:                     { FeedingController::startCycle(0, settings.waterPortionMl);
                                                startProcess(Page::HOME);
                                                break; }
        
        case Action::BOTH:                      { FeedingController::startCycle(settings.foodPortionG, settings.waterPortionMl);
                                                startProcess(Page::HOME);
                                                break; }
        
        case Action::SLOT:                      { state.editingSlot = row.argument;
                                                openPage(Page::SLOT);
                                                break; }
        
        case Action::ONE_SHOT:                  { openOneShotSettings(); break; }
        
        case Action::ONE_ARM:                   { armOneShot(); break; }
        
        case Action::ONE_CANCEL:                { cancelOneShot(); break; }
        
        case Action::AUGER:                     { openPage(Page::AUGER); break; }
        
        case Action::PUMP:                      { openPage(Page::PUMP); break; }
        
        case Action::SCALES:                    { openPage(Page::SCALES); break; }
        
        case Action::SENSOR_SETTINGS:           { openPage(Page::SENSOR_SETTINGS); break; }
        
        case Action::LIMITS:                    { openPage(Page::LIMITS); break; }
        
        case Action::DISPLAY_SETTINGS:          { openPage(Page::DISPLAY_SETTINGS); break; }
        
        case Action::CLOCK:                     { openClockSettings(); break; }
        
        case Action::RESET:                     { state.confirmReturn = Page::SERVICE;
                                                openPage(Page::CONFIRM);
                                                break; }
        
        case Action::EDIT:                      { startEdit(row); break; }
        
        case Action::AUGER_SPEED:               { state.augerSpeed = (state.augerSpeed + 1) %
                                                (sizeof(Settings::AUGER_TEST_PERIOD_US) / sizeof(uint32_t));
                                                state.redrawRequested = true;
                                                break; }

        case Action::JOG_FORWARD:
        case Action::JOG_BACKWARD:              { FeedingController::startAugerTest(row.action == Action::JOG_FORWARD,
                                                Settings::AUGER_TEST_PERIOD_US[state.augerSpeed]);
                                                startProcess(Page::AUGER);
                                                break; }

        case Action::AUGER_HOLD:                { FeedingController::startAugerHold();
                                                startProcess(Page::AUGER);
                                                break; }

        case Action::PUMP_TEST:                 { state.processReturn = Page::PUMP;
                                                openPage(Page::PUMP_SECONDS);
                                                break; }

        case Action::PUMP_CAL:                  { state.processReturn = Page::PUMP_VOLUME;
                                                openPage(Page::PUMP_SECONDS);
                                                break; }

        case Action::FOOD_SCALE:                { state.scaleId = ScaleId::FOOD;
                                                openPage(Page::SCALE);
                                                break; }

        case Action::WATER_SCALE:               { state.scaleId = ScaleId::WATER;
                                                openPage(Page::SCALE);
                                                break; }

        case Action::TARE:                      { openPage(Page::CAL_EMPTY); break; }

        case Action::CLOCK_SAVE:                { saveClockSettings(); break; }

        case Action::RTC_RETRY:                 { showNotice(Clock::retry() ? "RTC найден. Проверьте дату и время."
                                                : "RTC не найден. Проверьте подключение.", Page::CLOCK);
                                                break; }

        case Action::BACK:                      { openPage(parentPage(state.currentPage)); break; }
        }
    }
} // namespace UiInternal
