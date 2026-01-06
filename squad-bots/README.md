# Squad Bots: Omni-Node V1 ("Cosmic Cube")

**Status**: 🟢 **RC2 - Safety Patched** (Jan 6, 2026)  
**Hardware**: NodeMCU-32S (ESP32)  
**Role**: Integrated Droid Controller & Web Server

## 🤖 Architecture: "The Interface is the iPad"
The **Omni-Node** architecture eliminates the need for separate physical remotes. The ESP32 acts as the Brain, the Router, and the Web Server simultaneously.

### Dual-Core System
*   **Core 0 (Web/Comms)**: Handles the AsyncWebServer and WebSocket traffic from the iPad.
*   **Core 1 (Pilot)**: Runs the motor logic, watchdog, and RC interrupts.

## 🛡️ Safety Features (Red Team Verified)
Following the Jan 6 security audit, the following failsafes are active:
1.  **Dead-Man's Switch (Watchdog)**: If the iPad disconnects or WiFi lags for >500ms, motors STOP.
2.  **OTA Interlock**: Updating firmware wirelessly automatically kills motor power before writing to flash.
3.  **Connection Scrub**: Closing the browser tab performs an immediate E-Stop.

## 🎮 Controls (Web Interface)
Connect to WiFi `JL-BN-Net` (Password: `heroprops`) and visit `http://192.168.4.1`:
*   **Left Stick**: Drive (Forward/Turn)
*   **Right Stick**: Dome (Spin/Tilt)
*   **Action Buttons**: Triggers LED animations (Happy/Sad/Dance) - *Audio plays on iPad only*

## 🔌 Wiring (NodeMCU-32S)

| Component | Pin | Notes |
| :--- | :--- | :--- |
| **Left Motor** | GPIO 25 | Continuous Rotation Servo |
| **Right Motor** | GPIO 26 | Continuous Rotation Servo |
| **Dome Servo** | GPIO 13 | Standard Servo (Moved from 27) |
| **Neopixel** | GPIO 27 | Data Line |
| **RC Throttle** | GPIO 33 | Optional Physical RC |
| **RC Steering** | GPIO 32 | Optional Physical RC |
| **RC Spin**     | GPIO 35 | Optional Physical RC |

## 🚀 Prototype Quick Start
1.  **Open Folder**: Open `squad-bots` in VS Code / PlatformIO.
2.  **Upload**: Flash to NodeMCU-32S.
3.  **Verify Safety**:
    *   [ ] Connect iPad and Drive.
    *   [ ] Turn off WiFi on iPad.
    *   [ ] **Verify droid stops in <0.5s**.
