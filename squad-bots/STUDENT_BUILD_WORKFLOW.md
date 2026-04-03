# 🤖 L0-0N Student Bot Production Build

**Target**: Configure & flash 6 custom L0-0N variants for students  
**Date**: April 2, 2026  
**Students**: CRUST-P0, B00-B0, Sp0tty-M33P, RJ-BD3, KP-71, LU-CA

---

## Phase 1: Hardware Wiring Checklist

> **Do this FIRST before any firmware flashing**

For each bot, verify:

- [ ] **NodeMCU-32S ESP32** Board connected to USB
- [ ] **Motor Connections** (via servo headers on breadboard)
  - [ ] GPIO 25 → Left drive servo signal
  - [ ] GPIO 26 → Right drive servo signal
  - [ ] GPIO 13 → Dome servo signal
  - [ ] GND & 5V connected to servo power rail
- [ ] **NeoPixel Ring** (8-LED RGB strip)
  - [ ] GPIO 27 → Signal line (usually yellow)
  - [ ] GND & 5V connected
- [ ] **Optional DFPlayer** (if using audio)
  - [ ] GPIO 16 (RX2) → DFPlayer TX
  - [ ] GPIO 17 (TX2) → DFPlayer RX
  - [ ] GND & 5V connected

**Wiring Reference Diagram**: (See `docs/` or ask for updated schematic)

---

## Phase 2: Firmware Build & Flash

### Quick Build Script

Run this in `/squad-bots/` directory:

```bash
#!/bin/bash
# build_student_bots.sh
# Usage: ./build_student_bots.sh CRUST-P0

BOT_NAME=$1
WIKI_SSID="${BOT_NAME}-Net"
UPLOAD_PORT=${2:-/dev/cu.usbserial-0001}  # Default; override if needed

if [ -z "$BOT_NAME" ]; then
    echo "Usage: ./build_student_bots.sh <BOT_NAME> [UPLOAD_PORT]"
    echo "Example: ./build_student_bots.sh CRUST-P0"
    echo "         ./build_student_bots.sh B00-B0 /dev/cu.usbserial-0002"
    exit 1
fi

echo "🔧 Building for: $BOT_NAME"
echo "   WiFi SSID:   $WIKI_SSID"
echo "   Upload Port: $UPLOAD_PORT"

# Temporarily patch config.h with custom names
sed -i '' "s/#define BOT_NAME \".*\"/#define BOT_NAME \"$BOT_NAME\"/g" src/config.h
sed -i '' "s/#define WIFI_SSID_NAME \".*\"/#define WIFI_SSID_NAME \"$WIKI_SSID\"/g" src/config.h
sed -i '' "s/static const char \*MDNS_HOST = \".*\"/static const char *MDNS_HOST = \"$(echo $BOT_NAME | tr A-Z a-z)\"/g" src/main.cpp

# Build and upload
echo "⚙️  Compiling..."
~/.platformio/penv/bin/platformio run -e l0-0n

echo "📤 Flashing to $UPLOAD_PORT..."
~/.platformio/penv/bin/platformio run -e l0-0n -t upload --upload-port $UPLOAD_PORT

# Restore defaults (optional, for cleanliness)
git checkout src/config.h src/main.cpp

echo "✅ Build complete for $BOT_NAME!"
echo ""
echo "🔍 Test: Connect to WiFi '$WIKI_SSID' → Visit http://192.168.4.1"
echo "   Or   Visit http://$(echo $BOT_NAME | tr A-Z a-z).local"
```

### Manual Build (Per-Student)

If you prefer NOT to use a script, do this for each bot:

1. **Edit config.h**:
   ```diff
   - #define BOT_NAME "L0-0N"
   + #define BOT_NAME "CRUST-P0"
   
   - #define WIFI_SSID_NAME "L0-0N-Net"
   + #define WIFI_SSID_NAME "CRUST-P0-Net"
   ```

2. **Edit main.cpp** (find MDNS_HOST line ~18):
   ```diff
   - static const char *MDNS_HOST = "l00n";
   + static const char *MDNS_HOST = "crustp0";  // lowercase!
   ```

3. **Build & Flash**:
   ```bash
   ~/.platformio/penv/bin/platformio run -e l0-0n -t upload --upload-port /dev/cu.usbserial-XXXX
   ```

4. **Restore** (before committing):
   ```bash
   git checkout src/config.h src/main.cpp
   ```

---

## Phase 3: Post-Flash Validation (Per Bot)

After each board flashes:

1. **Check Serial Monitor** (optional, for debug):
   ```bash
   ~/.platformio/penv/bin/pio device monitor -p /dev/cu.usbserial-XXXX -b 115200
   ```
   Look for:
   - `[I] WiFi AP: CRUST-P0-Net (192.168.4.1)`
   - `[I] mDNS: crustp0.local`
   - `[I] Web server running...`

2. **Connect via WiFi**:
   - SSID: `<BOT_NAME>-Net` (e.g., `CRUST-P0-Net`)
   - Password: `heroprops`

3. **Load Web UI**:
   - Via IP: `http://192.168.4.1`
   - Or via mDNS: `http://<botname>.local` (e.g., `http://crustp0.local`)

4. **Test Controls**:
   - [ ] Dome slider moves left/right, then CENTER stops it
   - [ ] Forward/Back buttons engage left & right motors
   - [ ] LED animation buttons work
   - [ ] (Optional) Audio buttons visible if DFPlayer enabled

5. **Quick Hardware Check**:
   - [ ] Left motor spins on command
   - [ ] Right motor spins on command
   - [ ] Dome servo responds to slider (spins or points)
   - [ ] LED ring shows color/animation

---

## Batch Flashing Order

| # | Student  | Bot Name       | WiFi SSID          | mDNS Host      | Status |
|---|----------|----------------|-------------------|-----------------|--------|
| 1 | Stu #1   | CRUST-P0       | CRUST-P0-Net      | crustp0.local  | ⬜ |
| 2 | Stu #2   | B00-B0         | B00-B0-Net        | b00b0.local    | ⬜ |
| 3 | Stu #3   | Sp0tty-M33P    | Sp0tty-M33P-Net   | spotty.local   | ⬜ |
| 4 | Stu #4   | RJ-BD3         | RJ-BD3-Net        | rjbd3.local    | ⬜ |
| 5 | Stu #5   | KP-71          | KP-71-Net         | kp71.local     | ⬜ |
| 6 | Stu #6   | LU-CA          | LU-CA-Net         | luca.local     | ⬜ |

---

## Troubleshooting

**"Port not found" error**:
- Verify USB cable connected and board recognized: `ls /dev/cu.usb*`
- Replace `/dev/cu.usbserial-XXXX` with correct port from listing

**WiFi won't show up**:
- Check antenna connection on ESP32
- Try restarting board: Press EN button

**mDNS hostname not resolving**:
- Ensure you set `MDNS_HOST` in main.cpp (case-sensitive, lowercase preferred)
- May take 30sec to resolve after boot

**Web UI not loading**:
- Check serial monitor for boot errors
- Try `http://192.168.4.1` directly (IP fallback)

---

## Notes

- **Baseline Code**: `l0-0n-baseline-20260402-0853` tag on GitHub (stable, safe to use)
- **DFPlayer**: Currently enabled (`#define USE_DFPLAYER true`). Disable in config.h if not using.
- **No Code Commits**: Don't commit the custom names—use this workflow to patch before build, then restore after.
