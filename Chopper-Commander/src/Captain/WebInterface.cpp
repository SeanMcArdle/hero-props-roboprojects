#include "WebInterface.h"

#ifdef ROLE_CAPTAIN

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Initialize static member
WebInput WebInterface::_input = {0, 0, 0, 0, 0, false};

const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>CHOPPER CMD V3</title>
    <style>
        :root { --bg: #1a1a1a; --acc: #ff9d00; --txt: #eee; }
        body { background: var(--bg); color: var(--txt); font-family: monospace; overflow: hidden; margin: 0; touch-action: none; text-align: center; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; grid-template-rows: 1fr 100px; height: 100vh; width: 100vw; }
        .zone { position: relative; border: 2px solid #333; touch-action: none; }
        .knob { width: 60px; height: 60px; background: var(--acc); border-radius: 50%; position: absolute; top:50%; left:50%; transform: translate(-50%, -50%); opacity: 0.8;}
        .panel { grid-column: span 2; display: flex; justify-content: space-around; align-items: center; border-top: 2px solid #333; background: #222; }
        .btn { background: #444; color: var(--acc); border: 2px solid var(--acc); padding: 10px 20px; font-size: 16px; border-radius: 5px; }
        .btn:active { background: var(--acc); color: #000; }
        .estop { border-color: red; color: red; }
        .estop:active { background: red; color: white; }
    </style>
</head>
<body>
    <div class="grid">
        <div id="driveZone" class="zone"><div id="driveKnob" class="knob"></div><br>DRIVE</div>
        <div id="domeZone" class="zone"><div id="domeKnob" class="knob"></div><br>DOME</div>
        <div class="panel">
            <button class="btn" onclick="sendAction(1)">SND 1</button>
            <button class="btn" onclick="sendAction(2)">SND 2</button>
            <button class="btn" onclick="sendAction(3)">SND 3</button>
            <button class="btn estop" onclick="sendEStop()">STOP</button>
        </div>
    </div>
<script>
    var ws = new WebSocket('ws://' + window.location.hostname + '/ws');
    
    function setupJoystick(zoneId, knobId, type) {
        var zone = document.getElementById(zoneId);
        var knob = document.getElementById(knobId);
        var active = false;
        var startX, startY;

        zone.addEventListener('touchstart', function(e) {
            e.preventDefault();
            active = true;
            startX = e.touches[0].clientX;
            startY = e.touches[0].clientY;
        });

        zone.addEventListener('touchmove', function(e) {
            if (!active) return;
            e.preventDefault();
            var x = e.touches[0].clientX - startX;
            var y = e.touches[0].clientY - startY;
            var dist = Math.sqrt(x*x + y*y);
            var maxDist = 50;
            if (dist > maxDist) {
                x = (x / dist) * maxDist;
                y = (y / dist) * maxDist;
            }
            knob.style.transform = `translate(calc(-50% + ${x}px), calc(-50% + ${y}px))`;
            
            // Normalize -100 to 100
            var normX = Math.round((x / maxDist) * 100);
            var normY = Math.round((y / maxDist) * -100); // Invert Y
            
            // Debounce/Throttle could go here
            if(ws.readyState === 1) ws.send(`${type}:${normX},${normY}`);
        });

        var end = function(e) {
            active = false;
            knob.style.transform = `translate(-50%, -50%)`;
            if(ws.readyState === 1) ws.send(`${type}:0,0`);
        };
        zone.addEventListener('touchend', end);
        zone.addEventListener('touchcancel', end);
    }

    setupJoystick('driveZone', 'driveKnob', 'M');
    setupJoystick('domeZone', 'domeKnob', 'D');

    function sendAction(id) { if(ws.readyState === 1) ws.send(`A:${id}`); }
    function sendEStop() { if(ws.readyState === 1) ws.send(`E:STOP`); }

</script>
</body>
</html>
)rawliteral";

void WebInterface::begin() {
    server.on("/", HTTP_GET, handleRoot);
    server.onNotFound(handleNotFound);
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
}

void WebInterface::handleRoot(AsyncWebServerRequest *request) {
    request->send(200, "text/html", html_page);
}

void WebInterface::handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void WebInterface::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            handleWebSocketMessage(arg, data, len);
        }
    }
}

void WebInterface::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    data[len] = 0; // Null terminate
    char* str = (char*)data;
    
    // Protocol:
    // M:x,y (Move)
    // D:x,y (Dome)
    // A:id  (Action)
    // E:STOP (Estop)

    if (str[0] == 'M') {
        int x, y;
        if(sscanf(str, "M:%d,%d", &x, &y) == 2) {
            _input.driveX = x;
            _input.driveY = y;
        }
    } else if (str[0] == 'D') {
        int x, y;
        if(sscanf(str, "D:%d,%d", &x, &y) == 2) {
            _input.domeX = x;
            _input.domeY = y;
        }
    } else if (str[0] == 'A') {
        int id;
        if(sscanf(str, "A:%d", &id) == 1) {
            _input.actionId = id;
        }
    } else if (str[0] == 'E') {
        _input.eStop = true;
    }
}

WebInput WebInterface::getInput() {
    WebInput temp = _input;
    // Clear one-shot triggers
    _input.actionId = 0;
    _input.eStop = false; 
    return temp;
}
#endif
