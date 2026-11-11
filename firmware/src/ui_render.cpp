#include "ui_internal.hpp"

namespace UiInternal
{
    const char* pageTitle()
    {
        switch (state.currentPage)
        {
        case Page::HOME:                return "CYBER BOWL";
        
        case Page::MAIN:                return "Главное меню";
        
        case Page::MANUAL:              return "Подать сейчас";
        
        case Page::SCHEDULE:            return "Расписание";
        
        case Page::SLOT:                return "Ежедневный таймер";
        
        case Page::ONE_SHOT:            return "Разовый таймер";
        
        case Page::SERVICE:             return "Настройки";
        
        case Page::AUGER:               return "Шаговый двигатель";
        
        case Page::PUMP:                return "Насос";
        
        case Page::SCALES:              return "Весы";
        
        case Page::SCALE:               return selectedScaleName();
        
        case Page::SENSOR_SETTINGS:     return "Датчики";
        
        case Page::LIMITS:              return "Лимиты подачи";
        
        case Page::DISPLAY_SETTINGS:    return "Дисплей";
        
        case Page::CLOCK:               return "Часы и дата";
        
        case Page::DIAGNOSTICS:         return "Диагностика";
        
        case Page::CAMERA:              return "Камера: как открыть видео";
        
        case Page::EDIT:                return "Изменить значение";
        
        case Page::NOTICE:              return "Сообщение";
        
        case Page::CONFIRM:             return "Сброс настроек";
        
        case Page::CAL_EMPTY:           return "Тарирование весов";
        
        case Page::CAL_CAPTURE:         return "Измерение";
        
        case Page::PUMP_SECONDS:        return "Время теста насоса";
        
        case Page::PUMP_VOLUME:         return "Измеренный объём";
        
        case Page::PROCESS:
            return FeedingController::info().state == MachineState::ERROR ? "Ошибка" : "Выполнение";
        }

        return "CYBER BOWL";
    }

    const char* footerText()
    {
        if (FeedingController::busy())
            return "BACK: остановить подачу";
        
        if (state.currentPage == Page::HOME)
            return "OK: меню";
        
        if (state.currentPage == Page::EDIT         || 
            state.currentPage == Page::PUMP_SECONDS ||
            state.currentPage == Page::PUMP_VOLUME)
            { return "UP/DN:+/-  OK:принять  BACK:отмена"; }
        
        if (state.currentPage == Page::CAL_CAPTURE)
            { return "BACK: отменить измерение"; }

        if (state.currentPage == Page::PROCESS      || 
            state.currentPage == Page::NOTICE)
            { return "OK / BACK: закрыть"; }

        if (state.currentPage == Page::DIAGNOSTICS)
            { return "BACK: меню"; }

        if (state.currentPage == Page::CAMERA)
            { return "UP/DN:страница  BACK:меню"; }

        if (state.currentPage == Page::CAL_EMPTY    || 
            state.currentPage == Page::CONFIRM)
            { return "OK: принять  BACK: отмена"; }
        
        return "UP/DN:выбор  OK:вход  BACK:назад";
    }

    void drawChrome()
    {
        tft.fillScreen(BACKGROUND); // Only when changing pages, never on the periodic refresh.
        tft.fillRect(0, 0, 320, 28, PANEL);

        UiFont::draw(tft, 10, 5, pageTitle(), TEXT, 1, Ui::serviceMode() ? 230 : 298);
        if (Ui::serviceMode())
        {
            UiFont::draw(tft, 251, 5, "ПАУЗА", ORANGE, 1, 64);
            tft.fillRect(0, 216, 320, 24, PANEL);
        }    
        UiFont::draw(tft, 7, 219, footerText(), MUTED, 1, 306);
        
        for (auto& cache : state.rowCache)
        {
            cache[0] = '\0';
            state.scrollbarFirst = state.scrollbarCount = -1;
        }
    }

