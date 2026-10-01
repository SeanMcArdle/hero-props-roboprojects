#ifndef CONFIG_H
#define CONFIG_H

// -------------------------------------------------------------------------
// 👻 Prop Identity
// -------------------------------------------------------------------------
#ifndef BOT_NAME
#define BOT_NAME "Haunted-Footsteps"
#endif

// -------------------------------------------------------------------------
// 🔌 Pin Definitions (see docs/WIRING.md)
// -------------------------------------------------------------------------
// One continuous addressable strip runs heel -> (dark gap) -> toe -> (dark
// tail), all on a single data line.
// Moved off GPIO 25 -> GPIO 27: 25/26 are the ESP32's DAC1/DAC2 + ADC2
// channels, which have documented quirks when WiFi is active (this project
// runs a SoftAP continuously) and were producing a persistent full-white
// strip. GPIO 27 has no such shared peripheral duties.
#define PIN_LED_STRIP 27

#define PIN_BUTTON_TOE 32  // INPUT_PULLUP, switch to GND (pressed = LOW)
#define PIN_BUTTON_HEEL 33 // INPUT_PULLUP, switch to GND (pressed = LOW)

// -------------------------------------------------------------------------
// 💡 LED Strip Settings
// -------------------------------------------------------------------------
// Physical pixel map (1-indexed, as measured on the strip):
//   1-17   heel
//   18-19  dark gap
//   20-45  toe
//   46-60  dark tail (strip will likely be trimmed shorter once confirmed)
// TBD: the strip will be cut once placement is finalized — revisit this
// whole block (and NUM_LEDS_TOTAL) when that happens.
#define NUM_LEDS_TOTAL 60

// Converted to 0-indexed start/count for the firmware's pixel arrays.
#define HEEL_LED_START 0  // Pixel 1
#define HEEL_LED_COUNT 17 // Pixels 1-17

#define TOE_LED_START 19 // Pixel 20
#define TOE_LED_COUNT 26 // Pixels 20-45

#define LED_TYPE WS2812B
#define COLOR_ORDER GRB // matches Adafruit_NeoPixel's NEO_GRB flag in main.cpp

// Hard safety ceiling regardless of the "intensity" setting from the browser UI.
// Capped at 50% of 255 - full brightness on all 60 pixels was likely browning
// out the board on power-up/press. Raise cautiously and only after confirming
// the power supply can handle the higher inrush current.
#define MAX_BRIGHTNESS_CEILING 128

// Conservative current budget for a 4x AA pack. FastLED will auto-dim below
// this if all LEDs try to draw more than the pack can safely deliver.
#define CURRENT_LIMIT_MA 1200

// -------------------------------------------------------------------------
// 👻 Default Ghostly Flicker Settings (persisted to NVS, overridable via web UI)
// -------------------------------------------------------------------------
#define DEFAULT_COLOR_R 40
#define DEFAULT_COLOR_G 255
#define DEFAULT_COLOR_B 120

#define DEFAULT_FLICKER_SPEED 60 // 0-255, higher = faster shimmer drift
#define DEFAULT_INTENSITY 160    // 0-255, max envelope brightness

#define DEFAULT_FADE_IN_MS 600   // Button press -> full glow
#define DEFAULT_FADE_OUT_MS 3000 // Button release -> fully out (always slower than fade-in)

// Firmware never allows fade-out to be configured faster than this multiple of fade-in,
// no matter what the browser sends. Keeps the "never winks out quickly" requirement honest.
#define MIN_FADE_OUT_RATIO 1.5f

// Settings changes (color/flicker/fade sliders) are applied to the live
// effect immediately, but the NVS (flash) write is debounced by this many ms
// of inactivity. Dragging a slider fires many WebSocket updates per second;
// without debouncing, each one would trigger its own flash write, needlessly
// wearing the flash and blocking the async task.
#define SETTINGS_SAVE_DEBOUNCE_MS 1500

// -------------------------------------------------------------------------
// 📡 Network Settings (AP mode — same pattern as the other Hero Props rigs)
// -------------------------------------------------------------------------
#ifndef WIFI_SSID
#define WIFI_SSID "Haunted-Footsteps"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "heroprops"
#endif

// -------------------------------------------------------------------------
// 📲 OTA (Over-The-Air) Firmware Update
// -------------------------------------------------------------------------
// Browser-based update at http://<device-ip-or-mdns>/update — works from an
// iPad's Safari file picker, unlike the classic ArduinoOTA/espota protocol
// (which needs a PC running PlatformIO/Python). Gated by HTTP Basic Auth
// using its OWN credentials, deliberately separate from WIFI_PASS, so the
// WiFi password (which may get shared with actors/students) can't alone be
// used to flash new firmware.
//
// CHANGE THESE before deploying to a real show — override via platformio.ini
// build_flags (-D OTA_USERNAME=... -D OTA_PASSWORD=...) rather than editing
// this file directly, same pattern as WIFI_SSID/WIFI_PASS above.
#ifndef OTA_USERNAME
#define OTA_USERNAME "admin"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "change-me-ota"
#endif

#endif
