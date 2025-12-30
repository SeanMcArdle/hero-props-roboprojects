# BB-R2 Workshop Controller

**Status:** HELP WANTED - Servo drift issue with ESP32 WiFi  
**Deadline:** December 30, 2025 evening (workshop is live)  
**Author:** Seán McArdle / Hero Props Inc.  
**Based on:** [Bjoern Giesler's BB-R2 STEM ESP32](https://github.com/bjoerngiesler/BB-R2_STEM_ESP32)

---

## What This Is

A simplified web-controlled BB-R2 (Baby R2-D2) droid controller for a kids' robotics workshop at The Bakken Museum. 16 kids, 16 droids, ages 9-14.

## Key Change From Original

**Bjoern's version:** ESP32 connects to existing WiFi network (infrastructure mode)  
**This version:** ESP32 creates its own WiFi access point (AP mode)

### Why AP Mode?
- Museum WiFi is unreliable/restricted
- Each kid connects phone directly to their droid
- No network infrastructure needed
- Simpler for workshop environment

---

## The Problem

**Continuous rotation servos slowly drift clockwise when WiFi AP is active, even when commanded to stop.**

### Hardware
- ESP32 DevKit v1 (NodeMCU-32S style)
- FS90R continuous rotation servos (×2 for drive)
- Standard 180° servo (×1 for dome)
- 4×AA battery pack (~6V)
- Direct servo connection to ESP32 GPIO pins

### Observed Behavior
1. Servos calibrated correctly at 1503μs = stopped (verified with no-WiFi test)
2. When WiFi AP is enabled, servos drift slowly clockwise
3. Drift persists regardless of commanded microseconds
4. Drift varies in speed but doesn't stop
5. 180° dome servo unaffected (or less noticeably affected)

---

## What We've Tried

| Attempt | Result |
|---------|--------|
| Calibration sweep (1450-1550μs) | Found true neutral at 1503μs |
| Added trim constants (+3μs) | No change with WiFi active |
| Changed pins from 25/26 to 32/33 | No change |
| Avoided DAC pins (25/26) | No change |
| ESP32Servo library timers | Drift persists |
| LEDC direct control | Drift persists |
| MCPWM peripheral | Inconclusive (setup issues) |
| Increased dead zone | Drift persists |
| Heartbeat to prevent watchdog | Helps responsiveness, not drift |

### Test That Proved WiFi Is The Cause

**ServoTest.ino (no WiFi):** Servos stop correctly at 1503μs ✓  
**TestWiFi1503.ino (with WiFi AP):** Servos drift at 1503μs ✗

Same hardware, same pins, same microseconds. Only difference is `WiFi.softAP()`.

---

## Current Code

`BB-R2-Workshop.ino` - Working controller with known drift issue

### Protocol
- WiFi AP: `R2-BK00` / password `BK00droid`
- WebSocket on port 80 at `/ws`
- Drive command: `D:x,y` where x,y are -1.0 to 1.0
- Dome command: `M:v` where v is 0.0 to 1.0
- Heartbeat: JS sends joystick position every 50ms while active
- Watchdog: 2000ms timeout stops motors

### Pin Assignments
```
GPIO 25 - Left drive servo (continuous)
GPIO 26 - Right drive servo (continuous)  
GPIO 27 - Dome servo (180°)
```

### Libraries
- ESP32Servo
- ESPAsyncWebServer
- AsyncTCP

---

## What Would Help

1. **Anyone solved ESP32 WiFi + servo timing conflicts?**
2. **Alternative timer strategies?** (We tried LEDC and MCPWM)
3. **Hardware solutions?** (PCA9685 would work but no time to source)
4. **Is there an ESP32 core version that handles this better?**

---

## Fallback Plan

Flash all 16 droids with current code. They'll drift slowly but still be controllable. Kids will have fun anyway. But a fix would be better.

---

## Test Files Included

| File | Purpose |
|------|---------|
| `BB-R2-Workshop.ino` | Main controller (has drift issue) |
| `ServoTest.ino` | Minimal test, no WiFi (works correctly) |
| `ServoFinder.ino` | Calibration sweep tool |
| `TestWiFi1503.ino` | Proves WiFi causes drift |

---

## Contact

- GitHub Issues on this repo
- Or find me on the Baddeley BB Astromech Patreon/Discord

Workshop is TODAY. Any help appreciated.

---

## License

MIT - Do whatever you want with it.

## Credits

- Bjoern Giesler - Original BB-R2 STEM ESP32 code
- Michael Baddeley - BB Astromech designs
- The Bakken Museum - Hosting the workshop