    void paintRow(int slot, const char* left, const char* right = "", bool selected = false,
                  uint16_t color = TEXT, uint8_t scale = 1)
    {
        if (slot < 0 || slot >= VISIBLE_ROWS)
            return;
        
        char key[256];
        snprintf(key, sizeof(key), "%u/%u/%u/%s|%s", selected, color, scale, left, right);
        
        if (strcmp(state.rowCache[slot], key) == 0)
            return;
        
        snprintf(state.rowCache[slot], sizeof(state.rowCache[slot]), "%s", key);
        canvas.fillScreen(selected ? 0x1A0A : BACKGROUND);
        const int rightWidth = std::min(170, UiFont::width(right));
        
        if (selected)
        {
            canvas.fillRect(0, 1, 3, ROW_HEIGHT - 2, ACCENT);
            UiFont::draw(canvas, 7, 5, ">", ACCENT);
        }
        
        const int x = selected ? 22 : 7;
        const int available = right[0] ? 304 - rightWidth - x - 12 : 304 - x - 5;
        UiFont::draw(canvas, x, scale == 2 ? 0 : 5, left, color, scale, available);
        
        if (right[0])
            UiFont::draw(canvas, 299 - rightWidth, 5, right, 
                        selected ? ACCENT : MUTED, 1, rightWidth);
        tft.drawRGBBitmap(8, 36 + slot * ROW_HEIGHT, canvas.getBuffer(), 304, ROW_HEIGHT);
    }

    void paragraph(const char* text, int startRow = 0, uint16_t color = TEXT)
    {
        char lines[6][160] = {};
        int line = 0;
        
        while (*text && line < VISIBLE_ROWS - startRow)
        {
            while (*text == ' ')
                ++text;
            if (!*text)
                break;
            char word[112] = {};
            
            size_t n = 0;
            while (*text && *text != ' ' && *text != '\n')
            {
                if (n + 1 < sizeof(word))
                    word[n++] = *text;
                ++text;
            }
            
            char candidate[272];
            snprintf(candidate, sizeof(candidate), "%s%s%s", lines[line], lines[line][0] ? " " : "",
                     word);
            
            if (UiFont::width(candidate) > 286 && lines[line][0])
            {
                if (++line >= VISIBLE_ROWS - startRow)
                    break;
                snprintf(lines[line], sizeof(lines[line]), "%s", word);
            }
            else
            {
                const size_t length = std::min(strlen(candidate), sizeof(lines[line]) - 1);
                memcpy(lines[line], candidate, length);
                lines[line][length] = '\0';
            }
            
            if (*text == '\n')
            {
                ++line;
                ++text;
            }
        }
        
        for (int i = 0; i < VISIBLE_ROWS - startRow; ++i)
            paintRow(startRow + i, lines[i], "", false, color);
    }

    void drawHome()
    {
        char left[112], right[112];
        const auto& readings = Sensors::readings();
        
        if (Clock::valid())
        {
            const auto now = Clock::now();
            snprintf(left, sizeof(left), "%02u:%02u:%02u", now.hour(), now.minute(), now.second());
            snprintf(right, sizeof(right), "%02u.%02u.%04u", now.day(), now.month(), now.year());
        }
        else
        {
            snprintf(left, sizeof(left), "%s",
                     Clock::connected() ? "Настройте часы" : "RTC не найден");
            right[0] = '\0';
        }
        
        if (strcmp(state.rowCache[0], left) != 0)
        {
            snprintf(state.rowCache[0], sizeof(state.rowCache[0]), "%s", left);
            const uint8_t scale = Clock::valid() ? 4 : 1;
            canvas.fillScreen(BACKGROUND);
            UiFont::draw(canvas, (304 - UiFont::width(left, scale)) / 2, 0, left,
                         Clock::valid() ? ACCENT : ORANGE, scale);
            tft.drawRGBBitmap(8, 36, canvas.getBuffer(), 304, 2 * ROW_HEIGHT);
        }
        
        paintRow(2, right, "", false, TEXT, 2);
        
        if (readings.food.valid)
            snprintf(right, sizeof(right), "%.1f г", readings.food.grams);
        else
            snprintf(right, sizeof(right), "%s",
                     readings.food.connected ? "Калибровка" : "Нет данных");
        
        paintRow(3, "Миска корма", right);
        
        if (readings.water.valid)
            snprintf(right, sizeof(right), "%.1f г", readings.water.grams);
        else
            snprintf(right, sizeof(right), "%s",
                     readings.water.connected ? "Калибровка" : "Нет данных");
        
        paintRow(4, "Миска воды", right);
        paintRow(5, FeedingController::nextFeedingText(), "", false, ACCENT);
    }

    void drawDiagnostics()
    {
        const auto& readings = Sensors::readings();
        char value[96];
        
        paintRow(0, "RTC",
                 Clock::valid() ? "Работает" : (Clock::connected() ? "Нужно время" : "Нет связи"));
        
        snprintf(value, sizeof(value), "%s %ld", readings.food.connected ? "OK" : "Нет",
                 static_cast<long>(readings.food.raw));
        
        snprintf(value, sizeof(value), "%s %ld", readings.water.connected ? "OK" : "Нет",
                 static_cast<long>(readings.water.raw));

        paintRow(2, "HX711 вода", value);

        snprintf(value, sizeof(value), "%d / %s", readings.irRaw,
                 readings.foodAvailable ? "Корм есть" : "Пусто");

        paintRow(3, "ИК: GPIO", value);

        snprintf(value, sizeof(value), "%s / %d", readings.waterPresent ? "Да" : "Нет",
                 readings.tankRaw);

        paintRow(4, "Бак: вода / ADC", value);

        paintRow(5, "Память настроек", Config::storageReady() ? "Доступна" : "Ошибка");
    }

