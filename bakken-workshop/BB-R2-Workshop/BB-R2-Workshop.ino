/*
 * BB-R2 Workshop Droid Controller
 * Hero Props Inc. / Seán McArdle
 * heroprops.art
 * December 2025
 * 
 * Based on work by:
 *   - Bjoern Giesler (github.com/bjoerngiesler) - Original BB-R2 STEM ESP32 firmware
 *   - Michael Baddeley's 3D Printed Droids (patreon.com/mrbaddeley) - BB Astromech designs
 *   - The Droid Builders community (astromech.net)
 * 
 * Built for a 2-day kids workshop at The Bakken Museum, Minneapolis.
 * 16 kids (ages 9-14) assemble, wire, and decorate their own droids,
 * then drive them home.
 * 
 * Key features:
 *   - Each droid creates its own WiFi network (R2-BK01, R2-BK02, etc.)
 *   - Kids connect with any phone or tablet — no app install needed
 *   - Control via web browser: virtual joystick for driving, slider for dome
 *   - CUSTOMIZE YOUR DROID: Set your name and droid designation through the web interface!
 *   - mDNS support: access via bk01.local instead of IP address
 *   - Watchdog stops motors if connection drops (safety first)
 *   - Smooth ramping so movements feel natural, not jerky
 * 
 * Hardware: ESP32 DevKit + 3x FS90R continuous rotation servos
 * Pins: GPIO 25 (Left), GPIO 26 (Right), GPIO 27 (Dome)
 * 
 * License: MIT
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

// =============================================================================
// DROID IDENTITY
// =============================================================================
// Your droid's operator name and designation are set through the web interface!
// Just connect to your droid's WiFi and click the edit buttons in the UI.
//
// The network identity (WiFi SSID, password, and mDNS name below) stays in the
// code for technical reasons. These identify your droid on the network and are
// set once when the droid is built.
//
// For the curious: If you want to change your droid's network ID for a new
// designation (e.g., upgrading from BK-00 to BK-01), update these three values:
//   - AP_SSID: Your droid's WiFi network name (what you see when connecting)
//   - AP_PASS: The password to connect to your droid's WiFi
//   - MDNS_NAME: The .local address for browser access (e.g., bk00.local)
//
// These must match! For example, BK-05 would use:
//   AP_SSID = "R2-BK05", AP_PASS = "BK05droid", MDNS_NAME = "bk05"
//
// Default values below are for the BK-00 test unit.
// =============================================================================

// Network identity (WiFi credentials and mDNS name)
const char* AP_SSID = "R2-BK00";
const char* AP_PASS = "BK00droid";
const char* MDNS_NAME = "bk00";

// Display identity (customizable through web UI - these are just defaults)
String droidDesignation = "BK-00";
String operatorName = "Your Name Here";

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================
// Servo pins and trim values
// (Advanced users: trim values fine-tune servo center points if needed)
// =============================================================================

#define LEFT_PIN   25
#define RIGHT_PIN  26
#define DOME_PIN   27

#define LEFT_TRIM    3
#define RIGHT_TRIM   3

// =============================================================================
// GLOBAL OBJECTS AND VARIABLES
// =============================================================================
// Server and networking
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Servo motor objects
Servo leftServo;
Servo rightServo;
Servo domeServo;

// Motor control - target values (what we want) vs current values (where we are)
// The difference enables smooth ramping instead of jerky movements
float targetLeft = 0;
float targetRight = 0;
float targetDome = 90;

float currentLeft = 0;
float currentRight = 0;
float currentDome = 90;

// Smoothing factors: lower = smoother but slower response
#define DRIVE_SMOOTH 0.15
#define DOME_SMOOTH 0.1

// Timing and safety
unsigned long lastCmd = 0;
unsigned long lastLoop = 0;
#define WATCHDOG_MS 2000  // Stop motors after 2 seconds of no commands
#define LOOP_MS 20        // Update servos every 20ms (50Hz)

// =============================================================================
// WEB INTERFACE
// =============================================================================
// The HTML/CSS/JavaScript below creates the control interface students see
// in their web browser. Advanced students can explore this to learn about:
//   - Responsive web design with CSS
//   - Touch/mouse event handling
//   - WebSocket communication
//   - Local storage for persisting data
// =============================================================================

const char* HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>BB Droid Control</title>
  <style>
    * { box-sizing: border-box; user-select: none; -webkit-user-select: none; margin: 0; padding: 0; }
    html, body { width: 100%; height: 100%; overflow: hidden; touch-action: manipulation; }
    body {
      font-family: -apple-system, sans-serif;
      background: linear-gradient(180deg, #0a1628 0%, #12243d 100%);
      color: #ff6b35;
      display: flex;
      flex-direction: column;
      padding: 10px;
    }
    h1 { text-align: center; font-size: 1.8em; margin-bottom: 2px; cursor: pointer; }
    h1:hover { opacity: 0.8; }
    .owner { text-align: center; font-size: 1em; color: #3a8dde; margin-bottom: 5px; cursor: pointer; }
    .owner:hover { opacity: 0.8; }
    .edit-hint { text-align: center; font-size: 0.7em; color: #666; margin-bottom: 5px; }
    .status { text-align: center; font-size: 0.8em; color: #666; margin-bottom: 10px; }
    .status.connected { color: #0c0; }
    
    .edit-modal {
      display: none;
      position: fixed;
      top: 0; left: 0;
      width: 100%; height: 100%;
      background: rgba(0, 0, 0, 0.8);
      align-items: center;
      justify-content: center;
      z-index: 1000;
    }
    .edit-modal.show { display: flex; }
    .edit-box {
      background: #12243d;
      border: 2px solid #ff6b35;
      border-radius: 10px;
      padding: 20px;
      width: 80%;
      max-width: 400px;
    }
    .edit-box h2 { font-size: 1.2em; margin-bottom: 15px; text-align: center; }
    .edit-box input {
      width: 100%;
      padding: 12px;
      font-size: 1.1em;
      background: #0a1628;
      border: 2px solid #1e3a5f;
      border-radius: 5px;
      color: #ff6b35;
      margin-bottom: 15px;
      font-family: inherit;
    }
    .edit-box .buttons {
      display: flex;
      gap: 10px;
    }
    .edit-box button {
      flex: 1;
      padding: 12px;
      font-size: 1em;
      font-weight: bold;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-family: inherit;
    }
    .edit-box .save-btn {
      background: #0c0;
      color: #000;
    }
    .edit-box .cancel-btn {
      background: #666;
      color: #fff;
    }
    
    .controls {
      display: flex;
      flex-direction: row;
      justify-content: space-around;
      align-items: center;
      flex: 1;
    }
    .control-group { display: flex; flex-direction: column; align-items: center; }
    .label { font-size: 0.7em; color: #3a8dde; margin-bottom: 8px; letter-spacing: 2px; }
    
    .joystick {
      width: 160px;
      height: 160px;
      background: radial-gradient(circle, #2a4a6a 0%, #12243d 100%);
      border: 3px solid #1e3a5f;
      border-radius: 50%;
      position: relative;
      touch-action: none;
    }
    .stick {
      width: 65px;
      height: 65px;
      background: radial-gradient(circle, #ff8c5a 0%, #ff6b35 50%, #cc4a1a 100%);
      border-radius: 50%;
      position: absolute;
      top: 50%; left: 50%;
      transform: translate(-50%, -50%);
      pointer-events: none;
    }
    
    .dome-track {
      width: 160px;
      height: 60px;
      background: radial-gradient(ellipse, #2a4a6a 0%, #12243d 100%);
      border: 3px solid #1e3a5f;
      border-radius: 30px;
      position: relative;
      touch-action: none;
    }
    .dome-knob {
      width: 50px;
      height: 50px;
      background: radial-gradient(circle, #5af 0%, #38d 50%, #26b 100%);
      border-radius: 50%;
      position: absolute;
      left: 50%; top: 50%;
      transform: translate(-50%, -50%);
      pointer-events: none;
    }
    
    .log {
      height: 180px;
      background: #0a1220;
      border: 1px solid #1e3a5f;
      border-radius: 5px;
      padding: 10px;
      font-family: monospace;
      font-size: 1.1em;
      overflow-y: auto;
      color: #0c0;
      margin-top: 10px;
      line-height: 1.4;
    }
    .log .cmd { color: #ff6b35; }
    .log .info { color: #3a8dde; }
    .footer {
      text-align: center;
      font-size: 0.6em;
      color: #445;
      margin-top: 8px;
      line-height: 1.4;
    }
    .footer a { color: #3a8dde; text-decoration: none; }
  </style>
</head>
<body>
  <h1 id="designation" onclick="editDesignation()">R2-BK00</h1>
  <div class="owner" id="owner" onclick="editOwner()">Your Name Here's Droid</div>
  <div class="edit-hint">👆 Tap to customize your droid's name</div>
  <div class="status" id="status">CONNECTING...</div>
  
  <div class="edit-modal" id="designationModal">
    <div class="edit-box">
      <h2>Droid Designation</h2>
      <input type="text" id="designationInput" placeholder="e.g., BK-05" maxlength="10">
      <div class="buttons">
        <button class="cancel-btn" onclick="closeDesignationModal()">Cancel</button>
        <button class="save-btn" onclick="saveDesignation()">Save</button>
      </div>
    </div>
  </div>
  
  <div class="edit-modal" id="ownerModal">
    <div class="edit-box">
      <h2>Operator Name</h2>
      <input type="text" id="ownerInput" placeholder="Your name" maxlength="30">
      <div class="buttons">
        <button class="cancel-btn" onclick="closeOwnerModal()">Cancel</button>
        <button class="save-btn" onclick="saveOwner()">Save</button>
      </div>
    </div>
  </div>
  
  <div class="controls">
    <div class="control-group">
      <div class="label">DRIVE</div>
      <div class="joystick" id="joy">
        <div class="stick" id="stick"></div>
      </div>
    </div>
    <div class="control-group">
      <div class="label">DOME</div>
      <div class="dome-track" id="dome">
        <div class="dome-knob" id="knob"></div>
      </div>
    </div>
  </div>
  
  <div class="log" id="log"></div>
  <div class="footer">
    Workshop by <strong>Seán McArdle</strong> · Hero Props Inc.<br>
    <a href="mailto:sean@heroprops.art">sean@heroprops.art</a> · heroprops.art
  </div>

  <script>
    let ws;
    const joy = document.getElementById('joy');
    const stick = document.getElementById('stick');
    const dome = document.getElementById('dome');
    const knob = document.getElementById('knob');
    const status = document.getElementById('status');
    const log = document.getElementById('log');
    
    // Load saved values from localStorage
    let droidDesignation = localStorage.getItem('droidDesignation') || 'BK-00';
    let operatorName = localStorage.getItem('operatorName') || 'Your Name Here';
    
    document.getElementById('designation').textContent = 'R2-' + droidDesignation;
    document.getElementById('owner').textContent = operatorName + "'s Droid";
    document.title = 'R2-' + droidDesignation + ' - ' + operatorName;
    
    // Edit designation
    function editDesignation() {
      document.getElementById('designationInput').value = droidDesignation;
      document.getElementById('designationModal').classList.add('show');
      setTimeout(() => document.getElementById('designationInput').focus(), 100);
    }
    
    function closeDesignationModal() {
      document.getElementById('designationModal').classList.remove('show');
    }
    
    function saveDesignation() {
      const val = document.getElementById('designationInput').value.trim();
      if (val) {
        droidDesignation = val;
        localStorage.setItem('droidDesignation', val);
        document.getElementById('designation').textContent = 'R2-' + val;
        document.title = 'R2-' + val + ' - ' + operatorName;
        sendNow('I:' + val);
        addLog('Designation updated: ' + val, 'info');
      }
      closeDesignationModal();
    }
    
    // Edit owner
    function editOwner() {
      document.getElementById('ownerInput').value = operatorName;
      document.getElementById('ownerModal').classList.add('show');
      setTimeout(() => document.getElementById('ownerInput').focus(), 100);
    }
    
    function closeOwnerModal() {
      document.getElementById('ownerModal').classList.remove('show');
    }
    
    function saveOwner() {
      const val = document.getElementById('ownerInput').value.trim();
      if (val) {
        operatorName = val;
        localStorage.setItem('operatorName', val);
        document.getElementById('owner').textContent = val + "'s Droid";
        document.title = 'R2-' + droidDesignation + ' - ' + val;
        sendNow('O:' + val);
        addLog('Operator name updated: ' + val, 'info');
      }
      closeOwnerModal();
    }
    
    // Allow Enter key to save in modals
    document.getElementById('designationInput').addEventListener('keypress', (e) => {
      if (e.key === 'Enter') saveDesignation();
    });
    document.getElementById('ownerInput').addEventListener('keypress', (e) => {
      if (e.key === 'Enter') saveOwner();
    });
    
    let joyActive = false;
    let currentDx = 0;
    let currentDy = 0;
    
    let lastSend = 0;
    const THROTTLE_MS = 50;
    
    // Heartbeat every 50ms while joystick active
    setInterval(() => {
      if (joyActive && ws && ws.readyState === WebSocket.OPEN) {
        ws.send('D:' + currentDx.toFixed(2) + ',' + currentDy.toFixed(2));
      }
    }, 50);
    
    function connect() {
      ws = new WebSocket('ws://' + location.host + '/ws');
      ws.onopen = () => {
        status.textContent = 'CONNECTED';
        status.className = 'status connected';
        addLog('WebSocket connected', 'info');
        // Send current identity to droid
        sendNow('I:' + droidDesignation);
        sendNow('O:' + operatorName);
      };
      ws.onclose = () => {
        status.textContent = 'DISCONNECTED';
        status.className = 'status';
        addLog('Connection lost - reconnecting...', '');
        setTimeout(connect, 1000);
      };
      ws.onmessage = (e) => addLog('← ' + e.data, 'info');
    }
    
    function send(msg) {
      const now = Date.now();
      if (now - lastSend < THROTTLE_MS) return;
      lastSend = now;
      if (ws && ws.readyState === WebSocket.OPEN) ws.send(msg);
    }
    
    function sendNow(msg) {
      if (ws && ws.readyState === WebSocket.OPEN) ws.send(msg);
    }
    
    function addLog(msg, cls) {
      const span = document.createElement('span');
      span.className = cls || '';
      span.textContent = msg;
      log.appendChild(span);
      log.appendChild(document.createElement('br'));
      log.scrollTop = log.scrollHeight;
    }
    
    function joyMove(cx, cy) {
      const r = joy.getBoundingClientRect();
      const rad = r.width / 2 - 32;
      let dx = cx - r.left - r.width/2;
      let dy = cy - r.top - r.height/2;
      const d = Math.sqrt(dx*dx + dy*dy);
      if (d > rad) { dx = dx/d*rad; dy = dy/d*rad; }
      
      stick.style.left = (50 + dx/r.width*100) + '%';
      stick.style.top = (50 + dy/r.height*100) + '%';
      
      currentDx = dx/rad;
      currentDy = -dy/rad;
      
      send('D:' + currentDx.toFixed(2) + ',' + currentDy.toFixed(2));
    }
    
    let lastDriveLog = 0;
    function logDrive() {
      const now = Date.now();
      if (now - lastDriveLog > 200) {
        lastDriveLog = now;
        const pct = (v) => (v * 100).toFixed(0);
        addLog('DRIVE → X:' + pct(currentDx) + '% Y:' + pct(currentDy) + '%', 'cmd');
      }
    }
    
    function joyEnd() {
      if (!joyActive) return;
      joyActive = false;
      stick.style.left = '50%';
      stick.style.top = '50%';
      currentDx = 0;
      currentDy = 0;
      sendNow('D:0,0');
      addLog('DRIVE → STOP', 'cmd');
    }
    
    joy.onmousedown = (e) => { joyActive = true; joyMove(e.clientX, e.clientY); logDrive(); };
    joy.ontouchstart = (e) => { joyActive = true; e.preventDefault(); joyMove(e.touches[0].clientX, e.touches[0].clientY); logDrive(); };
    document.onmousemove = (e) => { if (joyActive) { joyMove(e.clientX, e.clientY); logDrive(); } };
    document.ontouchmove = (e) => { if (joyActive) { e.preventDefault(); joyMove(e.touches[0].clientX, e.touches[0].clientY); logDrive(); } };
    document.onmouseup = joyEnd;
    document.ontouchend = joyEnd;
    
    let domeActive = false;
    let lastDomeLog = 0;
    
    function domeMove(cx) {
      const r = dome.getBoundingClientRect();
      const margin = 28;
      let x = cx - r.left;
      x = Math.max(margin, Math.min(r.width - margin, x));
      knob.style.left = x + 'px';
      knob.style.top = '50%';
      const val = (x - margin) / (r.width - 2*margin);
      send('M:' + val.toFixed(2));
      
      const now = Date.now();
      if (now - lastDomeLog > 200) {
        lastDomeLog = now;
        const deg = (val * 180).toFixed(0);
        addLog('DOME → ' + deg + '°', 'info');
      }
    }
    
    dome.onmousedown = (e) => { domeActive = true; domeMove(e.clientX); };
    dome.ontouchstart = (e) => { domeActive = true; e.preventDefault(); domeMove(e.touches[0].clientX); };
    document.addEventListener('mousemove', (e) => { if (domeActive) domeMove(e.clientX); });
    document.addEventListener('touchmove', (e) => { if (domeActive) { e.preventDefault(); domeMove(e.touches[0].clientX); } }, {passive:false});
    document.addEventListener('mouseup', () => domeActive = false);
    document.addEventListener('touchend', () => domeActive = false);
    
    document.addEventListener('contextmenu', e => e.preventDefault());
    addLog('Droid control system initializing...', 'info');
    connect();
  </script>
</body>
</html>
)rawliteral";

// =============================================================================
// WebSocket Event Handler
// =============================================================================
// Handles incoming commands from the web interface:
//   D:x,y - Drive command (joystick: x and y are -1.0 to 1.0)
//   M:v   - Dome/head rotation (v is 0.0 to 1.0)
//   I:id  - Update droid designation (e.g., "BK-05")
//   O:name - Update operator name (e.g., "Alex")
// =============================================================================

void onWsEvent(AsyncWebSocket* srv, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("[WS] Client connected");
    ws.textAll("Connected to R2-" + droidDesignation);
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.println("[WS] Client disconnected");
    targetLeft = 0;
    targetRight = 0;
  }
  else if (type == WS_EVT_DATA) {
    lastCmd = millis();
    String cmd = String((char*)data).substring(0, len);
    
    if (cmd.startsWith("D:")) {
      // Drive command
      int c = cmd.indexOf(',');
      float x = cmd.substring(2, c).toFloat();
      float y = cmd.substring(c+1).toFloat();
      
      targetLeft = constrain(y + x, -1.0f, 1.0f);
      targetRight = constrain(y - x, -1.0f, 1.0f);
      
      if (x == 0 && y == 0) {
        currentLeft = 0;
        currentRight = 0;
        targetLeft = 0;
        targetRight = 0;
      }
    }
    else if (cmd.startsWith("M:")) {
      // Dome movement command
      float v = cmd.substring(2).toFloat();
      targetDome = v * 180.0f;
    }
    else if (cmd.startsWith("I:")) {
      // Identity update - designation
      droidDesignation = cmd.substring(2);
      Serial.println("[Identity] Designation: " + droidDesignation);
    }
    else if (cmd.startsWith("O:")) {
      // Identity update - operator name
      operatorName = cmd.substring(2);
      Serial.println("[Identity] Operator: " + operatorName);
    }
  }
}

// =============================================================================
// Setup Function
// =============================================================================
// Runs once at startup to initialize hardware and network services.
// Students curious about how the droid works can explore this section to see:
//   - How servos are configured and calibrated
//   - How the WiFi access point is created
//   - How the web server and WebSocket are set up
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n== BB-R2 Workshop ==");
  Serial.println("Designation: " + droidDesignation + " (customizable via web UI)");
  Serial.println("Operator: " + operatorName + " (customizable via web UI)");
  Serial.println("Network: " + String(AP_SSID));
  
  // Initialize servo pins
  pinMode(LEFT_PIN, OUTPUT);
  pinMode(RIGHT_PIN, OUTPUT);
  pinMode(DOME_PIN, OUTPUT);
  digitalWrite(LEFT_PIN, LOW);
  digitalWrite(RIGHT_PIN, LOW);
  digitalWrite(DOME_PIN, LOW);
  delay(100);
  
  // Allocate PWM timers for servo control
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  
  // Configure servo refresh rate (50Hz is standard for hobby servos)
  leftServo.setPeriodHertz(50);
  rightServo.setPeriodHertz(50);
  domeServo.setPeriodHertz(50);
  
  // Attach servos to pins with pulse width range
  leftServo.attach(LEFT_PIN, 500, 2400);
  rightServo.attach(RIGHT_PIN, 500, 2400);
  domeServo.attach(DOME_PIN, 500, 2400);
  
  // Set servos to neutral/center positions
  leftServo.writeMicroseconds(1500 + LEFT_TRIM);
  rightServo.writeMicroseconds(1500 + RIGHT_TRIM);
  domeServo.write(90);
  Serial.println("[OK] Servos");
  
  // Create WiFi access point so students can connect directly
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  Serial.println("[OK] WiFi: " + String(AP_SSID));
  Serial.println("     IP: " + WiFi.softAPIP().toString());
  
  // Start mDNS for easy .local address access
  if (MDNS.begin(MDNS_NAME)) {
    Serial.println("[OK] mDNS: http://" + String(MDNS_NAME) + ".local");
  }
  
  // Set up WebSocket for real-time control
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  // Serve the web interface
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", HTML);
  });
  server.begin();
  Serial.println("[OK] Server on port 80");
  
  Serial.println("\n>> DROID ONLINE <<");
  Serial.println("Connect to: " + String(AP_SSID));
  Serial.println("Password: " + String(AP_PASS));
  Serial.println("Then open browser to: http://" + String(MDNS_NAME) + ".local\n");
}

// =============================================================================
// Main Loop
// =============================================================================
// Runs continuously to:
//   1. Handle WebSocket client cleanup
//   2. Implement watchdog safety (stops motors if connection lost)
//   3. Smooth servo movements using interpolation for natural motion
//
// Advanced students: The smoothing algorithm (lines with DRIVE_SMOOTH and 
// DOME_SMOOTH) blends current position with target position each loop cycle,
// creating gradual acceleration and deceleration.
// =============================================================================

void loop() {
  ws.cleanupClients();
  
  unsigned long now = millis();
  
  // Watchdog: stop motors if no commands received for WATCHDOG_MS
  if (now - lastCmd > WATCHDOG_MS && lastCmd > 0) {
    targetLeft = 0;
    targetRight = 0;
    currentLeft = 0;
    currentRight = 0;
    leftServo.writeMicroseconds(1500 + LEFT_TRIM);
    rightServo.writeMicroseconds(1500 + RIGHT_TRIM);
    lastCmd = 0;
  }
  
  // Update servo positions at regular intervals (LOOP_MS)
  if (now - lastLoop >= LOOP_MS) {
    lastLoop = now;
    
    // Smooth interpolation toward target positions
    currentLeft = currentLeft * (1.0 - DRIVE_SMOOTH) + targetLeft * DRIVE_SMOOTH;
    currentRight = currentRight * (1.0 - DRIVE_SMOOTH) + targetRight * DRIVE_SMOOTH;
    currentDome = currentDome * (1.0 - DOME_SMOOTH) + targetDome * DOME_SMOOTH;
    
    // Write smoothed values to servos
    leftServo.writeMicroseconds(1500 + LEFT_TRIM + (int)(currentLeft * 400));
    rightServo.writeMicroseconds(1500 + RIGHT_TRIM - (int)(currentRight * 400));
    domeServo.write((int)constrain(currentDome, 0, 180));
  }
}
