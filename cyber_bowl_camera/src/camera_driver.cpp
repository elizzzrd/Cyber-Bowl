#include "camera_driver.hpp"
#include "camera_pins.hpp"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstdio>

namespace 
{
    SemaphoreHandle_t mutex = nullptr;

    bool available = false;
    bool psram = false;
    char failure[96] = {};

    CameraConfig::ImageSettings current;
    portMUX_TYPE lightMux = portMUX_INITIALIZER_UNLOCKED;
    bool light = false;
    uint32_t lightStartedMs = 0;

    framesize_t sizeOf(uint8_t size)
    {
        const framesize_t sizes[] = {FRAMESIZE_QVGA, FRAMESIZE_VGA, FRAMESIZE_SVGA};
        return sizes[size];
    }

    bool applyUnlocked(const CameraConfig::ImageSettings& c)
    {
        auto* sensor = esp_camera_sensor_get();
        if (!sensor) return false;
        bool okay = true;
    
        // Do not short-circuit: restore all properties even if one operation fails.
        okay &= sensor->set_framesize(sensor, sizeOf(c.size)) == 0;
        okay &= sensor->set_quality(sensor, c.quality) == 0;
        okay &= sensor->set_brightness(sensor, c.brightness) == 0;
        okay &= sensor->set_contrast(sensor, c.contrast) == 0;
        okay &= sensor->set_saturation(sensor, c.saturation) == 0;
        okay &= sensor->set_hmirror(sensor, c.mirror) == 0;
        okay &= sensor->set_vflip(sensor, c.flip) == 0;
    
        return okay;
    }
}


bool Camera::begin()
{
    digitalWrite(CameraPins::LIGHT, LOW);
    pinMode(CameraPins::LIGHT, OUTPUT);

    mutex = xSemaphoreCreateMutex();
    if (!mutex) { snprintf(failure, sizeof(failure), "Camera mutex allocation failed"); return false; }
    
    psram = psramFound();
    camera_config_t c{};

    c.ledc_channel = LEDC_CHANNEL_0; c.ledc_timer = LEDC_TIMER_0;
    c.pin_d0 = CameraPins::Y2; c.pin_d1 = CameraPins::Y3;
    c.pin_d2 = CameraPins::Y4; c.pin_d3 = CameraPins::Y5;
    c.pin_d4 = CameraPins::Y6; c.pin_d5 = CameraPins::Y7;
    c.pin_d6 = CameraPins::Y8; c.pin_d7 = CameraPins::Y9;
    c.pin_xclk = CameraPins::XCLK;
    c.pin_pclk = CameraPins::PCLK;
    c.pin_vsync = CameraPins::VSYNC;
    c.pin_href = CameraPins::HREF;
    c.pin_sccb_sda = CameraPins::SIOD;
    c.pin_sccb_scl = CameraPins::SIOC;
    c.pin_pwdn = CameraPins::PWDN;
    c.pin_reset = CameraPins::RESET;
    c.xclk_freq_hz = 20000000;
    c.pixel_format = PIXFORMAT_JPEG;
    // Allocate for the largest allowed resolution before selecting the saved smaller size.
    c.frame_size = psram ? FRAMESIZE_SVGA : FRAMESIZE_QVGA;
    c.jpeg_quality = 10;
    c.fb_count = psram ? 2 : 1;
    c.fb_location = psram ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    c.grab_mode = CAMERA_GRAB_LATEST;


    const esp_err_t result = esp_camera_init(&c);
    if (result != ESP_OK) 
    {
        snprintf(failure, sizeof(failure), "esp_camera_init: 0x%04x", static_cast<unsigned>(result));
        return false;
    }

    available = true;
    auto candidate = CameraConfig::get().image;

    if (!psram) candidate.size = 0;
    if (!apply(candidate)) 
    {
        available = false;
        snprintf(failure, sizeof(failure), "Could not apply sensor settings");
        return false;
    }
    return true;
}


bool Camera::ready()            { return available; }
bool Camera::hasPsram()         { return psram; }
const char* Camera::error()     { return failure; }


bool Camera::apply(const CameraConfig::ImageSettings& candidate)
{
    if (!available || 
        !CameraConfig::valid(candidate) || 
        (!psram && candidate.size != 0) ||
        !xSemaphoreTake(mutex, pdMS_TO_TICKS(1000))) 
        { return false; }

    const bool success = applyUnlocked(candidate);
    
    if (success) 
        current = candidate;
    else 
        applyUnlocked(current);
    
    
    xSemaphoreGive(mutex);
    return success;
}


CameraConfig::ImageSettings Camera::activeSettings()
{
    CameraConfig::ImageSettings copy;
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY)) 
    { 
        copy = current; 
        xSemaphoreGive(mutex); 
    }

    return copy;
}


camera_fb_t* Camera::acquire(uint32_t timeoutMs)
{
    if (!available || 
        !mutex     || 
        !xSemaphoreTake(mutex, pdMS_TO_TICKS(timeoutMs))) 
        { return nullptr; }

    auto* frame = esp_camera_fb_get();
    if (!frame) 
        xSemaphoreGive(mutex);
    
    return frame;
}


void Camera::release(camera_fb_t* frame)
{
    if (!frame) 
        return;
    e
    sp_camera_fb_return(frame);
    xSemaphoreGive(mutex);
}


void Camera::setLight(bool enabled)
{
    portENTER_CRITICAL(&lightMux);
    light = enabled;
    lightStartedMs = millis();
    digitalWrite(CameraPins::LIGHT, light ? HIGH : LOW);
    portEXIT_CRITICAL(&lightMux);
}


bool Camera::lightOn()
{
    portENTER_CRITICAL(&lightMux); 
    const bool result = light; 
    portEXIT_CRITICAL(&lightMux); 
    return result;
}


void Camera::update()
{
    portENTER_CRITICAL(&lightMux);
    
    if (light && millis() - lightStartedMs >= 30000) 
    {
        light = false;
        digitalWrite(CameraPins::LIGHT, LOW);
    
    }
    portEXIT_CRITICAL(&lightMux);
}