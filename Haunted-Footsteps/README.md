# Haunted Footsteps

Ethereal, glowing "ghost footprint" props for a Haunted Mansion-inspired stage play — recreating the
ride's glowing-footprint illusion as a wearable effect. Two translucent green TPU footprint shells
(heel + toe) each have an embedded channel of addressable 5V LEDs around their perimeter. The prints
sit on a sandal-style foot pad with momentary buttons under the heel and toe. When the actor's weight
lands on a button, that section fades up into a slow, random, ghostly flicker; when weight lifts, it
fades back down — **always slower on the fade-out than the fade-in**, so the glow never just winks out.

Everything is tunable live from a phone/laptop browser over WiFi (AP mode), the same pattern used by
the other Hero Props droids/rigs in this repo.

## Status
🛠️ **SCAFFOLDED** — firmware architecture in place, bench-testing in progress. See
[docs/WIRING.md](docs/WIRING.md) for open hardware questions.

## Hardware (per foot)
- 1x ESP32 dev board
- 1x continuous addressable 5V LED strip (WS2812B assumed — change `LED_TYPE`/`COLOR_ORDER` in
  [src/config.h](src/config.h) if using SK6812 or similar) on a single data pin, running through
  both the heel and toe prints — heel pixels, then a dark gap, then toe pixels, then a dark tail.
  See the pixel map in [docs/WIRING.md](docs/WIRING.md) for exact ranges.
- 2x momentary buttons (heel + toe), wired to GND when pressed
- 4x AA battery pack + physical ON/OFF toggle switch (in-line with the battery, not GPIO-controlled)
- Level shifter recommended on the data line (see [docs/WIRING.md](docs/WIRING.md))

`NUM_LEDS_TOTAL` and the `HEEL_LED_*`/`TOE_LED_*` ranges in [src/config.h](src/config.h) reflect the
strip as currently bench-measured (60 pixels total) — **the strip will likely be trimmed once
placement is finalized, so revisit these after cutting it.**

## Firmware Architecture
- **`src/config.h`** — pins, LED counts/type, WiFi AP identity, default flicker/fade settings.
- **`src/main.cpp`** — reads the heel/toe buttons, drives an independent "envelope" (fade in/out level)
  per foot section, and layers a slow Perlin-noise flicker on top for the ghostly, uneven glow.
  Heel and toe run the same simple script independently — pressing one never affects the other.
- **`src/web_server.{h,cpp}`** — AsyncWebServer + WebSocket UI server. Serves the browser UI from
  LittleFS (`data/`), pushes live telemetry (button/envelope state), and applies settings changes
  (color, flicker speed, intensity, fade times) sent from the browser. Settings persist across
  reboots via `Preferences` (NVS), same as the squad-bot droids.
- **`data/index.html`** — plain HTML/CSS/JS control panel (no external CDN dependencies — the ESP32
  AP has no internet access, so everything needed has to ship in this one file).

## Behavior Model
Each foot section (heel, toe) tracks a floating-point **envelope** (0.0–1.0):
- While the button is held down, the envelope ramps toward `1.0` at the **fade-in** rate.
- The instant the button releases, the envelope ramps back toward `0.0` at the **fade-out** rate.
- Firmware clamps `fadeOutMs >= 1.5 * fadeInMs` server-side no matter what the browser sends, so the
  glow can never be configured to snap off faster than it turns on.
- On top of the envelope, a slow per-pixel Perlin-noise flicker (`inoise8`) modulates brightness within
  a narrow band so the glow looks alive/uneven rather than a flat dimmer — heel and toe use different
  noise seeds so they never flicker in lockstep.

## Browser Control Panel
Connect to the ESP32's WiFi AP (see `WIFI_SSID`/`WIFI_PASS` in [src/config.h](src/config.h)) and open
`http://192.168.4.1` (or `http://haunted-footsteps.local` via mDNS). From there you can adjust:
- **Color** — the ghost-glow color (defaults to Haunted Mansion green)
- **Flicker Speed** — how fast the ethereal shimmer drifts
- **Intensity** — max brightness ceiling
- **Fade In / Fade Out** — timing of the press/release transition (fade-out is always the slower one)

Live telemetry (button pressed/released, current envelope %) is shown for both heel and toe so you can
verify the pads are triggering correctly during tech rehearsal.

## Firmware Updates (OTA via iPad/browser)
No laptop needed for routine updates. From any device on the `Haunted-Footsteps` WiFi (including an
iPad), open `http://haunted-footsteps.local/update` (or the control panel's "Firmware" card), sign in
with the OTA credentials, pick the `.bin` built by `pio run`, and upload. The device flashes it to the
inactive OTA partition and reboots into it automatically.

- **`.pio/build/esp32dev/firmware.bin`** is the file to upload (built by `pio run`, no upload needed first).
- The OTA page is gated by **`OTA_USERNAME`/`OTA_PASSWORD`** in [src/config.h](src/config.h) —
  intentionally a *separate* login from the WiFi AP password (`WIFI_PASS`), so sharing the WiFi
  password (e.g. with actors/students) doesn't also grant firmware-flashing access.
  **Change the default OTA password before using this on a real show** (override via `build_flags` in
  platformio.ini, e.g. `-D OTA_PASSWORD=\"...\"`, same pattern as `WIFI_SSID`/`WIFI_PASS`).
- LEDs freeze (stop updating) for the few seconds the upload takes, then the board reboots — this is
  expected, not a bug.
- This uses the ESP32 core's built-in `Update.h` HTTP-upload flow, not the classic `ArduinoOTA`/`espota`
  protocol (that one requires a PC running PlatformIO/Python and has no iPad client).
- Requires the custom [partitions.csv](partitions.csv) in this project (dual ~1.5MB OTA app slots) —
  don't switch `board_build.partitions` back to a single-app scheme like `huge_app.csv`, or OTA writes
  will fail with no partition to write into.

## Build
```bash
pio run                 # build
pio run -t upload       # flash firmware (first flash only needs USB; later ones can use /update over WiFi)
pio run -t uploadfs     # upload data/ (web UI) to LittleFS — required once, and again if UI changes
pio device monitor       # serial console
```

## Next Steps
- [ ] Trim the LED strip once placement is finalized, then re-confirm the pixel map/`NUM_LEDS_TOTAL`
      in `src/config.h` (see `docs/WIRING.md`)
- [ ] Bench test button wiring polarity (pullup vs pulldown) against the physical sandal pad
- [ ] Confirm AA pack voltage under load is safe for the LED strip / ESP32 5V rail
- [ ] Verify data-line signal integrity (add level shifter/resistor if the strip glitches)
- [ ] Tune default flicker speed / intensity / fade times to taste at tech rehearsal
- [ ] Change the default OTA password in `src/config.h` before first real use
