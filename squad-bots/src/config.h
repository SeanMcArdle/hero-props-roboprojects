#ifndef CONFIG_H
#define CONFIG_H

// -------------------------------------------------------------------------
// 🤖 Bot Identity
// -------------------------------------------------------------------------
#define BOT_ID          1       // Unique ID (1-15). 0 is Commander.
#define BOT_NAME        "jellybean" // mDNS name: jellybean.local

// -------------------------------------------------------------------------
// 🔌 Pin Definitions (NodeMCU-32S)
// -------------------------------------------------------------------------
// 🚨 THE 42 TRAP: If you see '42', change it to your actual pin!
#define PIN_MOTOR_LEFT  25
#define PIN_MOTOR_RIGHT 26
#define PIN_DOME        13 // Moved to clean pin (was 27)

// -------------------------------------------------------------------------
// 💡 LED Settings (Phase 2+)
// -------------------------------------------------------------------------
#define HAS_NEOPIXELS   true
#define NUM_LEDS        24
#define PIN_NEOPIXEL    27

// Audio Hardware Removed (Sound plays on iPad)

// -------------------------------------------------------------------------
// 🎮 RC Input (Spektrum/PWM)
// -------------------------------------------------------------------------
#define PIN_RC_THROTTLE 33
#define PIN_RC_STEERING 32
#define PIN_RC_SPIN     35      // Optional: Left Stick X for pure spin
#define RC_DEADZONE     15      // Ignore small stick movements
#define RC_MIN_PULSE    1000
#define RC_MAX_PULSE    2000
#define RC_CENTER       1500

// -------------------------------------------------------------------------
// ⚙️ Motor Settings
// -------------------------------------------------------------------------
#define MOTOR_STOP      90
#define MOTOR_MIN       0
#define MOTOR_MAX       180
#define TRIM_LEFT       0       // Adjust if bot drifts
#define TRIM_RIGHT      0

// ⚙️ Dome Settings
#define TRIM_DOME       -3      // Fix Clockwise Drift (Try -5 to 5)
#define DOME_SPEED_MAX  90      // MAX RANGE: 90 +/- 90 (0-180)
#define DOME_SMOOTH     0.015   // 50% Slower than 0.03. Cinema Smooth.

// ⚙️ Drive Settings (Smoothing & Physics)
#define DRIVE_SMOOTH    0.15    // Lower = Smoother, Higher = Snappier
#define DRIVE_SPEED_MAX 400     // Microseconds offset from 1500 (Max Speed Cap)

// -------------------------------------------------------------------------
// 📡 Network Settings
// -------------------------------------------------------------------------
#define WIFI_SSID       "JL-BN-Net"
#define WIFI_PASS       "heroprops"
#define USE_WIFI_AP     true    // Set false to save power/interference if only using ESP-NOW

#endif
