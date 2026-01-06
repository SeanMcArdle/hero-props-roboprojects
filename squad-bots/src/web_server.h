#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void setupWebServer();
void cleanWebServer(); // Cleanup if needed (optional)

// External variables to communicate with main loop
// These are Volatile because they might be updated from async callbacks
extern volatile int webDomeX; // 0-200 (Center 100)
extern volatile int webDomeY; // 0-200 (Center 100)
extern volatile int webDriveX; // -100 to 100 (Optional Web Drive)
extern volatile int webDriveY; // -100 to 100

// Command Queue (Simple Lock-free or volatile flag)
extern volatile int webCommandId; // 0 = None, >0 = Action ID

#endif
