#include "web_server.hpp"
#include "web_ui.hpp"
#include "camera_driver.hpp"
#include "network.hpp"
#include "config.hpp"
#include "form_codec.hpp"
#include <Arduino.h>
#include <esp_http_server.h>
#include <cstring>
#include <cstdio>


namespace 
{
    httpd_handle_t controlServer = nullptr;
    httpd_handle_t streamServer = nullptr;

    constexpr char BOUNDARY[] = "cyber_bowl_camera_frame";
    String jsonString(const char* source)
    {
        String out = "\"";
        while (*source) 
        {
            const unsigned char ch = static_cast<unsigned char>(*source++);
        
            if (ch == '"' || ch == '\\') 
            { 
                out += '\\'; 
                out += static_cast<char>(ch); 
            }
            else if (ch < 32) 
            { 
                char escaped[7]; 
                snprintf(escaped, sizeof(escaped), "\\u%04x", ch); 
                out += escaped; 
            }
            else 
                out += static_cast<char>(ch);
        }
        
        return out + "\"";
    }


    esp_err_t json(httpd_req_t* req, const String& body, const char* status = "200 OK")
    {
        httpd_resp_set_status(req, status);
        httpd_resp_set_type(req, "application/json; charset=utf-8");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        
        return httpd_resp_send(req, body.c_str(), body.length());
    }


    esp_err_t failure(httpd_req_t* req, const char* message, const char* status = "400 Bad Request")
    {
        return json(req, String("{\"error\":") + jsonString(message) + "}", status);
    }

    
    bool readForm(httpd_req_t* req, char* body, size_t capacity)
    {
        char header[8];
    
        if (httpd_req_get_hdr_value_str(req, "X-CyberBowl", header, sizeof(header)) != ESP_OK || 
            strcmp(header, "1")) 
            { return false; }
        
        if (req->content_len == 0 || req->content_len >= capacity) 
            { return false; }

        size_t offset = 0;
        while (offset < req->content_len) 
        {
            const int count = httpd_req_recv(req, body + offset, req->content_len - offset);
            if (count <= 0) return false;
            offset += count;
        }
        body[offset] = 0;
        
        return true;
    }


    bool field(const char* form, const char* key, char* value, size_t capacity)
    {
        char encoded[256];
        return httpd_query_key_value(form, key, encoded, sizeof(encoded)) == ESP_OK &&
                                     FormCodec::decode(encoded, value, capacity);
    }


    bool number(const char* form, const char* key, int& value, int low, int high)
    {
        char text[16];
        return field(form, key, text, sizeof(text)) && 
                     FormCodec::integer(text, value) && value >= low && 
                     value <= high;
    }


    esp_err_t pageHandler(httpd_req_t* req)
    {
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        
        return httpd_resp_send(req, CAMERA_PAGE, HTTPD_RESP_USE_STRLEN);
    }


    esp_err_t statusHandler(httpd_req_t* req)
    {
        const auto config = CameraConfig::get();
        const auto active = Camera::activeSettings();
    
        char image[256];
        snprintf(image, sizeof(image), 
                "{\"size\":%u,\"quality\":%u,\"fps\":%u,\"brightness\":%d,\"contrast\":%d,\"saturation\":%d,\"mirror\":%s,\"flip\":%s}",
                active.size, 
                active.quality, 
                active.fps, 
                active.brightness, 
                active.contrast, 
                active.saturation,
                (active.mirror) ? "true" : "false", 
                (active.flip) ? "true" : "false");

        String out; 
        out.reserve(700);
        out = String("{\"camera\":") + ((Camera::ready()) ? "true" : "false") + 
              ",\"cameraError\":" + jsonString(Camera::error()) +
              ",\"psram\":" + ((Camera::hasPsram()) ? "true" : "false") + 
              ",\"connected\":" + (Network::connected() ? "true" : "false") +
              ",\"ssid\":" + jsonString(config.ssid) + 
              ",\"apName\":" + jsonString(Network::apName().c_str()) +
              ",\"stationIp\":" + jsonString(Network::stationAddress().c_str()) + 
              ",\"apIp\":" + jsonString(Network::apAddress().c_str()) +
              ",\"rssi\":" + String(Network::signalDbm()) + 
              ",\"uptime\":" + String(millis() / 1000) +
              ",\"light\":" + ((Camera::lightOn()) ? "true" : "false") + 
              ",\"storage\":" + (CameraConfig::ready() ? "true" : "false") +
              ",\"image\":" + image + "}";
        
        return json(req, out);
    }


    esp_err_t imageHandler(httpd_req_t* req)
    {
        char form[1025];
        if (!readForm(req, form, sizeof(form))) 
            return failure(req, "Не удалось прочитать запрос.");
    
        int size, quality, fps, brightness, contrast, saturation, mirror, flip;
        
        if (!number(form, "size", size, 0, 2)               || 
            !number(form, "quality", quality, 10, 40)       ||
            !number(form, "fps", fps, 1, 20)                || 
            !number(form, "brightness", brightness, -2, 2)  ||
            !number(form, "contrast", contrast, -2, 2)      || 
            !number(form, "saturation", saturation, -2, 2)  ||
            !number(form, "mirror", mirror, 0, 1)           || 
            !number(form, "flip", flip, 0, 1))
            { return failure(req, "Параметры изображения вне допустимого диапазона."); }

        auto candidate = CameraConfig::get();
        const auto old = Camera::activeSettings();
        
        candidate.image.size = size; 
        candidate.image.quality = quality; 
        candidate.image.fps = fps;
        candidate.image.brightness = brightness; 
        candidate.image.contrast = contrast; 
        candidate.image.saturation = saturation;
        candidate.image.mirror = mirror; 
        candidate.image.flip = flip;
        
        if (!Camera::apply(candidate.image))
            return failure(req, "Камера занята или недоступна. Без PSRAM используйте 320 x 240.", "409 Conflict");
        
        if (!CameraConfig::save(candidate)) 
        {
            const bool restored = Camera::apply(old);
            return failure(req, 
                          (restored) ? "Не удалось сохранить параметры. Прежние значения восстановлены." :
                          "Не удалось сохранить параметры. Остановите видео и повторите настройку.", 
                          "500 Internal Server Error");
        }

        return json(req, "{\"ok\":true}");
    }


