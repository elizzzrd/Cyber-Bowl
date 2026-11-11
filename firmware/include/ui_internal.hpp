#pragma once

// Implementation details shared by the UI source files.
// Other firmware modules use include/ui.hpp.
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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace UiInternal
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

    enum class Page : uint8_t
    {
        HOME,
        MAIN,
        MANUAL,
        SCHEDULE,
        SLOT,
        ONE_SHOT,
        SERVICE,
        AUGER,
        PUMP,
        SCALES,
        SCALE,
        SENSOR_SETTINGS,
        LIMITS,
        DISPLAY_SETTINGS,
        CLOCK,
        DIAGNOSTICS,
        EDIT,
        NOTICE,
        CONFIRM,
        CAL_EMPTY,
        CAL_CAPTURE,

        PUMP_SECONDS,
        PUMP_VOLUME,
        PROCESS,
        CAMERA
    };

    enum class Field : uint8_t
    {
        FOOD,
        WATER,
        SLOT_ON,
        SLOT_HOUR,
        SLOT_MINUTE,
        SLOT_FOOD,
        SLOT_WATER,
        ONE_DELAY,
        ONE_FOOD,
        ONE_WATER,
        PUMP_FACTOR,

        FOOD_LIMIT,
        WATER_LIMIT,
        FEED_TIMEOUT,
        WATER_TIMEOUT,
        BRIGHTNESS,
        INVERT,
        YEAR,
        MONTH,
        DAY,
        HOUR,
        MINUTE
    };

    enum class Action : uint8_t
    {
        NONE,
        MANUAL,
        SCHEDULE,
        DIAGNOSTICS,
        SERVICE,
        HOME,
        FOOD,
        WATER,
        BOTH,
        SLOT,
        ONE_SHOT,
        ONE_ARM,
        ONE_CANCEL,
        AUGER,
        PUMP,
        SCALES,
        SENSOR_SETTINGS,
        LIMITS,
        DISPLAY_SETTINGS,
        CLOCK,
        RESET,
        EDIT,
        AUGER_SPEED,
        JOG_FORWARD,
        JOG_BACKWARD,
        AUGER_HOLD,
        PUMP_TEST,
        PUMP_CAL,
        FOOD_SCALE,
        WATER_SCALE,
        TARE,

        CLOCK_SAVE,
        RTC_RETRY,
        BACK,
        CAMERA
    };

    struct Row
    {
        char label[112] = {};
        char value[112] = {};
        Action action = Action::NONE;
        Field field = Field::FOOD;
        int argument = 0;
    };

    struct State
    {
        // Motor test
        uint8_t augerSpeed = 0;

        // Menu navigation
        Row rows[16];
        int rowCount = 0;
        int selection = 0;
        int firstVisible = 0;
        Page currentPage = Page::HOME;
        Page renderedPage = Page::NOTICE;

        // Return destinations
        Page editReturn = Page::MAIN;
        int editSelection = 0;
        int editFirstVisible = 0;
        Page noticeReturn = Page::MAIN;
        Page processReturn = Page::HOME;
        Page confirmReturn = Page::HOME;
        
        // Transient text and row-render cache
        char noticeText[192] = {};
        char editLabel[112] = {};
        char rowCache[VISIBLE_ROWS][256] = {};
        
        // Numeric editor
        Field editField = Field::FOOD;
        double editValue = 0;
        double editMin = 0;
        double editMax = 0;
        double editStep = 1;
        
        // Schedule editor
        int editingSlot = 0;
        uint32_t oneDelayMinutes = 60;
        float oneFoodG = 30;
        float oneWaterMl = 50;

        // RTC editor
        int clockYear = 2026;
        int clockMonth = 1;
        int clockDay = 1;
        int clockHour = 12;
        int clockMinute = 0;

        // Scale measurement workflow
        ScaleId scaleId = ScaleId::FOOD;
        
        // Pump calibration workflow
        uint32_t pumpSeconds = 10;
        float measuredMl = 10;
        
        // Display refresh and active-operation tracking
        uint32_t lastDrawMs = 0;
        bool operationWasRunning = false;
        bool redrawRequested = true;
        bool displayReady = false;
        uint8_t cameraHelpPage = 0;
        int scrollbarFirst = -1;
        int scrollbarCount = -1;
    };

    extern State state;
    extern Adafruit_ST7789 tft;
    extern GFXcanvas16 canvas;
    extern Button back;
    extern Button up;
    extern Button down;
    extern Button ok;

    void openPage(Page next);
    void showNotice(const char* text, Page returnPage);
    bool saveSettings(const Settings::Data& candidate, Page returnPage);
    const char* selectedScaleName();
    void buildRows();
    Page parentPage(Page currentPage);
    void activateMenuRow(const Row& row);
    void startProcess(Page returnPage);
    void startEdit(const Row& row);
    void commitEdit();
    void beginTareCapture();
    void updateCapture();
    void drawChrome();
    void drawPage();
} // namespace UiInternal
