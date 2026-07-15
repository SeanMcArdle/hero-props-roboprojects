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

// -------------------------------------------------------------------------
// ✏️  Persistent Droid Name (Rename Bridge)
// -------------------------------------------------------------------------
// activeBotName is owned/written by main.cpp (loads from NVS on boot, saves
// on rename). web_server.cpp only reads it to answer CFG_REQ / broadcasts.
#define MAX_BOT_NAME_LEN 20
extern char activeBotName[MAX_BOT_NAME_LEN + 1];

// Cross-task hand-off for rename requests. The WebSocket callback (a
// different task/core) writes pendingBotName then sets renameRequested.
// main.cpp::loop() is the ONLY place that applies it and writes to NVS,
// avoiding concurrent Preferences/flash access from the async WS context.
extern char pendingBotName[MAX_BOT_NAME_LEN + 1];
extern volatile bool renameRequested;

// Sends the current activeBotName to all connected clients as "CFG:<name>".
void broadcastConfig();

#endif
