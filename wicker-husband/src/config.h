#ifndef CONFIG_H
#define CONFIG_H

// -------------------------------------------------------------------------
// 🪆 Wicker Husband — Animatronic Eyes
// Adapted from mklements/AIChatbot EyeMovement.py
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// 📡 Network Settings
// -------------------------------------------------------------------------
#define WIFI_SSID "WICKER-NET"
#define WIFI_PASS "heroprops"
#define USE_WIFI_AP 1

// HeroPropsProtocol device ID (must be unique in the group, range 1–15)
// ID 5 reserves this node as the "eyes" peripheral; 1=Captain, 2=Lookout, etc.
#define DEVICE_ID 5

// -------------------------------------------------------------------------
// 🔌 Pin Definitions (NodeMCU-32S / ESP32 Dev Board)
// 6 servos driven directly from ESP32 hardware PWM (no PCA9685 needed)
// -------------------------------------------------------------------------
// Left Eye
#define PIN_LEFT_X      25  // Left eye horizontal pan
#define PIN_LEFT_Y      26  // Left eye vertical tilt
#define PIN_LEFT_BLINK  27  // Left eyelid

// Right Eye
#define PIN_RIGHT_X     14  // Right eye horizontal pan
#define PIN_RIGHT_Y     12  // Right eye vertical tilt
#define PIN_RIGHT_BLINK 13  // Right eyelid

// -------------------------------------------------------------------------
// 👁️ Servo Travel Limits (degrees)
// Adapted from EyeMovement.py: X_LIMITS=(70,110), Y_LIMITS=(70,110)
// -------------------------------------------------------------------------
#define EYE_X_MIN       70
#define EYE_X_MAX      110
#define EYE_Y_MIN       70
#define EYE_Y_MAX      110

// Blink: 0=fully open, 40=fully closed (BLINK_LIMITS in source)
#define BLINK_OPEN       0
#define BLINK_CLOSED    40

// Per-eye open trim offsets (BLINK_OPEN_LEFT / BLINK_OPEN_RIGHT in source)
#define BLINK_TRIM_LEFT   -12  // Left eyelid rests slightly more open
#define BLINK_TRIM_RIGHT    0

// -------------------------------------------------------------------------
// ⚙️ Movement Settings
// Adapted from EyeMovement.py constants
// -------------------------------------------------------------------------
// Gaze movement — how fast the eyes glide to a new position
#define MOVE_STEP_DEG    1    // Degrees per step
#define MOVE_DELAY_MS   10   // Milliseconds between steps (~100 steps/sec)

// Blinking
#define BLINK_STEP_MS    3   // Milliseconds per eyelid step
#define BLINK_HOLD_MS  100   // Milliseconds held closed
// Stagger between left and right eyelid during blink
// (BLINK_SIDE_DELAY=0.03s / BLINK_STEP_MS=3ms → 10 steps lag)
#define BLINK_SIDE_STEPS 10

// Auto-blink timing (random interval, milliseconds)
#define BLINK_INTERVAL_MIN_MS  4000
#define BLINK_INTERVAL_MAX_MS 12000

// Idle gaze: time to pause at each random position (milliseconds)
#define IDLE_PAUSE_MIN_MS  500
#define IDLE_PAUSE_MAX_MS 2000

// -------------------------------------------------------------------------
// 🎮 Servo Direction Multipliers
// Mirrors the DIR_* constants from EyeMovement.py.
// +1 = normal, -1 = invert (physical servo mounted reversed)
// -------------------------------------------------------------------------
#define DIR_LEFT_X        1
#define DIR_LEFT_Y        1
#define DIR_LEFT_BLINK    1
#define DIR_RIGHT_X       1
#define DIR_RIGHT_Y      -1   // Right eye tilt servo is physically inverted
#define DIR_RIGHT_BLINK  -1   // Right eyelid servo is physically inverted

#endif // CONFIG_H
