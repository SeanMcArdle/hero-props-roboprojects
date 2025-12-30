/*
 * BB-R2 Workshop - Working Version
 * Pins: 25=Left, 26=Right, 27=Dome
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

const char* DROID_ID = "BK-00";
const char* AP_SSID = "R2-BK00";
const char* AP_PASS = "BK00droid";

#define LEFT_PIN   25
#define RIGHT_PIN  26
#define DOME_PIN   27

#define LEFT_TRIM    3
#define RIGHT_TRIM   3

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

Servo leftServo;
Servo rightServo;
Servo domeServo;

float targetLeft = 0;
float targetRight = 0;
float targetDome = 90;

float currentLeft = 0;
float currentRight = 0;
float currentDome = 90;

#define DRIVE_SMOOTH 0.15
#define DOME_SMOOTH 0.1

unsigned long lastCmd = 0;
unsigned long lastLoop = 0;
#define WATCHDOG_MS 2000
#define LOOP_MS 20

const char* HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>R2-BK00</title>
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
    h1 { text-align: center; font-size: 1.5em; margin-bottom: 5px; }
    .status { text-align: center; font-size: 0.8em; color: #666; margin-bottom: 10px; }
    .status.connected { color: #0c0; }
    
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
      height: 50px;
      background: #0a1220;
      border: 1px solid #1e3a5f;
      border-radius: 5px;
      padding: 5px;
      font-family: monospace;
      font-size: 0.65em;
      overflow-y: auto;
      color: #666;
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <h1>R2-BK00</h1>
  <div class="status" id="status">CONNECTING...</div>
  
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

  <script>
    let ws;
    const joy = document.getElementById('joy');
    const stick = document.getElementById('stick');
    const dome = document.getElementById('dome');
    const knob = document.getElementById('knob');
    const status = document.getElementById('status');
    const log = document.getElementById('log');
    
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
        addLog('Connected');
      };
      ws.onclose = () => {
        status.textContent = 'DISCONNECTED';
        status.className = 'status';
        setTimeout(connect, 1000);
      };
      ws.onmessage = (e) => addLog(e.data);
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
    
    function addLog(msg) {
      log.innerHTML += msg + '<br>';
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
    
    function joyEnd() {
      if (!joyActive) return;
      joyActive = false;
      stick.style.left = '50%';
      stick.style.top = '50%';
      currentDx = 0;
      currentDy = 0;
      sendNow('D:0,0');
    }
    
    joy.onmousedown = (e) => { joyActive = true; joyMove(e.clientX, e.clientY); };
    joy.ontouchstart = (e) => { joyActive = true; e.preventDefault(); joyMove(e.touches[0].clientX, e.touches[0].clientY); };
    document.onmousemove = (e) => { if (joyActive) joyMove(e.clientX, e.clientY); };
    document.ontouchmove = (e) => { if (joyActive) { e.preventDefault(); joyMove(e.touches[0].clientX, e.touches[0].clientY); } };
    document.onmouseup = joyEnd;
    document.ontouchend = joyEnd;
    
    let domeActive = false;
    
    function domeMove(cx) {
      const r = dome.getBoundingClientRect();
      const margin = 28;
      let x = cx - r.left;
      x = Math.max(margin, Math.min(r.width - margin, x));
      knob.style.left = x + 'px';
      knob.style.top = '50%';
      const val = (x - margin) / (r.width - 2*margin);
      send('M:' + val.toFixed(2));
    }
    
    dome.onmousedown = (e) => { domeActive = true; domeMove(e.clientX); };
    dome.ontouchstart = (e) => { domeActive = true; e.preventDefault(); domeMove(e.touches[0].clientX); };
    document.addEventListener('mousemove', (e) => { if (domeActive) domeMove(e.clientX); });
    document.addEventListener('touchmove', (e) => { if (domeActive) { e.preventDefault(); domeMove(e.touches[0].clientX); } }, {passive:false});
    document.addEventListener('mouseup', () => domeActive = false);
    document.addEventListener('touchend', () => domeActive = false);
    
    document.addEventListener('contextmenu', e => e.preventDefault());
    connect();
  </script>
</body>
</html>
)rawliteral";

void onWsEvent(AsyncWebSocket* srv, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("[WS] Client connected");
    ws.textAll("Connected to " + String(DROID_ID));
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
      float v = cmd.substring(2).toFloat();
      targetDome = v * 180.0f;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n== BB-R2 Workshop ==");
  Serial.println("Droid: " + String(DROID_ID));
  
  pinMode(LEFT_PIN, OUTPUT);
  pinMode(RIGHT_PIN, OUTPUT);
  pinMode(DOME_PIN, OUTPUT);
  digitalWrite(LEFT_PIN, LOW);
  digitalWrite(RIGHT_PIN, LOW);
  digitalWrite(DOME_PIN, LOW);
  delay(100);
  
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  
  leftServo.setPeriodHertz(50);
  rightServo.setPeriodHertz(50);
  domeServo.setPeriodHertz(50);
  
  leftServo.attach(LEFT_PIN, 500, 2400);
  rightServo.attach(RIGHT_PIN, 500, 2400);
  domeServo.attach(DOME_PIN, 500, 2400);
  
  leftServo.writeMicroseconds(1500 + LEFT_TRIM);
  rightServo.writeMicroseconds(1500 + RIGHT_TRIM);
  domeServo.write(90);
  Serial.println("[OK] Servos");
  
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  Serial.println("[OK] WiFi: " + String(AP_SSID));
  Serial.println("     IP: " + WiFi.softAPIP().toString());
  
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", HTML);
  });
  server.begin();
  Serial.println("[OK] Server on port 80");
  
  Serial.println("\n>> DROID ONLINE <<\n");
}

void loop() {
  ws.cleanupClients();
  
  unsigned long now = millis();
  
  if (now - lastCmd > WATCHDOG_MS && lastCmd > 0) {
    targetLeft = 0;
    targetRight = 0;
    currentLeft = 0;
    currentRight = 0;
    leftServo.writeMicroseconds(1500 + LEFT_TRIM);
    rightServo.writeMicroseconds(1500 + RIGHT_TRIM);
    lastCmd = 0;
  }
  
  if (now - lastLoop >= LOOP_MS) {
    lastLoop = now;
    
    currentLeft = currentLeft * (1.0 - DRIVE_SMOOTH) + targetLeft * DRIVE_SMOOTH;
    currentRight = currentRight * (1.0 - DRIVE_SMOOTH) + targetRight * DRIVE_SMOOTH;
    currentDome = currentDome * (1.0 - DOME_SMOOTH) + targetDome * DOME_SMOOTH;
    
    leftServo.writeMicroseconds(1500 + LEFT_TRIM + (int)(currentLeft * 400));
    rightServo.writeMicroseconds(1500 + RIGHT_TRIM - (int)(currentRight * 400));
    domeServo.write((int)constrain(currentDome, 0, 180));
  }
}
