#ifndef CONFIG_H
#define CONFIG_H

// -------------------------------------------------------------------------
// 🤖 Bot Identity
// -------------------------------------------------------------------------
#ifndef BOT_ID
#define BOT_ID 1 // Default ID
#endif

#ifndef BOT_NAME
#define BOT_NAME "L0-0N"
#endif

#ifndef WIFI_SSID_NAME
#define WIFI_SSID_NAME "L0-0N-Net"
#endif

// -------------------------------------------------------------------------
// 🔌 Pin Definitions (NodeMCU-32S)
// -------------------------------------------------------------------------
// 🚨 THE 42 TRAP: If you see '42', change it to your actual pin!
#define PIN_MOTOR_LEFT 25
#define PIN_MOTOR_RIGHT 26
#define PIN_DOME 13 // Moved to clean pin (was 27)

// -------------------------------------------------------------------------
// 💡 LED Settings (Phase 2+)
// -------------------------------------------------------------------------
#define HAS_NEOPIXELS true
#define NUM_LEDS 8
#define PIN_NEOPIXEL 27

// Audio Hardware Removed (Sound plays on iPad)

// -------------------------------------------------------------------------
// 🎮 RC Input (Spektrum/PWM)
// -------------------------------------------------------------------------
// Camp baseline is Web-only. Enable RC only for projects that wire a receiver.
#define USE_RC_INPUT false

#define PIN_RC_THROTTLE 33
#define PIN_RC_STEERING 32
#define PIN_RC_SPIN 35 // Optional: Left Stick X for pure spin
#define RC_DEADZONE 15 // Ignore small stick movements
#define RC_MIN_PULSE 1000
#define RC_MAX_PULSE 2000
#define RC_CENTER 1500

// -------------------------------------------------------------------------
// ⚙️ Motor Settings
// -------------------------------------------------------------------------
#define MOTOR_STOP 90
#define MOTOR_MIN 0
#define MOTOR_MAX 180
#define TRIM_LEFT 0 // Adjust if bot drifts
#define TRIM_RIGHT 0

// ⚙️ Dome Settings (360° Continuous Rotation Servo)
// Note: 1500µs = STOP, <1500 = CCW, >1500 = CW
#define DOME_SPEED_MAX 200 // Microsecond offset from 1500 (safe speed range: ±200µs)
#define DOME_SMOOTH 0.10   // Ramp speed changes (same as drive smoothing)

// ⚙️ Drive Settings (Smoothing & Physics)
#define DRIVE_SMOOTH 0.15   // Lower = Smoother, Higher = Snappier
#define DRIVE_SPEED_MAX 400 // Microseconds offset from 1500 (Max Speed Cap)

// -------------------------------------------------------------------------
// 📡 Network Settings
// -------------------------------------------------------------------------
#define WIFI_SSID "L0-0N-Net"
#define WIFI_PASS "heroprops"
#define USE_WIFI_AP 1 // Set 0 to disable AP/web UI when running ESP-NOW only

#endif
