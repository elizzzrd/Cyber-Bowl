#include "network.hpp"
#include "config.hpp"
#include <WiFi.h>
#include <ESPmDNS.h>

namespace 
{
    char apSsid[40] = {};
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    bool pending = false;

    uint32_t requestedMs = 0;
    uint32_t lastAttemptMs = 0;

    bool mdnsStarted = false;
    bool wasConnected = false;

    void connect()
    {
        const auto data = CameraConfig::get();
        WiFi.disconnect(false, false);
        
        if (data.ssid[0]) 
            WiFi.begin(data.ssid, data.password);
        lastAttemptMs = millis();
    }
}


void Network::begin()
{
    WiFi.persistent(false); // Credentials are owned by our versioned NVS settings.
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.setHostname("cyberbowl-camera");

    const String mac = WiFi.macAddress();
    String suffix = mac.substring(mac.length() - 5);

    suffix.replace(":", "");
    snprintf(apSsid, sizeof(apSsid), "CyberBowl-Camera-%s", suffix.c_str());
    const auto data = CameraConfig::get();

    WiFi.softAP(apSsid, data.apPassword, 1, false, 2);
    connect();

    Serial.printf("Camera setup Wi-Fi: %s\nCamera setup URL: http://%s/\n", apSsid, apAddress().c_str());
}


void Network::reconnectSoon()
{
    portENTER_CRITICAL(&mux); 
    pending = true; 
    requestedMs = millis(); 
    portEXIT_CRITICAL(&mux);
}


void Network::update()
{
    bool reconnect = false;

    portENTER_CRITICAL(&mux);
    if (pending && millis() - requestedMs >= 1500) 
    { 
        pending = false; 
        reconnect = true; 
    }
    portEXIT_CRITICAL(&mux);

    if (reconnect) 
    {
        if (mdnsStarted) 
        { 
            MDNS.end(); 
            mdnsStarted = false; 
        }

        const auto data = CameraConfig::get();
        WiFi.softAP(apSsid, data.apPassword, 1, false, 2);
        connect();
    }

    if (connected()) 
    {
        if (!mdnsStarted) 
        {
            mdnsStarted = MDNS.begin("cyberbowl-camera");
            if (mdnsStarted) MDNS.addService("http", "tcp", 80);
        }

        if (!wasConnected) {
            Serial.printf("Camera LAN URL: http://%s/ or http://cyberbowl-camera.local/\n", 
                          stationAddress().c_str());
            }
        wasConnected = true;
    } 
    else 
    {
        wasConnected = false;
        if (CameraConfig::get().ssid[0] && millis() - lastAttemptMs >= 30000) connect();
    }
}


String Network::apName()           { return apSsid; }
String Network::apAddress()        { return WiFi.softAPIP().toString(); }
String Network::stationAddress()   { return connected() ? WiFi.localIP().toString() : String(""); }
bool Network::connected()          { return WiFi.status() == WL_CONNECTED; }
int Network::signalDbm()           { return connected() ? WiFi.RSSI() : 0; }