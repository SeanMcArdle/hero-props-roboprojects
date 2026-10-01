#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>

#include "config.h"
#include "web_server.h"
#include "effects.h"

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static Preferences prefs;

static const char *PREFS_NAMESPACE = "footsteps";

volatile bool otaInProgress = false;
static unsigned long otaRebootAtMs = 0;   // 0 = no reboot scheduled
static unsigned long pendingSaveAtMs = 0; // 0 = no debounced NVS write scheduled

// Minimal, dependency-free upload page — works fine from iPad Safari.
// No CDN assets (the AP has no internet access), matches data/index.html's style.
static const char OTA_UPDATE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Haunted Footsteps &mdash; Firmware Update</title>
<style>
body{background:#0a0a0c;color:#eafff2;font-family:'Segoe UI',Roboto,Helvetica,Arial,sans-serif;margin:0;padding:20px;}
.container{max-width:480px;margin:0 auto;}
h1{font-style:italic;letter-spacing:1px;text-shadow:0 0 12px #1a7d47;font-size:1.5rem;margin-bottom:4px;}
.card{background:#14181a;border:1px solid #26332c;border-radius:8px;padding:18px;margin-top:16px;}
input[type=file]{width:100%;margin-bottom:14px;color:#eafff2;}
button{background:#1a7d47;color:#eafff2;border:none;padding:10px 18px;border-radius:6px;font-weight:bold;text-transform:uppercase;letter-spacing:1px;cursor:pointer;width:100%;}
button:disabled{opacity:0.5;}
.bar-track{height:20px;border-radius:4px;background:#050705;border:1px solid #26332c;overflow:hidden;margin-top:12px;}
.bar-fill{height:100%;width:0%;background:linear-gradient(90deg,#1a7d47,#33ff88);}
#status{margin-top:10px;font-size:0.85rem;color:#7fa895;}
.warn{color:#ffb347;font-size:0.8rem;margin-top:10px;}
</style></head>
<body><div class="container">
<h1>Firmware Update</h1>
<div class="card">
<form id="f">
<input type="file" id="file" accept=".bin">
<button type="submit">Upload &amp; Flash</button>
</form>
<div class="bar-track"><div id="bar" class="bar-fill"></div></div>
<div id="status">Select the .bin file built by "pio run" (.pio/build/esp32dev/firmware.bin).</div>
<div class="warn">Do not close this tab or remove power while the upload is in progress. The device reboots automatically when done.</div>
</div>
</div>
<script>
document.getElementById('f').addEventListener('submit', function(e){
  e.preventDefault();
  var fileInput = document.getElementById('file');
  if (!fileInput.files.length) return;
  var data = new FormData();
  data.append('file', fileInput.files[0]);
  var xhr = new XMLHttpRequest();
  var statusEl = document.getElementById('status');
  var barEl = document.getElementById('bar');
  document.querySelector('button').disabled = true;
  xhr.upload.onprogress = function(evt){
    if (evt.lengthComputable) barEl.style.width = Math.round(evt.loaded / evt.total * 100) + '%';
  };
  xhr.onload = function(){
    statusEl.textContent = xhr.responseText || ('HTTP ' + xhr.status);
    if (xhr.status === 200) barEl.style.width = '100%';
  };
  xhr.onerror = function(){ statusEl.textContent = 'Upload failed (connection error).'; document.querySelector('button').disabled = false; };
  xhr.open('POST', '/update');
  xhr.send(data);
});
</script>
</body></html>)HTML";

void enforceFadeSafety()
{
    settings.fadeOutMs = enforceFadeSafety(settings.fadeInMs, settings.fadeOutMs, MIN_FADE_OUT_RATIO);
}

static void loadSettings()
{
    prefs.begin(PREFS_NAMESPACE, true); // read-only
    settings.colorR = prefs.getUChar("colorR", DEFAULT_COLOR_R);
    settings.colorG = prefs.getUChar("colorG", DEFAULT_COLOR_G);
    settings.colorB = prefs.getUChar("colorB", DEFAULT_COLOR_B);
    settings.flickerSpeed = prefs.getUChar("flickerSpd", DEFAULT_FLICKER_SPEED);
    settings.intensity = prefs.getUChar("intensity", DEFAULT_INTENSITY);
    settings.fadeInMs = prefs.getUInt("fadeInMs", DEFAULT_FADE_IN_MS);
    settings.fadeOutMs = prefs.getUInt("fadeOutMs", DEFAULT_FADE_OUT_MS);
    prefs.end();
    enforceFadeSafety();
}

static void saveSettings()
{
    prefs.begin(PREFS_NAMESPACE, false); // read-write
    prefs.putUChar("colorR", settings.colorR);
    prefs.putUChar("colorG", settings.colorG);
    prefs.putUChar("colorB", settings.colorB);
    prefs.putUChar("flickerSpd", settings.flickerSpeed);
    prefs.putUChar("intensity", settings.intensity);
    prefs.putUInt("fadeInMs", settings.fadeInMs);
    prefs.putUInt("fadeOutMs", settings.fadeOutMs);
    prefs.end();
}

static void sendSettings(AsyncWebSocketClient *client)
{
    JsonDocument doc;
    doc["type"] = "settings";
    char hex[8];
    sprintf(hex, "#%02X%02X%02X", settings.colorR, settings.colorG, settings.colorB);
    doc["colorHex"] = hex;
    doc["flickerSpeed"] = settings.flickerSpeed;
    doc["intensity"] = settings.intensity;
    doc["fadeInMs"] = settings.fadeInMs;
    doc["fadeOutMs"] = settings.fadeOutMs;

    String output;
    serializeJson(doc, output);
    if (client)
    {
        client->text(output);
    }
    else
    {
        ws.textAll(output);
    }
}

void broadcastTelemetry()
{
    if (ws.count() == 0)
        return;

    JsonDocument doc;
    doc["type"] = "telemetry";
    doc["toePressed"] = telemetryToePressed;
    doc["heelPressed"] = telemetryHeelPressed;
    doc["toeEnvelope"] = telemetryToeEnvelope;
    doc["heelEnvelope"] = telemetryHeelEnvelope;

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
}

static void applySettingsUpdate(JsonDocument &doc)
{
    if (doc["colorHex"].is<const char *>())
    {
        const char *hex = doc["colorHex"];
        if (hex[0] == '#' && strlen(hex) == 7)
        {
            long number = strtol(&hex[1], NULL, 16);
            settings.colorR = (number >> 16) & 0xFF;
            settings.colorG = (number >> 8) & 0xFF;
            settings.colorB = number & 0xFF;
        }
    }
    if (doc["flickerSpeed"].is<int>())
        settings.flickerSpeed = constrain((int)doc["flickerSpeed"], 0, 255);
    if (doc["intensity"].is<int>())
        settings.intensity = constrain((int)doc["intensity"], 0, 255);
    if (doc["fadeInMs"].is<int>())
        settings.fadeInMs = constrain((int)doc["fadeInMs"], 50, 20000);
    if (doc["fadeOutMs"].is<int>())
        settings.fadeOutMs = constrain((int)doc["fadeOutMs"], 50, 20000);

    enforceFadeSafety();   // fade-out can never end up faster than fade-in, regardless of input
    sendSettings(nullptr); // Echo the (possibly clamped) authoritative state to every client

    // Debounce the flash write: a slider drag can fire many updates per
    // second, but we only need the final value persisted. Each new update
    // just pushes the write further out; serviceWebServer() performs the
    // actual save once things go quiet for SETTINGS_SAVE_DEBOUNCE_MS.
    pendingSaveAtMs = millis() + SETTINGS_SAVE_DEBOUNCE_MS;
}

static void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT))
        return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    const char *type = doc["type"];
    if (type && strcmp(type, "update_settings") == 0)
    {
        applySettingsUpdate(doc);
    }
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        sendSettings(client);
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

// -------------------------------------------------------------------------
// OTA (browser-based firmware update, e.g. from an iPad's Safari file picker)
// -------------------------------------------------------------------------
// Gated by its own HTTP Basic Auth credentials (OTA_USERNAME/OTA_PASSWORD),
// deliberately separate from the WiFi AP password.
static void handleOtaUploadChunk(AsyncWebServerRequest *request, String filename, size_t index,
                                 uint8_t *data, size_t len, bool final)
{
    if (!request->authenticate(OTA_USERNAME, OTA_PASSWORD))
    {
        return; // Unauthenticated chunks are silently dropped; the onRequest handler below will 401.
    }

    if (index == 0)
    {
        Serial.printf("OTA: upload started (%s)\n", filename.c_str());
        otaInProgress = true;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
            Update.printError(Serial);
        }
    }

    if (Update.isRunning())
    {
        if (Update.write(data, len) != len)
        {
            Update.printError(Serial);
        }
    }

    if (final && Update.isRunning())
    {
        if (Update.end(true))
        {
            Serial.printf("OTA: write complete (%u bytes)\n", (unsigned)(index + len));
        }
        else
        {
            Update.printError(Serial);
        }
    }
}

static void handleOtaComplete(AsyncWebServerRequest *request)
{
    if (!request->authenticate(OTA_USERNAME, OTA_PASSWORD))
    {
        return request->requestAuthentication();
    }

    bool ok = Update.isFinished() && !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/plain", ok ? "Update OK. Rebooting..." : "Update FAILED - check serial log, device did not reboot.");
    response->addHeader("Connection", "close");
    request->send(response);

    otaInProgress = false;
    if (ok)
    {
        otaRebootAtMs = millis() + 1000; // Give the response time to flush before restarting
    }
}

static void handleOtaPage(AsyncWebServerRequest *request)
{
    if (!request->authenticate(OTA_USERNAME, OTA_PASSWORD))
    {
        return request->requestAuthentication();
    }
    request->send_P(200, "text/html", OTA_UPDATE_HTML);
}

void setupWebServer()
{
    loadSettings();

    if (!LittleFS.begin(true))
    {
        Serial.println("LittleFS Mount Failed");
    }

    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    IPAddress ip = WiFi.softAPIP();
    Serial.print("AP started. IP: ");
    Serial.println(ip);

    if (MDNS.begin("haunted-footsteps"))
    {
        Serial.println("mDNS responder started: http://haunted-footsteps.local");
    }

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/update", HTTP_GET, handleOtaPage);
    server.on("/update", HTTP_POST, handleOtaComplete, handleOtaUploadChunk);

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    server.begin();

    Serial.println("HTTP server started");
}

void serviceWebServer()
{
    ws.cleanupClients();

    if (pendingSaveAtMs != 0 && (long)(millis() - pendingSaveAtMs) >= 0)
    {
        saveSettings();
        pendingSaveAtMs = 0;
    }

    if (otaRebootAtMs != 0 && (long)(millis() - otaRebootAtMs) >= 0)
    {
        Serial.println("OTA: rebooting into new firmware...");
        ESP.restart();
    }
}
