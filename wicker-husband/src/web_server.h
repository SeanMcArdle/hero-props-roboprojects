#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// -------------------------------------------------------------------------
// Web server setup and maintenance
// -------------------------------------------------------------------------
void setupWebServer();
void serviceWebServer();

// -------------------------------------------------------------------------
// Bridge variables — written by web callbacks, read in main loop
// -------------------------------------------------------------------------
// Gaze joystick: -100 to 100 (center = 0)
extern volatile int webGazeX;
extern volatile int webGazeY;

// Command triggers (0 = none)
// 1=Blink, 2=Alert, 3=Sleepy, 4=Idle, 5=Center
extern volatile int webCommandId;

// Watchdog — updated every packet so main loop can detect a dead connection
extern volatile unsigned long lastWebPacket;

#endif // WEB_SERVER_H
