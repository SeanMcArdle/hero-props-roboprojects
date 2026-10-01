#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

// -------------------------------------------------------------------------
// Shared "Ghostly Flicker" settings.
// -------------------------------------------------------------------------
// Read by the effects engine in main.cpp every frame, written by the browser
// UI (via WebSocket, in web_server.cpp). Note: on ESP32 Arduino, AsyncTCP
// callbacks run on their own FreeRTOS task (not necessarily the same core as
// loop()), so a settings update *could* in principle be read half-applied by
// a single render frame. Every field here is a single byte/word, so torn
// reads are effectively a non-issue in practice — worst case is one frame
// using a stale value for one field before it self-corrects on the next
// frame. Plain volatiles (no mutex) are an intentional, low-risk trade-off
// for that bound, not an oversight.
struct FootstepSettings
{
    volatile uint8_t colorR = DEFAULT_COLOR_R;
    volatile uint8_t colorG = DEFAULT_COLOR_G;
    volatile uint8_t colorB = DEFAULT_COLOR_B;

    volatile uint8_t flickerSpeed = DEFAULT_FLICKER_SPEED; // 0-255
    volatile uint8_t intensity = DEFAULT_INTENSITY;        // 0-255

    volatile uint32_t fadeInMs = DEFAULT_FADE_IN_MS;
    volatile uint32_t fadeOutMs = DEFAULT_FADE_OUT_MS;
};

extern FootstepSettings settings;

// Live telemetry for the browser UI (button state + current envelope 0-255).
// Written by main.cpp's effects engine, read by web_server.cpp to broadcast.
extern volatile bool telemetryToePressed;
extern volatile bool telemetryHeelPressed;
extern volatile uint8_t telemetryToeEnvelope;
extern volatile uint8_t telemetryHeelEnvelope;

// True while a /update firmware upload is being written to flash. main.cpp's
// loop() must NOT call FastLED.show() (or anything else that disables
// interrupts for precise timing) while this is true — doing so concurrently
// with a flash write is a known source of corrupted OTA uploads/crashes.
extern volatile bool otaInProgress;

void setupWebServer();
void serviceWebServer();   // Periodic maintenance (client cleanup, deferred OTA reboot)
void broadcastTelemetry(); // Push current button/envelope state to clients

// Clamps fadeOutMs so it can never be configured faster than
// MIN_FADE_OUT_RATIO * fadeInMs. Used both on settings load and on any
// browser-driven update.
void enforceFadeSafety();

#endif
