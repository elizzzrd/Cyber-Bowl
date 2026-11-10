#include <Arduino.h>
#include "config.hpp"
#include "camera_driver.hpp"
#include "network.hpp"
#include "web_server.hpp"


void setup()
{
    Serial.begin(115200);
    CameraConfig::begin();
    const bool ready = Camera::begin();
 
    Serial.printf("Cyber Bowl camera: %s; PSRAM: %s\n", 
                  (ready) ? "OK" : Camera::error(), Camera::hasPsram() ? "YES" : "NO");              
    Network::begin(); // Network setup remains available even when the camera has an error.
    
    if (!CameraWeb::begin()) 
        Serial.println("Camera HTTP servers failed to start");
}


void loop()
{
    Network::update();
    Camera::update();
    delay(10);
}