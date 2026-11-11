#include "ui_internal.hpp"

namespace UiInternal
{
    void setEditRange(double value, double minimum, double maximum, double step = 1)
    {
        state.editValue = value;
        state.editMin = minimum;
        state.editMax = maximum;
        state.editStep = step;
    }

    void startEdit(const Row& row)
    {
        state.editField = row.field;
        state.editReturn = state.currentPage;
        state.editSelection = state.selection;
        state.editFirstVisible = state.firstVisible;

        snprintf(state.editLabel, sizeof(state.editLabel), "%s", row.label);
        
        const auto& settings = Config::get();
        const auto& slot = settings.schedule[state.editingSlot];
        
        switch (state.editField)
        {
        case Field::FOOD:           setEditRange(settings.foodPortionG, 1, Settings::MAX_FOOD_PORTION_G);
                                    break;
        
        case Field::WATER:          setEditRange(settings.waterPortionMl, 1, Settings::MAX_WATER_PORTION_ML);
                                    break;
        
        case Field::SLOT_ON:        setEditRange(slot.enabled, 0, 1);
                                    break;
        
        case Field::SLOT_HOUR:      setEditRange(slot.hour, 0, 23);
                                    break;

        case Field::SLOT_MINUTE:    setEditRange(slot.minute, 0, 59);
                                    break;

        case Field::SLOT_FOOD:      setEditRange(slot.foodG, 0, Settings::MAX_FOOD_PORTION_G);
                                    break;

        case Field::SLOT_WATER:     setEditRange(slot.waterMl, 0, Settings::MAX_WATER_PORTION_ML);
                                    break;

        case Field::ONE_DELAY:      setEditRange(state.oneDelayMinutes, 1, 10080, 5);
                                    break;

        case Field::ONE_FOOD:       setEditRange(state.oneFoodG, 0, Settings::MAX_FOOD_PORTION_G);
                                    break;

        case Field::ONE_WATER:      setEditRange(state.oneWaterMl, 0, Settings::MAX_WATER_PORTION_ML);
                                    break;

        case Field::PUMP_FACTOR:    setEditRange(settings.pumpMsPerMl, 10, 20000, 10);
                                    break;

        case Field::FOOD_LIMIT:     setEditRange(settings.foodBowlMaxG, 10, 2000, 10);
                                    break;

        case Field::WATER_LIMIT:    setEditRange(settings.waterBowlMaxG, 10, 2000, 10);
                                    break;

        case Field::FEED_TIMEOUT:   setEditRange(settings.feedTimeoutMs / 1000, 5, 180);
                                    break;

        case Field::WATER_TIMEOUT:  setEditRange(settings.waterTimeoutMs / 1000, 1, 300);
                                    break;

        case Field::BRIGHTNESS:     setEditRange(settings.brightnessPercent, 10, 100, 5);
                                    break;

        case Field::INVERT:         setEditRange(settings.invertDisplay, 0, 1);
                                    break;

        case Field::YEAR:           setEditRange(state.clockYear, 2024, 2099);
                                    break;

        case Field::MONTH:          setEditRange(state.clockMonth, 1, 12);
                                    break;

        case Field::DAY:            setEditRange(state.clockDay, 1, 31);
                                    break;

        case Field::HOUR:           setEditRange(state.clockHour, 0, 23);
                                    break;

        case Field::MINUTE:         setEditRange(state.clockMinute, 0, 59);
                                    break;
        }

        state.editValue = std::max(state.editMin, 
                          std::min(state.editMax, state.editValue));
        
        openPage(Page::EDIT);
    }

