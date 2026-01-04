/*
 * CHOPPER V3 - Hero Props Droid Control System
 * Body ESP32 Ã¢â‚¬â€ WiFi AP + Web UI + ESP-NOW + OTA
 * 
 * By SeÃƒÂ¡n McArdle / Hero Props Inc.
 * Built with assistance from Claude (Anthropic)
 * 
 * V3 CHANGES:
 *   - Removed DFPlayer (hardware disconnected)
 *   - Added E-STOP with RESET capability
 *   - Reduced dome power (stripped horn screw)
 *   - Watchdog stops dome too
 * 
 * License: MIT
 * 
 * Network: CHOPPER / droid123
 * Web UI:  http://chopper.local (or 192.168.4.1)
 * 
 * Pin assignments:
 *   P25 Ã¢â€ â€™ Left Motor (FEETECH CH1)
 *   P26 Ã¢â€ â€™ Right Motor (FEETECH CH2)
 *   P27 Ã¢â€ â€™ Dome Spin Servo (360 continuous)
 *   P14 Ã¢â€ â€™ Front Arm Servo (180, limited 0-45)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

// ============== CONFIGURATION ==============

const char* AP_SSID = "CHOPPER";
const char* AP_PASS = "droid123";

// ESP-NOW broadcast address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ESP-NOW command structures
typedef struct dome_command {
  char cmd[16];
  int16_t value;
  uint8_t param;
} dome_command;

typedef struct swarm_command {
  char cmd[16];
  uint8_t speed;
} swarm_command;

// Pin assignments
#define LEFT_MOTOR_PIN   25
#define RIGHT_MOTOR_PIN  26
#define DOME_SPIN_PIN    27
#define FRONT_ARM_PIN    14

// Limits
#define FRONT_ARM_MIN    0
#define FRONT_ARM_MAX    45

// Speed limits
#define MAX_DRIVE_POWER  0.90
#define MAX_SPIN_POWER   0.80
#define MAX_DOME_POWER   0.35  // V3: Reduced from 0.70 (stripped horn screw)

// ============== GLOBALS ==============

Servo leftMotor;
Servo rightMotor;
Servo domeSpinServo;
Servo frontArmServo;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;
Preferences preferences;

// Trim values (all saved to flash)
int leftTrim = 0;
int rightTrim = 0;
int domeTrim = 0;
int frontArmTrim = 0;

// Current state
int frontArmPosition = 0;
bool estopped = false;  // V3: E-STOP state

// Watchdog
unsigned long lastCommandTime = 0;
#define WATCHDOG_TIMEOUT 500

// ============== FUNCTION PROTOTYPES ==============

void stopMotors();
void setDrive(int x, int y);
void setDomeSpin(int value);
void setFrontArm(int position);
void sendDomeCommand(const char* cmd, int16_t value, uint8_t param = 0);
void broadcastSwarmCommand(const char* cmd, uint8_t speed);
void wsLog(String message);
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
               AwsEventType type, void *arg, uint8_t *data, size_t len);
String getHTML();
void onEspNowSend(const uint8_t *mac_addr, esp_now_send_status_t status);
void saveTrim();
void loadTrim();

// ============== WEB PAGE ==============

String getHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <meta charset="UTF-8">
  <title>CHOPPER</title>
  <style>
    :root {
      --orange: #e67e22;
      --orange-dark: #a55a1a;
      --green: #2d5a3d;
      --green-light: #3d7a52;
      --yellow: #b8960b;
      --red: #c0392b;
      --grey: #5a5a5a;
      --grey-light: #7a7a7a;
      --bg: #1a1a1a;
      --bg-light: #222;
      --bg-lighter: #2a2a2a;
    }
    
    * { box-sizing: border-box; touch-action: manipulation; -webkit-user-select: none; user-select: none; }
    body {
      font-family: -apple-system, sans-serif;
      background: var(--bg);
      color: #fff;
      margin: 0;
      padding: 10px;
      overflow-x: hidden;
    }
    
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 5px 10px;
      border-bottom: 1px solid #333;
      margin-bottom: 10px;
    }
    .header h1 { margin: 0; font-size: 20px; color: var(--orange); }
    .status {
      font-size: 12px;
      padding: 4px 10px;
      border-radius: 10px;
      background: #333;
    }
    .status.connected { background: var(--green); }
    .status.estopped { background: var(--red); }
    
    .btn-group {
      display: flex;
      gap: 8px;
    }
    .estop-btn {
      background: var(--red);
      color: white;
      border: 3px solid #ff6666;
      border-radius: 8px;
      padding: 8px 16px;
      font-size: 14px;
      font-weight: bold;
      cursor: pointer;
      text-transform: uppercase;
      box-shadow: 0 4px 8px rgba(0,0,0,0.4);
    }
    .estop-btn:active { background: #ff0000; transform: scale(0.95); }
    .reset-btn {
      background: var(--green);
      color: white;
      border: 3px solid #66ff66;
      border-radius: 8px;
      padding: 8px 16px;
      font-size: 14px;
      font-weight: bold;
      cursor: pointer;
      text-transform: uppercase;
      box-shadow: 0 4px 8px rgba(0,0,0,0.4);
      display: none;
    }
    .reset-btn:active { background: #00ff00; transform: scale(0.95); }
    
    .gamepad {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 12px;
      padding: 10px;
    }
    .gamepad.disabled { opacity: 0.4; pointer-events: none; }
    
    .joystick-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 5px;
      flex-shrink: 0;
    }
    .control-label {
      font-size: 11px;
      color: #888;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    .joystick-zone {
      width: 130px;
      height: 130px;
      background: radial-gradient(circle, var(--bg-lighter) 0%, var(--bg) 100%);
      border: 2px solid var(--orange);
      border-radius: 50%;
      position: relative;
      touch-action: none;
    }
    .joystick-knob {
      width: 55px;
      height: 55px;
      background: radial-gradient(circle, var(--orange) 0%, var(--orange-dark) 100%);
      border: 2px solid var(--orange);
      border-radius: 50%;
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      pointer-events: none;
    }
    .joystick-values {
      font-family: monospace;
      font-size: 10px;
      color: #666;
    }
    .speed-btns {
      display: flex;
      gap: 4px;
      margin-top: 6px;
    }
    .speed-btn {
      padding: 6px 10px;
      border: 2px solid var(--grey);
      border-radius: 6px;
      background: var(--bg);
      color: var(--grey-light);
      font-size: 11px;
      font-weight: bold;
      cursor: pointer;
    }
    .speed-btn.active {
      border-color: var(--orange);
      color: var(--orange);
      background: var(--bg-lighter);
    }
    
    .right-section {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 10px;
      width: 100px;
      flex-shrink: 0;
    }
    
    .control-group {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 4px;
      width: 100%;
    }
    
    .dome-slider {
      width: 100%;
      height: 20px;
      -webkit-appearance: none;
      background: linear-gradient(to right, var(--green) 0%, #333 45%, #333 55%, var(--green) 100%);
      border-radius: 10px;
    }
    .dome-slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 26px;
      height: 26px;
      background: radial-gradient(circle, var(--green-light) 0%, var(--green) 100%);
      border: 2px solid var(--green-light);
      border-radius: 50%;
      cursor: pointer;
    }
    .dome-labels {
      display: flex;
      justify-content: space-between;
      width: 100%;
      font-size: 9px;
      color: #666;
    }
    
    .center-btn {
      padding: 6px 14px;
      border: 2px solid var(--green-light);
      border-radius: 6px;
      background: transparent;
      color: var(--green-light);
      font-size: 10px;
      font-weight: bold;
      cursor: pointer;
    }
    .center-btn:active { background: var(--green); color: #fff; }
    
    .arm-slider {
      width: 100%;
      height: 14px;
      -webkit-appearance: none;
      background: #333;
      border-radius: 7px;
    }
    .arm-slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 20px;
      height: 20px;
      background: var(--grey-light);
      border-radius: 50%;
      cursor: pointer;
    }
    .arm-slider.front::-webkit-slider-thumb { background: var(--orange); }
    .arm-labels {
      display: flex;
      justify-content: space-between;
      width: 100%;
      font-size: 8px;
      color: #555;
    }
    
    .section {
      margin-top: 15px;
      padding: 15px;
      background: var(--bg-light);
      border-radius: 10px;
    }
    .section-title {
      font-size: 14px;
      font-weight: bold;
      margin-bottom: 10px;
      display: flex;
      align-items: center;
      gap: 8px;
      cursor: pointer;
    }
    .section-title.settings { color: var(--grey-light); }
    .section-title::after { content: ' Ã¢â€“Â¼'; font-size: 10px; }
    .section-title.collapsed::after { content: ' Ã¢â€“Â¶'; }
    .collapse-content.hidden { display: none; }
    
    .trim-section { display: flex; gap: 20px; align-items: center; justify-content: center; flex-wrap: wrap; }
    .trim-group { display: flex; align-items: center; gap: 8px; }
    .trim-btn {
      width: 30px;
      height: 30px;
      border: 2px solid var(--grey-light);
      border-radius: 6px;
      background: transparent;
      color: var(--grey-light);
      font-size: 16px;
      font-weight: bold;
      cursor: pointer;
    }
    .trim-btn:active { background: var(--grey); color: #fff; }
    .trim-val { font-family: monospace; font-size: 14px; width: 30px; text-align: center; }
    
    .console {
      margin-top: 10px;
      width: 100%;
      height: 80px;
      background: #0d0d0d;
      border: 1px solid #333;
      border-radius: 8px;
      padding: 8px;
      font-family: monospace;
      font-size: 10px;
      overflow-y: auto;
      color: var(--green-light);
    }
    .console .error { color: var(--red); }
    .console .info { color: #666; }
    
    .debug {
      margin-top: 15px;
      background: #0a0a0a;
      border: 2px solid var(--green);
      border-radius: 8px;
      padding: 10px;
      font-family: monospace;
      font-size: 11px;
    }
    .debug h3 { margin: 0 0 8px; color: var(--green); font-size: 12px; }
    .debug-row { display: flex; gap: 20px; margin: 4px 0; flex-wrap: wrap; justify-content: center; }
    .debug-label { color: #666; width: 60px; text-align: right; }
    .debug-value { color: var(--orange); min-width: 40px; text-align: left; }
    .debug h3 { text-align: center; }
  </style>
</head>
<body>
  <div class="header">
    <h1>Ã°Å¸Â¤â€“ CHOPPER</h1>
    <div class="btn-group">
      <button class="estop-btn" onclick="estop()">Ã°Å¸â€ºâ€˜ STOP</button>
      <button class="reset-btn" id="reset-btn" onclick="resetEstop()">Ã¢â€“Â¶ RESET</button>
    </div>
    <div class="status" id="status">Connecting...</div>
  </div>
  
  <div class="gamepad" id="gamepad">
    <div class="joystick-container">
      <div class="control-label">Drive</div>
      <div class="joystick-zone" id="drive-zone">
        <div class="joystick-knob" id="drive-knob"></div>
      </div>
      <div class="joystick-values" id="drive-values">X:0 Y:0</div>
      <div class="speed-btns">
        <button class="speed-btn" id="spd50">50%</button>
        <button class="speed-btn active" id="spd75">75%</button>
        <button class="speed-btn" id="spd100">100%</button>
      </div>
    </div>
    
    <div class="right-section">
      <div class="control-group">
        <div class="control-label">Dome</div>
        <input type="range" class="dome-slider" id="dome-slider" min="-100" max="100" value="0">
        <div class="dome-labels"><span>L</span><span>R</span></div>
        <button class="center-btn" id="dome-center">Ã¢Å â„¢ CTR</button>
      </div>
      <div class="control-group">
        <div class="control-label">Front</div>
        <input type="range" class="arm-slider front" id="front-arm" min="0" max="45" value="0">
        <div class="arm-labels"><span>0</span><span>45</span></div>
      </div>
      <div class="control-group">
        <div class="control-label" style="color:#555">L Arm</div>
        <input type="range" class="arm-slider" id="left-arm" min="0" max="90" value="0">
      </div>
      <div class="control-group">
        <div class="control-label" style="color:#555">R Arm</div>
        <input type="range" class="arm-slider reversed" id="right-arm" min="0" max="90" value="0" style="direction: rtl;">
      </div>
    </div>
  </div>
  
  <div class="section">
    <div class="section-title settings" onclick="toggleSection(this)">Ã¢Å¡â„¢Ã¯Â¸Â Trim Settings</div>
    <div class="collapse-content">
      <div class="trim-section">
        <div class="trim-group">
          <span>L Motor:</span>
          <button class="trim-btn" onclick="adjustTrim('L', -1)">-</button>
          <span class="trim-val" id="trim-l">0</span>
          <button class="trim-btn" onclick="adjustTrim('L', 1)">+</button>
        </div>
        <div class="trim-group">
          <span>R Motor:</span>
          <button class="trim-btn" onclick="adjustTrim('R', -1)">-</button>
          <span class="trim-val" id="trim-r">0</span>
          <button class="trim-btn" onclick="adjustTrim('R', 1)">+</button>
        </div>
      </div>
      <div class="trim-section" style="margin-top: 12px;">
        <div class="trim-group">
          <span>Dome:</span>
          <button class="trim-btn" onclick="adjustTrim('D', -1)">-</button>
          <span class="trim-val" id="trim-d">0</span>
          <button class="trim-btn" onclick="adjustTrim('D', 1)">+</button>
        </div>
        <div class="trim-group">
          <span>Front Arm:</span>
          <button class="trim-btn" onclick="adjustTrim('F', -1)">-</button>
          <span class="trim-val" id="trim-f">0</span>
          <button class="trim-btn" onclick="adjustTrim('F', 1)">+</button>
        </div>
      </div>
    </div>
  </div>

  <div class="debug">
    <h3>Ã°Å¸â€œÅ  DEBUG</h3>
    <div class="debug-row">
      <span class="debug-label">Drive X:</span>
      <span class="debug-value" id="dbg-x">0</span>
      <span class="debug-label">Drive Y:</span>
      <span class="debug-value" id="dbg-y">0</span>
      <span class="debug-label">Speed:</span>
      <span class="debug-value" id="dbg-speed">75%</span>
    </div>
    <div class="debug-row">
      <span class="debug-label">Dome:</span>
      <span class="debug-value" id="dbg-dome">STOP</span>
      <span class="debug-label">Front:</span>
      <span class="debug-value" id="dbg-front">0Ã‚Â°</span>
    </div>
    <div class="debug-row">
      <span class="debug-label">L Arm:</span>
      <span class="debug-value" id="dbg-larm">0Ã‚Â°</span>
      <span class="debug-label">R Arm:</span>
      <span class="debug-value" id="dbg-rarm">0Ã‚Â°</span>
    </div>
  </div>
  
  <div class="console" id="console"></div>
  
  <script>
    let ws;
    let connected = false;
    let isEstopped = false;
    const consoleEl = document.getElementById('console');
    
    function log(msg, type = '') {
      const line = document.createElement('div');
      line.className = type;
      line.textContent = '> ' + msg;
      consoleEl.appendChild(line);
      consoleEl.scrollTop = consoleEl.scrollHeight;
      // Parse trim responses
      const trimMatch = msg.match(/Trim: L=(-?\d+), R=(-?\d+), D=(-?\d+), F=(-?\d+)/);
      if (trimMatch) {
        document.getElementById('trim-l').textContent = trimMatch[1];
        document.getElementById('trim-r').textContent = trimMatch[2];
        document.getElementById('trim-d').textContent = trimMatch[3];
        document.getElementById('trim-f').textContent = trimMatch[4];
      }
      // Check for ESTOP state
      if (msg.includes('[ESTOP]')) {
        setEstopState(true);
      }
      if (msg.includes('[RESET]')) {
        setEstopState(false);
      }
      // Check for estopped state on connect
      if (msg.includes('ESTOPPED: true')) {
        setEstopState(true);
      } else if (msg.includes('ESTOPPED: false')) {
        setEstopState(false);
      }
    }
    
    function setEstopState(stopped) {
      isEstopped = stopped;
      const gamepad = document.getElementById('gamepad');
      const resetBtn = document.getElementById('reset-btn');
      const status = document.getElementById('status');
      if (stopped) {
        gamepad.classList.add('disabled');
        resetBtn.style.display = 'block';
        if (connected) {
          status.textContent = 'ESTOPPED';
          status.className = 'status estopped';
        }
      } else {
        gamepad.classList.remove('disabled');
        resetBtn.style.display = 'none';
        if (connected) {
          status.textContent = 'Connected';
          status.className = 'status connected';
        }
      }
    }
    
    function connect() {
      ws = new WebSocket('ws://' + location.host + '/ws');
      ws.onopen = () => {
        connected = true;
        document.getElementById('status').textContent = 'Connected';
        document.getElementById('status').className = 'status connected';
        log('Connected', 'info');
      };
      ws.onclose = () => {
        connected = false;
        document.getElementById('status').textContent = 'Disconnected';
        document.getElementById('status').className = 'status';
        log('Disconnected - reconnecting...', 'error');
        setTimeout(connect, 2000);
      };
      ws.onmessage = (e) => log(e.data);
      ws.onerror = () => log('WebSocket error', 'error');
    }
    
    function send(msg) {
      if (connected && ws.readyState === WebSocket.OPEN) ws.send(msg);
    }
    
    function toggleSection(el) {
      el.classList.toggle('collapsed');
      el.nextElementSibling.classList.toggle('hidden');
    }
    
    // Speed control
    let speedMultiplier = 0.75;
    document.getElementById('spd50').onclick = () => setSpeed(0.50, 'spd50');
    document.getElementById('spd75').onclick = () => setSpeed(0.75, 'spd75');
    document.getElementById('spd100').onclick = () => setSpeed(1.00, 'spd100');
    
    function setSpeed(mult, btnId) {
      speedMultiplier = mult;
      document.querySelectorAll('.speed-btn').forEach(b => b.classList.remove('active'));
      document.getElementById(btnId).classList.add('active');
      document.getElementById('dbg-speed').textContent = Math.round(mult * 100) + '%';
      log('Speed: ' + Math.round(mult * 100) + '%');
    }
    
    // Drive joystick
    const driveZone = document.getElementById('drive-zone');
    const driveKnob = document.getElementById('drive-knob');
    const driveValues = document.getElementById('drive-values');
    let driveActive = false;
    let lastDriveX = 0, lastDriveY = 0;
    
    function handleDriveMove(clientX, clientY) {
      if (isEstopped) return;
      const rect = driveZone.getBoundingClientRect();
      let x = (clientX - rect.x - rect.width/2) / (rect.width/2);
      let y = (clientY - rect.y - rect.height/2) / (rect.height/2);
      const dist = Math.sqrt(x*x + y*y);
      if (dist > 1) { x /= dist; y /= dist; }
      const knobX = x * (rect.width/2 - 28) + rect.width/2;
      const knobY = y * (rect.height/2 - 28) + rect.height/2;
      driveKnob.style.left = knobX + 'px';
      driveKnob.style.top = knobY + 'px';
      driveKnob.style.transform = 'translate(-50%, -50%)';
      const driveX = Math.round(x * 100 * speedMultiplier);
      const driveY = Math.round(-y * 100 * speedMultiplier);
      lastDriveX = driveX;
      lastDriveY = driveY;
      driveValues.textContent = 'X:' + driveX + ' Y:' + driveY;
      document.getElementById('dbg-x').textContent = driveX;
      document.getElementById('dbg-y').textContent = driveY;
      send('DRIVE:' + driveX + ',' + driveY);
    }
    
    function handleDriveEnd() {
      if (!driveActive) return;
      driveActive = false;
      lastDriveX = 0;
      lastDriveY = 0;
      driveKnob.style.left = '50%';
      driveKnob.style.top = '50%';
      driveValues.textContent = 'X:0 Y:0';
      document.getElementById('dbg-x').textContent = '0';
      document.getElementById('dbg-y').textContent = '0';
      send('DRIVE:0,0');
    }
    
    driveZone.addEventListener('touchstart', (e) => { e.preventDefault(); driveActive = true; handleDriveMove(e.touches[0].clientX, e.touches[0].clientY); });
    driveZone.addEventListener('touchmove', (e) => { e.preventDefault(); if (driveActive) handleDriveMove(e.touches[0].clientX, e.touches[0].clientY); });
    driveZone.addEventListener('touchend', handleDriveEnd);
    driveZone.addEventListener('touchcancel', handleDriveEnd);
    driveZone.addEventListener('mousedown', (e) => { driveActive = true; handleDriveMove(e.clientX, e.clientY); });
    document.addEventListener('mousemove', (e) => { if (driveActive) handleDriveMove(e.clientX, e.clientY); });
    document.addEventListener('mouseup', handleDriveEnd);
    
    // Heartbeat: resend drive state every 100ms to keep watchdog fed
    setInterval(() => {
      if (driveActive && connected && !isEstopped) {
        send('DRIVE:' + lastDriveX + ',' + lastDriveY);
      }
    }, 100);
    
    // Dome slider
    const domeSlider = document.getElementById('dome-slider');
    domeSlider.addEventListener('input', (e) => {
      if (isEstopped) return;
      const val = parseInt(e.target.value);
      send('DOME:' + val);
      if (val === 0) {
        document.getElementById('dbg-dome').textContent = 'STOP';
      } else if (val > 0) {
        document.getElementById('dbg-dome').textContent = 'Ã¢â€ â€™ ' + val + '%';
      } else {
        document.getElementById('dbg-dome').textContent = 'Ã¢â€ Â ' + Math.abs(val) + '%';
      }
    });
    function domeRelease() {
      domeSlider.value = 0;
      send('DOME:0');
      document.getElementById('dbg-dome').textContent = 'STOP';
    }
    domeSlider.addEventListener('touchend', domeRelease);
    domeSlider.addEventListener('mouseup', domeRelease);
    document.getElementById('dome-center').addEventListener('click', domeRelease);
    
    // Arm sliders
    document.getElementById('front-arm').addEventListener('input', (e) => {
      if (isEstopped) return;
      send('FRONTARM:' + e.target.value);
      document.getElementById('dbg-front').textContent = e.target.value + 'Ã‚Â°';
    });
    document.getElementById('left-arm').addEventListener('input', (e) => {
      if (isEstopped) return;
      send('LEFTARM:' + e.target.value);
      document.getElementById('dbg-larm').textContent = e.target.value + 'Ã‚Â°';
    });
    document.getElementById('right-arm').addEventListener('input', (e) => {
      if (isEstopped) return;
      send('RIGHTARM:' + e.target.value);
      document.getElementById('dbg-rarm').textContent = e.target.value + 'Ã‚Â°';
    });
    
    // Trim controls
    function adjustTrim(motor, delta) { 
      send('TRIM:' + motor + (delta > 0 ? '+' : '-')); 
    }
    
    // E-STOP and RESET
    function estop() {
      send('ESTOP');
      setEstopState(true);
      log('[ESTOP] ALL STOP', 'error');
    }
    
    function resetEstop() {
      send('RESET');
    }
    
    connect();
  </script>
</body>
</html>
)rawliteral";
}

// ============== WEBSOCKET HANDLING ==============

void wsLog(String message) {
  ws.textAll(message);
  Serial.println(message);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String cmd = String((char*)data);
    lastCommandTime = millis();
    
    // E-STOP handler
    if (cmd == "ESTOP") {
      estopped = true;
      stopMotors();
      setDomeSpin(0);
      wsLog("[ESTOP] ALL STOP");
      return;
    }
    
    // RESET handler
    if (cmd == "RESET") {
      estopped = false;
      wsLog("[RESET] Controls enabled");
      return;
    }
    
    if (cmd.startsWith("DRIVE:")) {
      if (estopped) return;
      String params = cmd.substring(6);
      int comma = params.indexOf(',');
      int x = params.substring(0, comma).toInt();
      int y = params.substring(comma + 1).toInt();
      setDrive(x, y);
    }
    else if (cmd.startsWith("DOME:")) {
      if (estopped) return;
      int spin = cmd.substring(5).toInt();
      setDomeSpin(spin);
    }
    else if (cmd.startsWith("FRONTARM:")) {
      if (estopped) return;
      int pos = cmd.substring(9).toInt();
      setFrontArm(pos);
    }
    else if (cmd.startsWith("LEFTARM:")) {
      if (estopped) return;
      int pos = cmd.substring(8).toInt();
      sendDomeCommand("LARM", pos);
    }
    else if (cmd.startsWith("RIGHTARM:")) {
      if (estopped) return;
      int pos = cmd.substring(9).toInt();
      sendDomeCommand("RARM", pos);
    }
    else if (cmd.startsWith("TRIM:")) {
      char motor = cmd.charAt(5);
      char dir = cmd.charAt(6);
      int delta = (dir == '+') ? 1 : -1;
      
      if (motor == 'L') leftTrim += delta;
      else if (motor == 'R') rightTrim += delta;
      else if (motor == 'D') domeTrim += delta;
      else if (motor == 'F') frontArmTrim += delta;
      
      saveTrim();
      wsLog("Trim: L=" + String(leftTrim) + ", R=" + String(rightTrim) + 
            ", D=" + String(domeTrim) + ", F=" + String(frontArmTrim));
    }
    else if (cmd.startsWith("LED:")) {
      if (estopped) return;
      int pattern = cmd.substring(4).toInt();
      sendDomeCommand("LED", pattern);
      wsLog("[DOME] LED: " + String(pattern));
    }
    else if (cmd.startsWith("SWARM:")) {
      if (estopped) return;
      String swarmCmd = cmd.substring(6);
      broadcastSwarmCommand(swarmCmd.c_str(), 30);
    }
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      wsLog("Client connected");
      wsLog("Trim: L=" + String(leftTrim) + ", R=" + String(rightTrim) + 
            ", D=" + String(domeTrim) + ", F=" + String(frontArmTrim));
      wsLog("ESTOPPED: " + String(estopped ? "true" : "false"));
      break;
    case WS_EVT_DISCONNECT:
      wsLog("Client disconnected");
      stopMotors();
      setDomeSpin(0);
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// ============== MOTOR FUNCTIONS ==============

void setDrive(int x, int y) {
  if (estopped) return;
  
  float throttle = y / 100.0 * MAX_DRIVE_POWER;
  float turn = x / 100.0 * MAX_SPIN_POWER;
  
  float leftPower = throttle + turn;
  float rightPower = throttle - turn;
  
  leftPower = constrain(leftPower, -1.0f, 1.0f);
  rightPower = constrain(rightPower, -1.0f, 1.0f);
  
  int leftUs = 1500 + (int)(leftPower * 500) + (leftTrim * 5);
  int rightUs = 1500 - (int)(rightPower * 500) + (rightTrim * 5);  // Right inverted (opposite side mounting)
  
  leftMotor.writeMicroseconds(constrain(leftUs, 1000, 2000));
  rightMotor.writeMicroseconds(constrain(rightUs, 1000, 2000));
}

void stopMotors() {
  leftMotor.writeMicroseconds(1500 + leftTrim * 5);
  rightMotor.writeMicroseconds(1500 + rightTrim * 5);
}

void setDomeSpin(int value) {
  if (estopped && value != 0) return;
  int us = 1500 + (int)(value * MAX_DOME_POWER * 5) + (domeTrim * 5);
  domeSpinServo.writeMicroseconds(constrain(us, 1000, 2000));
}

void setFrontArm(int position) {
  if (estopped) return;
  int reversedPos = FRONT_ARM_MAX - constrain(position, FRONT_ARM_MIN, FRONT_ARM_MAX);
  int adjustedPos = reversedPos + frontArmTrim;
  frontArmServo.write(constrain(adjustedPos, 0, 180));
  frontArmPosition = position;
}

void saveTrim() {
  preferences.begin("chopper", false);
  preferences.putInt("leftTrim", leftTrim);
  preferences.putInt("rightTrim", rightTrim);
  preferences.putInt("domeTrim", domeTrim);
  preferences.putInt("frontArmTrim", frontArmTrim);
  preferences.end();
}

void loadTrim() {
  preferences.begin("chopper", true);
  leftTrim = preferences.getInt("leftTrim", 0);
  rightTrim = preferences.getInt("rightTrim", 0);
  domeTrim = preferences.getInt("domeTrim", 0);
  frontArmTrim = preferences.getInt("frontArmTrim", 0);
  preferences.end();
}

// ============== ESP-NOW FUNCTIONS ==============

void onEspNowSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Silent
}

void sendDomeCommand(const char* cmd, int16_t value, uint8_t param) {
  dome_command command;
  strncpy(command.cmd, cmd, sizeof(command.cmd) - 1);
  command.cmd[sizeof(command.cmd) - 1] = '\0';
  command.value = value;
  command.param = param;
  esp_now_send(broadcastAddress, (uint8_t*)&command, sizeof(command));
}

void broadcastSwarmCommand(const char* cmd, uint8_t speed) {
  swarm_command command;
  strncpy(command.cmd, cmd, sizeof(command.cmd) - 1);
  command.cmd[sizeof(command.cmd) - 1] = '\0';
  command.speed = speed;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&command, sizeof(command));
  if (result == ESP_OK) {
    wsLog("[SWARM] " + String(cmd));
  }
}

// ============== SETUP ==============

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n================================");
  Serial.println("  CHOPPER V3 - Hero Props Droid");
  Serial.println("================================\n");
  
  loadTrim();
  Serial.println("[OK] Trim loaded: L=" + String(leftTrim) + ", R=" + String(rightTrim) + 
                 ", D=" + String(domeTrim) + ", F=" + String(frontArmTrim));
  
  // CRITICAL: Set motor pins LOW before attaching servos
  pinMode(LEFT_MOTOR_PIN, OUTPUT);
  pinMode(RIGHT_MOTOR_PIN, OUTPUT);
  pinMode(DOME_SPIN_PIN, OUTPUT);
  pinMode(FRONT_ARM_PIN, OUTPUT);
  digitalWrite(LEFT_MOTOR_PIN, LOW);
  digitalWrite(RIGHT_MOTOR_PIN, LOW);
  digitalWrite(DOME_SPIN_PIN, LOW);
  digitalWrite(FRONT_ARM_PIN, LOW);
  delay(100);
  
  // Now attach servos
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  leftMotor.setPeriodHertz(50);
  rightMotor.setPeriodHertz(50);
  domeSpinServo.setPeriodHertz(50);
  frontArmServo.setPeriodHertz(50);
  
  leftMotor.attach(LEFT_MOTOR_PIN, 500, 2400);
  rightMotor.attach(RIGHT_MOTOR_PIN, 500, 2400);
  domeSpinServo.attach(DOME_SPIN_PIN, 500, 2400);
  frontArmServo.attach(FRONT_ARM_PIN, 500, 2400);
  
  stopMotors();
  setDomeSpin(0);
  setFrontArm(0);
  delay(50);
  Serial.println("[OK] Servos initialized (anti-blast protection)");
  
  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("[OK] WiFi AP: " + String(AP_SSID));
  Serial.println("    IP: " + IP.toString());
  
  // mDNS
  if (MDNS.begin("chopper")) {
    Serial.println("[OK] mDNS: http://chopper.local");
  }
  
  // Captive portal
  dnsServer.start(53, "*", IP);
  Serial.println("[OK] Captive portal active");
  
  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[!!] ESP-NOW init failed");
  } else {
    esp_now_register_send_cb(onEspNowSend);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("[!!] ESP-NOW peer add failed");
    } else {
      Serial.println("[OK] ESP-NOW broadcast ready");
    }
  }
  
  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  // Web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", getHTML());
  });
  
  // Captive portal redirects
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
  server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
  server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
  server.onNotFound([](AsyncWebServerRequest *request) { request->redirect("/"); });
  
  server.begin();
  Serial.println("[OK] Web server started");
  
  // OTA
  ArduinoOTA.setHostname("chopper");
  ArduinoOTA.onStart([]() { stopMotors(); setDomeSpin(0); Serial.println("[OTA] Starting..."); });
  ArduinoOTA.onEnd([]() { Serial.println("[OTA] Done!"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) { Serial.println("[OTA] Error"); });
  ArduinoOTA.begin();
  Serial.println("[OK] OTA ready");
  
  Serial.println("\n>> CHOPPER V3 ONLINE <<\n");
}

// ============== MAIN LOOP ==============

void loop() {
  dnsServer.processNextRequest();
  ArduinoOTA.handle();
  ws.cleanupClients();
  
  // V3: Watchdog stops BOTH drive AND dome
  if (millis() - lastCommandTime > WATCHDOG_TIMEOUT) {
    stopMotors();
    setDomeSpin(0);
  }
}