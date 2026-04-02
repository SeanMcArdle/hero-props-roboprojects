#include "web_server.h"
#include "config.h"
#include <AsyncTCP.h>

AsyncWebServer server(80);

// Global Variables (Bridge to Main Loop)
volatile int webDomeX = 100;
volatile int webDomeY = 100;
volatile int webDriveX = 0;
volatile int webDriveY = 0;
volatile int webCommandId = 0;
volatile int webRed = 0;
volatile int webGreen = 0;
volatile int webBlue = 0;
volatile unsigned long lastWebPacket = 0;

// The HTML (Embedded for simplicity - No SPIFFS required)
const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>Jellybean Command</title>
    <style>
        :root {
            --prince-bg: #150015;
            --prince-panel: #2a002a;
            --prince-purple: #9400D3; /* Dark Orchid */
            --prince-gold: #C0C0C0; /* Silver */
            --text-color: #E6E6FA; /* Lavender */
        }
        body { 
            background-color: var(--prince-bg); 
            color: var(--text-color); 
            font-family: 'Courier New', monospace; 
            text-align: center; 
            overflow: hidden; 
            touch-action: none; 
            margin: 0;
            position: fixed; /* Lock to viewport */
            top: 0; left: 0; right: 0; bottom: 0;
        }
        .container { 
            display: flex; 
            flex-direction: column; 
            height: 100%; 
            width: 100%;
            justify-content: space-between; 
            padding: 10px; 
            box-sizing: border-box; 
        }
        
        /* Header */
        .header { display: flex; justify-content: space-between; align-items: center; border-bottom: 2px solid var(--prince-purple); padding-bottom: 5px; margin-bottom: 10px; }
        .title { color: var(--prince-gold); font-weight: bold; font-size: 20px; letter-spacing: 2px; }
        .status { font-size: 12px; color: #555; font-weight: bold; }
        .connected { color: #00ff00; text-shadow: 0 0 5px #00ff00; }
        
        /* Joysticks Container */
        .controls-area { 
            display: flex; 
            flex: 1; 
            justify-content: space-around; 
            align-items: center; 
            width: 100%;
        }

        /* Joystick Zone */
        .zone { 
            position: relative; 
            width: 140px; 
            height: 140px; 
            background: radial-gradient(circle, var(--prince-panel) 0%, var(--prince-bg) 70%);
            border: 2px solid var(--prince-purple); 
            border-radius: 50%; 
            touch-action: none;
            box-shadow: 0 0 15px rgba(148, 0, 211, 0.2);
        }
        
        .label { 
            position: absolute; 
            bottom: -25px; 
            width: 100%; 
            text-align: center; 
            color: var(--prince-gold); 
            font-size: 12px; 
            text-transform: uppercase; 
        }

        /* The Stick */
        .knob { 
            width: 50px; 
            height: 50px; 
            background: radial-gradient(circle, var(--prince-gold) 0%, #696969 100%); 
            border-radius: 50%; 
            position: absolute;  
            top: 50%; 
            left: 50%; 
            transform: translate(-50%, -50%); 
            box-shadow: 0 0 10px var(--prince-gold);
            pointer-events: none; /* Let clicks pass to zone */
        }
        .knob.pilot { background: radial-gradient(circle, var(--prince-purple) 0%, #4B0082 100%); box-shadow: 0 0 10px var(--prince-purple); }

        /* Sliders */
        .slider-container {
            display: flex;
            flex-direction: column;
            gap: 5px;
            padding: 10px;
            background: var(--prince-panel);
            border-radius: 15px;
            border: 1px solid var(--prince-purple);
            margin-bottom: 10px;
        }
        .slider-row { display: flex; align-items: center; color: var(--prince-gold); font-size: 10px; }
        .slider-label { width: 40px; text-align: right; margin-right: 5px; }
        input[type=range] { 
            flex: 1; 
            accent-color: var(--prince-purple); 
            height: 40px; /* BIGGER TOUCH AREA */
            -webkit-appearance: none;
            background: rgba(255,255,255,0.1);
            border-radius: 20px;
        }
        input[type=range]::-webkit-slider-thumb {
            -webkit-appearance: none;
            height: 35px;
            width: 35px;
            border-radius: 50%;
            background: var(--prince-gold);
            cursor: pointer;
            box-shadow: 0 0 10px #000;
            border: 2px solid white;
            margin-top: 0px; 
        }

        /* Action Bar */
        .actions { 
            display: grid; 
            grid-template-columns: 1fr 1fr 1fr 1fr 1fr; 
            gap: 5px; 
            padding: 5px; 
            background: var(--prince-panel);
            border-radius: 15px;
            border: 1px solid var(--prince-purple);
        }
        .btn { 
            padding: 12px 2px; 
            font-size: 10px; 
            border: none; 
            border-radius: 8px; 
            font-weight: bold; 
            cursor: pointer; 
            background: #3a1a3a;
            color: var(--prince-gold);
            border: 1px solid var(--prince-gold);
            text-transform: uppercase;
        }
        .btn:active { background: var(--prince-gold); color: #000; transform: scale(0.95); }

        /* Debug Text / Console */
        .debug { display: none; } /* Hide old debug */
        
        .console-box {
            flex: 1; /* Take remaining space */
            background: #000;
            border: 1px solid #333;
            border-top: 2px solid var(--prince-purple);
            margin-top: 10px;
            padding: 8px;
            font-family: 'Courier New', monospace;
            font-size: 11px;
            text-align: left;
            overflow-y: hidden; /* Auto scroll layout */
            display: flex;
            flex-direction: column;
            justify-content: flex-end; /* Keep new items at bottom */
            opacity: 0.9;
            min-height: 100px;
        }

        .log-line { margin: 2px 0; }
        .log-drive { color: #FF8C00; } /* Dark Orange - legible */
        .log-dome { color: #00BFFF; } /* Deep Sky Blue - legible */
        .log-cmd { color: #32CD32; } /* Lime Green */

    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div id="displayTitle" class="title">DROID</div>
            <div id="status" class="status">CONNECTING...</div>
        </div>

        <div class="controls-area">
            <!-- Left Stick (Drive) -->
            <div class="zone" id="joyDrive">
                <div class="knob pilot" id="knobDrive"></div>
                <div class="label">PILOT</div>
            </div>

            <!-- Right Stick (Dome) -->
            <div class="zone" id="joyDome">
                <div class="knob" id="knobDome"></div>
                <div class="label">PERFORMER</div>
            </div>
        </div>

        <div class="slider-container">
            <div class="slider-row">
                <div class="slider-label" style="color:red">RED</div>
                <input type="range" min="0" max="255" value="0" id="slideR" oninput="sendColor()">
            </div>
            <div class="slider-row">
                <div class="slider-label" style="color:green">GRN</div>
                <input type="range" min="0" max="255" value="0" id="slideG" oninput="sendColor()">
            </div>
            <div class="slider-row">
                <div class="slider-label" style="color:cyan">BLU</div>
                <input type="range" min="0" max="255" value="0" id="slideB" oninput="sendColor()">
            </div>
        </div>

        <div class="actions">
            <!-- LED MODES -->
            <button class="btn" style="border-color:violet" onmousedown="sendCmd(10)">PULSE</button>
            <button class="btn" style="border-color:white" onmousedown="sendCmd(11)">PARTY</button>
            <button class="btn" style="border-color:orange" onmousedown="sendCmd(12)">RAINBOW</button>
            <button class="btn" style="border-color:cyan" onmousedown="sendCmd(13)">SCANNER</button>
            <button class="btn" style="border-color:gray" onmousedown="sendCmd(14)">MANUAL</button>
        </div>

        <div class="console-box" id="console">
            <div class="log-line" style="color:#666">System Ready...</div>
        </div>
    </div>

    <script>
        // WebSocket
        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;
        
        function log(msg, cls) {
            var c = document.getElementById('console');
            var line = document.createElement('div');
            line.className = 'log-line ' + cls;
            line.innerHTML = msg;
            c.appendChild(line);
            
            // Limit history to keep layout stable
            if(c.childElementCount > 6) {
                c.removeChild(c.firstChild);
            }
        }

        function initWebSocket() {
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
        }

        function onOpen(event) {
            document.getElementById('status').innerText = "CONNECTED";
            document.getElementById('status').classList.add("connected");
            websocket.send("CFG_REQ");
        }
        function onClose(event) {
            document.getElementById('status').innerText = "DISCONNECTED";
            document.getElementById('status').classList.remove("connected");
            setTimeout(initWebSocket, 2000);
        }

        function onMessage(event) {
            let msg = event.data;
            if (msg.startsWith("CFG:")) {
                document.getElementById('displayTitle').innerText = msg.substring(4);
            }
        }

        // Joystick Logic Factory
        function createJoystick(zoneId, knobId, prefix, isDrive) {
            const zone = document.getElementById(zoneId);
            const knob = document.getElementById(knobId);
            let isDragging = false;
            let inputX = 0;
            let inputY = 0;

            // Heartbeat: Send data at 10Hz to prevent watchdog timeout
            setInterval(() => {
                if (!isDragging) return;
                
                let rect = zone.getBoundingClientRect();
                let maxRadius = rect.width / 2 - 25; 

                // Normalize
                let valX, valY;
                if (isDrive) {
                    valX = Math.floor((inputX / maxRadius) * 100);
                    valY = Math.floor((inputY / maxRadius) * -100); // Invert Y
                } else {
                    valX = Math.floor((inputX / maxRadius) * 100) + 100;
                    valY = Math.floor((inputY / maxRadius) * 100) + 100;
                }

                sendData(prefix, valX, valY);
            }, 100);

            function updateVisuals(x, y) {
                // Visuals
                knob.style.transform = `translate(${x}px, ${y}px) translate(-50%, -50%)`; 
                inputX = x;
                inputY = y;
            }

            function handleDrag(e) {
                if (!isDragging) return;
                e.preventDefault();
                let clientX = e.touches ? e.touches[0].clientX : e.clientX;
                let clientY = e.touches ? e.touches[0].clientY : e.clientY;

                let rect = zone.getBoundingClientRect();
                let centerX = rect.width / 2;
                let centerY = rect.height / 2;

                let x = clientX - rect.left - centerX;
                let y = clientY - rect.top - centerY;
                
                let distance = Math.sqrt(x*x + y*y);
                let maxRadius = rect.width / 2 - 25; 
                if (distance > maxRadius) {
                    let angle = Math.atan2(y, x);
                    x = Math.cos(angle) * maxRadius;
                    y = Math.sin(angle) * maxRadius;
                }
                updateVisuals(x, y);
            }

            function start(e) { isDragging = true; handleDrag(e); }
            function end() { 
                isDragging = false; 
                knob.style.transform = `translate(-50%, -50%)`; // CSS Center
                if (isDrive) sendData(prefix, 0, 0);
                else sendData(prefix, 100, 100);
            }

            zone.addEventListener('mousedown', start);
            zone.addEventListener('touchstart', start, {passive: false});
            window.addEventListener('mousemove', handleDrag);
            window.addEventListener('touchmove', handleDrag, {passive: false});
            window.addEventListener('mouseup', end);
            window.addEventListener('touchend', end);
        }

        // Initialize Joysticks
        createJoystick('joyDrive', 'knobDrive', 'D', true);
        createJoystick('joyDome',  'knobDome',  'J', false);

        // Color Mixer Throttler
        let lastColorSend = 0;
        function sendColor() {
            let now = Date.now();
            if (now - lastColorSend < 100) return; // Limit to 10Hz
            lastColorSend = now;

            let r = document.getElementById('slideR').value;
            let g = document.getElementById('slideG').value;
            let b = document.getElementById('slideB').value;

            if (websocket.readyState === WebSocket.OPEN) {
                // Send "L:255,0,128"
                websocket.send(`L:${r},${g},${b}`);
            }
        }

        function sendData(prefix, x, y) {
            if (websocket.readyState === WebSocket.OPEN) {
                websocket.send(`${prefix}:${x},${y}`);
                
                let type = (prefix === 'D') ? "DRIVE" : "DOME";
                let cls = (prefix === 'D') ? "log-drive" : "log-dome";
                
                if (prefix === 'D' && x === 0 && y === 0) {
                     log(`${type} &#8594; STOP`, cls);
                } else if (prefix === 'J' && x === 100 && y === 100) {
                     log(`${type} &#8594; CENTER`, cls);
                } else {
                     let suffix = (prefix === 'D') ? "%" : ""; 
                     log(`${type} &#8594; X:${x}${suffix} Y:${y}${suffix}`, cls);
                }
            }
        }

        function sendCmd(id) {
            if (websocket.readyState === WebSocket.OPEN) {
                websocket.send(`C:${id}`);
                if (navigator.vibrate) navigator.vibrate(50);
                log(`CMD &#8594; ACTION ${id}`, "log-cmd");
            }
        }

        window.onload = initWebSocket;
    </script>
</body>
</html>
)rawliteral";

AsyncWebSocket ws("/ws");
uint32_t activeControllerId = 0;
bool hasActiveController = false;

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        if (!hasActiveController) {
            activeControllerId = client->id();
            hasActiveController = true;
            Serial.printf("🌐 WS CONNECT: client=%u (owner)\n", client->id());
        } else {
            Serial.printf("⛔ WS REJECT: client=%u (owner=%u)\n", client->id(), activeControllerId);
            client->text("BUSY");
            client->close();
        }
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("🌐 WS DISCONNECT: client=%u\n", client->id());
        if (hasActiveController && client->id() == activeControllerId) {
            hasActiveController = false;
            activeControllerId = 0;
            // Immediate stop if the active controller disconnects.
            webDriveX = 0;
            webDriveY = 0;
        }
    } else if (type == WS_EVT_DATA) {
        // Ignore packets from non-owner clients.
        if (hasActiveController && client->id() != activeControllerId) {
            return;
        }

        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            String msg;
            msg.reserve(len);
            for (size_t i = 0; i < len; ++i) {
                msg += (char)data[i];
            }

            // Debug Message Parsing
            Serial.print("Rx: "); Serial.println(msg);

            bool validControlPacket = false;

            if (msg.startsWith("CFG_REQ")) {
                 String cfg = "CFG:";
                 cfg += String(BOT_NAME);
                 client->text(cfg);
            }

            // Parse "J:150,150" or "C:1"
            if (msg.startsWith("D:")) {
                int comma = msg.indexOf(',');
                if (comma > 0) {
                    webDriveX = constrain(msg.substring(2, comma).toInt(), -100, 100); // Turn
                    webDriveY = constrain(msg.substring(comma+1).toInt(), -100, 100);  // Throttle
                    validControlPacket = true;
                }
            } else if (msg.startsWith("J:")) {
                int comma = msg.indexOf(',');
                if (comma > 0) {
                    webDomeX = constrain(msg.substring(2, comma).toInt(), 0, 200); // Pan (Spin)
                    webDomeY = constrain(msg.substring(comma+1).toInt(), 0, 200);  // Tilt (Not used yet?)
                    validControlPacket = true;
                }
            } else if (msg.startsWith("C:")) {
                webCommandId = constrain(msg.substring(2).toInt(), 0, 99);
                validControlPacket = true;
            } else if (msg.startsWith("L:")) {
                // Parse L:R,G,B
                int first = msg.indexOf(',');
                int second = msg.indexOf(',', first + 1);
                
                if (first > 0 && second > first) {
                    webRed = constrain(msg.substring(2, first).toInt(), 0, 255);
                    webGreen = constrain(msg.substring(first+1, second).toInt(), 0, 255);
                    webBlue = constrain(msg.substring(second+1).toInt(), 0, 255);
                    // Auto-trigger manual mode if user touches slider
                    webCommandId = 14; 
                    Serial.printf("🎨 LED MIX: R%d G%d B%d\n", webRed, webGreen, webBlue);
                    validControlPacket = true;
                }
            }

            // Heartbeat: Reset watchdog only for successfully parsed control packets.
            if (validControlPacket) {
                lastWebPacket = millis();
            }
        }
    }
}

void setupWebServer() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", html_page);
    });

    server.begin();
}

void serviceWebServer() {
    ws.cleanupClients();
}
