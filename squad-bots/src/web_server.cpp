#include "web_server.h"
#include <AsyncTCP.h>

AsyncWebServer server(80);

// Global Variables (Bridge to Main Loop)
volatile int webDomeX = 100;
volatile int webDomeY = 100;
volatile int webDriveX = 0;
volatile int webDriveY = 0;
volatile int webCommandId = 0;
volatile unsigned long lastWebPacket = 0;

// The HTML (Embedded for simplicity - No SPIFFS required)
const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>SquadBot Command</title>
    <style>
        body { background-color: #222; color: white; font-family: sans-serif; text-align: center; overflow: hidden; touch-action: none; }
        .container { display: flex; flex-direction: column; height: 100vh; justify-content: space-around; }
        .status { font-size: 14px; margin-top: 5px; color: #00ff00; }
        
        .zone { display: flex; justify-content: center; align-items: center; flex: 1; border: 1px dashed #555; position: relative; }
        .label { position: absolute; top: 10px; left: 10px; color: #888; font-size: 12px; }
        
        /* Joystick */
        .joystick-area { width: 250px; height: 250px; background: rgba(255,255,255,0.1); border-radius: 50%; position: relative; touch-action: none; }
        .knob { width: 80px; height: 80px; background: #0088ff; border-radius: 50%; position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); box-shadow: 0 0 15px #0088ff; }

        /* Buttons */
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; width: 90%; max-width: 400px; }
        .btn { padding: 20px; font-size: 18px; border: none; border-radius: 10px; font-weight: bold; cursor: pointer; }
        .btn:active { transform: scale(0.95); opacity: 0.8; }
        .btn-happy { background: linear-gradient(145deg, #00ff00, #00aa00); color: black; }
        .btn-sad { background: linear-gradient(145deg, #00aaff, #0055aa); color: black; }
        .btn-angry { background: linear-gradient(145deg, #ff4444, #aa0000); color: white; }
        .btn-dance { background: linear-gradient(145deg, #ffff00, #aaaa00); color: black; }

    </style>
</head>
<body>
    <div class="container">
        <div id="status" class="status">Connecting...</div>

        <!-- Dome Joystick -->
        <div class="zone">
            <div class="label">PERFORMER (DOME)</div>
            <div id="joyParams" style="display:none">0,0</div>
            <div id="joystick" class="joystick-area">
                <div id="knob" class="knob"></div>
            </div>
        </div>

        <!-- Action Pad -->
        <div class="zone">
            <div class="label">ACTIONS</div>
            <div class="grid">
                <button class="btn btn-happy" onmousedown="sendCmd(1)">HAPPY</button>
                <button class="btn btn-sad" onmousedown="sendCmd(2)">SAD</button>
                <button class="btn btn-angry" onmousedown="sendCmd(3)">ANGRY</button>
                <button class="btn btn-dance" onmousedown="sendCmd(4)">DANCE</button>
            </div>
        </div>
    </div>

    <script>
        // WebSocket
        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;
        
        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage;
        }

        function onOpen(event) {
            document.getElementById('status').innerText = "CONNECTED";
            document.getElementById('status').style.color = "#00ff00";
        }
        function onClose(event) {
            document.getElementById('status').innerText = "DISCONNECTED";
            document.getElementById('status').style.color = "red";
            setTimeout(initWebSocket, 2000);
        }
        function onMessage(event) { 
            // Handle feedback
        }

        // Joystick Logic
        const joystick = document.getElementById('joystick');
        const knob = document.getElementById('knob');
        let rect = joystick.getBoundingClientRect();
        let centerX = rect.width / 2;
        let centerY = rect.height / 2;
        let isDragging = false;
        let limiter = 0; // Rate limiter for slow wifi

        joystick.addEventListener('touchstart', startDrag, {passive: false});
        joystick.addEventListener('touchmove', drag, {passive: false});
        joystick.addEventListener('touchend', endDrag);
        
        // Mouse support for testing
        joystick.addEventListener('mousedown', startDrag);
        document.addEventListener('mousemove', drag);
        document.addEventListener('mouseup', endDrag);

        function startDrag(e) {
            isDragging = true;
            drag(e);
        }

        function drag(e) {
            if (!isDragging) return;
            e.preventDefault();
            
            let clientX, clientY;
            if(e.touches) {
                clientX = e.touches[0].clientX;
                clientY = e.touches[0].clientY;
            } else {
                clientX = e.clientX;
                clientY = e.clientY;
            }

            rect = joystick.getBoundingClientRect();
            // Recalculate center
            centerX = rect.width / 2;
            centerY = rect.height / 2;

            let x = clientX - rect.left - centerX;
            let y = clientY - rect.top - centerY;
            
            // Limit Radius
            let distance = Math.sqrt(x*x + y*y);
            let maxRadius = rect.width / 2 - 40; // 40 is knob radius
            if (distance > maxRadius) {
                let angle = Math.atan2(y, x);
                x = Math.cos(angle) * maxRadius;
                y = Math.sin(angle) * maxRadius;
            }

            // Update Knob
            knob.style.transform = `translate(${x - 40}px, ${y - 40}px)`; // -40 centers knob

            // Send data (Rate limited)
            limiter++;
            if (limiter % 3 === 0) {
                // Map to 0-200
                let mapX = Math.floor((x / maxRadius) * 100) + 100;
                let mapY = Math.floor((y / maxRadius) * 100) + 100;
                sendJoy(mapX, mapY);
            }
        }

        function endDrag() {
            isDragging = false;
            knob.style.transform = `translate(-50%, -50%)`; // Back to center CSS
            sendJoy(100, 100);
        }

        function sendJoy(x, y) {
            if (websocket.readyState === WebSocket.OPEN) {
                websocket.send(`J:${x},${y}`);
            }
        }

        function sendCmd(id) {
            if (websocket.readyState === WebSocket.OPEN) {
                websocket.send(`C:${id}`);
                // Haptic feedback if available
                if (navigator.vibrate) navigator.vibrate(50);
            }
        }

        window.onload = initWebSocket;
    </script>
</body>
</html>
)rawliteral";

AsyncWebSocket ws("/ws");

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DISCONNECT) {
        // Immediate stop on disconnect
        webDriveX = 0;
        webDriveY = 0;
    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            data[len] = 0;
            String msg = (char*)data;
            
            // Heartbeat: Reset watchdog timer on valid data
            if (msg.startsWith("D:") || msg.startsWith("J:") || msg.startsWith("C:")) {
                lastWebPacket = millis();
            }

            // Parse "J:150,150" or "C:1"
            if (msg.startsWith("J:")) {
                int comma = msg.indexOf(',');
                if (comma > 0) {
                    webDomeX = msg.substring(2, comma).toInt(); // Pan (Spin)
                    webDomeY = msg.substring(comma+1).toInt();  // Tilt (Not used yet?)
                }
            } else if (msg.startsWith("C:")) {
                webCommandId = msg.substring(2).toInt();
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
