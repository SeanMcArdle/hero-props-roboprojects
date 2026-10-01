# Wiring — Haunted Footsteps

Reference: hand-drawn wiring sketch (single-foot module). Transcribed pin mapping below; **verify
polarity/voltages on the bench before attaching to the actual TPU prints.**

**Update (superseding the original sketch):** the heel and toe are actually one continuous
addressable strip on a single data line, not two independent strips as the original sketch's
"TOE DATA"/"HEEL DATA" wires implied. See the pixel map below.

## Layout (per foot)
```
[4x AA]--(red)--[ON/OFF Toggle]--(red)--+---> ESP32 5V/VIN
                                         +---> LED strip VCC

[4x AA]--(black/GND)---------------------> ESP32 GND + LED strip GND (common ground)

ESP32 --(data, single line)--> Strip data-in --> heel pixels --> (dark gap) --> toe pixels --> (dark tail)

Toe momentary switch  --(blue)--> ESP32 (GPIO, other leg to GND)
Heel momentary switch --(blue)--> ESP32 (GPIO, other leg to GND)
```

The ON/OFF toggle in the sketch is a **physical inline switch on the battery pack**, not GPIO-controlled
— it just kills power to the whole rig. No firmware involvement.

## Pixel Map (single strip, `src/config.h`)
As measured on the bench (1-indexed pixel numbers):

| Pixels (1-indexed) | Section       | Firmware constant                          |
|---------------------|---------------|--------------------------------------------|
| 1–17                | Heel          | `HEEL_LED_START=0`, `HEEL_LED_COUNT=17`     |
| 18–19               | Dark gap      | (never written, stays off)                 |
| 20–45               | Toe           | `TOE_LED_START=19`, `TOE_LED_COUNT=26`      |
| 46–60               | Dark tail     | (never written, stays off)                 |

Total strip length is currently **60** (`NUM_LEDS_TOTAL`) — the tail beyond pixel 45 is unused and
will likely be trimmed once the final placement/count is confirmed. **Re-verify this whole table
after cutting the strip**, since trimming past pixel 45 doesn't matter, but cutting anywhere before
that would change the heel/toe pixel counts.

## Pin Mapping (`src/config.h`)
| Signal              | GPIO | Notes                                                              |
|---------------------|------|---------------------------------------------------------------------|
| `PIN_LED_STRIP`      | 25   | Single data line for the whole strip (heel + gap + toe + tail)     |
| `PIN_BUTTON_TOE`     | 32   | `INPUT_PULLUP`, switch wired to GND (pressed = LOW)                |
| `PIN_BUTTON_HEEL`    | 33   | `INPUT_PULLUP`, switch wired to GND (pressed = LOW)                |

GPIO 26 is now free (previously reserved for a second, independent heel data line before the
single-strip wiring was confirmed).

GPIO 25/32/33 were chosen because they're free of boot-strapping restrictions and (32/33) support
internal pull-ups, unlike the input-only 34–39 pins. Adjust in `src/config.h` if the dev board pinout
forces different routing.

## Open Hardware Questions
1. **Power rail** — 4x AA (fresh alkaline ~6V, NiMH ~4.8V) feeding the ESP32's 5V/VIN pin directly.
   Confirm the specific dev board's onboard regulator tolerates this range, and that the LED strips
   get a clean 5V (voltage sag under load as batteries drain can cause flicker/brownout resets —
   independent of the intentional flicker effect!). A small buck/boost regulator inline may be worth
   adding if this is unreliable on the bench.
2. **Data signal level** — ESP32 data pins are 3.3V logic; WS2812-family strips are typically happier
   with a ~5V data signal, especially over any wire length. If the prints flicker/glitch randomly
   (distinct from the intentional ghostly flicker), add a level shifter (e.g. 74AHCT125) or the
   classic 300–500Ω series resistor + close proximity as a first mitigation.
3. **Button wiring polarity** — sketch shows both switches as simple 2-wire momentary contacts to the
   ESP32. Firmware assumes `INPUT_PULLUP` (switch shorts to GND when weight is on the pad). If the
   sandal pad wiring is normally-closed instead, flip the logic in `main.cpp`'s `isPressed()` helper.
4. **Final strip length** — confirmed at 60 pixels for now, but the strip will be trimmed once
   placement is finalized. Update `NUM_LEDS_TOTAL` (and re-verify the pixel map above) after cutting.
