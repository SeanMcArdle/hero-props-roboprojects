#ifndef CONFIG_H
#define CONFIG_H

// -------------------------------------------------------------------------
// 🤖 Bot Identity
// -------------------------------------------------------------------------
#define BOT_ID          1       // Unique ID (1-15). 0 is Commander.
#define BOT_NAME        "JL-BN" // "Jellybean"

// -------------------------------------------------------------------------
// 🔌 Pin Definitions (NodeMCU-32S)
// -------------------------------------------------------------------------
// 🚨 THE 42 TRAP: If you see '42', change it to your actual pin!
#define PIN_MOTOR_LEFT  25
#define PIN_MOTOR_RIGHT 26
#define PIN_NEOPIXEL    27
#define PIN_DFPLAYER_RX 16
#define PIN_DFPLAYER_TX 17

// -------------------------------------------------------------------------
// ⚙️ Motor Settings
// -------------------------------------------------------------------------
#define MOTOR_STOP      90
#define MOTOR_MIN       0
#define MOTOR_MAX       180
#define TRIM_LEFT       0       // Adjust if bot drifts
#define TRIM_RIGHT      0

// -------------------------------------------------------------------------
// 📡 Network Settings
// -------------------------------------------------------------------------
#define WIFI_SSID       "JL-BN-Net"
#define WIFI_PASS       "heroprops"
#define USE_WIFI_AP     true    // Set false to save power/interference if only using ESP-NOW

#endif
