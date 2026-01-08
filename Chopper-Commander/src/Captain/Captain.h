#ifndef CAPTAIN_H
#define CAPTAIN_H

#include <Arduino.h>
#include <ESP32Servo.h>
#include "hp_espnow.h"
#include "WebInterface.h"

class Captain {
public:
    void setup();
    void loop();

private:
    void setupWiFi();
    void setupMotors();
    void setupComms();
    
    void handleDrive(int x, int y);
    void handleDome(int x, int y);
    
    // Motor Objects
    Servo leftMotor;
    Servo rightMotor;
    Servo domeSpinMotor;

    HeroPropsProtocol comms;
    WebInterface web;

    // Helper to send to Bard
    void sendToBard(String cmd);
};

#endif