    void drawProcess()
    {
        const auto& processInfo = FeedingController::info();
        const auto& readings = Sensors::readings();
        char text[112];
        
        const uint32_t elapsed = millis() - processInfo.startedMs;
        const uint32_t remaining =
            elapsed >= processInfo.durationMs ? 0 : processInfo.durationMs - elapsed;
        
        if (processInfo.state == MachineState::ERROR)
        {
            paragraph(processInfo.message, 0, RED);
            return;
        }
        
        if (!FeedingController::busy())
        {
            paragraph(processInfo.message, 0, processInfo.lastSuccess ? GREEN : ORANGE);
            return;
        }
        
        paintRow(0, processInfo.message, "", false, ACCENT);
        snprintf(text, sizeof(text), "%.1f с", remaining / 1000.0f);
        paintRow(1, "Осталось до лимита", text);
        
        if (processInfo.state == MachineState::FEEDING)
        {
            snprintf(text, sizeof(text), "%.1f / %.1f г", readings.food.grams,
                     processInfo.foodTargetG);
            paintRow(2, "Вес / цель", text);
            snprintf(text, sizeof(text), "%lu",
                     static_cast<unsigned long>(processInfo.completedSteps));
            paintRow(3, "Импульсов STEP", text);
        }
        else if (processInfo.state == MachineState::WATERING)
        {
            snprintf(text, sizeof(text), "%.0f мл", processInfo.requestedWaterMl);
            paintRow(2, "Порция воды", text);
            snprintf(text, sizeof(text), "%.1f г", readings.water.grams);
            paintRow(3, "Вес миски", text);
        }
        else if (processInfo.state == MachineState::AUGER_TEST)
        {
            snprintf(text, sizeof(text), "%lu / %lu",
                     static_cast<unsigned long>(processInfo.completedSteps),
                     static_cast<unsigned long>(Settings::AUGER_TEST_STEPS));
            paintRow(2, "Импульсов STEP", text);
            paintRow(3, "Режим", "Тест без весов");
        }
        else if (processInfo.state == MachineState::AUGER_HOLD)
        {
            paintRow(2, "Вал удерживается");
            paintRow(3, "");
        }
        else
        {
            paintRow(2, "Режим", "Тест по времени");
            paintRow(3, "Подготовьте", "Мерную ёмкость");
        }
        
        paintRow(4, "BACK останавливает подачу", "", false, ORANGE);
        paintRow(5, "", "");
    }

    void drawMenuRows()
    {
        state.selection = std::max(0, std::min(state.selection, state.rowCount - 1));
        
        if (state.selection < state.firstVisible)
            state.firstVisible = state.selection;
        
        if (state.selection >= state.firstVisible + VISIBLE_ROWS)
            state.firstVisible = state.selection - VISIBLE_ROWS + 1;
        
        if (state.rowCount > VISIBLE_ROWS &&
            (state.scrollbarFirst != state.firstVisible || state.scrollbarCount != state.rowCount))
        {
            const int height = ROW_HEIGHT * VISIBLE_ROWS;
            const int thumb = height * VISIBLE_ROWS / state.rowCount;
            const int y =
                36 + (height - thumb) * state.firstVisible / (state.rowCount - VISIBLE_ROWS);
            tft.fillRect(316, 36, 2, height, PANEL);
            tft.fillRect(316, y, 2, thumb, ACCENT);
            state.scrollbarFirst = state.firstVisible;
            state.scrollbarCount = state.rowCount;
        }
        
        for (int i = 0; i < VISIBLE_ROWS; ++i)
        {
            const int index = state.firstVisible + i;
            if (index < state.rowCount)
                paintRow(i, state.rows[index].label, state.rows[index].value,
                         index == state.selection);
            else
                paintRow(i, "");
        }
    }

