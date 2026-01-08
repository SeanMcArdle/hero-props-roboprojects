#ifndef EYE_H
#define EYE_H

#include <Arduino.h>

#ifdef ROLE_EYE
#include "esp_camera.h"
#include <WiFi.h>
#endif

class Eye {
public:
    void setup();
    void loop();

private:
#ifdef ROLE_EYE
    void initCamera();
    void startCameraServer();
    void setupWiFi();
#endif
};

#endif
