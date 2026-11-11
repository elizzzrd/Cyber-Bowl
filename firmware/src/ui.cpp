#include "ui_internal.hpp"
#include <esp_arduino_version.h>


namespace UiInternal
{
    State state;
    Adafruit_ST7789 tft(&SPI, Pins::TFT_CS_PIN, Pins::TFT_DC_PIN, Pins::TFT_RST_PIN);
    GFXcanvas16 canvas(304, ROW_HEIGHT * 2);

    Button back(Pins::BUTTON_BACK_PIN);
    Button up(Pins::BUTTON_UP_PIN);
    Button down(Pins::BUTTON_DOWN_PIN);
    Button ok(Pins::BUTTON_OK_PIN);

    void openPage(Page next)
    {
        state.currentPage = next;
        state.selection = state.firstVisible = 0;
        state.redrawRequested = true;
    }

    void showNotice(const char* text, Page returnPage)
    {
        snprintf(state.noticeText, sizeof(state.noticeText), "%s", text);
        state.noticeReturn = returnPage;
        openPage(Page::NOTICE);
    }

    bool saveSettings(const Settings::Data& candidate, Page returnPage)
    {
        if (!Settings::valid(candidate))
        {
            showNotice("Недопустимое значение настройки.", returnPage);
            return false;
        }
        if (!Config::commit(candidate))
        {
            showNotice("Не удалось сохранить настройки. Проверьте память устройства.", returnPage);
            return false;
        }
        Sensors::applySettings();
        Ui::applyDisplaySettings();
        return true;
    }

    const char* selectedScaleName()
    {
        return state.scaleId == ScaleId::FOOD ? "Весы корма" : "Весы воды";
    }

    void handleProcessButtons()
    {
        if (state.operationWasRunning)
        {
            state.operationWasRunning = false;
            state.renderedPage = Page::NOTICE;
            if (state.processReturn == Page::PUMP_VOLUME && FeedingController::info().lastSuccess)
            {
                state.measuredMl = std::max(
                    0.5f, std::min(1000.0f, std::round(state.pumpSeconds * Settings::MS_PER_SECOND /
                                                       Config::get().pumpMsPerMl * 2) / 2));
                openPage(Page::PUMP_VOLUME);
                return;
            }
        }
        if (ok.pressed() || back.pressed())
        {
            FeedingController::acknowledge();
            openPage(state.processReturn == Page::PUMP_VOLUME ? Page::PUMP : state.processReturn);
        }
    }

    void handleConfirmButtons()
    {
        if (back.pressed())
            openPage(state.confirmReturn);
        else if (ok.pressed() && saveSettings(Settings::defaults(), state.confirmReturn))
            showNotice("Заводские настройки восстановлены.", Page::HOME);
    }

    void handleEditButtons()
    {
        if (back.pressed())
        {
            openPage(state.editReturn);
            state.selection = state.editSelection;
            state.firstVisible = state.editFirstVisible;
            return;
        }
        if (up.repeated())
            state.editValue = std::min(state.editMax, state.editValue + state.editStep);
        if (down.repeated())
            state.editValue = std::max(state.editMin, state.editValue - state.editStep);
        if (ok.pressed())
            commitEdit();
    }

    void handleScalePreparationButtons()
    {
        if (back.pressed())
            openPage(Page::SCALE);
        else if (ok.pressed())
            beginTareCapture();
    }

    void handleCaptureButtons()
    {
        if (back.pressed())
        {
            Sensors::cancelCapture();
            openPage(Page::SCALE);
        }
    }

    void handlePumpTimeButtons()
    {
        if (back.pressed())
        {
            openPage(Page::PUMP);
            return;
        }
        if (up.repeated())
            state.pumpSeconds = std::min(Settings::PUMP_TEST_MAX_MS / Settings::MS_PER_SECOND,
                                         state.pumpSeconds + 1);
        if (down.repeated())
            state.pumpSeconds = std::max(Settings::PUMP_TEST_MIN_MS / Settings::MS_PER_SECOND,
                                         state.pumpSeconds - 1);
        if (ok.pressed())
        {
            FeedingController::startPumpTest(state.pumpSeconds * Settings::MS_PER_SECOND);
            startProcess(state.processReturn);
        }
    }

    void handlePumpVolumeButtons()
    {
        if (back.pressed())
        {
            openPage(Page::PUMP);
            return;
        }
        if (up.repeated())
            state.measuredMl = std::min(1000.0f, state.measuredMl + 0.5f);
        if (down.repeated())
            state.measuredMl = std::max(0.5f, state.measuredMl - 0.5f);
        if (ok.pressed())
        {
            auto settings = Config::get();
            settings.pumpMsPerMl = state.pumpSeconds * Settings::MS_PER_SECOND / state.measuredMl;
            if (saveSettings(settings, Page::PUMP))
                showNotice("Расход насоса сохранён.", Page::PUMP);
        }
    }

