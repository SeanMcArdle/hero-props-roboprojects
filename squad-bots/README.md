# Squad Bots: JL-BN ("Jellybean") Prototype

**Status**: 🛠️ IN DEVELOPMENT  
**Hardware**: NodeMCU-32S (ESP32)  
**Role**: Swarm Bot / Hybrid Controller

## 🤖 About JL-BN
"Jellybean" is the Golden Master prototype for the Squad Bot platform. It is designed to be a low-cost, replicable droid for the Bakken Museum workshops.

### Features
*   **Swarm Mode**: Controlled via ESP-NOW by a Commander unit.
*   **Hybrid Mode**: Can host its own WiFi Access Point and Web UI for direct control.
*   **Multimedia**: DFPlayer Mini for sound, Neopixels for status/personality.
*   **Motion**: Differential drive (Continuous Rotation Servos or DC Motors).

## 🔌 Wiring (NodeMCU-32S)

| Component | Pin | Notes |
| :--- | :--- | :--- |
| **Left Motor** | GPIO 25 | Servo or H-Bridge PWM |
| **Right Motor** | GPIO 26 | Servo or H-Bridge PWM |
| **Neopixel** | GPIO 27 | Data Line |
| **DFPlayer RX** | GPIO 16 | Connect to TX of Player |
| **DFPlayer TX** | GPIO 17 | Connect to RX of Player |
| **SDA (I2C)** | GPIO 21 | For future expansion |
| **SCL (I2C)** | GPIO 22 | For future expansion |

## 🚀 Getting Started
1.  Open this folder in VS Code.
2.  Check `src/config.h` (Ensure no "42" traps!).
3.  Upload to your NodeMCU-32S.
