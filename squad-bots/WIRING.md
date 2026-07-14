# Squad Bot Receiver Wiring (JL-BN)

## Pinout Configuration

The firmware now supports multiple ESP32 board pinouts. Choose the matching target when building:

- `l0-0n` → NodeMCU-32S pinout
- `l0-0n-devkit` → generic ESP32 DevKit / WROOM-32 pinout

### NodeMCU-32S

| Component | Pin Name | ESP32 GPIO | Description |
|-----------|----------|------------|-------------|
| **Left Drive** | Signal | 25 | Continuous Rotation Servo |
| **Right Drive** | Signal | 26 | Continuous Rotation Servo |
| **Dome** | Signal | 13 | Standard Servo |
| **Neopixel** | Data In | 27 | LED Strip (WS2812B) |
| **RC Throttle** | Signal | 33 | Optional RC Input |
| **RC Steering** | Signal | 32 | Optional RC Input |

### Generic ESP32 DevKit / WROOM-32

| Component | Pin Name | ESP32 GPIO | Description |
|-----------|----------|------------|-------------|
| **Left Drive** | Signal | 13 | Continuous Rotation Servo |
| **Right Drive** | Signal | 12 | Continuous Rotation Servo |
| **Dome** | Signal | 14 | Standard Servo |
| **Neopixel** | Data In | 2 | LED Strip (WS2812B) |
| **RC Throttle** | Signal | 34 | Optional RC Input |
| **RC Steering** | Signal | 35 | Optional RC Input |

> **Note**: Audio hardware has been removed. Sound effects play through the controller iPad speakers.
> **Note**: The `receiver_wiring.svg` file is likely outdated. Please refer to this table.

## Power Distribution
*   **Servos**: Power directly from 5V/6V BEC (Do not power 3 servos from ESP32 5V pin).
*   **ESP32**: Power via USB or Vin (5V).
*   **Common Ground**: Ensure ESP32 GND connects to Battery/BEC GND.

## Verified Pin-Test (DevKit)

The `l0-0n-devkit-test` firmware was used to verify a small set of GPIOs on a generic ESP32 DevKit. Observed output (repeating) on the serial monitor:

- `PIN TEST: toggled pin 15 (idx=0)`
- `PIN TEST: toggled pin 2  (idx=1)`
- `PIN TEST: toggled pin 4  (idx=2)`
- `PIN TEST: toggled pin 16 (idx=3)`
- `PIN TEST: toggled pin 17 (idx=4)`

Flash and monitor the test firmware with:

```bash
cd squad-bots
~/.platformio/penv/bin/pio run -e l0-0n-devkit-test -t upload --upload-port /dev/cu.usbserial-2120
~/.platformio/penv/bin/pio device monitor -p /dev/cu.usbserial-2120 -b 115200
```

Use this test to confirm a board's pinout before doing a full student flash.