    void handleCameraButtons()
    {
        if (back.pressed())
        {
            openPage(Page::MAIN);
            return;
        }
        const bool previous = up.pressed();
        const bool next = down.pressed();
        const bool advance = ok.pressed();
        if (previous || next || advance)
        {
            state.cameraHelpPage ^= 1;
            state.redrawRequested = true;
        }
    }

    void handleMenuButtons()
    {
        if (back.pressed())
        {
            openPage(parentPage(state.currentPage));
            return;
        }
        buildRows();
        if (state.rowCount == 0)
            return;
        if (up.repeated())
            state.selection = (state.selection + state.rowCount - 1) % state.rowCount;
        if (down.repeated())
            state.selection = (state.selection + 1) % state.rowCount;
        if (ok.pressed())
            activateMenuRow(state.rows[state.selection]);
    }
} // namespace UiInternal

namespace Ui
{
    using namespace UiInternal;

    bool begin()
    {
        back.begin();
        up.begin();
        down.begin();
        ok.begin();
        SPI.begin(Pins::TFT_SCK_PIN, -1, Pins::TFT_MOSI_PIN, Pins::TFT_CS_PIN);
        tft.init(240, 320);
        tft.setRotation(1);
        state.displayReady = canvas.getBuffer() != nullptr;
        pinMode(Pins::TFT_BL_PIN, OUTPUT);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcAttach(Pins::TFT_BL_PIN, 5000, 8);
#else
        ledcSetup(0, 5000, 8);
        ledcAttachPin(Pins::TFT_BL_PIN, 0);
#endif
        applyDisplaySettings();
        return state.displayReady;
    }
    void applyDisplaySettings()
    {
        if (!state.displayReady)
            return;
        const auto& settings = Config::get();
        tft.invertDisplay(settings.invertDisplay);
        const uint32_t duty = static_cast<uint32_t>(settings.brightnessPercent) * 255 / 100;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(Pins::TFT_BL_PIN, duty);
#else
        ledcWrite(0, duty);
#endif
        state.renderedPage = Page::NOTICE;
        state.redrawRequested = true;
    }
    bool serviceMode()
    {
        return state.currentPage != Page::HOME && state.currentPage != Page::MAIN &&
               state.currentPage != Page::MANUAL && state.currentPage != Page::PROCESS &&
               state.currentPage != Page::DIAGNOSTICS && state.currentPage != Page::CAMERA;
    }

    void handleButtons()
    {
        const uint32_t now = millis();
        back.update(now);
        up.update(now);
        down.update(now);
        ok.update(now);
        if (FeedingController::busy())
        {
            if (state.currentPage != Page::PROCESS)
            {
                state.processReturn = Page::HOME;
                openPage(Page::PROCESS);
            }
            state.operationWasRunning = true;
            if (back.pressed())
            {
                FeedingController::stop();
                state.processReturn =
                    state.processReturn == Page::PUMP_VOLUME ? Page::PUMP : state.processReturn;
                state.renderedPage = Page::NOTICE;
            }
            return;
        }
        if (FeedingController::info().state == MachineState::ERROR &&
            state.currentPage != Page::PROCESS)
        {
            state.processReturn = Page::HOME;
            openPage(Page::PROCESS);
        }

        switch (state.currentPage)
        {
        case Page::HOME:
            if (ok.pressed())
                openPage(Page::MAIN);
            break;
        case Page::NOTICE:
            if (ok.pressed() || back.pressed())
                openPage(state.noticeReturn);
            break;
        case Page::PROCESS:
            handleProcessButtons();
            break;
        case Page::CONFIRM:
            handleConfirmButtons();
            break;
        case Page::EDIT:
            handleEditButtons();
            break;
        case Page::CAL_EMPTY:
            handleScalePreparationButtons();
            break;
        case Page::CAL_CAPTURE:
            handleCaptureButtons();
            break;
        case Page::PUMP_SECONDS:
            handlePumpTimeButtons();
            break;
        case Page::PUMP_VOLUME:
            handlePumpVolumeButtons();
            break;
        case Page::CAMERA:
            handleCameraButtons();
            break;
        default:
            handleMenuButtons();
            break;
        }
    }

    void update()
    {
        if (!state.displayReady)
            return;
        updateCapture();
        if (FeedingController::busy() && state.currentPage != Page::PROCESS)
        {
            state.processReturn = Page::HOME;
            state.operationWasRunning = true;
            openPage(Page::PROCESS);
        }
        if (FeedingController::info().state == MachineState::ERROR &&
            state.currentPage != Page::PROCESS)
        {
            state.processReturn = Page::HOME;
            openPage(Page::PROCESS);
        }
        const bool pageChanged = state.currentPage != state.renderedPage;
        if (!pageChanged && !state.redrawRequested &&
            millis() - state.lastDrawMs < Settings::DISPLAY_INTERVAL_MS)
            return;
        state.lastDrawMs = millis();
        state.redrawRequested = false;
        if (pageChanged)
        {
            drawChrome();
            state.renderedPage = state.currentPage;
        }
        drawPage();
    }
} // namespace Ui
