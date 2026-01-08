# Chopper Commander V3 - System Architecture

**Status:** 🏗️ ARCHITECTING
**Date:** Jan 7, 2026
**Protocol:** HeroPropsProtocol V1.1

## 🎯 Vision
A modular, distributed control system for the C1-10P droid, consisting of multiple specialized subsystems communicating via ESP-NOW and WiFi.

## 🧩 System Topology

### 1. The Captain (Torso Brain)
*   **MCU:** ESP32 (DevKit)
*   **Role:** Central Orchestrator, Motion Control, User Interface.
*   **Connectivity:**
    *   **WiFi AP:** `CHOPPER` (Web Controller host).
    *   **ESP-NOW:** Broadcasts state to Dome & Swarm.
    *   **UART/Serial:** Direct link to Audio subsystem (TBC).
*   **Hardware:**
    *   Left/Right Drive Motors (Pins 25/26).
    *   Dome Rotation Servo (Pin 27 - Continuous).
    *   *New:* Hall Effect Sensor (Pin TBD) for Dome "Home" alignment.

### 2. The Bard (Audio Subsystem)
*   **MCU:** ESP32
*   **Role:** High-quality audio playback and processing.
*   **Connectivity:**
    *   **UART:** Receives commands from "The Captain" (e.g., `Play Sound 3`, `Volume 50`).
*   **Hardware:**
    *   **Output:** MAX98357A I2S Amplifier -> Speaker.
    *   **Input:** INMP441 I2S Microphone (Voice Command/Analysis).
    *   **Storage:** MicroSD Card (for local wav/mp3 storage) OR Internal SPIFFS?

### 3. The Lookout (Dome FX)
*   **MCU:** ESP32
*   **Power:** Independent (AA pack).
*   **Role:** Animating the dome internals.
*   **Connectivity:**
    *   **ESP-NOW:** Peer listener (Receives `DomeCmd` packets).
*   **Hardware:**
    *   **Eyes:** 2x 8mm NeoPixels (Daisy chained).
    *   **Periscope:** 1x 8mm NeoPixel.
    *   **Servo 1:** Left Arm (180).
    *   **Servo 2:** Right Arm (180).
    *   **Servo 3:** Head Tilt/Rock (High Torque).

### 4. The Eye (Camera)
*   **MCU:** ESP32-CAM
*   **Role:** Video streaming.
*   **Connectivity:** WiFi Station (Connects to `CHOPPER` AP or venue WiFi).

---

## 🔌 Wiring & Protocols

### "The Captain" -> "The Bard" (Inter-Chip Link)
Since they are physically close, a direct Serial link is robust.
*   **Captain TX** -> **Bard RX**
*   **Captain RX** <- **Bard TX** (for "Sound Done" signals).

### "The Captain" -> "The Lookout" (Dome Link)
Wireless ESP-NOW.
*   **Packet Type:** `HeroPropsProtocol::Packet`
*   **Group ID:** `DROID_DOME`
*   **Function:** Syncs light states and arm positions with general droid mood.

---

## 📝 Critical Decisions TBD
1.  **Audio Storage:** Does "The Bard" stream from web or play from SD Card? (SD Card recommended for instant latency).
2.  **Dome Home:** Hall Effect Sensor proposed for alignment.
