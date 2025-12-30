/*
 * BB-R2 WiFi Controller v12-fixed
 * - Gemini fixes applied: longer timeout, microseconds dome, better smoothing
 * - Seán's actual pins: 13, 12, 14
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESP32Servo.h>

// ============== CHANGE THESE FOR EACH DROID ==============
const char* DROID_NAME = "R2-BK00";
const char* WIFI_PASSWORD = "droidBK00";
// =========================================================

// Seán's actual wiring - verified working pins
#define LEFT_PIN 13
#define RIGHT_PIN 12
#define DOME_PIN 14

#define STOP 90
#define MAX_SPEED 30
#define DOME_MIN 1000     // Microseconds (safe range)
#define DOME_MAX 2000     // Microseconds (safe range)

Servo servoL, servoR, servoD;
WebServer server(80);
WebSocketsServer ws(81);

// Raw input from joystick
float inX = 0, inY = 0, inD = 0;

// Smoothed values
float smoothX = 0, smoothY = 0, smoothD = 0;
float currentMicroSecs = 1500;  // Dome position in microseconds

int leftOut = STOP, rightOut = STOP;
unsigned long lastIn = 0;
String lastLog = "";

// Tuning - Gemini recommended values
#define DRIVE_SMOOTHING 0.15
#define DOME_SMOOTHING 0.20
#define SAFETY_TIMEOUT 1000   // 1 second - fixes WiFi stutter

void logMsg(String msg) {
    Serial.println(msg);
    lastLog = msg;
}

void updateServos() {
    // Timeout - stop if no input (1 second window)
    if (millis() - lastIn > SAFETY_TIMEOUT) {
        inX = 0;
        inY = 0;
    }
    
    // SMOOTH the drive inputs
    smoothX += (inX - smoothX) * DRIVE_SMOOTHING;
    smoothY += (inY - smoothY) * DRIVE_SMOOTHING;
    
    // Dead zone on smoothed values
    if (abs(smoothX) < 0.05) smoothX = 0;
    if (abs(smoothY) < 0.05) smoothY = 0;
    
    // Calculate motor outputs from smoothed values
    int throttle = (int)(smoothY * MAX_SPEED);
    int steering = (int)(smoothX * MAX_SPEED);
    leftOut = STOP - throttle + steering;
    rightOut = STOP + throttle + steering;
    leftOut = constrain(leftOut, 0, 180);
    rightOut = constrain(rightOut, 0, 180);
    
    // Drive servos
    if (abs(smoothX) > 0.05 || abs(smoothY) > 0.05) {
        if (!servoL.attached()) servoL.attach(LEFT_PIN);
        if (!servoR.attached()) servoR.attach(RIGHT_PIN);
        servoL.write(leftOut);
        servoR.write(rightOut);
    } else {
        leftOut = STOP;
        rightOut = STOP;
        servoL.write(STOP);
        servoR.write(STOP);
    }
    
    // DOME - single smooth pass
    float microSecTarget = map(inD * 100, -100, 100, DOME_MIN, DOME_MAX);
    currentMicroSecs += (microSecTarget - currentMicroSecs) * DOME_SMOOTHING;
    servoD.writeMicroseconds((int)currentMicroSecs);
}

void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
    if (type == WStype_CONNECTED) {
        logMsg("[WS] Connected");
    }
    else if (type == WStype_DISCONNECTED) {
        logMsg("[WS] Disconnected");
        inX = 0; inY = 0;
    }
    else if (type == WStype_TEXT) {
        String msg = String((char*)payload);
        
        // Hero Props format: joy:x,y
        if (msg.startsWith("joy:")) {
            int comma = msg.indexOf(',');
            if (comma > 4) {
                inX = msg.substring(4, comma).toFloat();
                inY = msg.substring(comma + 1).toFloat();
                lastIn = millis();
            }
        }
        // Hero Props format: dome:value
        else if (msg.startsWith("dome:")) {
            inD = msg.substring(5).toFloat();
            lastIn = millis();
        }
        // Hero Props format: reset
        else if (msg == "reset") {
            inX = 0; inY = 0; inD = 0;
            lastIn = millis();
        }
        // Original JSON format: {"joy":"drive","x":...}
        else if (msg.indexOf("\"drive\"") > 0) {
            int xPos = msg.indexOf("\"x\":");
            int yPos = msg.indexOf("\"y\":");
            if (xPos > 0 && yPos > 0) {
                inX = msg.substring(xPos + 4).toFloat();
                inY = msg.substring(yPos + 4).toFloat();
                lastIn = millis();
            }
        }
        // Original JSON format: {"joy":"dome","x":...}
        else if (msg.indexOf("\"dome\"") > 0) {
            int xPos = msg.indexOf("\"x\":");
            if (xPos > 0) {
                inD = msg.substring(xPos + 4).toFloat();
                lastIn = millis();
            }
        }
    }
}

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>BK-00 | Hero Props</title>
  <style>
    * {margin:0;padding:0;box-sizing:border-box;user-select:none;-webkit-user-select:none;}
    html,body{width:100%;height:100%;overflow:hidden;}
    body{font-family:Arial,sans-serif;background:linear-gradient(180deg,#0a1628 0%,#12243d 100%);color:#ff6b35;display:flex;flex-direction:column;padding:15px;touch-action:manipulation;}
    h1{text-align:center;font-size:2.2em;color:#ff6b35;margin-bottom:2px;letter-spacing:2px;}
    .operator{text-align:center;color:#3a8dde;font-size:0.85em;letter-spacing:2px;margin-bottom:4px;}
    .subtitle{text-align:center;color:#ff6b35;font-size:0.75em;letter-spacing:3px;margin-bottom:8px;}
    .status{text-align:center;font-size:0.9em;margin-bottom:10px;color:#4a4a4a;}
    .status.connected{color:#00cc66;}
    .controls-row{display:flex;justify-content:space-between;align-items:center;padding:0 20px;margin:5px 0;}
    .control-group{display:flex;flex-direction:column;align-items:center;}
    .control-group h3{color:#ff6b35;font-size:0.85em;letter-spacing:2px;margin-bottom:10px;}
    .joystick{width:160px;height:160px;background:radial-gradient(circle at 30% 30%,#2a4a6a 0%,#12243d 100%);border:3px solid #1e3a5f;border-radius:50%;position:relative;touch-action:none;}
    .joystick-stick{width:60px;height:60px;background:radial-gradient(circle at 30% 30%,#ff8c5a 0%,#ff6b35 50%,#cc4a1a 100%);border-radius:50%;position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);box-shadow:0 4px 15px rgba(255,107,53,0.4);}
    .dome-slider{width:160px;height:50px;background:radial-gradient(ellipse at 30% 30%,#2a4a6a 0%,#12243d 100%);border:3px solid #1e3a5f;border-radius:25px;position:relative;touch-action:none;}
    .dome-knob{width:44px;height:44px;background:radial-gradient(circle at 30% 30%,#ff8c5a 0%,#ff6b35 50%,#cc4a1a 100%);border-radius:50%;position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);box-shadow:0 4px 15px rgba(255,107,53,0.4);}
    .telemetry-center{display:flex;justify-content:center;align-items:center;}
    .telemetry-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;}
    .telemetry-box{background:transparent;border:2px solid #1e3a5f;border-radius:8px;padding:4px 12px;text-align:center;min-width:50px;}
    .telemetry-box .label{font-size:0.55em;color:#3a8dde;letter-spacing:1px;}
    .telemetry-box .value{font-size:1em;color:#3a8dde;font-weight:bold;}
    .button-row{display:flex;justify-content:space-between;padding:0 20px;margin:8px 0;}
    .stop-btn{background:#990000;border:2px solid #cc0000;color:white;padding:12px 25px;border-radius:5px;font-size:0.9em;font-weight:bold;cursor:pointer;}
    .stop-btn:active{background:#cc0000;}
    .reset-btn{background:#006600;border:2px solid #009900;color:white;padding:12px 25px;border-radius:5px;font-size:0.9em;font-weight:bold;cursor:pointer;}
    .reset-btn:active{background:#009900;}
    .serial-container{margin-top:8px;width:100%;flex:1;display:flex;flex-direction:column;}
    .serial-label{color:#ff6b35;font-size:0.65em;letter-spacing:2px;margin-bottom:4px;}
    .serial-log{background:#0a1220;border:1px solid #1e3a5f;border-radius:5px;padding:6px;font-family:'Courier New',monospace;font-size:0.65em;flex:1;width:100%;overflow-y:auto;color:#888;}
    @media (orientation:landscape){body{padding:8px 15px;}h1{font-size:1.6em;margin-bottom:0;}.subtitle{font-size:0.6em;margin-bottom:4px;}.status{font-size:0.75em;margin-bottom:4px;}.joystick{width:130px;height:130px;}.joystick-stick{width:50px;height:50px;}.dome-slider{width:130px;height:45px;}.dome-knob{width:38px;height:38px;}.serial-log{height:50px;}}
  </style>
</head>
<body>
  <h1>BK-00</h1>
  <p class="subtitle">HERO PROPS ROBOT CONTROL</p>
  <div id="status" class="status">CONNECTING...</div>
  <div class="controls-row">
    <div class="control-group">
      <h3>DRIVE</h3>
      <div class="joystick" id="joystick"><div class="joystick-stick" id="stick"></div></div>
    </div>
    <div class="telemetry-center">
      <div class="telemetry-grid">
        <div class="telemetry-box"><div class="label">LEFT</div><div class="value" id="tel-left">0</div></div>
        <div class="telemetry-box"><div class="label">RIGHT</div><div class="value" id="tel-right">0</div></div>
        <div class="telemetry-box"><div class="label">DOME</div><div class="value" id="tel-dome">90</div></div>
        <div class="telemetry-box"><div class="label">X</div><div class="value" id="input-x">0</div></div>
        <div class="telemetry-box"><div class="label">Y</div><div class="value" id="input-y">0</div></div>
        <div class="telemetry-box"><div class="label">D</div><div class="value" id="input-d">0</div></div>
      </div>
    </div>
    <div class="control-group">
      <h3>DOME</h3>
      <div class="dome-slider" id="dome-slider"><div class="dome-knob" id="dome-knob"></div></div>
    </div>
  </div>
  <div class="button-row">
    <button class="stop-btn" onclick="stopAll()">E-STOP</button>
    <button class="reset-btn" onclick="resetEstop()">RESET</button>
  </div>
  <div class="serial-container">
    <div class="serial-label">SERIAL MONITOR</div>
    <div class="serial-log" id="serial-log"></div>
  </div>
<script>
let ws,wsConnected=false,curX=0,curY=0,curD=0,joyActive=false,domeActive=false;

function connect(){
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.onopen=function(){wsConnected=true;document.getElementById('status').textContent='CONNECTED';document.getElementById('status').className='status connected';addLog('Connected');};
  ws.onclose=function(){wsConnected=false;document.getElementById('status').textContent='DISCONNECTED';document.getElementById('status').className='status';setTimeout(connect,2000);};
  ws.onmessage=function(e){try{var d=JSON.parse(e.data);if(d.left!==undefined){document.getElementById('tel-left').textContent=d.left;document.getElementById('tel-right').textContent=d.right;document.getElementById('tel-dome').textContent=d.dome;}}catch(x){}};
}

function send(m){if(wsConnected)ws.send(m);}
function addLog(m){var l=document.getElementById('serial-log');l.innerHTML+='> '+m+'<br>';l.scrollTop=l.scrollHeight;}

function sendJoy(x,y){
  send('joy:'+x.toFixed(2)+','+y.toFixed(2));
  document.getElementById('input-x').textContent=x.toFixed(2);
  document.getElementById('input-y').textContent=y.toFixed(2);
}

function sendDome(d){
  send('dome:'+d.toFixed(2));
  document.getElementById('input-d').textContent=Math.round((d+1)*90);
}

function stopAll(){curX=0;curY=0;sendJoy(0,0);rstJoy();addLog('E-STOP');}
function resetEstop(){send('reset');addLog('RESET');}

// JOYSTICK
var joy=document.getElementById('joystick'),stick=document.getElementById('stick');
function joyMove(cx,cy){
  var r=joy.getBoundingClientRect(),rad=r.width/2-30;
  var dx=cx-r.left-r.width/2,dy=cy-r.top-r.height/2;
  var dist=Math.hypot(dx,dy);if(dist>rad){dx=dx/dist*rad;dy=dy/dist*rad;}
  stick.style.left=(50+dx/r.width*100)+'%';stick.style.top=(50+dy/r.height*100)+'%';
  curX=dx/rad;curY=-dy/rad;sendJoy(curX,curY);
}
function rstJoy(){stick.style.left='50%';stick.style.top='50%';curX=0;curY=0;sendJoy(0,0);}
joy.onmousedown=joy.ontouchstart=function(e){e.preventDefault();joyActive=true;var t=e.touches?e.touches[0]:e;joyMove(t.clientX,t.clientY);};
document.addEventListener('mousemove',function(e){if(joyActive)joyMove(e.clientX,e.clientY);});
document.addEventListener('touchmove',function(e){if(joyActive){e.preventDefault();joyMove(e.touches[0].clientX,e.touches[0].clientY);}},{passive:false});
document.addEventListener('mouseup',function(){if(joyActive){joyActive=false;rstJoy();}});
document.addEventListener('touchend',function(){if(joyActive){joyActive=false;rstJoy();}});

// DOME
var dslider=document.getElementById('dome-slider'),dknob=document.getElementById('dome-knob');
function domeMove(cx){
  var r=dslider.getBoundingClientRect(),margin=25;
  var x=Math.max(margin,Math.min(r.width-margin,cx-r.left));
  dknob.style.left=x+'px';
  curD=(x-margin)/(r.width-2*margin)*2-1;
  sendDome(curD);
}
dslider.onmousedown=dslider.ontouchstart=function(e){e.preventDefault();domeActive=true;var t=e.touches?e.touches[0]:e;domeMove(t.clientX);};
document.addEventListener('mousemove',function(e){if(domeActive)domeMove(e.clientX);});
document.addEventListener('touchmove',function(e){if(domeActive){e.preventDefault();domeMove(e.touches[0].clientX);}},{passive:false});
document.addEventListener('mouseup',function(){domeActive=false;});
document.addEventListener('touchend',function(){domeActive=false;});

// KEEPALIVES
setInterval(function(){if(joyActive)sendJoy(curX,curY);if(domeActive)sendDome(curD);},200);

connect();
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
    String page = FPSTR(PAGE);
    page.replace("%DROID%", DROID_NAME);
    server.send(200, "text/html", page);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== " + String(DROID_NAME) + " v12-fixed ===\n");
    
    // Initialize dome servo
    servoD.attach(DOME_PIN);
    servoD.writeMicroseconds(1500);  // Center
    currentMicroSecs = 1500;
    logMsg("[BOOT] Ready");
    
    WiFi.softAP(DROID_NAME, WIFI_PASSWORD);
    WiFi.setSleep(false);  // Disable power saving - fixes periodic jerk
    logMsg("[WIFI] " + String(DROID_NAME));
    logMsg("[PASS] " + String(WIFI_PASSWORD));
    
    server.on("/", handleRoot);
    server.begin();
    ws.begin();
    ws.onEvent(wsEvent);
}

void loop() {
    server.handleClient();
    ws.loop();
    updateServos();
    
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 100) {
        lastSend = millis();
        String json = "{\"left\":" + String(leftOut - STOP);
        json += ",\"right\":" + String(rightOut - STOP);
        json += ",\"dome\":" + String((int)map(currentMicroSecs, DOME_MIN, DOME_MAX, 0, 180));
        json += ",\"inX\":" + String(inX, 2);
        json += ",\"inY\":" + String(inY, 2);
        json += ",\"inD\":" + String(inD, 2);
        json += ",\"log\":\"" + lastLog + "\"}";
        ws.broadcastTXT(json);
        lastLog = "";
    }
    delay(1);  // Gemini fix: reduced from delay(10)
}