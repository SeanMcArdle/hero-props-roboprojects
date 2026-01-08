# Chopper Commander V3 — Hero Props Modular Robotics Camp

**Chopper Commander** is the next-generation robotics camp and teaching platform from Hero Props. This project contains the firmware for the "Chopper Commander" droid V3, utilizing a modular multi-MCU architecture.

---

## 🏗 System Architecture (Tri-Core + Eye)

The Chopper V3 is powered by four distinct ESP32 controllers working in unison via **ESP-NOW** (HeroPropsProtocol). All roles share a single unified codebase but compile different feature sets based on the build flag.

### 1. The Captain (Torso / Main Brain)
- **Role:** `ROLE_CAPTAIN`
- **Hardware:** ESP32 Dev Module
- **Environment:** `env:captain`
- **Functions:** 
  - Hosts the WiFi Access Point (`CHOPPER_NET`).
  - Runs the Web Dashboard for driving and control.
  - Handles motor mixing and drive train logic.
  - Sends commands to other modules via ESP-NOW/Serial.

### 2. The Bard (Audio System)
- **Role:** `ROLE_BARD`
- **Hardware:** ESP32 Dev Module + I2S Amp (MAX98357A) + SD Card Reader
- **Environment:** `env:bard`
- **Functions:**
  - Plays audio files (MP3/WAV) from SD card.
  - Receives playback commands from The Captain.

### 3. The Lookout (Dome Controller)
- **Role:** `ROLE_LOOKOUT`
- **Hardware:** ESP32 Dev Module
- **Environment:** `env:lookout`
- **Functions:**
  - Controls Dome rotation (Motor/Servo).
  - Controls Dome Arm servos and Holoprojectors.
  - Manages Dome status LEDs (NeoPixels).
  - Performs animations (Happy, Angry, Scan).

### 4. The Eye (Camera System)
- **Role:** `ROLE_EYE`
- **Hardware:** ESP32-CAM (AI Thinker)
- **Environment:** `env:eye`
- **Functions:**
  - Connects to `CHOPPER_NET`.
  - Streams low-latency MJPEG video to the Web Dashboard.

---

## 📂 Project Structure

- **`src/`**: Shared source code.
    - **`Captain/`**: Brain & Web logic.
    - **`Bard/`**: Audio logic (Uses `ESP32-audioI2S`).
    - **`Lookout/`**: Dome animation logic.
    - **`Eye/`**: Camera streaming logic.
    - **`main.cpp`**: Entry point, selects module based on compile flags.
    - **`config.h`**: Shared pin definitions and network credentials.
- **`platformio.ini`**: Defines the build environments and libraries for each role.

---

## ⚡️ Quick Start / Flashing

Connect the appropriate ESP32 board and run the specific upload command:

### Flash The Captain
```bash
pio run -e captain -t upload
```

### Flash The Bard (Audio)
```bash
pio run -e bard -t upload
```

### Flash The Lookout (Dome)
```bash
pio run -e lookout -t upload
```

### Flash The Eye (Camera)
*Note: Connect GPIO 0 to GND before plugging in USB.*
```bash
pio run -e eye -t upload
```

---

## 🔧 Libraries
This project uses:
- **HeroPropsProtocol**: Internal ESP-NOW communication.
- **ESPAsyncWebServer**: For the Captain's dashboard and the Eye's stream.
- **ESP32-audioI2S**: For high-quality audio playback.
- **Adafruit NeoPixel**: For status lights.
- **ESP32Servo**: For dome mechanisms.
