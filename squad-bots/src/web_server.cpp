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
volatile int webPixelIdx = -1;
volatile int webPixelR = 0;
volatile int webPixelG = 0;
volatile int webPixelB = 0;

// The HTML (Embedded for simplicity - No SPIFFS required)
const char *html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>Jellybean Command</title>
    <style>
        :root {
            --prince-bg: #160203;
            --prince-panel: #2b080a;
            --prince-purple: #c1121f; /* L0-0N Primary Red */
            --prince-gold: #d9d9d9; /* Silver */
            --text-color: #ffeaea; /* Soft Rose White */
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
            box-shadow: 0 0 15px rgba(193, 18, 31, 0.25);
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

        .dome-slider-wrap {
            width: 160px;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 8px;
        }

        .dome-center-btn {
            padding: 6px 10px;
            font-size: 10px;
            border: 1px solid var(--prince-gold);
            border-radius: 8px;
            background: #3a1214;
            color: var(--prince-gold);
            text-transform: uppercase;
            font-weight: bold;
        }

        .dome-slider-label {
            color: var(--prince-gold);
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .dome-slider {
            width: 160px;
            accent-color: var(--prince-purple);
            -webkit-appearance: none;
            height: 34px;
            background: rgba(255,255,255,0.1);
            border-radius: 20px;
        }

        .dome-slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            height: 28px;
            width: 28px;
            border-radius: 50%;
            background: var(--prince-gold);
            cursor: pointer;
            border: 2px solid white;
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
        .knob.pilot { background: radial-gradient(circle, var(--prince-purple) 0%, #6b0f1a 100%); box-shadow: 0 0 10px var(--prince-purple); }

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
            background: #3a1214;
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

            <!-- Right Control (Dome Slider) -->
            <div class="dome-slider-wrap">
                <div class="dome-slider-label">PERFORMER</div>
                <input class="dome-slider" type="range" min="0" max="200" value="100" id="domeSlide" oninput="sendDomeSlider()">
                <button class="dome-center-btn" onmousedown="centerDomeSlider()">CENTER</button>
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
            <button class="btn" style="border-color:#c1121f" onmousedown="sendCmd(10)">PULSE</button>
            <button class="btn" style="border-color:#e63946" onmousedown="sendCmd(11)">PARTY</button>
            <button class="btn" style="border-color:#f77f00" onmousedown="sendCmd(12)">RAINBOW</button>
            <button class="btn" style="border-color:#ff9e00" onmousedown="sendCmd(13)">SCANNER</button>
            <button class="btn" style="border-color:#9a031e" onmousedown="sendCmd(14)">MANUAL</button>
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

        // Initialize Drive Joystick
        createJoystick('joyDrive', 'knobDrive', 'D', true);

        // Dome uses a horizontal slider (left/right) instead of a 2D joystick.
        function sendDomeSlider() {
            let x = parseInt(document.getElementById('domeSlide').value, 10);
            sendData('J', x, 100);
        }

        function centerDomeSlider() {
            document.getElementById('domeSlide').value = 100;
            sendData('J', 100, 100);
        }

        // Keep dome command alive at 10Hz so continuous spin persists while connected.
        setInterval(() => {
            let x = parseInt(document.getElementById('domeSlide').value, 10);
            sendData('J', x, 100);
        }, 100);

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

// ---- LED Lab Page (/led) ------------------------------------------------
const char *html_led_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>LED Lab</title>
    <style>
        body { background:#000; color:#00ff00; font-family:'Courier New',monospace; margin:0; padding:10px; box-sizing:border-box; }
        .header { display:flex; justify-content:space-between; align-items:center; border-bottom:1px solid #1a1a1a; padding-bottom:8px; margin-bottom:12px; }
        .title { color:#00ff00; font-size:18px; font-weight:bold; letter-spacing:3px; }
        .status { font-size:13px; color:#00aa00; font-weight:bold; }
        .connected { color:#00ff00; text-shadow:0 0 5px #00ff00; }
        .back { font-size:13px; color:#00aa00; text-decoration:none; font-family:'Courier New',monospace; }
        /* Preview ring */
        .strip-label { font-size:12px; color:#00aa00; text-align:center; margin-bottom:4px; letter-spacing:2px; }
        .ring-container { position:relative; width:200px; height:200px; margin:0 auto 16px auto; }
        .led-pixel { position:absolute; width:44px; height:44px; border-radius:50%; border:2px solid #333; background:#0a0a0a; transition:background 0.12s, box-shadow 0.12s; }
        .led-label { position:absolute; top:50%; left:50%; transform:translate(-50%,-50%); font-size:12px; color:#00aa00; font-weight:bold; }
        /* Console */
        .console-box { background:#050505; border:1px solid #333; padding:8px; overflow-y:auto; font-size:15px; margin-bottom:10px; height:180px; }
        .log-line { margin:3px 0; white-space:pre-wrap; word-break:break-all; }
        .log-cmd { color:#00ff00; }
        .log-sys { color:#00bb00; }
        .log-err { color:#ff5555; }
        /* Input row */
        .cmd-line { display:flex; align-items:center; gap:6px; }
        .cmd-prompt { color:#00ff00; font-weight:bold; font-size:20px; flex-shrink:0; }
        .cmd-input { flex:1; background:#111; border:2px solid #333; border-radius:6px; color:#00ff00; font-family:'Courier New',monospace; font-size:16px; padding:10px; outline:none; min-width:0; -webkit-appearance:none; }
        .cmd-input:focus { border-color:#00ff00; }
        .cmd-go { padding:10px 16px; background:#111; border:2px solid #00ff00; color:#00ff00; font-size:14px; border-radius:6px; font-family:'Courier New',monospace; font-weight:bold; cursor:pointer; flex-shrink:0; -webkit-appearance:none; }
        .cmd-go:active { background:#00ff00; color:#000; }
    </style>
</head>
<body>
    <div class="header">
        <div class="title">&#x1F9EA; LED LAB</div>
        <div style="display:flex;align-items:center;gap:14px;">
            <div id="status" class="status">CONNECTING...</div>
            <a href="/" class="back">&#8592; CONTROL</a>
        </div>
    </div>
    <div class="strip-label">LED RING (0 &mdash; 7)</div>
    <div class="ring-container" id="ledStrip"></div>
    <div class="console-box" id="console">
        <div class="log-line log-sys">LED Lab ready. Type HELP for commands.</div>
    </div>
    <div class="cmd-line">
        <span class="cmd-prompt">&gt;</span>
        <input type="text" id="cmdInput" class="cmd-input"
               placeholder="color red  |  pixel 3 blue  |  help"
               autocapitalize="none" autocorrect="off" autocomplete="off" spellcheck="false">
        <button class="cmd-go" onclick="runCmd()">RUN</button>
    </div>
    <script>
        // Build LED ring visualizer
        var pixelEls = [];
        (function() {
            var strip = document.getElementById('ledStrip');
            var cx = 100, cy = 100, r = 72, size = 44;
            for (var i = 0; i < 8; i++) {
                var angle = (i * 45 - 90) * Math.PI / 180;
                var x = cx + r * Math.cos(angle) - size / 2;
                var y = cy + r * Math.sin(angle) - size / 2;
                var d = document.createElement('div');
                d.className = 'led-pixel';
                d.style.left = Math.round(x) + 'px';
                d.style.top  = Math.round(y) + 'px';
                d.innerHTML = '<span class="led-label">' + i + '</span>';
                strip.appendChild(d);
                pixelEls.push(d);
            }
        })();

        function setPixelVisual(idx, r, g, b) {
            if (idx < 0 || idx >= 8) return;
            var el = pixelEls[idx];
            el.style.background = 'rgb('+r+','+g+','+b+')';
            var lum = r*0.299 + g*0.587 + b*0.114;
            el.style.boxShadow = lum > 15 ? '0 0 12px 3px rgb('+r+','+g+','+b+')' : 'none';
            el.style.borderColor = lum > 15 ? 'rgb('+r+','+g+','+b+')' : '#222';
        }
        function setAllVisual(r, g, b) { for (var i=0;i<8;i++) setPixelVisual(i,r,g,b); }

        // WebSocket
        var gateway = 'ws://' + window.location.hostname + '/ws';
        var websocket;
        function log(msg, cls) {
            var c = document.getElementById('console');
            var line = document.createElement('div');
            line.className = 'log-line ' + (cls||'');
            line.innerHTML = msg;
            c.appendChild(line);
            if (c.childElementCount > 60) c.removeChild(c.firstChild);
            c.scrollTop = c.scrollHeight;
        }
        function initWebSocket() {
            websocket = new WebSocket(gateway);
            websocket.onopen = function() {
                document.getElementById('status').innerText = 'CONNECTED';
                document.getElementById('status').className = 'status connected';
                log('// Connected! Type HELP to see commands.', 'log-sys');
            };
            websocket.onclose = function() {
                document.getElementById('status').innerText = 'DISCONNECTED';
                document.getElementById('status').className = 'status';
                log('// Disconnected. Reconnecting...', 'log-err');
                setTimeout(initWebSocket, 2000);
            };
            websocket.onmessage = function(e) {
                if (e.data === 'BUSY') log('// Droid busy \u2014 close the Control page first.', 'log-err');
            };
        }
        function ws_send(msg) {
            if (websocket && websocket.readyState === WebSocket.OPEN) { websocket.send(msg); return true; }
            log('// Not connected.', 'log-err'); return false;
        }

        // ---- Arduino NeoPixel Emulator ----

        // Color constants (like #defines in a real sketch)
        var COLOR_CONSTS = {
            'RED':[255,0,0], 'GREEN':[0,200,0], 'BLUE':[0,0,255],
            'WHITE':[255,255,255], 'YELLOW':[255,200,0], 'PURPLE':[128,0,255],
            'ORANGE':[255,80,0], 'PINK':[255,20,60], 'CYAN':[0,200,255],
            'BLACK':[0,0,0], 'GOLD':[255,180,0], 'VIOLET':[148,0,211]
        };

        // pending = staged buffer; current = what's on the LEDs right now
        var pending = [], current = [], pendingBright = 255;
        for (var _i=0; _i<8; _i++) { pending.push([0,0,0]); current.push([0,0,0]); }

        // Evaluate strip.Color(R,G,B) or strip.Color(COLORNAME)
        function evalColor(expr) {
            var m = expr.match(/strip\.Color\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)/);
            if (m) {
                var r=parseInt(m[1]), g=parseInt(m[2]), b=parseInt(m[3]);
                if (!isNaN(r)&&!isNaN(g)&&!isNaN(b)) return [Math.min(255,r),Math.min(255,g),Math.min(255,b)];
            }
            var m2 = expr.match(/strip\.Color\(\s*([A-Za-z]+)\s*\)/);
            if (m2 && COLOR_CONSTS[m2[1].toUpperCase()]) return COLOR_CONSTS[m2[1].toUpperCase()].slice();
            return null;
        }

        function execLine(raw) {
            var line = raw.trim().replace(/;\s*$/, '').trim();
            if (!line) return;
            log(line + ';', 'log-cmd');

            // strip.setPixelColor(idx, strip.Color(...))
            var m1 = line.match(/^strip\.setPixelColor\(\s*(\d+)\s*,\s*(strip\.Color\([^)]*\))\s*\)$/);
            if (m1) {
                var idx = parseInt(m1[1]), color = evalColor(m1[2]);
                if (idx < 0 || idx > 7) { log('//  Error: index must be 0\u20137', 'log-err'); return; }
                if (!color) { log('//  Error: bad color \u2014 use strip.Color(255, 0, 0)', 'log-err'); return; }
                pending[idx] = color;
                log('//  pixel ' + idx + ' staged \u2192 rgb(' + color + ') \u2014 call strip.show() to send', 'log-sys');
                return;
            }

            // strip.fill(strip.Color(...)) or strip.fill(strip.Color(...), first, count)
            var m2 = line.match(/^strip\.fill\(\s*(strip\.Color\([^)]*\))(?:\s*,\s*(\d+)\s*,\s*(\d+))?\s*\)$/);
            if (m2) {
                var color = evalColor(m2[1]);
                if (!color) { log('//  Error: bad color \u2014 use strip.Color(255, 0, 0)', 'log-err'); return; }
                var first = m2[2] !== undefined ? parseInt(m2[2]) : 0;
                var count = m2[3] !== undefined ? parseInt(m2[3]) : (8 - first);
                for (var i=first; i<first+count && i<8; i++) pending[i] = color.slice();
                log('//  pixels ' + first + '\u2013' + (Math.min(first+count,8)-1) + ' staged \u2192 rgb(' + color + ') \u2014 call strip.show() to send', 'log-sys');
                return;
            }

            // strip.clear()
            if (/^strip\.clear\(\s*\)$/.test(line)) {
                for (var i=0; i<8; i++) pending[i] = [0,0,0];
                log('//  all pixels staged \u2192 OFF \u2014 call strip.show() to send', 'log-sys');
                return;
            }

            // strip.setBrightness(val)
            var mb = line.match(/^strip\.setBrightness\(\s*(\d+)\s*\)$/);
            if (mb) {
                pendingBright = Math.min(255, Math.max(0, parseInt(mb[1])));
                log('//  brightness staged \u2192 ' + pendingBright, 'log-sys');
                return;
            }

            // strip.show()
            if (/^strip\.show\(\s*\)$/.test(line)) {
                doShow(); return;
            }

            log('//  Syntax error. Type HELP to see examples.', 'log-err');
        }

        function doShow() {
            var scale = pendingBright / 255;
            var changed = [];
            for (var i=0; i<8; i++) {
                if (pending[i][0]!==current[i][0] || pending[i][1]!==current[i][1] || pending[i][2]!==current[i][2]) changed.push(i);
            }
            if (changed.length === 0) { log('//  strip.show() \u2014 nothing to update', 'log-sys'); return; }

            // Optimization: all 8 same color -> use L: broadcast
            var allSame = changed.length === 8;
            if (allSame) {
                for (var i=1; i<8; i++) {
                    if (pending[i][0]!==pending[0][0]||pending[i][1]!==pending[0][1]||pending[i][2]!==pending[0][2]) { allSame=false; break; }
                }
            }
            if (allSame && changed.length === 8) {
                var r=Math.round(pending[0][0]*scale), g=Math.round(pending[0][1]*scale), b=Math.round(pending[0][2]*scale);
                if (ws_send('L:'+r+','+g+','+b)) {
                    setAllVisual(r,g,b);
                    for (var i=0;i<8;i++) current[i]=pending[i].slice();
                }
            } else {
                for (var i=0; i<changed.length; i++) {
                    (function(idx, delay) {
                        setTimeout(function() {
                            var p=pending[idx];
                            var r=Math.round(p[0]*scale), g=Math.round(p[1]*scale), b=Math.round(p[2]*scale);
                            if (ws_send('P:'+idx+','+r+','+g+','+b)) {
                                setPixelVisual(idx,r,g,b);
                                current[idx]=pending[idx].slice();
                            }
                        }, delay);
                    })(changed[i], i*50);
                }
            }
            log('//  \u2705 strip.show() \u2014 ' + changed.length + ' pixel' + (changed.length!==1?'s':'') + ' sent to LEDs', 'log-sys');
        }

        function showHelp() {
            log('// --- Arduino NeoPixel Commands ---', 'log-sys');
            log('strip.setPixelColor(0, strip.Color(255, 0, 0));', 'log-cmd');
            log('//   set pixel 0 red  (pixels 0\u20137)', 'log-sys');
            log('strip.fill(strip.Color(0, 0, 255));', 'log-cmd');
            log('//   fill all 8 pixels blue', 'log-sys');
            log('strip.fill(strip.Color(0, 255, 0), 0, 4);', 'log-cmd');
            log('//   fill pixels 0\u20133 green  (start, count)', 'log-sys');
            log('strip.clear();', 'log-cmd');
            log('//   all pixels off', 'log-sys');
            log('strip.setBrightness(128);', 'log-cmd');
            log('//   brightness 0\u2013255', 'log-sys');
            log('strip.show();', 'log-cmd');
            log('//   &larr; REQUIRED: pushes all staged changes to LEDs', 'log-sys');
            log('//', 'log-sys');
            log('// Color names: RED GREEN BLUE WHITE YELLOW PURPLE', 'log-sys');
            log('//              ORANGE PINK CYAN GOLD VIOLET BLACK', 'log-sys');
            log('// strip.Color(RED) also works!', 'log-sys');
        }

        function runCmd() {
            var input = document.getElementById('cmdInput');
            var raw = input.value.trim();
            if (!raw) return;
            input.value = '';
            if (raw.toUpperCase()==='HELP'||raw==='?') { showHelp(); return; }
            execLine(raw);
        }

        document.getElementById('cmdInput').addEventListener('keydown', function(e) {
            if (e.key==='Enter') runCmd();
        });
        window.onload = initWebSocket;
    </script>
</body>
</html>
)rawliteral";

AsyncWebSocket ws("/ws");
uint32_t activeControllerId = 0;
bool hasActiveController = false;

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        if (!hasActiveController)
        {
            activeControllerId = client->id();
            hasActiveController = true;
            Serial.printf("🌐 WS CONNECT: client=%u (owner)\n", client->id());
        }
        else
        {
            Serial.printf("⛔ WS REJECT: client=%u (owner=%u)\n", client->id(), activeControllerId);
            client->text("BUSY");
            client->close();
        }
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("🌐 WS DISCONNECT: client=%u\n", client->id());
        if (hasActiveController && client->id() == activeControllerId)
        {
            hasActiveController = false;
            activeControllerId = 0;
            // Immediate stop if the active controller disconnects.
            webDriveX = 0;
            webDriveY = 0;
        }
    }
    else if (type == WS_EVT_DATA)
    {
        // Ignore packets from non-owner clients.
        if (hasActiveController && client->id() != activeControllerId)
        {
            return;
        }

        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
        {
            String msg;
            msg.reserve(len);
            for (size_t i = 0; i < len; ++i)
            {
                msg += (char)data[i];
            }

            // Debug Message Parsing
            Serial.print("Rx: ");
            Serial.println(msg);

            bool validControlPacket = false;

            if (msg.startsWith("CFG_REQ"))
            {
                String cfg = "CFG:";
                cfg += String(BOT_NAME);
                client->text(cfg);
            }

            // Parse "J:150,150" or "C:1"
            if (msg.startsWith("D:"))
            {
                int comma = msg.indexOf(',');
                if (comma > 0)
                {
                    webDriveX = constrain(msg.substring(2, comma).toInt(), -100, 100);  // Turn
                    webDriveY = constrain(msg.substring(comma + 1).toInt(), -100, 100); // Throttle
                    validControlPacket = true;
                }
            }
            else if (msg.startsWith("J:"))
            {
                int comma = msg.indexOf(',');
                if (comma > 0)
                {
                    webDomeX = constrain(msg.substring(2, comma).toInt(), 0, 200);  // Pan (Spin)
                    webDomeY = constrain(msg.substring(comma + 1).toInt(), 0, 200); // Tilt (Not used yet?)
                    validControlPacket = true;
                }
            }
            else if (msg.startsWith("C:"))
            {
                webCommandId = constrain(msg.substring(2).toInt(), 0, 99);
                validControlPacket = true;
            }
            else if (msg.startsWith("L:"))
            {
                // Parse L:R,G,B
                int first = msg.indexOf(',');
                int second = msg.indexOf(',', first + 1);

                if (first > 0 && second > first)
                {
                    webRed = constrain(msg.substring(2, first).toInt(), 0, 255);
                    webGreen = constrain(msg.substring(first + 1, second).toInt(), 0, 255);
                    webBlue = constrain(msg.substring(second + 1).toInt(), 0, 255);
                    // Auto-trigger manual mode if user touches slider
                    webCommandId = 14;
                    Serial.printf("🎨 LED MIX: R%d G%d B%d\n", webRed, webGreen, webBlue);
                    validControlPacket = true;
                }
            }
            else if (msg.startsWith("P:"))
            {
                // P:n,R,G,B — set individual pixel
                int first = msg.indexOf(',');
                int second = msg.indexOf(',', first + 1);
                int third = msg.indexOf(',', second + 1);
                if (first > 0 && second > first && third > second)
                {
                    webPixelIdx = constrain(msg.substring(2, first).toInt(), 0, NUM_LEDS - 1);
                    webPixelR = constrain(msg.substring(first + 1, second).toInt(), 0, 255);
                    webPixelG = constrain(msg.substring(second + 1, third).toInt(), 0, 255);
                    webPixelB = constrain(msg.substring(third + 1).toInt(), 0, 255);
                    webCommandId = 15; // Per-pixel mode
                    Serial.printf("🖊️ PIXEL %d: R%d G%d B%d\n", webPixelIdx, webPixelR, webPixelG, webPixelB);
                    validControlPacket = true;
                }
            }

            // Heartbeat: Reset watchdog only for successfully parsed control packets.
            if (validControlPacket)
            {
                lastWebPacket = millis();
            }
        }
    }
}

void setupWebServer()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", html_page); });

    server.on("/led", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", html_led_page); });

    server.begin();
}

void serviceWebServer()
{
    ws.cleanupClients();
}
