# CHOPPER COMMANDER — GitHub Development Handoff
## Hero Props Droid Control System
**Version:** 3.0  
**Last Updated:** January 1, 2026  
**Author:** Seán McArdle / Hero Props Inc.  
**License:** MIT  

---

# TABLE OF CONTENTS

1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Hardware Specification](#hardware-specification)
4. [Pin Assignments](#pin-assignments)
5. [Wiring Diagram](#wiring-diagram)
6. [Communication Protocols](#communication-protocols)
7. [WebSocket API](#websocket-api)
8. [ESP-NOW Swarm Protocol](#esp-now-swarm-protocol)
9. [Web UI Architecture](#web-ui-architecture)
10. [Build Configuration](#build-configuration)
11. [OTA Update System](#ota-update-system)
12. [Known Issues & Technical Debt](#known-issues--technical-debt)
13. [Future Development Roadmap](#future-development-roadmap)
14. [File Structure](#file-structure)
15. [Development Workflow](#development-workflow)

---

# PROJECT OVERVIEW

## What Is Chopper Commander?

Chopper Commander is an ESP32-based droid control system designed as a "squad commander" for the Hero Props BB-R2 STEM workshop fleet. The droid is based on the Star Wars Rebels character "Chopper" (C1-10P) and serves as both:

1. **A standalone controllable droid** — WiFi-controlled via web interface on any smartphone/tablet
2. **A swarm commander** — Uses ESP-NOW to broadcast commands to multiple BB-R2 droids simultaneously

## Design Philosophy

- **Look cheap, BE sophisticated** — The physical build is intentionally scrappy (educational/workshop context), but the control system is professional-grade
- **Phone-first control** — No dedicated hardware controller; uses the device everyone already has
- **Captive portal UX** — Connect to WiFi, browser auto-opens, immediate control
- **Fail-safe by design** — Watchdog timers, E-STOP, connection loss = motors stop
- **OTA-updateable** — Push firmware updates wirelessly to entire fleet

## Current State (V3)

| Feature | Status | Notes |
|---------|--------|-------|
| Drive motors | ✅ Working | Differential drive via FEETECH controller |
| Dome rotation | ⚠️ Partial | Works but power reduced (stripped horn screw) |
| Front arm | ✅ Working | 0-45° range, reversed direction |
| Left/Right arms | 🔌 Stubbed | Commands sent via ESP-NOW, hardware TBD |
| WiFi AP | ✅ Working | SSID: CHOPPER / Pass: droid123 |
| Web UI | ✅ Working | Touch joystick, dome slider, trim controls |
| ESP-NOW broadcast | ✅ Working | Swarm commands functional |
| OTA updates | ✅ Working | `pio run -t upload --upload-port 192.168.4.1` |
| mDNS | ✅ Working | http://chopper.local |
| Captive portal | ✅ Working | Auto-redirect on WiFi connect |
| E-STOP | ✅ Working | V3 addition with reset capability |
| DFPlayer audio | ❌ Removed | Hardware disconnected, code stubbed |

---

# SYSTEM ARCHITECTURE

```
┌──────────────────────────────────────────────────────────────────┐
│                        USER'S PHONE                               │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │              Web UI (embedded HTML/CSS/JS)                  │  │
│  │  ┌───────────┐  ┌───────────┐  ┌───────────────────────┐  │  │
│  │  │ Joystick  │  │   Dome    │  │   Arm Sliders         │  │  │
│  │  │  (Touch)  │  │  Slider   │  │   Front/Left/Right    │  │  │
│  │  └─────┬─────┘  └─────┬─────┘  └───────────┬───────────┘  │  │
│  │        │              │                    │               │  │
│  │        └──────────────┴────────────────────┘               │  │
│  │                         │                                   │  │
│  │              WebSocket (ws://192.168.4.1/ws)                │  │
│  └─────────────────────────┼──────────────────────────────────┘  │
└────────────────────────────┼─────────────────────────────────────┘
                             │
                             ▼
┌──────────────────────────────────────────────────────────────────┐
│                      ESP32 (CHOPPER BODY)                         │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │                   AsyncWebServer                          │    │
│  │   - Serves HTML UI on GET /                               │    │
│  │   - WebSocket endpoint /ws                                │    │
│  │   - Captive portal redirects                              │    │
│  └────────────────────────┬─────────────────────────────────┘    │
│                           │                                       │
│  ┌────────────────────────┴─────────────────────────────────┐    │
│  │                 Command Parser                            │    │
│  │   DRIVE:x,y → setDrive()       DOME:value → setDomeSpin() │    │
│  │   TRIM:L+   → adjustTrim()     SWARM:cmd  → broadcast()   │    │
│  │   ESTOP     → stopAll()        RESET      → enable()      │    │
│  └────────────────────────┬─────────────────────────────────┘    │
│                           │                                       │
│  ┌───────────┬────────────┼────────────┬───────────┐             │
│  │           │            │            │           │             │
│  ▼           ▼            ▼            ▼           ▼             │
│ GPIO25     GPIO26      GPIO27      GPIO14     ESP-NOW            │
│ Left       Right       Dome        Front      Broadcast          │
│ Motor      Motor       Spin        Arm        FF:FF:FF:FF:FF:FF  │
│  │           │            │            │           │             │
└──┼───────────┼────────────┼────────────┼───────────┼─────────────┘
   │           │            │            │           │
   ▼           ▼            ▼            ▼           ▼
┌──────────────────────┐ ┌──────┐ ┌──────┐  ┌─────────────────────┐
│   FEETECH 2CH        │ │ 360° │ │ 180° │  │   BB-R2 DROIDS      │
│   Motor Controller   │ │Servo │ │Servo │  │   (Squad Members)   │
│   ┌──────┐ ┌──────┐  │ │      │ │      │  │   BK-01 → BK-16     │
│   │  M2  │ │  M1  │  │ └──┬───┘ └──┬───┘  │   Same ESP-NOW      │
│   └──┬───┘ └──┬───┘  │    │        │      │   protocol          │
└──────┼───────┼───────┘    │        │      └─────────────────────┘
       │       │            │        │
       ▼       ▼            ▼        ▼
    ┌─────┐ ┌─────┐     ┌──────┐ ┌──────┐
    │LEFT │ │RIGHT│     │ DOME │ │FRONT │
    │MOTOR│ │MOTOR│     │      │ │ ARM  │
    │DAGU │ │DAGU │     │      │ │      │
    └─────┘ └─────┘     └──────┘ └──────┘
```

---

# HARDWARE SPECIFICATION

## Bill of Materials

| Component | Quantity | Model/Spec | Notes |
|-----------|----------|------------|-------|
| ESP32 Dev Board | 1 | ESP32-WROOM-32 NodeMCU | 38-pin variant |
| Motor Controller | 1 | FEETECH 2CH Servo Driver | Converts PWM to DC motor control |
| DC Motors | 2 | DAGU DG01D 48:1 | Yellow gearmotor, 6V nominal |
| 360° Servo | 1 | FS90R (continuous rotation) | Dome spin |
| 180° Servo | 1 | SG90 or equivalent | Front arm, limited 0-45° |
| Battery Holder | 1 | 4×AA (6V nominal) | Powers entire system |
| Wago Connectors | 2 | 5-port lever connectors | Power distribution |
| Jumper Wires | ~20 | Dupont male-male/female | Various lengths |

## Motor Controller Details

The FEETECH 2CH board converts standard servo-style PWM signals into bidirectional DC motor control:

- **Input:** 3-wire servo cables (GND, VCC, Signal)
- **Output:** 2-wire DC motor connections (reversible polarity)
- **PWM Mapping:**
  - 1500µs = STOP
  - &gt;1500µs = Forward (up to 2000µs)
  - &lt;1500µs = Reverse (down to 1000µs)

---

# PIN ASSIGNMENTS

## ESP32 GPIO Map

```
ESP32 NodeMCU 38-Pin Board
Board Label → Actual GPIO

┌─────────────────────────────────────────┐
│                 USB                      │
├──────────────┬──────────────────────────┤
│     3U3      │      5V     ◄── POWER IN │
│     GND      │      GND    ◄── GROUND   │
│     P15      │      P13                 │
│     P2       │      P12                 │
│     P25 ◄────│──────P14    ◄── FRONT ARM│
│  LEFT MOTOR  │                          │
│     P26 ◄────│──────P32                 │
│  RIGHT MOTOR │                          │
│     P27 ◄────│──────P33                 │
│  DOME SPIN   │                          │
├──────────────┴──────────────────────────┤
│                 ─────                    │
└─────────────────────────────────────────┘
```

## Pin Assignment Table

| GPIO | Function | Type | Signal Range | Notes |
|------|----------|------|--------------|-------|
| 25 | Left Motor | PWM (servo-style) | 1000-2000µs | Via FEETECH CH1 |
| 26 | Right Motor | PWM (servo-style) | 1000-2000µs | Via FEETECH CH2, **inverted** |
| 27 | Dome Spin | PWM (servo-style) | 1000-2000µs | 360° continuous servo |
| 14 | Front Arm | PWM (servo-style) | 0-45° | 180° servo, range limited |
| 16 | DFPlayer TX | Serial TX | — | **STUBBED** (hardware removed) |
| 17 | DFPlayer RX | Serial RX | — | **STUBBED** (hardware removed) |

## Power Distribution

```
4×AA Battery Pack (6V)
         │
         ├──────┬──────────────────────────────────┐
         │      │                                  │
         ▼      ▼                                  ▼
    WAGO POWER  WAGO GROUND                   ESP32 5V Pin
    (Red bus)   (Black bus)                   (Vin tolerance)
         │           │
         ├───────────┼──► FEETECH Left Input (+/-)
         ├───────────┼──► FEETECH Right Input (+/-)
         └───────────┴──► ESP32 GND
```

**Critical Note:** Use the GND pin on the **opposite side** from 5V on the ESP32. Some boards have a GND that doesn't share the same ground plane. This was the "GND GREMLIN" bug that caused early failures.

---

# WIRING DIAGRAM

## ASCII Schematic

```
                    ┌─────────────────┐
                    │   4×AA BATTERY  │
                    │    ┌─┐   ┌─┐    │
                    │    │+│   │-│    │
                    └────┴┬┴───┴┬┴────┘
                          │     │
         ┌────────────────┴─┐ ┌─┴────────────────┐
         │  WAGO POWER (+)  │ │  WAGO GROUND (-) │
         │  ┌─┬─┬─┬─┐       │ │  ┌─┬─┬─┬─┐       │
         │  │1│2│3│4│       │ │  │1│2│3│4│       │
         └──┴┬┴┬┴┬┴┬┴───────┘ └──┴┬┴┬┴┬┴┬┴───────┘
              │ │ │ │              │ │ │ │
              │ │ │ └──────────────│─│─│─┘ (to ESP32 5V)
              │ │ │                │ │ │
              │ │ └────────────────│─│─┘   (to ESP32 GND)
              │ │                  │ │
              │ └──► FEETECH R (+) │ └──► FEETECH R (-)
              │                    │
              └────► FEETECH L (+) └────► FEETECH L (-)


┌─────────────────────────────────────────────────────────────────┐
│                    FEETECH 2CH CONTROLLER                        │
│                                                                  │
│   LEFT INPUT         RIGHT INPUT                                 │
│   ┌─┬─┬─┐           ┌─┬─┬─┐                                     │
│   │-│+│S│           │-│+│S│     S = Signal (PWM from ESP32)     │
│   └┬┴┬┴┬┘           └┬┴┬┴┬┘                                     │
│    │ │ │             │ │ │                                       │
│    │ │ └─────────────│─│─│───────► ESP32 GPIO25 (Left Sig)      │
│    │ │               │ │ └───────► ESP32 GPIO26 (Right Sig)     │
│    │ └── WAGO PWR    │ └── WAGO PWR                             │
│    └──── WAGO GND    └──── WAGO GND                             │
│                                                                  │
│   M2 OUTPUT          M1 OUTPUT                                   │
│   ┌─┬─┐             ┌─┬─┐                                       │
│   │+│-│             │+│-│                                       │
│   └┬┴┬┘             └┬┴┬┘                                       │
│    │ │               │ │                                         │
│    │ └───────────────│─│────────► LEFT MOTOR (DAGU)             │
│    └─────────────────│─┘                                         │
│                      └──────────► RIGHT MOTOR (DAGU)             │
└─────────────────────────────────────────────────────────────────┘


SERVO CONNECTIONS (Direct to ESP32)
─────────────────────────────────────
Dome Spin (360°):   GND ─► ESP32 GND
                    VCC ─► WAGO POWER
                    SIG ─► ESP32 GPIO27

Front Arm (180°):   GND ─► ESP32 GND
                    VCC ─► WAGO POWER
                    SIG ─► ESP32 GPIO14
```

## Wire Color Convention

| Color | Function |
|-------|----------|
| Red | Power (+6V) |
| Black | Ground |
| Orange | Signal (Left Motor, GPIO25) |
| Yellow | Signal (Right Motor, GPIO26) |
| Green | Signal (Dome, GPIO27) |
| Blue | Signal (Front Arm, GPIO14) |

---

# COMMUNICATION PROTOCOLS

## WiFi Access Point Configuration

| Setting | Value |
|---------|-------|
| SSID | `CHOPPER` |
| Password | `droid123` |
| IP Address | `192.168.4.1` |
| Channel | 1 |
| Max Clients | 4 |
| mDNS Hostname | `chopper.local` |

## Connection Flow

```
1. User connects phone to CHOPPER WiFi
2. Captive portal detection triggers
3. Browser redirects to http://192.168.4.1/
4. Web UI loads (embedded HTML)
5. JavaScript opens WebSocket to ws://192.168.4.1/ws
6. Bidirectional control begins
```

---

# WEBSOCKET API

## Connection

- **Endpoint:** `ws://192.168.4.1/ws`
- **Protocol:** Raw text commands (no JSON)
- **Reconnection:** Client-side auto-reconnect after 2 seconds

## Command Reference

### Drive Control

```
DRIVE:x,y
```
- **x:** Turn value (-100 to +100, negative = left, positive = right)
- **y:** Throttle value (-100 to +100, negative = reverse, positive = forward)
- **Example:** `DRIVE:50,75` → Turn right while moving forward

### Dome Control

```
DOME:value
```
- **value:** -100 to +100 (negative = left spin, positive = right spin, 0 = stop)
- **Note:** Slider snaps back to 0 on release (spring-return behavior)
- **Example:** `DOME:-50` → Spin dome left at 50% power

### Arm Control

```
FRONTARM:position
LEFTARM:position
RIGHTARM:position
```
- **position:** 0-45 for front arm, 0-90 for side arms
- **Note:** Left/Right arms are sent via ESP-NOW to dome ESP32 (not implemented on body)
- **Example:** `FRONTARM:30` → Move front arm to 30°

### Trim Adjustment

```
TRIM:motor±
```
- **motor:** `L` (left), `R` (right), `D` (dome), `F` (front arm)
- **±:** `+` or `-`
- **Persistence:** Saved to ESP32 flash memory
- **Example:** `TRIM:L+` → Increase left motor trim by 1

### Emergency Stop

```
ESTOP
```
- Immediately stops all motors and dome
- Disables all controls until reset
- **Response:** `[ESTOP] ALL STOP`

### Reset from E-STOP

```
RESET
```
- Re-enables controls after E-STOP
- **Response:** `[RESET] Controls enabled`

### LED Control (Dome ESP32)

```
LED:pattern
```
- **pattern:** Integer pattern ID
- **Note:** Forwarded via ESP-NOW to dome controller
- **Example:** `LED:3` → Activate LED pattern 3

### Swarm Commands

```
SWARM:command
```
- Broadcast command to all ESP-NOW peers
- **command:** String command name
- **Example:** `SWARM:PARADE` → All droids drive forward

## Server Responses

All responses are plain text, logged to both WebSocket and Serial:

```
Client connected
Trim: L=0, R=0, D=0, F=0
ESTOPPED: false
[DOME] LED: 3
[SWARM] PARADE
```

---

# ESP-NOW SWARM PROTOCOL

## Overview

ESP-NOW enables low-latency peer-to-peer communication between ESP32 devices without requiring a WiFi router. Chopper acts as the broadcast master, sending commands to all BB-R2 droids in range.

## Network Configuration

| Setting | Value |
|---------|-------|
| Protocol | ESP-NOW |
| Channel | 1 (must match WiFi AP channel) |
| Broadcast Address | `FF:FF:FF:FF:FF:FF` |
| Encryption | Disabled |
| Max Peers | 20 (ESP-NOW limit) |

## Command Structures

### Dome Command (19 bytes)

```c
typedef struct dome_command {
  char cmd[16];    // Command string (null-terminated)
  int16_t value;   // Numeric value (-32768 to +32767)
  uint8_t param;   // Optional parameter byte
} dome_command;
```

### Swarm Command (17 bytes)

```c
typedef struct swarm_command {
  char cmd[16];    // Command string (null-terminated)
  uint8_t speed;   // Speed parameter (0-255)
} swarm_command;
```

## Defined Swarm Behaviors

| Command | Description | Implementation Status |
|---------|-------------|----------------------|
| `PARADE` | All droids drive forward at `speed` | Defined |
| `SCATTER` | Random directions | Defined |
| `REGROUP` | All stop, domes center | Defined |
| `THE_WAVE` | Domes rotate in sequence | Defined |
| `SYNC_SPIN` | All domes spin together | Defined |

**Note:** BB-R2 firmware must implement receivers for these commands. The command definitions exist but fleet behavior depends on fleet firmware.

---

# WEB UI ARCHITECTURE

## Embedded HTML Structure

The entire UI is embedded in the firmware as a raw string literal (`R"rawliteral(...)`). This eliminates the need for SPIFFS/LittleFS and ensures instant loading.

## UI Layout (Mobile-First)

```
┌─────────────────────────────────────────────┐
│ 🤖 CHOPPER    [🛑 STOP] [▶ RESET]  Connected │  ← Header
├─────────────────────────────────────────────┤
│                                             │
│  ┌───────────┐            ┌─────────────┐  │
│  │           │            │    Dome     │  │
│  │  DRIVE    │            │   [====●]   │  │
│  │   ◉       │            │   L     R   │  │
│  │           │            │  [⊙ CTR]    │  │
│  │           │            │             │  │
│  └───────────┘            │    Front    │  │
│   X:0  Y:0                │   [====●]   │  │
│  [50%][75%][100%]         │    0   45   │  │
│                           │             │  │
│                           │   L Arm     │  │
│                           │   [====●]   │  │
│                           │             │  │
│                           │   R Arm     │  │
│                           │   [====●]   │  │
│                           └─────────────┘  │
├─────────────────────────────────────────────┤
│ ⚙️ Trim Settings                       [▼] │  ← Collapsible
│   L Motor: [-] 0 [+]    R Motor: [-] 0 [+] │
│   Dome:    [-] 0 [+]    Front:   [-] 0 [+] │
├─────────────────────────────────────────────┤
│ 📊 DEBUG                                    │
│   Drive X: 0    Drive Y: 0    Speed: 75%   │
│   Dome: STOP    Front: 0°                  │
│   L Arm: 0°     R Arm: 0°                  │
├─────────────────────────────────────────────┤
│ > Connected                                 │  ← Console
│ > Trim: L=0, R=0, D=0, F=0                 │
│ > ESTOPPED: false                          │
└─────────────────────────────────────────────┘
```

## CSS Custom Properties (Theme)

```css
:root {
  --orange: #e67e22;       /* Chopper brand color, primary accent */
  --orange-dark: #a55a1a;  /* Joystick knob gradient */
  --green: #2d5a3d;        /* Connected status, dome slider */
  --green-light: #3d7a52;  /* Button highlights */
  --yellow: #b8960b;       /* Secondary accent */
  --red: #c0392b;          /* E-STOP, errors */
  --grey: #5a5a5a;         /* Disabled states */
  --grey-light: #7a7a7a;   /* Secondary text */
  --bg: #1a1a1a;           /* Main background */
  --bg-light: #222;        /* Section background */
  --bg-lighter: #2a2a2a;   /* Elevated elements */
}
```

## JavaScript Architecture

### Touch Joystick Implementation

```javascript
// Joystick zone captures touch events
driveZone.addEventListener('touchstart', (e) => {
  e.preventDefault();
  driveActive = true;
  handleDriveMove(e.touches[0].clientX, e.touches[0].clientY);
});

// Calculate position relative to zone center
function handleDriveMove(clientX, clientY) {
  const rect = driveZone.getBoundingClientRect();
  let x = (clientX - rect.x - rect.width/2) / (rect.width/2);
  let y = (clientY - rect.y - rect.height/2) / (rect.height/2);
  
  // Clamp to unit circle
  const dist = Math.sqrt(x*x + y*y);
  if (dist > 1) { x /= dist; y /= dist; }
  
  // Apply speed multiplier and send
  const driveX = Math.round(x * 100 * speedMultiplier);
  const driveY = Math.round(-y * 100 * speedMultiplier);  // Y inverted
  send('DRIVE:' + driveX + ',' + driveY);
}
```

### WebSocket Management

```javascript
function connect() {
  ws = new WebSocket('ws://' + location.host + '/ws');
  
  ws.onopen = () => {
    connected = true;
    // Update UI status
  };
  
  ws.onclose = () => {
    connected = false;
    setTimeout(connect, 2000);  // Auto-reconnect
  };
  
  ws.onmessage = (e) => {
    // Parse responses, update trim displays, detect ESTOP state
  };
}
```

---

# BUILD CONFIGURATION

## PlatformIO Setup

### platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    madhephaestus/ESP32Servo@^1.2.0
    me-no-dev/AsyncTCP@^1.1.1
    me-no-dev/ESP Async WebServer@^1.2.3
    dfrobot/DFRobotDFPlayerMini@^1.0.6

; OTA settings (default)
upload_protocol = espota
upload_port = 192.168.4.1
upload_flags =
    --port=3232
    --auth=

build_flags =
    -DCORE_DEBUG_LEVEL=0
    -DARDUINO_USB_CDC_ON_BOOT=0
```

### Library Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| ESP32Servo | ^1.2.0 | PWM servo control on any GPIO |
| AsyncTCP | ^1.1.1 | Non-blocking TCP for WebSocket |
| ESP Async WebServer | ^1.2.3 | Async HTTP + WebSocket server |
| DFRobotDFPlayerMini | ^1.0.6 | **UNUSED** (kept for potential future use) |

## Build Commands

```bash
# Compile only
pio run

# Compile and upload via USB
# (Edit platformio.ini: comment OTA lines, uncomment USB lines)
pio run -t upload

# Compile and upload via OTA
# (Connect to CHOPPER WiFi first)
pio run -t upload --upload-port 192.168.4.1

# Serial monitor
pio device monitor
```

---

# OTA UPDATE SYSTEM

## How It Works

ArduinoOTA runs in the main loop, listening on port 3232. When a PlatformIO OTA upload is initiated:

1. Current motor/dome states are stopped (safety)
2. Binary is received and written to OTA partition
3. ESP32 reboots into new firmware
4. Process takes ~15-30 seconds

## OTA Configuration in Code

```cpp
void setup() {
  // ... other setup ...
  
  ArduinoOTA.setHostname("chopper");
  ArduinoOTA.onStart([]() {
    stopMotors();
    setDomeSpin(0);
    Serial.println("[OTA] Starting...");
  });
  ArduinoOTA.onEnd([]() { Serial.println("[OTA] Done!"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) { Serial.println("[OTA] Error"); });
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();  // Must be called frequently
  // ...
}
```

## Fleet OTA Pattern

For updating multiple droids:

```bash
# Update Chopper
pio run -t upload --upload-port 192.168.4.1

# Update BB-R2 BK-01 (connect to its WiFi first)
pio run -t upload --upload-port 192.168.4.1

# Repeat for each droid
```

**Future Enhancement:** Central OTA server that all droids connect to for batch updates.

---

# KNOWN ISSUES & TECHNICAL DEBT

## Hardware Issues

| Issue | Severity | Workaround | Fix Required |
|-------|----------|------------|--------------|
| Dome horn screw stripped | Medium | Reduced MAX_DOME_POWER to 0.35 | Replace horn or use set screw |
| GND GREMLIN | Resolved | Use GND opposite from 5V | Document in wiring guide |
| Motor direction inconsistency | Low | Right motor inverted in code | None (working as designed) |

## Software Issues

| Issue | Severity | Status | Notes |
|-------|----------|--------|-------|
| DFPlayer code present but unused | Low | Stubbed | Remove entirely or implement |
| No servo easing/ramping | Low | Open | Abrupt starts may stress mechanics |
| WebSocket reconnect on mobile | Medium | Working | Test on iOS Safari |
| ESP-NOW + WiFi AP coexistence | Low | Working | Must be same channel |

## Technical Debt

1. **Embedded HTML size** — Current UI is ~15KB; could be compressed or moved to SPIFFS
2. **No authentication** — Anyone in WiFi range can control the droid
3. **No telemetry persistence** — Trim values saved, but no logging/history
4. **Single client assumption** — Multiple simultaneous controllers may conflict
5. **Magic numbers** — Speed limits defined as macros, should be configurable

---

# FUTURE DEVELOPMENT ROADMAP

## Phase 1: Stability (Current)

- [x] Basic drive control
- [x] Dome rotation
- [x] E-STOP/Reset
- [x] OTA updates
- [x] Trim persistence
- [ ] Motor direction swap in UI (no code change needed)
- [ ] Servo easing for smooth starts/stops

## Phase 2: Dome Integration

- [ ] Second ESP32 in dome
- [ ] ESP-NOW communication body ↔ dome
- [ ] LED control (NeoPixel ring)
- [ ] Sound playback (DFPlayer or I2S)
- [ ] Camera mount (ESP32-CAM)

## Phase 3: Swarm Features

- [ ] BB-R2 fleet firmware (receivers for swarm commands)
- [ ] Synchronized behaviors (parade, wave, scatter)
- [ ] Voice control via Web Speech API
- [ ] POV camera streaming from Chopper dome

## Phase 4: Polish

- [ ] UI themes (Rebel, Empire, Mandalorian)
- [ ] Sound effects library
- [ ] Pre-programmed routines
- [ ] Web-based firmware update (no PlatformIO needed)
- [ ] Authentication/pairing

---

# FILE STRUCTURE

## Repository Layout

```
chopper-commander/
├── src/
│   └── main.cpp              # Main firmware (chopper_main_v3.cpp)
├── include/
│   └── (empty, all in main.cpp)
├── lib/
│   └── (empty, using PlatformIO lib_deps)
├── docs/
│   ├── CHOPPER_COMMANDER_HANDOFF.md    # This document
│   ├── wiring_diagram.html              # Visual wiring reference
│   └── images/
│       ├── chopper_photo.jpg
│       └── wiring_photo.jpg
├── platformio.ini
├── README.md
└── LICENSE
```

## Key Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | All firmware code (single-file architecture) |
| `platformio.ini` | Build configuration, library deps, OTA settings |
| `docs/wiring_diagram.html` | Interactive wiring reference |
| `CHOPPER_COMMANDER_HANDOFF.md` | This comprehensive documentation |

---

# DEVELOPMENT WORKFLOW

## Initial Setup

```bash
# Clone repository
git clone https://github.com/heroprops/chopper-commander.git
cd chopper-commander

# Install PlatformIO Core (if not using VSCode extension)
pip install platformio

# Build to verify environment
pio run
```

## Making Changes

1. **Edit `src/main.cpp`** — All code is in one file for simplicity
2. **Test compile:** `pio run`
3. **Flash via USB** (first time or after WiFi changes):
   - Edit `platformio.ini` to use USB upload
   - `pio run -t upload`
4. **Flash via OTA** (normal development):
   - Connect to CHOPPER WiFi
   - `pio run -t upload --upload-port 192.168.4.1`
5. **Monitor serial:** `pio device monitor`

## Debugging Tips

- **Motors spin on boot:** Ensure `digitalWrite(PIN, LOW)` before `servo.attach()`
- **Can't connect to WiFi:** Check channel 1, verify SSID/password
- **OTA fails:** Ensure you're connected to CHOPPER WiFi, not home network
- **Servos jitter:** Check power supply; 4×AA may be low
- **WebSocket disconnects:** Check `ws.cleanupClients()` is in loop

## Code Style

- **Single file:** Keep all code in `main.cpp` unless splitting is necessary
- **Comments:** Use `// ============== SECTION ==============` dividers
- **Constants:** Use `#define` for hardware configs, `const` for runtime values
- **Functions:** Declare prototypes at top, implement after `getHTML()`

---

# APPENDIX: VERSION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| V1 | Dec 2025 | Initial development, basic drive |
| V2 | Dec 2025 | Added ESP-NOW, dome control, trim |
| V3 | Dec 2025 | Removed DFPlayer, added E-STOP, reduced dome power |

---

# APPENDIX: REFERENCE LINKS

- **ESP32Servo Library:** https://github.com/madhephaestus/ESP32Servo
- **ESPAsyncWebServer:** https://github.com/me-no-dev/ESPAsyncWebServer
- **ESP-NOW Documentation:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
- **PlatformIO ESP32:** https://docs.platformio.org/en/latest/platforms/espressif32.html
- **Matt Zwarts Chopper STLs:** (Patreon/Thingiverse — verify license)
- **Michael Baddeley BB Astromech:** (Patreon — curriculum resource)

---

# APPENDIX: CONTACT

**Project Lead:** Seán McArdle  
**Organization:** Hero Props Inc.  
**Email:** sean@heroprops.art  
**GitHub:** github.com/SeanMcArdle  

---

*Document generated: January 1, 2026*  
*For GitHub Copilot development assistance*
