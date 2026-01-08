#ifndef LOOKOUT_H
#define LOOKOUT_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include "hp_espnow.h"

class Lookout {
public:
    void setup();
    void loop();

private:
    void setupServos();
    void setupLights();
    void onPacketReceived(const HpHeader& header, const uint8_t* payload, size_t len);
    
    // Actions
    void triggerAction(uint8_t actionId);
    void updateAnimations();

    Adafruit_NeoPixel lights;
    Servo armLeft;
    Servo armRight;
    Servo headTilt;
    
    HeroPropsProtocol comms;

    // State
    unsigned long lastActionTime = 0;
    int currentAnimation = 0;
};

#endif