    void drawCamera()
    {
        char value[112];
        if (state.cameraHelpPage == 0)
        {
            paintRow(0, "Прямое подключение (1/2)", "", false, ACCENT);
            paintRow(1, "Подключите телефон/ПК к Wi-Fi:", "", false, MUTED);
            paintRow(2, CameraInfo::ACCESS_POINT);
            snprintf(value, sizeof(value), "Пароль: %s", CameraInfo::ACCESS_PASSWORD);
            paintRow(3, value);
            paintRow(4, CameraInfo::SETUP_URL, "", false, GREEN);
            paintRow(5, "Откройте адрес в браузере", "", false, MUTED);
        }
        else
        {
            paintRow(0, "Домашняя сеть (2/2)", "", false, ACCENT);
            paintRow(1, "Телефон/ПК и камера: один Wi-Fi");
            paintRow(2, "Сеть задаётся на сайте камеры", "", false, MUTED);
            paintRow(3, CameraInfo::HOME_URL, "", false, GREEN);
            paintRow(4, "Если имя не открывается:", "", false, MUTED);
            paintRow(5, "IP: сайт камеры > Состояние");
        }
    }

    void drawNotice()
    {
        paragraph(state.noticeText, 0, ORANGE);
    }

    void drawConfirm()
    {
        paragraph("Вернуть заводские настройки? Расписание и калибровки будут удалены.\nOK: "
                  "сбросить\nBACK: отменить",
                  0, ORANGE);
    }

    void drawEdit()
    {
        char value[112];
        paintRow(0, state.editLabel, "", false, ACCENT);
        if (state.editMax == 1 && state.editMin == 0)
            snprintf(value, sizeof(value), "%s", state.editValue != 0 ? "Да" : "Нет");
        else
            snprintf(value, sizeof(value), "%.0f", state.editValue);
        paintRow(1, value, "", false, TEXT, 2);
        snprintf(value, sizeof(value), "От %.0f до %.0f", state.editMin, state.editMax);
        paintRow(2, value, "", false, MUTED);
        paintRow(3, "Удерживайте UP / DOWN", "", false, MUTED);
        paintRow(4, "OK сохраняет значение", "", false, GREEN);
        paintRow(5, "BACK отменяет изменение", "", false, MUTED);
    }

    void drawTarePreparation()
    {
        paintRow(0, selectedScaleName(), "", false, ACCENT);
        paragraph("Уберите всё из выбранной миски. Дождитесь стабильности.\nOK: измерить "
                  "ноль\nBACK: отменить",
                  1);
    }

    void drawCalCapture()
    {
        char value[112];
        snprintf(value, sizeof(value), "%u / %u", Sensors::captureProgress(),
                 Settings::CAPTURE_SAMPLES);
        paintRow(0, "Собираем показания", value, false, ACCENT);
        paragraph("Не трогайте миску и груз. Измерение можно отменить кнопкой BACK.", 1);
    }

    void drawPumpSeconds()
    {
        char value[112];
        paintRow(0, "Подготовьте мерную ёмкость", "", false, ORANGE);
        snprintf(value, sizeof(value), "%lu с", static_cast<unsigned long>(state.pumpSeconds));
        paintRow(1, value, "", false, TEXT, 2);
        paragraph("UP / DOWN: от 1 до 60 с.\nOK: запустить насос\nBACK: отменить", 2);
    }

    void drawPumpVolume()
    {
        char value[112];
        paintRow(0, "Сколько воды набралось?", "", false, ACCENT);
        snprintf(value, sizeof(value), "%.1f мл", state.measuredMl);
        paintRow(1, value, "", false, TEXT, 2);
        snprintf(value, sizeof(value), "Тест длился %lu с",
                 static_cast<unsigned long>(state.pumpSeconds));
        paintRow(2, value);
        paragraph("UP / DOWN: объём с шагом 0.5 мл.\nOK: сохранить расход\nBACK: отменить", 3);
    }

    void drawPage()
    {
        buildRows();
        if (state.rowCount > 0)
        {
            drawMenuRows();
            return;
        }

        switch (state.currentPage)
        {
        case Page::HOME:                { drawHome();               break; }

        case Page::DIAGNOSTICS:         { drawDiagnostics();        break; }

        case Page::CAMERA:              { drawCamera();             break; }

        case Page::PROCESS:             { drawProcess();            break; }

        case Page::NOTICE:              { drawNotice();             break; }

        case Page::CONFIRM:             { drawConfirm();            break; }

        case Page::EDIT:                { drawEdit();               break; }

        case Page::CAL_EMPTY:           { drawTarePreparation();    break; }

        case Page::CAL_CAPTURE:         { drawCalCapture();         break; }

        case Page::PUMP_SECONDS:        { drawPumpSeconds();        break; }

        case Page::PUMP_VOLUME:         { drawPumpVolume();         break; }
        
        default:                        { break; }
        }
    }
} // namespace UiInternal
