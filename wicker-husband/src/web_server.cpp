// -------------------------------------------------------------------------
// web_server.cpp — Local WiFi control UI for Wicker Husband eyes
// -------------------------------------------------------------------------

#include "web_server.h"
#include "config.h"
#include <AsyncTCP.h>

static AsyncWebServer server(80);

// Bridge variables
volatile int webGazeX = 0;
volatile int webGazeY = 0;
volatile int webCommandId = 0;
volatile unsigned long lastWebPacket = 0;

// -------------------------------------------------------------------------
// Embedded HTML — no SPIFFS required
// -------------------------------------------------------------------------
static const char *html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>👁️ Wicker Husband Eyes</title>
    <style>
        :root {
            --bg: #0d0d14;
            --panel: #1a1a2e;
            --accent: #7b2d8b;
            --accent2: #c77dff;
            --text: #e0d7f5;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            background: var(--bg);
            color: var(--text);
            font-family: 'Courier New', monospace;
            text-align: center;
            overflow: hidden;
            touch-action: none;
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            display: flex;
            flex-direction: column;
            height: 100%;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 8px 16px;
            border-bottom: 2px solid var(--accent);
            flex-shrink: 0;
        }
        .title { color: var(--accent2); font-size: 18px; font-weight: bold; letter-spacing: 2px; }
        .status { font-size: 12px; color: #555; }
        .connected { color: #00ff88; text-shadow: 0 0 5px #00ff88; }
        .controls-area {
            display: flex;
            flex: 1;
            justify-content: center;
            align-items: center;
            gap: 30px;
            padding: 10px;
        }
        /* Gaze joystick zone */
        .zone {
            position: relative;
            width: 160px;
            height: 160px;
            background: radial-gradient(circle, var(--panel) 0%, var(--bg) 70%);
            border: 2px solid var(--accent);
            border-radius: 50%;
            touch-action: none;
            box-shadow: 0 0 20px rgba(123, 45, 139, 0.4);
            flex-shrink: 0;
        }
        .zone-label {
            position: absolute;
            bottom: -22px;
            width: 100%;
            text-align: center;
            color: var(--accent2);
            font-size: 11px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .knob {
            position: absolute;
            width: 36px;
            height: 36px;
            background: radial-gradient(circle, var(--accent2), var(--accent));
            border-radius: 50%;
            top: 50%; left: 50%;
            transform: translate(-50%, -50%);
            pointer-events: none;
            box-shadow: 0 0 12px var(--accent2);
        }
        /* Expression buttons */
        .btn-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 8px;
            width: 180px;
            flex-shrink: 0;
        }
        .btn {
            background: var(--panel);
            color: var(--accent2);
            border: 1px solid var(--accent);
            border-radius: 8px;
            padding: 12px 6px;
            font-family: 'Courier New', monospace;
            font-size: 13px;
            cursor: pointer;
            transition: background 0.1s, box-shadow 0.1s;
            touch-action: manipulation;
        }
        .btn:active, .btn.pressed {
            background: var(--accent);
            color: #fff;
            box-shadow: 0 0 10px var(--accent);
        }
        .btn.wide { grid-column: 1 / -1; }
        /* Bottom info bar */
        .footer {
            padding: 6px;
            border-top: 1px solid #222;
            font-size: 10px;
            color: #444;
            flex-shrink: 0;
        }
    </style>
</head>
<body>
<div class="header">
    <div class="title">👁️ WICKER EYES</div>
    <div class="status" id="statusTxt">OFFLINE</div>
</div>

<div class="controls-area">
    <!-- Gaze Joystick -->
    <div style="position:relative; margin-top:20px;">
        <div class="zone" id="gazeZone">
            <div class="knob" id="gazeKnob"></div>
            <span class="zone-label">GAZE</span>
        </div>
    </div>

    <!-- Expression Buttons -->
    <div class="btn-grid">
        <button class="btn wide" onclick="sendCmd(1)">👁️ BLINK</button>
        <button class="btn" onclick="sendCmd(2)">⚡ ALERT</button>
        <button class="btn" onclick="sendCmd(3)">😴 SLEEPY</button>
        <button class="btn" onclick="sendCmd(4)">🔀 IDLE</button>
        <button class="btn wide" onclick="sendCmd(5)">🎯 CENTER</button>
    </div>
</div>

<div class="footer" id="footerTxt">Connect to WICKER-NET • heroprops</div>

<script>
const zone = document.getElementById('gazeZone');
const knob = document.getElementById('gazeKnob');
const statusEl = document.getElementById('statusTxt');
const footerEl = document.getElementById('footerTxt');

let gazeX = 0, gazeY = 0;
let isDragging = false;

// ---- Joystick logic ----
function getZoneCenter(el) {
    const r = el.getBoundingClientRect();
    return { cx: r.left + r.width / 2, cy: r.top + r.height / 2, radius: r.width / 2 };
}

function updateKnob(x, y) {
    const half = zone.offsetWidth / 2;
    knob.style.left = (half + x) + 'px';
    knob.style.top  = (half - y) + 'px';
    knob.style.transform = 'translate(-50%, -50%)';
}

function clampJoy(dx, dy, radius) {
    const dist = Math.sqrt(dx * dx + dy * dy);
    const limit = radius * 0.85;
    if (dist > limit) {
        dx = dx / dist * limit;
        dy = dy / dist * limit;
    }
    return { dx, dy };
}

function pointerToGaze(e) {
    const zc = getZoneCenter(zone);
    let dx = (e.clientX || (e.touches && e.touches[0].clientX)) - zc.cx;
    let dy = (e.clientY || (e.touches && e.touches[0].clientY)) - zc.cy;
    const clamped = clampJoy(dx, dy, zc.radius);
    dx = clamped.dx; dy = clamped.dy;
    gazeX = Math.round(dx / (zc.radius * 0.85) * 100);
    gazeY = Math.round(-dy / (zc.radius * 0.85) * 100); // Invert Y (up = positive)
    updateKnob(dx, dy);
}

zone.addEventListener('mousedown', e => { isDragging = true; pointerToGaze(e); });
zone.addEventListener('mousemove', e => { if (isDragging) pointerToGaze(e); });
zone.addEventListener('mouseup',   e => { isDragging = false; gazeX = 0; gazeY = 0; updateKnob(0, 0); });
zone.addEventListener('touchstart', e => { e.preventDefault(); isDragging = true; pointerToGaze(e); }, {passive:false});
zone.addEventListener('touchmove',  e => { e.preventDefault(); if (isDragging) pointerToGaze(e); }, {passive:false});
zone.addEventListener('touchend',   e => { isDragging = false; gazeX = 0; gazeY = 0; updateKnob(0, 0); });

// ---- Network send ----
function sendGaze() {
    const url = `/gaze?x=${gazeX}&y=${gazeY}`;
    fetch(url).then(r => {
        if (r.ok) {
            statusEl.textContent = 'ONLINE';
            statusEl.className = 'status connected';
        }
    }).catch(() => {
        statusEl.textContent = 'OFFLINE';
        statusEl.className = 'status';
    });
}

function sendCmd(id) {
    fetch(`/cmd?id=${id}`).catch(() => {});
}

// Send gaze at ~20 Hz
setInterval(sendGaze, 50);
updateKnob(0, 0);
</script>
</body>
</html>
)rawliteral";

// -------------------------------------------------------------------------
// setupWebServer()
// -------------------------------------------------------------------------
void setupWebServer() {
    // Serve the control page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "text/html", html_page);
    });

    // Gaze joystick — /gaze?x=<-100..100>&y=<-100..100>
    server.on("/gaze", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (req->hasParam("x")) {
            webGazeX = constrain(req->getParam("x")->value().toInt(), -100, 100);
        }
        if (req->hasParam("y")) {
            webGazeY = constrain(req->getParam("y")->value().toInt(), -100, 100);
        }
        lastWebPacket = millis();
        req->send(200, "text/plain", "OK");
    });

    // Expression commands — /cmd?id=<1..5>
    server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *req) {
        if (req->hasParam("id")) {
            webCommandId = req->getParam("id")->value().toInt();
        }
        lastWebPacket = millis();
        req->send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println("🌐 Web server started");
}

// -------------------------------------------------------------------------
// serviceWebServer() — periodic maintenance; call from main loop
// -------------------------------------------------------------------------
void serviceWebServer() {
    // AsyncWebServer handles everything internally; nothing to do here.
    // Kept for API parity with squad-bots pattern.
}
