# 🎓 Jellybean: Teacher Edition Firmware

**Date:** January 7, 2026  
**Version:** Teacher Demo V1  
**Target:** ESP32 NodeMCU-32S ("Jellybean")

## 🌟 Overview
This firmware version is specifically designed for classroom demonstrations to teach students about **RGBW Color Mixing** and **Robot Logic**. It features a specialized Web UI with large sliders for manual color control and several pre-programmed animations.

## 🎮 Controls (Web UI)
**Connect:** WiFi `Jellybean-Net` (Pass: `squadgoals`)  
**URL:** `http://192.168.4.1`

### 1. Manual Color Mixer (The Science Part)
- **RED / GREEN / BLUE Sliders:** independently control the color channels. Use this to demonstrate how light adds up (e.g., Red + Green = Yellow).
- **WHITE Slider:** Controls the dedicated White LED channel (W). Demonstrate the difference between "Mixed White" (R+G+B) and "True White" (W).
- *Note:* Touching any slider automatically overrides active animations and enters **MANUAL MODE**.

### 2. Animations (The Fun Part)
- **PULSE:** A "Prince-themed" breathing effect (Purple/Gold).
- **PARTY:** Random high-energy sparkles (Heavy Purple bias for school colors).
- **RAINBOW:** A "marching ants" theater chase style rainbow pattern.
- **SCANNER:** A beefy 5-pixel wide "Cylon/KITT" eye scanner.
- **MANUAL:** Stops animations and holds the last slider color.

### 3. Robot Movement
- **PILOT (Left Stick):** Controls the drive motors (Differential Drive).
- **PERFORMER (Right Stick):** Controls the Dome Servo (Pan/Spin).

## 🛠 Technical Notes

### OTA Updates (Over-the-Air)
This firmware supports wireless updates.
1. Connect computer to `Jellybean-Net`.
2. Run: `pio run -t upload -e jl-bn-ota`

### Serial Debugging
- Baud Rate: `115200`
- The Serial Monitor works in real-time even while WiFi is active.
- **New Feature:** Moving sliders prints mixing data: `Rx: L:255,0,128,0` (useful for showing students the data flow).

### Hardware Config
- **LEDs:** Adafruit NeoPixel (Ring of 24) on Pin `27`.
- **Motors:** PWM Servos/ESCS on Pins `25, 26`.
- **Dome:** Servo on Pin `18`.
