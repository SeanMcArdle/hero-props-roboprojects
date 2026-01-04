# Universal Roomba Controller

A generic, open-source ESP32 controller for Roomba-based robotics projects. Designed for the Hero Props education community.

## Features
- **RC Control**: Use any standard hobby RC receiver (PWM) to control the Roomba.
- **Web Dashboard**: Connect via WiFi to view telemetry (Battery, Signal) and control the robot via a virtual joystick.
- **Safety First**: Includes signal timeout protection, startup delays, and emergency stop functionality.
- **Roomba OI**: Communicates with iRobot Roomba 400/500/600 series via the Open Interface (DIN port).

## Hardware Requirements
- **Microcontroller**: ESP32 DevKit V1
- **Robot Base**: iRobot Roomba (with DIN serial port)
- **Power**: Buck converter (Roomba Battery Voltage -> 5V for ESP32)
- **RC Receiver**: Minimum 3 channels (Steering, Throttle, Spin/Aux)

## Pinout
| ESP32 Pin | Function | Description |
|-----------|----------|-------------|
| GPIO 16 | RX | Connect to Roomba TX |
| GPIO 17 | TX | Connect to Roomba RX |
| GPIO 23 | BRC | Baud Rate Change / Wakeup |
| GPIO 33 | RC CH2 | Steering Input |
| GPIO 35 | RC CH3 | Throttle Input |
| GPIO 34 | RC CH4 | Spin/Aux Input |

## Setup
1. Install [PlatformIO](https://platformio.org/) in VS Code.
2. Open this folder in VS Code.
3. Connect your ESP32.
4. Upload the firmware.
5. Connect to the WiFi Access Point: `DROID-BASE` (Password: `heroprops`).
6. Navigate to `http://192.168.4.1` in your browser.

## Usage
- **Arming**: The system waits 3 seconds after startup. You must have a valid RC signal (transmitter on) to arm.
- **Driving**: Use the RC transmitter to drive. If signal is lost, the robot will stop.
- **Web Control**: You can also drive using the on-screen joysticks if RC is idle.

## License
MIT License - Free for educational and community use.
