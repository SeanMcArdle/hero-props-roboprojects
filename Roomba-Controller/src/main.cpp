/*
 * HERO PROPS - UNIVERSAL ROOMBA CONTROLLER
 * 
 * A generic ESP32 controller for Roomba-based robotics projects.
 * Features:
 *   - RC Control via PWM inputs (Steering, Throttle, Spin)
 *   - Web Dashboard for telemetry and control
 *   - Roomba Open Interface (OI) communication
 *   - Safety features: E-Stop, Signal Timeout, Startup Delay
 * 
 * Hardware:
 *   - ESP32 DevKit
 *   - iRobot Roomba (400/500/600 series)
 *   - RC Receiver (3+ channels)
 *   - Buck Converter (Roomba Battery -> 5V)
 * 
 * License: MIT
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <HardwareSerial.h>

// ============== CONFIGURATION ==============

const char* AP_SSID = "DROID-BASE";
const char* AP_PASSWORD = "heroprops";

// Pin Definitions
#define ROOMBA_RX_PIN 16
#define ROOMBA_TX_PIN 17
#define ROOMBA_BRC_PIN 23

#define RC_STEERING_PIN 33
#define RC_THROTTLE_PIN 35
#define RC_SPIN_PIN 34

// RC Settings
#define RC_DEADZONE 70
#define STEERING_CENTER 1500
#define STEERING_MIN 1100
#define STEERING_MAX 1900
#define THROTTLE_CENTER 1500
#define THROTTLE_MIN 1100
#define THROTTLE_MAX 1900
#define SPIN_CENTER 1500
#define SPIN_MIN 1100
#define SPIN_MAX 1900

// Roomba Speed Limits (mm/s)
#define MAX_SPEED 250
#define SPIN_SPEED 150

// Safety Timings
#define SIGNAL_TIMEOUT 250
#define RC_MIN_VALID 900
#define RC_MAX_VALID 2100
#define STARTUP_DELAY 3000
#define WEB_JOYSTICK_TIMEOUT 200

// Ramping
#define ACCEL_RAMP_RATE 15
#define DECEL_RAMP_RATE 25
#define EMERGENCY_DECEL_RATE 50

// Battery Monitoring
#define BATTERY_VOLTAGE_MIN 10000  // 10V in mV
#define BATTERY_VOLTAGE_MAX 20000  // 20V in mV
#define BATTERY_CHECK_INTERVAL 5000

// ============== GLOBALS ==============

WebServer server(80);
WebSocketsServer webSocket(81);
HardwareSerial RoombaSerial(2);

// RC Pulse Tracking
volatile unsigned long steeringPulseStart = 0;
volatile unsigned long steeringPulseWidth = 1500;
volatile unsigned long throttlePulseStart = 0;
volatile unsigned long throttlePulseWidth = 1500;
volatile unsigned long spinPulseStart = 0;
volatile unsigned long spinPulseWidth = 1500;

// State Tracking
unsigned long lastValidSignal = 0;
unsigned long lastStatusPrint = 0;
unsigned long lastControlUpdate = 0;
unsigned long lastBatteryCheck = 0;
unsigned long startupTime = 0;

bool roombaReady = false;
bool rcActive = false;
bool systemArmed = false;
bool emergencyStopped = false;
bool emergencyDecelActive = false;

// Web Control State
bool webJoystickActive = false;
unsigned long lastWebJoystickUpdate = 0;
float webSpinInput = 0;
float webThrottleInput = 0;
float webSteerInput = 0;

// Motor State
int currentLeftSpeed = 0;
int currentRightSpeed = 0;
int targetLeftSpeed = 0;
int targetRightSpeed = 0;

// Battery State
int batteryPercent = 0;
int batteryVoltage = 0;

// ============== INTERRUPTS ==============

void IRAM_ATTR steeringInterrupt() {
    if (digitalRead(RC_STEERING_PIN) == HIGH) {
        steeringPulseStart = micros();
    } else {
        steeringPulseWidth = micros() - steeringPulseStart;
    }
}

void IRAM_ATTR throttleInterrupt() {
    if (digitalRead(RC_THROTTLE_PIN) == HIGH) {
        throttlePulseStart = micros();
    } else {
        throttlePulseWidth = micros() - throttlePulseStart;
    }
}

void IRAM_ATTR spinInterrupt() {
    if (digitalRead(RC_SPIN_PIN) == HIGH) {
        spinPulseStart = micros();
    } else {
        spinPulseWidth = micros() - spinPulseStart;
    }
}

// ============== UTILITIES ==============

void logMessage(String msg) {
    Serial.println(msg);
    webSocket.broadcastTXT(msg);
}

bool isSignalValid(unsigned long pulseWidth) {
    return (pulseWidth >= RC_MIN_VALID && pulseWidth <= RC_MAX_VALID);
}

// ============== ROOMBA CONTROL ==============

void wakeRoomba() {
    logMessage("[ROOMBA] Waking up...");
    pinMode(ROOMBA_BRC_PIN, OUTPUT);
    for (int i = 0; i < 3; i++) {
        digitalWrite(ROOMBA_BRC_PIN, LOW);
        delay(100);
        digitalWrite(ROOMBA_BRC_PIN, HIGH);
        delay(100);
    }
    delay(500);
    logMessage("[ROOMBA] Awake");
}

void disableAllMotors() {
    RoombaSerial.write(138);
    RoombaSerial.write(0);
}

void ensureVacuumOff() {
    RoombaSerial.write(138);
    RoombaSerial.write(0);
}

void startRoombaOI() {
    logMessage("[ROOMBA] Starting OI...");
    RoombaSerial.write(128); // Start
    delay(100);
    logMessage("[ROOMBA] Safe Mode...");
    RoombaSerial.write(131); // Safe Mode
    delay(100);
    disableAllMotors();
    roombaReady = true;
    logMessage("[ROOMBA] Ready");
}

void roombaBeep() {
    RoombaSerial.write(140);
    RoombaSerial.write(0);
    RoombaSerial.write(1);
    RoombaSerial.write(72);
    RoombaSerial.write(16);
    delay(50);
    RoombaSerial.write(141);
    RoombaSerial.write(0);
}

void stopRoomba() {
    if (!roombaReady) return;
    emergencyDecelActive = true;
    targetLeftSpeed = 0;
    targetRightSpeed = 0;
    ensureVacuumOff();
}

void driveWheels(int rightSpeed, int leftSpeed) {
    rightSpeed = constrain(rightSpeed, -500, 500);
    leftSpeed = constrain(leftSpeed, -500, 500);
    RoombaSerial.write(145); // Drive Direct
    RoombaSerial.write((rightSpeed >> 8) & 0xFF);
    RoombaSerial.write(rightSpeed & 0xFF);
    RoombaSerial.write((leftSpeed >> 8) & 0xFF);
    RoombaSerial.write(leftSpeed & 0xFF);
}

void requestBatteryData() {
    if (!roombaReady) return;
    
    while (RoombaSerial.available()) {
        RoombaSerial.read();
    }
    
    RoombaSerial.write(142); // Sensors
    RoombaSerial.write(3);   // Packet ID 3 (Battery Charge, Capacity, Voltage, Current, Temp, Charge)
    delay(100);
    
    if (RoombaSerial.available() >= 10) {
        byte data[10];
        for (int i = 0; i < 10; i++) {
            data[i] = RoombaSerial.read();
        }
        
        int newVoltage = (data[1] << 8) | data[2];
        int charge = (data[6] << 8) | data[7];
        int capacity = (data[8] << 8) | data[9];
        
        if (newVoltage >= BATTERY_VOLTAGE_MIN && newVoltage <= BATTERY_VOLTAGE_MAX && capacity > 0) {
            batteryVoltage = newVoltage;
            batteryPercent = (charge * 100) / capacity;
            batteryPercent = constrain(batteryPercent, 0, 100);
        }
    }
}

void resetSystem() {
    logMessage("[SYSTEM] Resetting...");
    
    systemArmed = false;
    emergencyStopped = false;
    emergencyDecelActive = false;
    
    currentLeftSpeed = 0;
    currentRightSpeed = 0;
    targetLeftSpeed = 0;
    targetRightSpeed = 0;
    driveWheels(0, 0);
    ensureVacuumOff();
    
    RoombaSerial.write(173); // Stop OI
    delay(100);
    
    wakeRoomba();
    startRoombaOI();
    
    startupTime = millis();
    roombaBeep();
    
    logMessage("[SYSTEM] Reset complete - waiting to arm");
}

// ============== CONTROL LOGIC ==============

int rampSpeed(int current, int target, int accelRate, int decelRate) {
    int diff = target - current;
    if (diff > 0) {
        return current + min(diff, accelRate);
    } else if (diff < 0) {
        return current - min(-diff, decelRate);
    }
    return target;
}

int mapThrottle(int rcValue) {
    if (abs(rcValue - THROTTLE_CENTER) < RC_DEADZONE) return 0;
    int speed;
    if (rcValue > THROTTLE_CENTER + RC_DEADZONE) {
        speed = map(rcValue, THROTTLE_CENTER + RC_DEADZONE, THROTTLE_MAX, 0, MAX_SPEED);
    } else {
        speed = map(rcValue, THROTTLE_MIN, THROTTLE_CENTER - RC_DEADZONE, -MAX_SPEED, 0);
    }
    return constrain(speed, -MAX_SPEED, MAX_SPEED);
}

int mapSpin(int rcValue) {
    if (abs(rcValue - SPIN_CENTER) < RC_DEADZONE) return 0;
    int speed;
    if (rcValue > SPIN_CENTER + RC_DEADZONE) {
        speed = map(rcValue, SPIN_CENTER + RC_DEADZONE, SPIN_MAX, 0, SPIN_SPEED);
    } else {
        speed = map(rcValue, SPIN_MIN, SPIN_CENTER - RC_DEADZONE, -SPIN_SPEED, 0);
    }
    return constrain(speed, -SPIN_SPEED, SPIN_SPEED);
}

void checkArming() {
    unsigned long now = millis();
    
    if (emergencyStopped) return;
    
    if (!systemArmed) {
        if (now - startupTime >= STARTUP_DELAY) {
            if (isSignalValid(throttlePulseWidth) && isSignalValid(spinPulseWidth)) {
                systemArmed = true;
                logMessage("[ARM] SYSTEM ARMED");
                roombaBeep();
            }
        }
    } else {
        if (!isSignalValid(throttlePulseWidth) || !isSignalValid(spinPulseWidth)) {
            if (now - lastValidSignal > SIGNAL_TIMEOUT) {
                systemArmed = false;
                rcActive = false;
                logMessage("[ARM] DISARMED - Signal lost");
                stopRoomba();
            }
        }
    }
}

void processControl() {
    unsigned long now = millis();
    int leftSpeed = 0;
    int rightSpeed = 0;
    bool shouldStop = false;
    
    if (!systemArmed || emergencyStopped) {
        shouldStop = true;
    } else {
        unsigned long steer = steeringPulseWidth;
        unsigned long throttle = throttlePulseWidth;
        unsigned long spin = spinPulseWidth;
        
        if (!isSignalValid(throttle) || !isSignalValid(spin)) {
            if (now - lastValidSignal > SIGNAL_TIMEOUT) {
                if (rcActive) {
                    logMessage("[RC] Signal lost - STOPPING");
                    rcActive = false;
                }
                shouldStop = true;
                emergencyDecelActive = true;
            }
        } else {
            lastValidSignal = now;
            rcActive = true;
            
            int throttleInput = mapThrottle(throttle);
            int spinInput = mapSpin(spin);
            
            bool rcHasInput = (throttleInput != 0 || spinInput != 0);
            bool webActive = webJoystickActive && (now - lastWebJoystickUpdate < WEB_JOYSTICK_TIMEOUT);
            
            if (rcHasInput) {
                if (spinInput != 0) {
                    leftSpeed = spinInput;
                    rightSpeed = -spinInput;
                } else if (throttleInput != 0) {
                    leftSpeed = throttleInput;
                    rightSpeed = throttleInput;
                    
                    if (isSignalValid(steer)) {
                        int steerOffset = steer - STEERING_CENTER;
                        if (abs(steerOffset) > RC_DEADZONE) {
                            int reduction;
                            if (steerOffset > 0) {
                                reduction = map(steerOffset, RC_DEADZONE, STEERING_MAX - STEERING_CENTER, 0, 100);
                                reduction = constrain(reduction, 0, 100);
                                leftSpeed = throttleInput * (100 - reduction) / 100;
                            } else {
                                reduction = map(-steerOffset, RC_DEADZONE, STEERING_CENTER - STEERING_MIN, 0, 100);
                                reduction = constrain(reduction, 0, 100);
                                rightSpeed = throttleInput * (100 - reduction) / 100;
                            }
                        }
                    }
                }
            } else if (webActive) {
                if (webSpinInput != 0) {
                    int webSpin = (int)(webSpinInput * SPIN_SPEED);
                    leftSpeed = webSpin;
                    rightSpeed = -webSpin;
                } else if (webThrottleInput != 0) {
                    int webThrottle = (int)(webThrottleInput * MAX_SPEED);
                    leftSpeed = webThrottle;
                    rightSpeed = webThrottle;
                    
                    if (webSteerInput != 0) {
                        int reduction = (int)(abs(webSteerInput) * 100);
                        reduction = constrain(reduction, 0, 100);
                        if (webSteerInput > 0) {
                            leftSpeed = webThrottle * (100 - reduction) / 100;
                        } else {
                            rightSpeed = webThrottle * (100 - reduction) / 100;
                        }
                    }
                }
            }
        }
    }
    
    if (shouldStop || (leftSpeed == 0 && rightSpeed == 0)) {
        targetLeftSpeed = 0;
        targetRightSpeed = 0;
    } else {
        targetLeftSpeed = leftSpeed;
        targetRightSpeed = rightSpeed;
    }
    
    int accelRate = ACCEL_RAMP_RATE;
    int decelRate = emergencyDecelActive ? EMERGENCY_DECEL_RATE : DECEL_RAMP_RATE;
    
    currentLeftSpeed = rampSpeed(currentLeftSpeed, targetLeftSpeed, accelRate, decelRate);
    currentRightSpeed = rampSpeed(currentRightSpeed, targetRightSpeed, accelRate, decelRate);
    
    if (emergencyDecelActive && currentLeftSpeed == 0 && currentRightSpeed == 0) {
        emergencyDecelActive = false;
    }
    
    driveWheels(currentRightSpeed, currentLeftSpeed);
    
    if (shouldStop || (leftSpeed == 0 && rightSpeed == 0)) {
        ensureVacuumOff();
    }
}

// ============== WEB INTERFACE ==============

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Droid Base</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: monospace;
            background: #0a0a0a;
            color: #00ff41;
            padding: 8px;
            font-size: 14px;
        }
        .header {
            text-align: center;
            margin-bottom: 10px;
        }
        .header h1 {
            font-size: 18px;
            color: #00ff41;
            letter-spacing: 3px;
            margin-bottom: 2px;
        }
        .header .sub {
            font-size: 11px;
            color: #006620;
            letter-spacing: 1px;
        }
        .top-row {
            display: flex;
            gap: 8px;
            margin-bottom: 8px;
        }
        .section {
            background: #0a0a0a;
            border: 1px solid #00ff41;
            padding: 8px;
        }
        .section.flex1 { flex: 1; }
        .section-title {
            color: #00ff41;
            font-size: 12px;
            margin-bottom: 6px;
            border-bottom: 1px solid #003300;
            padding-bottom: 3px;
            letter-spacing: 1px;
        }
        .row {
            display: flex;
            justify-content: space-between;
            padding: 3px 0;
            font-size: 13px;
        }
        .label { color: #008830; }
        .value { color: #00ff41; font-weight: bold; }
        .value.bad { color: #ff4444; }
        .value.good { color: #00ff41; }
        .buttons {
            display: flex;
            gap: 6px;
            margin-top: 6px;
        }
        .btn {
            flex: 1;
            padding: 8px 4px;
            font-size: 12px;
            font-family: monospace;
            border: 1px solid #00ff41;
            background: #0a0a0a;
            color: #00ff41;
            cursor: pointer;
        }
        .btn:active {
            background: #00ff41;
            color: #000;
        }
        .btn.stop {
            border-color: #ff4444;
            color: #ff4444;
        }
        .btn.stop:active {
            background: #ff4444;
            color: #000;
        }
        .btn.reset {
            border-color: #ffcc00;
            color: #ffcc00;
        }
        .btn.reset:active {
            background: #ffcc00;
            color: #000;
        }
        .motors {
            display: flex;
            justify-content: space-around;
            padding: 8px 0;
        }
        .motor { text-align: center; }
        .motor-value {
            font-size: 28px;
            font-weight: bold;
            color: #00ff41;
        }
        .motor-label {
            font-size: 11px;
            color: #008830;
        }
        .steer-row {
            display: flex;
            justify-content: space-between;
            padding: 4px 0;
            border-top: 1px solid #003300;
            margin-top: 4px;
        }
        .channels {
            display: flex;
            gap: 6px;
        }
        .channel {
            flex: 1;
            background: #051505;
            border: 1px solid #004400;
            padding: 6px;
            text-align: center;
        }
        .channel-name {
            font-size: 10px;
            color: #006620;
            margin-bottom: 2px;
        }
        .channel-value {
            font-size: 18px;
            font-weight: bold;
            color: #00ff41;
        }
        .battery-bar {
            width: 100%;
            height: 16px;
            background: #050505;
            border: 1px solid #00ff41;
            margin-top: 6px;
        }
        .battery-fill {
            height: 100%;
            background: #00ff41;
            transition: width 0.5s;
        }
        .log {
            background: #050505;
            border: 1px solid #003300;
            height: 80px;
            overflow-y: auto;
            padding: 4px;
            font-size: 11px;
            color: #008830;
        }
        .joystick-container {
            display: flex;
            justify-content: space-around;
            padding: 10px 0;
        }
        .joystick-wrapper {
            text-align: center;
        }
        .joystick-label {
            font-size: 11px;
            color: #008830;
            margin-bottom: 4px;
        }
        .joystick {
            width: 120px;
            height: 120px;
            border-radius: 50%;
            background: #051505;
            border: 2px solid #003300;
            position: relative;
            touch-action: none;
        }
        .joystick-knob {
            width: 40px;
            height: 40px;
            border-radius: 50%;
            background: #00ff41;
            border: 2px solid #003300;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            cursor: pointer;
        }
        .rc-warning {
            text-align: center;
            color: #ff4444;
            font-size: 10px;
            margin-top: 6px;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>DROID BASE CONTROLLER</h1>
        <div class="sub">UNIVERSAL ROOMBA INTERFACE v1.0</div>
    </div>
    
    <div class="top-row">
        <div class="section flex1">
            <div class="section-title">SYSTEM STATUS</div>
            <div class="row">
                <span class="label">ARMED</span>
                <span class="value" id="armed">---</span>
            </div>
            <div class="row">
                <span class="label">RC SIGNAL</span>
                <span class="value" id="signal">---</span>
            </div>
            <div class="row">
                <span class="label">ROOMBA</span>
                <span class="value" id="bump">---</span>
            </div>
            <div class="buttons">
                <button class="btn" onclick="sendCmd('beep')">TEST</button>
                <button class="btn reset" onclick="sendCmd('reset')">RESET</button>
                <button class="btn stop" onclick="sendCmd('stop')">E-STOP</button>
            </div>
        </div>
        
        <div class="section flex1">
            <div class="section-title">MOTOR OUTPUT</div>
            <div class="motors">
                <div class="motor">
                    <div class="motor-value" id="left">0</div>
                    <div class="motor-label">LEFT</div>
                </div>
                <div class="motor">
                    <div class="motor-value" id="right">0</div>
                    <div class="motor-label">RIGHT</div>
                </div>
            </div>
            <div class="steer-row">
                <span class="label">STEERING</span>
                <span class="value" id="steer">0</span>
            </div>
        </div>
        
        <div class="section flex1">
            <div class="section-title">POWER CELL</div>
            <div class="row">
                <span class="label">CHARGE</span>
                <span class="value" id="batPct">---%</span>
            </div>
            <div class="row">
                <span class="label">VOLTAGE</span>
                <span class="value" id="batV">---V</span>
            </div>
            <div class="battery-bar">
                <div class="battery-fill" id="batBar" style="width: 0%"></div>
            </div>
        </div>
    </div>
    
    <div class="section">
        <div class="section-title">RC INPUT CHANNELS</div>
        <div class="channels">
            <div class="channel">
                <div class="channel-name">CH2 (D33)</div>
                <div class="channel-value" id="ch2">----</div>
            </div>
            <div class="channel">
                <div class="channel-name">CH3 (D35)</div>
                <div class="channel-value" id="throttle">----</div>
            </div>
            <div class="channel">
                <div class="channel-name">CH4 (D34)</div>
                <div class="channel-value" id="spin">----</div>
            </div>
        </div>
    </div>
    
    <div class="section">
        <div class="section-title">MANUAL OVERRIDE</div>
        <div class="joystick-container">
            <div class="joystick-wrapper">
                <div class="joystick-label">SPIN</div>
                <div class="joystick" id="joyLeft">
                    <div class="joystick-knob" id="knobLeft"></div>
                </div>
            </div>
            <div class="joystick-wrapper">
                <div class="joystick-label">DRIVE</div>
                <div class="joystick" id="joyRight">
                    <div class="joystick-knob" id="knobRight"></div>
                </div>
            </div>
        </div>
        <div class="rc-warning">!! RC INPUT TAKES PRIORITY WHEN ACTIVE !!</div>
    </div>
    
    <div class="section">
        <div class="section-title">SYSTEM LOG</div>
        <div class="log" id="log"></div>
    </div>
    
    <script>
        var ws;
        var logDiv = document.getElementById('log');
        
        function connect() {
            ws = new WebSocket('ws://' + window.location.hostname + ':81/');
            ws.onopen = function() { addLog('Connected'); };
            ws.onmessage = function(e) {
                var msg = e.data;
                if (msg.startsWith('{')) {
                    try {
                        var d = JSON.parse(msg);
                        document.getElementById('armed').textContent = d.armed ? 'ARMED' : 'DISARMED';
                        document.getElementById('armed').className = 'value ' + (d.armed ? 'good' : 'bad');
                        document.getElementById('signal').textContent = d.signal ? 'OK' : 'LOST';
                        document.getElementById('signal').className = 'value ' + (d.signal ? 'good' : 'bad');
                        document.getElementById('bump').textContent = 'CONNECTED';
                        document.getElementById('bump').className = 'value good';
                        document.getElementById('ch2').textContent = d.steer;
                        document.getElementById('steer').textContent = d.steer - 1500;
                        document.getElementById('throttle').textContent = d.throttle;
                        document.getElementById('spin').textContent = d.spin;
                        document.getElementById('left').textContent = d.leftSpeed;
                        document.getElementById('right').textContent = d.rightSpeed;
                        document.getElementById('batPct').textContent = d.battery + '%';
                        document.getElementById('batV').textContent = (d.voltage / 1000).toFixed(1) + 'V';
                        document.getElementById('batBar').style.width = d.battery + '%';
                    } catch(err) { addLog(msg); }
                } else { addLog(msg); }
            };
            ws.onclose = function() {
                addLog('Disconnected - reconnecting...');
                setTimeout(connect, 2000);
            };
        }
        
        function addLog(text) {
            var line = document.createElement('div');
            line.textContent = '> ' + text;
            logDiv.appendChild(line);
            logDiv.scrollTop = logDiv.scrollHeight;
            while (logDiv.children.length > 50) {
                logDiv.removeChild(logDiv.firstChild);
            }
        }
        
        function sendCmd(cmd) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(cmd);
                addLog('Sent: ' + cmd);
            }
        }
        
        function initJoystick(joyId, knobId, isLeftJoy) {
            var joy = document.getElementById(joyId);
            var knob = document.getElementById(knobId);
            var active = false;
            var centerX = 60, centerY = 60;
            var maxDist = 40;
            var lastSendTime = 0;
            var sendInterval = 50;
            
            function getPos(e) {
                var rect = joy.getBoundingClientRect();
                var clientX, clientY;
                if (e.touches) {
                    clientX = e.touches[0].clientX;
                    clientY = e.touches[0].clientY;
                } else {
                    clientX = e.clientX;
                    clientY = e.clientY;
                }
                return {
                    x: clientX - rect.left - centerX,
                    y: clientY - rect.top - centerY
                };
            }
            
            function updateKnob(dx, dy) {
                var dist = Math.sqrt(dx*dx + dy*dy);
                if (dist > maxDist) {
                    dx = dx / dist * maxDist;
                    dy = dy / dist * maxDist;
                }
                knob.style.left = (centerX + dx) + 'px';
                knob.style.top = (centerY + dy) + 'px';
                
                var now = Date.now();
                if (now - lastSendTime >= sendInterval) {
                    lastSendTime = now;
                    var normX = dx / maxDist;
                    var normY = -dy / maxDist;
                    if (isLeftJoy) {
                        sendJoy('left', normX, 0);
                    } else {
                        sendJoy('right', normX, normY);
                    }
                }
            }
            
            function sendJoy(side, x, y) {
                if (ws && ws.readyState === WebSocket.OPEN) {
                    var msg;
                    if (side === 'left') {
                        msg = '{"joy":"left","x":' + x.toFixed(2) + '}';
                    } else {
                        msg = '{"joy":"right","x":' + x.toFixed(2) + ',"y":' + y.toFixed(2) + '}';
                    }
                    ws.send(msg);
                }
            }
            
            function resetKnob() {
                knob.style.left = centerX + 'px';
                knob.style.top = centerY + 'px';
                if (isLeftJoy) {
                    sendJoy('left', 0, 0);
                } else {
                    sendJoy('right', 0, 0);
                }
            }
            
            function onStart(e) {
                e.preventDefault();
                active = true;
                var pos = getPos(e);
                updateKnob(pos.x, pos.y);
            }
            
            function onMove(e) {
                if (!active) return;
                e.preventDefault();
                var pos = getPos(e);
                updateKnob(pos.x, pos.y);
            }
            
            function onEnd(e) {
                if (!active) return;
                active = false;
                resetKnob();
            }
            
            joy.addEventListener('mousedown', onStart);
            joy.addEventListener('touchstart', onStart);
            document.addEventListener('mousemove', onMove);
            document.addEventListener('touchmove', onMove);
            document.addEventListener('mouseup', onEnd);
            document.addEventListener('touchend', onEnd);
            
            resetKnob();
        }
        
        initJoystick('joyLeft', 'knobLeft', true);
        initJoystick('joyRight', 'knobRight', false);
        
        connect();
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", HTML_PAGE);
}

float parseJsonFloat(const String& json, const char* key) {
    String search = String("\"") + key + "\":";
    int idx = json.indexOf(search);
    if (idx < 0) return 0;
    idx += search.length();
    while (idx < json.length() && json.charAt(idx) == ' ') idx++;
    int endIdx = json.indexOf(',', idx);
    if (endIdx < 0) endIdx = json.indexOf('}', idx);
    if (endIdx < 0) return 0;
    return json.substring(idx, endIdx).toFloat();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            logMessage("[WEB] Client disconnected");
            break;
        case WStype_CONNECTED:
            logMessage("[WEB] Client connected");
            break;
        case WStype_TEXT:
            {
                String cmd = String((char*)payload);
                if (cmd.startsWith("{\"joy\":\"left\"")) {
                    webSpinInput = parseJsonFloat(cmd, "x");
                    webJoystickActive = true;
                    lastWebJoystickUpdate = millis();
                } else if (cmd.startsWith("{\"joy\":\"right\"")) {
                    webThrottleInput = parseJsonFloat(cmd, "y");
                    webSteerInput = parseJsonFloat(cmd, "x");
                    webJoystickActive = true;
                    lastWebJoystickUpdate = millis();
                } else if (cmd == "beep") {
                    logMessage("[WEB] Test beep");
                    roombaBeep();
                } else if (cmd == "stop") {
                    logMessage("[WEB] EMERGENCY STOP");
                    emergencyStopped = true;
                    systemArmed = false;
                    stopRoomba();
                } else if (cmd == "reset") {
                    resetSystem();
                }
            }
            break;
    }
}

void sendStatusUpdate() {
    unsigned long steer = steeringPulseWidth;
    unsigned long throttle = throttlePulseWidth;
    unsigned long spin = spinPulseWidth;
    
    String json = "{";
    json += "\"armed\":" + String(systemArmed ? "true" : "false") + ",";
    json += "\"emergency\":" + String(emergencyStopped ? "true" : "false") + ",";
    json += "\"signal\":" + String(isSignalValid(throttle) && isSignalValid(spin) ? "true" : "false") + ",";
    json += "\"bump\":false,";
    json += "\"steer\":" + String(steer) + ",";
    json += "\"throttle\":" + String(throttle) + ",";
    json += "\"spin\":" + String(spin) + ",";
    json += "\"leftSpeed\":" + String(currentLeftSpeed) + ",";
    json += "\"rightSpeed\":" + String(currentRightSpeed) + ",";
    json += "\"battery\":" + String(batteryPercent) + ",";
    json += "\"voltage\":" + String(batteryVoltage);
    json += "}";
    
    webSocket.broadcastTXT(json);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("================================");
    Serial.println("  HERO PROPS - DROID BASE");
    Serial.println("  Universal Roomba Controller");
    Serial.println("================================");
    Serial.println();
    
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("[WIFI] SSID: ");
    Serial.println(AP_SSID);
    Serial.print("[WIFI] http://");
    Serial.println(IP);
    
    server.on("/", handleRoot);
    server.begin();
    
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    startupTime = millis();
    
    RoombaSerial.begin(19200, SERIAL_8N1, ROOMBA_RX_PIN, ROOMBA_TX_PIN);
    
    pinMode(RC_STEERING_PIN, INPUT);
    pinMode(RC_THROTTLE_PIN, INPUT);
    pinMode(RC_SPIN_PIN, INPUT);
    
    attachInterrupt(digitalPinToInterrupt(RC_STEERING_PIN), steeringInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RC_THROTTLE_PIN), throttleInterrupt, CHANGE);
    attachInterrupt(digitalPinToInterrupt(RC_SPIN_PIN), spinInterrupt, CHANGE);
    
    wakeRoomba();
    startRoombaOI();
    roombaBeep();
    
    Serial.println("[RC] Steering on D33 (CH2)");
    Serial.println("[RC] Throttle on D35 (CH3)");
    Serial.println("[RC] Spin on D34 (CH4)");
    Serial.println();
    Serial.println(">>> Waiting 3 seconds to arm <<<");
    
    lastValidSignal = millis();
}

void loop() {
    unsigned long now = millis();
    
    server.handleClient();
    webSocket.loop();
    
    checkArming();
    
    if (now - lastControlUpdate >= 50) {
        lastControlUpdate = now;
        processControl();
    }
    
    if (now - lastStatusPrint >= 500) {
        lastStatusPrint = now;
        sendStatusUpdate();
        ensureVacuumOff();
    }
    
    if (now - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
        lastBatteryCheck = now;
        requestBatteryData();
    }
}
