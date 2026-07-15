# 🤖 L0-0N Student Bot Production Build

**Target**: Configure & flash student-named L0-0N variants for camp  
**Date**: April 2, 2026  
**Students**: Use each student's first name, plus last initial when needed for duplicates

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

### Recommended: Locked CSV Batch Workflow (No Guessing)

Use the CSV roster as the single source of truth. This prevents accidental extra names or missing boards.

From `/squad-bots/`:

```bash
# 1) Validate the roster only (no flashing)
./batch_build_student_bots_csv.sh --csv bakken-campers-summer-2026.csv --expected-count 23 --dry-run

# 2) Run actual flashing only after roster check passes
./batch_build_student_bots_csv.sh --csv bakken-campers-summer-2026.csv --expected-count 23
```

Safety features now built into this script:

- Refuses to run if parsed roster count does not match `--expected-count`
- Refuses to run if duplicate WiFi SSIDs are found
- Prints full planned roster before flashing
- Writes an audit log (`flash_audit_YYYYMMDD-HHMMSS.log`) with per-board outcomes
- Supports resume with `--start-at <row>` if the session is interrupted

### Quick Build Script

Run this in `/squad-bots/` directory:

```bash
#!/bin/bash
# build_student_bots.sh
# Usage: ./build_student_bots.sh LucasJ

BOT_NAME=$1
SANITIZED_NAME=$(echo "$BOT_NAME" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+//g')
WIFI_SSID="$SANITIZED_NAME"
WIFI_PASS="${SANITIZED_NAME}123"
UPLOAD_PORT=${2:-/dev/cu.usbserial-0001}  # Default; override if needed
BOARD_ENV=${3:-l0-0n-devkit}

if [ -z "$BOT_NAME" ]; then
    echo "Usage: ./build_student_bots.sh <STUDENT_NAME> [UPLOAD_PORT] [BOARD_ENV]"
    echo "Example: ./build_student_bots.sh LucasJ"
    echo "         ./build_student_bots.sh LucasD /dev/cu.usbserial-0002"
    echo "         ./build_student_bots.sh Mia /dev/cu.usbserial-0003 l0-0n-devkit"
    exit 1
fi

echo "🔧 Building for: $BOT_NAME"
echo "   WiFi SSID:   $WIFI_SSID"
echo "   Password:    $WIFI_PASS"
echo "   Upload Port: $UPLOAD_PORT"

# Temporarily patch config.h with simple student-name values
sed -i '' "s/#define BOT_NAME \".*\"/#define BOT_NAME "$BOT_NAME"/g" src/config.h
sed -i '' "s/#define WIFI_SSID_NAME \".*\"/#define WIFI_SSID_NAME "$WIFI_SSID"/g" src/config.h
sed -i '' "s/#define WIFI_SSID \".*\"/#define WIFI_SSID "$WIFI_SSID"/g" src/config.h
sed -i '' "s/#define WIFI_PASS \".*\"/#define WIFI_PASS "$WIFI_PASS"/g" src/config.h
sed -i '' "s/static const char \*MDNS_HOST = \".*\"/static const char *MDNS_HOST = "$SANITIZED_NAME"/g" src/main.cpp

# Build and upload
echo "⚙️  Compiling..."
~/.platformio/penv/bin/platformio run -e l0-0n-devkit

echo "📤 Flashing to $UPLOAD_PORT..."
~/.platformio/penv/bin/platformio run -e l0-0n-devkit -t upload --upload-port $UPLOAD_PORT

# Restore defaults (optional, for cleanliness)
git checkout src/config.h src/main.cpp

echo "✅ Build complete for $BOT_NAME!"
echo ""
echo "🔍 Test: Connect to WiFi '$WIFI_SSID' → Visit http://192.168.4.1"
echo "   Or   Visit http://$SANITIZED_NAME.local"
```

### Manual Build (Per-Student)

If you prefer NOT to use a script, do this for each bot:

1. **Edit config.h**:
   ```diff
   - #define BOT_NAME "L0-0N"
   + #define BOT_NAME "LucasJ"
   
   - #define WIFI_SSID_NAME "L0-0N-Net"
   + #define WIFI_SSID_NAME "lucasj"
   
   - #define WIFI_PASS "heroprops"
   + #define WIFI_PASS "lucasj123"
   ```

2. **Edit main.cpp** (find MDNS_HOST line ~18):
   ```diff
   - static const char *MDNS_HOST = "l00n";
   + static const char *MDNS_HOST = "lucasj";  // lowercase, no spaces
   ```

3. **Build & Flash**:
   ```bash
   ~/.platformio/penv/bin/platformio run -e l0-0n-devkit -t upload --upload-port /dev/cu.usbserial-XXXX
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
   - `[I] WiFi AP: lucasj (192.168.4.1)`
   - `[I] mDNS: lucasj.local`
   - `[I] Web server running...`

2. **Connect via WiFi**:
   - SSID: `<studentname>` (e.g., `lucasj`)
   - Password: `<studentname>123` (e.g., `lucasj123`)

3. **Load Web UI**:
   - Via IP: `http://192.168.4.1`
   - Or via mDNS: `http://<studentname>.local` (e.g., `http://lucasj.local`)

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

### Verified Pin-Test (ESP32 DevKit)

We validated the `l0-0n-devkit` pinout using the `l0-0n-devkit-test` build. The test firmware toggles a small set of GPIOs and prints status to Serial. Observed (repeating):

- `PIN TEST: toggled pin 15 (idx=0)`
- `PIN TEST: toggled pin 2  (idx=1)`
- `PIN TEST: toggled pin 4  (idx=2)`
- `PIN TEST: toggled pin 16 (idx=3)`
- `PIN TEST: toggled pin 17 (idx=4)`

To reproduce the test on a board connected at `/dev/cu.usbserial-2120`:

```bash
~/.platformio/penv/bin/pio run -e l0-0n-devkit-test -t upload --upload-port /dev/cu.usbserial-2120
~/.platformio/penv/bin/pio device monitor -p /dev/cu.usbserial-2120 -b 115200
```

If the test does not toggle pins, check that the board GND is common with the target peripherals, verify the USB cable, and free the serial port using `lsof -t /dev/cu.usbserial-2120 | xargs -r kill` before retrying.

---

## Batch Flashing Order

| # | Student | Bot Name | WiFi SSID | Password | mDNS Host | Status |
|---|---------|----------|-----------|----------|-----------|--------|
| 1 | Student 1 | LucasJ | lucasj | lucasj123 | lucasj.local | ⬜ |
| 2 | Student 2 | LucasD | lucasd | lucasd123 | lucasd.local | ⬜ |
| 3 | Student 3 | Mia | mia | mia123 | mia.local | ⬜ |

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