    void commitEdit()
    {
        auto settings = Config::get();
        auto& slot = settings.schedule[state.editingSlot];
        bool persistent = true;
        switch (state.editField)
        {
        case Field::FOOD:           settings.foodPortionG = state.editValue;
                                    break;

        case Field::WATER:          settings.waterPortionMl = state.editValue;
                                    break;

        case Field::SLOT_ON:        slot.enabled = state.editValue != 0;
                                    break;

        case Field::SLOT_HOUR:      slot.hour = state.editValue;
                                    break;

        case Field::SLOT_MINUTE:    slot.minute = state.editValue;
                                    break;

        case Field::SLOT_FOOD:      slot.foodG = state.editValue;
                                    break;

        case Field::SLOT_WATER:     slot.waterMl = state.editValue;
                                    break;

        case Field::ONE_DELAY:      state.oneDelayMinutes = state.editValue;
                                    persistent = false;
                                    break;

        case Field::ONE_FOOD:       state.oneFoodG = state.editValue;
                                    persistent = false;
                                    break;

        case Field::ONE_WATER:      state.oneWaterMl = state.editValue;
                                    persistent = false;
                                    break;

        case Field::PUMP_FACTOR:    settings.pumpMsPerMl = state.editValue;
                                    break;

        case Field::FOOD_LIMIT:     settings.foodBowlMaxG = state.editValue;
                                    break;

        case Field::WATER_LIMIT:    settings.waterBowlMaxG = state.editValue;
                                    break;

        case Field::FEED_TIMEOUT:   settings.feedTimeoutMs = state.editValue * 1000;
                                    break;

        case Field::WATER_TIMEOUT:  settings.waterTimeoutMs = state.editValue * 1000;
                                    break;

        case Field::BRIGHTNESS:     settings.brightnessPercent = state.editValue;
                                    break;

        case Field::INVERT:         settings.invertDisplay = state.editValue != 0;
                                    break;

        case Field::YEAR:           state.clockYear = state.editValue;
                                    persistent = false;
                                    break;

        case Field::MONTH:          state.clockMonth = state.editValue;
                                    persistent = false;
                                    break;

        case Field::DAY:            state.clockDay = state.editValue;
                                    persistent = false;
                                    break;

        case Field::HOUR:           state.clockHour = state.editValue;
                                    persistent = false;
                                    break;

        case Field::MINUTE:         state.clockMinute = state.editValue;
                                    persistent = false;
                                    break;
        }
        
        if (!persistent || saveSettings(settings, state.editReturn))
        {
            openPage(state.editReturn);
            state.selection = state.editSelection;
            state.firstVisible = state.editFirstVisible;
        }
    }

    void beginTareCapture()
    {
        if (Sensors::capture(state.scaleId))
        {
            openPage(Page::CAL_CAPTURE);
        }
        else
        {
            showNotice("Нет данных датчика. Проверьте подключение и повторите.", Page::SCALE);
        }
    }

    void saveTareResult(int32_t raw, int32_t span)
    {
        auto settings = Config::get();
        auto& calibration =
            state.scaleId == ScaleId::FOOD ? settings.foodScale : settings.waterScale;

        if (calibration.calibrated && span / std::fabs(calibration.factor) > 3)
        {
            showNotice("Вес нестабилен. Уберите вибрацию и повторите тарирование.", Page::SCALE);
            return;
        }

        calibration.offset = raw;
        calibration.tared = true;
        if (saveSettings(settings, Page::SCALE))
        {
            showNotice("Ноль весов сохранён. Автоматического тарирования при включении нет.",
                       Page::SCALE);
        }
    }

    void updateCapture()
    {
        if (state.currentPage != Page::CAL_CAPTURE)
            return;

        const auto captureState = Sensors::captureState();
        if (captureState == CaptureState::FAILED)
        {
            Sensors::cancelCapture();
            showNotice("Датчик не ответил вовремя. Настройки не изменены.", Page::SCALE);
            return;
        }
        if (captureState != CaptureState::DONE)
            return;

        const int32_t raw = Sensors::capturedRaw();
        const int32_t span = Sensors::capturedSpan();
        Sensors::cancelCapture();
        saveTareResult(raw, span);
    }
} // namespace UiInternal
