# Bakken Workshop

2-day robotics workshop at The Bakken Museum, Minneapolis.
Dec 30-31, 2025 | 16 kids, ages 9-14 | SOLD OUT

## Status

### Working
- Chopper rolls, steers correctly (left = y - x, right = y + x)
- OTA firmware updates
- Watchdog timer (500ms, stops on disconnect)
- WiFi stability (sleep disabled)
- Voice control ready (Web Speech API)

### Hardware
- 19 droids: BK-00 (demo), BK-01–BK-16 (kids), BK-17 (spare)
- Chopper: Squad commander, ESP-NOW broadcast

### Pins (Chopper)
| Pin | Function |
|-----|----------|
| P25 | Left Motor |
| P26 | Right Motor |
| P27 | Dome Servo |
| P14 | Front Arm |

### Next
- [ ] Dome servo wired (P27)
- [ ] Swarm test with 2-3 droids
- [ ] Flash all 16 ESP32s