    esp_err_t wifiHandler(httpd_req_t* req)
    {
        char form[1025], ssid[33], password[65], apPassword[64];
        
        int open;
        if (!readForm(req, form, sizeof(form))                         || 
            !field(form, "ssid", ssid, sizeof(ssid))                   ||
            !field(form, "password", password, sizeof(password))       || 
            !field(form, "apPassword", apPassword, sizeof(apPassword)) ||
            !number(form, "open", open, 0, 1)) 
            { return failure(req, "Проверьте название сети и пароли."); }
        
        auto candidate = CameraConfig::get();
        if (ssid[0] && !password[0] && 
            !open && strcmp(ssid, candidate.ssid))
            { return failure(req, "Для новой сети введите пароль или отметьте сеть без пароля."); }
        
        snprintf(candidate.ssid, sizeof(candidate.ssid), "%s", ssid);
        if (open || !ssid[0]) { 
            candidate.password[0] = 0; }
        else if (password[0]) {
            snprintf(candidate.password, sizeof(candidate.password), "%s", password); }

        if (apPassword[0]) 
            snprintf(candidate.apPassword, sizeof(candidate.apPassword), "%s", apPassword);
        
        if (!CameraConfig::valid(candidate)) 
            return failure(req, "Пароль Wi-Fi должен содержать от 8 до 63 символов (или 64 hex-символа).");
        
        if (!CameraConfig::save(candidate)) 
            return failure(req, "Не удалось сохранить сеть.", "500 Internal Server Error");
        
        Network::reconnectSoon();
    
        return json(req, "{\"ok\":true}");
    }


    esp_err_t lightHandler(httpd_req_t* req)
    {
        char form[128]; int enabled;
    
        if (!readForm(req, form, sizeof(form)) || 
            !number(form, "enabled", enabled, 0, 1)) 
            { return failure(req, "Некорректная команда света."); }
        
        Camera::setLight(enabled != 0);
    
        return json(req, "{\"ok\":true}");
    }

    
    esp_err_t snapshotHandler(httpd_req_t* req)
    {
        auto* frame = Camera::acquire(1000);
        if (!frame) 
            return failure(req, "Не удалось получить снимок.", "503 Service Unavailable");
    
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=cyber-bowl.jpg");
        
        const auto result = httpd_resp_send(req, reinterpret_cast<const char*>(frame->buf), frame->len);
        Camera::release(frame);
        return result;
    }

    
    esp_err_t streamHandler(httpd_req_t* req)
    {
        if (!Camera::ready()) 
            return failure(req, "Камера не инициализирована.", "503 Service Unavailable");
        
        httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=cyber_bowl_camera_frame");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    
        while (true) 
        {
            const uint32_t started = millis();
            const auto settings = Camera::activeSettings();
        
            auto* frame = Camera::acquire(1000);
            if (!frame) 
                return ESP_FAIL;
            
            char header[160];
            const int length = snprintf(header, sizeof(header), "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", BOUNDARY, static_cast<unsigned>(frame->len));
        
            esp_err_t result = httpd_resp_send_chunk(req, header, length);
            if (result == ESP_OK) 
                result = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(frame->buf), frame->len);
            
            Camera::release(frame);
            if (result != ESP_OK) 
                return result;
            
            const uint32_t elapsed = millis() - started;
            const uint32_t interval = 1000 / settings.fps;
        
            delay(elapsed < interval ? interval - elapsed : 1);
        }
    }


    bool route(httpd_handle_t server, const char* path, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*))
    {
        httpd_uri_t uri{}; 
        uri.uri = path; 
        uri.method = method; 
        uri.handler = handler;
        
        return httpd_register_uri_handler(server, &uri) == ESP_OK;
    }
}


bool CameraWeb::begin()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    config.server_port = 80; 
    config.ctrl_port = 32768;
    config.stack_size = 8192; 
    config.max_uri_handlers = 10;
    config.max_open_sockets = 4; 
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 3; 
    config.send_wait_timeout = 3;
    
    if (httpd_start(&controlServer, &config) != ESP_OK) 
        return false;
    
        const bool registered = route(controlServer, "/", HTTP_GET, pageHandler)                &&
                                route(controlServer, "/api/status", HTTP_GET, statusHandler)    &&
                                route(controlServer, "/api/image", HTTP_POST, imageHandler)     &&
                                route(controlServer, "/api/wifi", HTTP_POST, wifiHandler)       &&
                                route(controlServer, "/api/light", HTTP_POST, lightHandler)     &&
                                route(controlServer, "/snapshot", HTTP_GET, snapshotHandler);
    
    config.server_port = 81; 
    config.ctrl_port = 32769; 
    config.stack_size = 4096;
    config.max_open_sockets = 2;
    
    if (!registered || httpd_start(&streamServer, &config) != ESP_OK) {
        httpd_stop(controlServer); controlServer = nullptr; return false;
    }

    if (!route(streamServer, "/stream", HTTP_GET, streamHandler)) {
        httpd_stop(streamServer); 
        httpd_stop(controlServer);
        streamServer = controlServer = nullptr; 
        
        return false;
    }

    return true;
}