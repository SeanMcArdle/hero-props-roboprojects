# Squad Bot Receiver Wiring (JL-BN)

## Pinout Configuration (NodeMCU-32S)

| Component | Pin Name | ESP32 GPIO | Description |
|-----------|----------|------------|-------------|
| **Left Drive** | Signal | 25 | Continuous Rotation Servo |
| **Right Drive** | Signal | 26 | Continuous Rotation Servo |
| **Dome** | Signal | 13 | Standard Servo |
| **Neopixel** | Data In | 27 | LED Strip (WS2812B) |
| **RC Throttle** | Signal | 33 | Optional RC Input |
| **RC Steering** | Signal | 32 | Optional RC Input |

> **Note**: Audio hardware has been removed. Sound effects play through the controller iPad speakers.
> **Note**: The `receiver_wiring.svg` file is likely outdated. Please refer to this table.

## Power Distribution
*   **Servos**: Power directly from 5V/6V BEC (Do not power 3 servos from ESP32 5V pin).
*   **ESP32**: Power via USB or Vin (5V).
*   **Common Ground**: Ensure ESP32 GND connects to Battery/BEC GND.
