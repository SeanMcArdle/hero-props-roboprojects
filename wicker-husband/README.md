# 🪆 Wicker Husband — Animatronic Eyes

ESP32 adaptation of **[mklements/AIChatbot `EyeMovement.py`](https://github.com/mklements/AIChatbot)** (Raspberry Pi 5 + PCA9685), ported to the **Hero Props Proptocols** ecosystem.

---

## What This Does

| Original (Raspberry Pi)                     | This Project (ESP32)                          |
|---------------------------------------------|-----------------------------------------------|
| Python `EyeMovement.py`                     | C++ Arduino `AnimatronicEyes.cpp`             |
| Adafruit PCA9685 I2C servo driver           | 6× direct-GPIO PWM (ESP32 hardware timers)    |
| Blocking `time.sleep()` loops               | Non-blocking `millis()` state machine         |
| Terminal / keyboard interaction             | WiFi web UI + ESP-NOW (HeroPropsProtocol)     |
| `random.uniform()` idle wander              | `random()` idle wander                        |

6 servos control both animatronic eyes:

| Channel        | Servo          | Config Pin       |
|----------------|----------------|------------------|
| `CH_LEFT_X`    | Left eye pan   | `PIN_LEFT_X = 25`   |
| `CH_LEFT_Y`    | Left eye tilt  | `PIN_LEFT_Y = 26`   |
| `CH_LEFT_BLINK`| Left eyelid    | `PIN_LEFT_BLINK = 27` |
| `CH_RIGHT_X`   | Right eye pan  | `PIN_RIGHT_X = 14`  |
| `CH_RIGHT_Y`   | Right eye tilt | `PIN_RIGHT_Y = 12`  |
| `CH_RIGHT_BLINK`| Right eyelid  | `PIN_RIGHT_BLINK = 13` |

---

## Hardware

- ESP32 dev board (NodeMCU-32S or similar)
- 6× micro servos (SG90 / MG90S)
- 5 V servo power supply (separate from ESP32 3.3 V rail)
- Logic-level signal wires from ESP32 GPIO to servo signal lines

> **Note:** All servo signal pins support 50 Hz PWM via ESP32's hardware LEDC timers.  
> GPIO 12 must be LOW during boot — this is safe as the ESP32 will be powered before the servo rail.

---

## Wiring Diagram

```
ESP32          Servo
------         -----
GPIO 25  ──►  Left Eye   X (pan)
GPIO 26  ──►  Left Eye   Y (tilt)
GPIO 27  ──►  Left Eye   BLINK (eyelid)
GPIO 14  ──►  Right Eye  X (pan)
GPIO 12  ──►  Right Eye  Y (tilt)
GPIO 13  ──►  Right Eye  BLINK (eyelid)
GND      ──►  All servo GND rails
```

Power the servo 5 V rail from a dedicated BEC / USB power bank — do **not** power servos from the ESP32 3.3 V pin.

---

## Configuration (`src/config.h`)

All travel limits, direction multipliers, and timing constants mirror the
`EyeMovement.py` configuration block:

| Config constant       | Python equivalent      | Default  |
|-----------------------|------------------------|----------|
| `EYE_X_MIN/MAX`       | `X_LIMITS`             | 70 / 110 |
| `EYE_Y_MIN/MAX`       | `Y_LIMITS`             | 70 / 110 |
| `BLINK_OPEN`          | `BLINK_LIMITS[0]`      | 0        |
| `BLINK_CLOSED`        | `BLINK_LIMITS[1]`      | 40       |
| `BLINK_TRIM_LEFT`     | `BLINK_OPEN_LEFT`      | -12      |
| `BLINK_TRIM_RIGHT`    | `BLINK_OPEN_RIGHT`     | 0        |
| `MOVE_STEP_DEG`       | `MOVE_STEP`            | 1°       |
| `MOVE_DELAY_MS`       | `MOVE_DELAY * 1000`    | 10 ms    |
| `BLINK_STEP_MS`       | `BLINK_SPEED * 1000`   | 3 ms     |
| `BLINK_HOLD_MS`       | `BLINK_HOLD * 1000`    | 100 ms   |
| `BLINK_SIDE_STEPS`    | `BLINK_SIDE_DELAY / BLINK_SPEED` | 10 steps |
| `BLINK_INTERVAL_MIN_MS` | `BLINK_INTERVAL[0] * 1000` | 4000 ms |
| `BLINK_INTERVAL_MAX_MS` | `BLINK_INTERVAL[1] * 1000` | 12000 ms|
| `DIR_LEFT_X/Y/BLINK`  | `DIR_LEFT_*`           | +1       |
| `DIR_RIGHT_X`         | `DIR_RIGHT_X`          | +1       |
| `DIR_RIGHT_Y`         | `DIR_RIGHT_Y`          | -1       |
| `DIR_RIGHT_BLINK`     | `DIR_RIGHT_BLINK`      | -1       |

Flip any `DIR_*` to `-1` if a servo is physically mounted in reverse.

---

## Build & Flash

```bash
cd wicker-husband
pio run -e wicker-husband           # compile
pio run -e wicker-husband -t upload # flash via USB
```

OTA update (after first flash):

```bash
pio run -e wicker-husband-ota -t upload
```

---

## Web Control UI

Connect to WiFi **WICKER-NET** (password: `heroprops`), then open  
`http://192.168.4.1` (or `http://wicker-eyes.local`).

Controls:
- **GAZE joystick** — drag to move both eyes in real time
- **BLINK** — trigger a single blink
- **ALERT** — wide eyes, rapid random gaze
- **SLEEPY** — half-closed eyelids
- **IDLE** — resume autonomous random wandering
- **CENTER** — lock eyes to center position

---

## ESP-NOW (HeroPropsProtocol) Commands

When a Commander is on the network, the eyes respond to:

| Message type      | Payload           | Effect                                         |
|-------------------|-------------------|------------------------------------------------|
| `HP_MSG_DRIVE`    | throttle / turn   | Gaze to mapped X/Y position                    |
| `HP_MSG_ACTION`   | actionId 1        | Blink                                          |
| `HP_MSG_ACTION`   | actionId 2        | Alert expression                               |
| `HP_MSG_ACTION`   | actionId 3        | Sleepy expression                              |
| `HP_MSG_ACTION`   | actionId 4        | Resume idle wander                             |
| `HP_MSG_ACTION`   | actionId 5        | Center and stop                                |
| `HP_MSG_ESTOP`    | —                 | Center eyes immediately (safety)               |

Device ID is `5` (set `DEVICE_ID` in `config.h` if you need a different slot).

---

## Credit

Eye movement logic adapted from  
**[mklements/AIChatbot `EyeMovement.py`](https://github.com/mklements/AIChatbot)**  
by Michael Klements — licensed under MIT.
