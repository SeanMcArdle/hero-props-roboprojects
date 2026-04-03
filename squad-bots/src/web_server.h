#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void setupWebServer();
void serviceWebServer(); // Periodic maintenance (client cleanup)

// External variables to communicate with main loop
// These are Volatile because they might be updated from async callbacks
extern volatile int webDomeX;  // 0-200 (Center 100)
extern volatile int webDomeY;  // 0-200 (Center 100)
extern volatile int webDriveX; // -100 to 100 (Optional Web Drive)
extern volatile int webDriveY; // -100 to 100

extern volatile int webCommandId;
extern volatile int webRed;
extern volatile int webGreen;
extern volatile int webBlue;
extern volatile unsigned long lastWebPacket;
extern volatile int webPixelIdx; // -1 = none, 0+ = pixel to set
extern volatile int webPixelR;
extern volatile int webPixelG;
extern volatile int webPixelB;

#endif
