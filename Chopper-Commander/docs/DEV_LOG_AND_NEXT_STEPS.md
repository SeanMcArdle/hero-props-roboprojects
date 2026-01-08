# Development Log & Next Steps
**Date:** January 7, 2026
**Status:** Phase 2 Complete (Architecture & Compilation)

## 🏗 System State: "Tri-Core + Eye"
The Chopper Commander V3 system has been successfully re-architected into four distinct modules sharing a single codebase.

| Role | Environment | Hardware | Status | Notes |
|---|---|---|---|---|
| **The Captain** | `env:captain` | ESP32 Dev | ✅ Ready | Brain, WiFi, Motor Mixing, Web UI |
| **The Bard** | `env:bard` | ESP32 Dev | ✅ Ready | Audio (I2S), SD Card, Serial Comms |
| **The Lookout** | `env:lookout` | ESP32 Dev | ✅ Ready | Dome Servos, NeoPixels, ESP-NOW Rx |
| **The Eye** | `env:eye` | ESP32-CAM | ✅ Ready | MJPEG Streamer, WiFi Client |

---

## 📝 Recent Changes
1.  **Unified Codebase**: `src/main.cpp` switches roles based on `platformio.ini` build flags.
2.  **Dependency Management**: 
    - Fixed `ESP32-audioI2S` C++20 build error by pinning version `3.0.12`.
    - Guarded `Audio.h` includes in `Bard.h` to prevent `Lookout` build failures.
3.  **Eye Module**: Created `src/Eye` to handle video streaming independently.
4.  **Communication**:
    - **Captain -> Lookout**: Uses ESP-NOW (HeroPropsProtocol). Dome Y-stick controls Head Tilt remotely.
    - **Captain -> Bard**: Uses Serial1 (UART). Sends text commands (`PLAY:1`, `STOP`).
5.  **Housecleaning**: Removed legacy `dome-cam`, `dome-leds`, and old Readmes.

---

## 🔮 Next Steps (Checklist for Resume)

### 1. Hardware Verification
- [ ] **Flash All Boards**: Run the 4 upload commands (see `README.md`).
- [ ] **Network Check**: Connect to `CHOPPER_NET` (Pass: `droid1234`).
- [ ] **Web UI Check**: Open `http://192.168.4.1` (Captain IP) and verify controls load.

### 2. Physical Integration & Tuning
- [ ] **Motor Direction**: Check `Captain.cpp` lines 122-126. If wheels spin opposite, uncomment the inversion line.
- [ ] **Audio Pinout**: The current `platformio.ini` configures I2S for `env:bard`. Verify wiring matches:
    - BCLK: 14
    - LRC: 12
    - DOUT: 27
- [ ] **SD Card Conflict**: `Bard.cpp` notes a potential pin conflict between VSPI (SD) and I2S Mic. **Mic is currently ignored** to ensure SD playback works.
- [ ] **Dome Servo Limits**: Check `Lookout.cpp` line 67. Tilt is mapped to 45-135 degrees. Ensure this doesn't stall the servo.

### 3. Feature Expansion
- [ ] **Sound Files**: Populate SD card with `001.mp3`, `002.mp3`... matching the Web UI IDs.
- [ ] **Animations**: `Lookout.cpp` has basic Happy/Angry/Scan animations. Add more as needed.
- [ ] **Camera Tuning**: `Eye.cpp` defaults to VGA (if PSRAM) or SVGA. Adjust quality if frame rate is low.

---

## 🛠 Useful Commands

**Build & Upload Specific Roles:**
```bash
pio run -e captain -t upload
pio run -e bard -t upload
pio run -e lookout -t upload
pio run -e eye -t upload
```

**Monitor Serial Output:**
```bash
pio device monitor -e captain
```
