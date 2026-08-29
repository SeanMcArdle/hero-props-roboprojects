// -------------------------------------------------------------------------
// 🪆 Wicker Husband — Animatronic Eyes
//
// ESP32 adaptation of mklements/AIChatbot EyeMovement.py (Raspberry Pi)
//
// Architecture:
//   • AnimatronicEyes — non-blocking 6-servo state machine
//   • HeroPropsProtocol — ESP-NOW receive (HP_MSG_DRIVE = gaze,
//                          HP_MSG_ACTION = expressions, HP_MSG_ESTOP = center)
//   • WiFi AP + web UI — local joystick control
//   • ArduinoOTA — wireless firmware updates
// -------------------------------------------------------------------------

#include <Arduino.h>
#include "config.h"
#include "AnimatronicEyes.h"
#include "web_server.h"
#include "hp_espnow.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------
AnimatronicEyes eyes;
HeroPropsProtocol radio;

static const char *MDNS_HOST = "wicker-eyes";

unsigned long lastWsCleanup = 0;

// Watchdog: if no web packet for 500 ms, stop joystick-driven gaze
// and hand control back to idle wander
static const unsigned long WEB_WATCHDOG_MS = 500;
bool webGazeActive = false;

// -------------------------------------------------------------------------
// HP_MSG_DRIVE  → gaze direction
// HP_MSG_ACTION → expression preset
//   actionId 1=Blink, 2=Alert, 3=Sleepy, 4=Idle, 5=Center
// HP_MSG_ESTOP  → center and stop
// -------------------------------------------------------------------------
void onDataReceived(const HpHeader &header, const uint8_t *payload, size_t len) {
    switch (header.msgType) {

        case HP_MSG_DRIVE: {
            if (len == sizeof(HpPayloadDrive)) {
                const HpPayloadDrive *drv = reinterpret_cast<const HpPayloadDrive *>(payload);
                // Map -100..100 to eye travel limits
                int targetX = map(drv->throttle, -100, 100, EYE_X_MIN, EYE_X_MAX);
                int targetY = map(drv->turn,     -100, 100, EYE_Y_MIN, EYE_Y_MAX);
                eyes.lookAt(targetX, targetY);
            }
            break;
        }

        case HP_MSG_ACTION: {
            if (len == sizeof(HpPayloadAction)) {
                const HpPayloadAction *act = reinterpret_cast<const HpPayloadAction *>(payload);
                eyes.setExpression(static_cast<EyeExpression>(act->actionId));
            }
            break;
        }

        case HP_MSG_ESTOP: {
            Serial.println("🚨 E-STOP — centering eyes");
            eyes.centerAndStop();
            break;
        }

        default:
            break;
    }
}

// -------------------------------------------------------------------------
// setup()
// -------------------------------------------------------------------------
void setup() {
    delay(1000); // Let power stabilise
    Serial.begin(115200);
    Serial.println("\n\n🪆 WICKER HUSBAND — Animatronic Eyes Booting...");

    // 1. Initialise eyes (servos + state machine)
    eyes.begin();

    // 2. WiFi Access Point
    if (USE_WIFI_AP) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_SSID, WIFI_PASS, 1, 0, 4);
        WiFi.setTxPower(WIFI_POWER_11dBm);

        Serial.print("📡 WiFi AP: ");
        Serial.println(WIFI_SSID);
        Serial.print("👉 http://");
        Serial.println(WiFi.softAPIP());

        if (MDNS.begin(MDNS_HOST)) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("🌐 mDNS: http://%s.local\n", MDNS_HOST);
        }

        setupWebServer();
    }

    // 3. OTA updates
    ArduinoOTA.setHostname(MDNS_HOST);
    ArduinoOTA.setPassword("heroprops");
    ArduinoOTA.onStart([]() { eyes.centerAndStop(); });
    ArduinoOTA.begin();

    // 4. ESP-NOW (receive gaze/action commands from a Commander)
    if (radio.begin(DEVICE_ID)) {
        Serial.println("✅ ESP-NOW Ready");
        radio.onReceive(onDataReceived);
    } else {
        Serial.println("❌ ESP-NOW Init Failed");
    }

    Serial.println("✅ Boot complete — eyes wandering");
}

// -------------------------------------------------------------------------
// loop()
// -------------------------------------------------------------------------
void loop() {
    ArduinoOTA.handle();
    radio.update();

    // Periodic web server maintenance
    if (millis() - lastWsCleanup > 1000) {
        serviceWebServer();
        lastWsCleanup = millis();
    }

    // ----------------------------------------------------------------
    // Web gaze joystick control
    // If the web UI is sending joystick data, map it to an eye target.
    // Watchdog: if no packet for WEB_WATCHDOG_MS, resume idle wander.
    // ----------------------------------------------------------------
    bool packetFresh = (millis() - lastWebPacket < WEB_WATCHDOG_MS);

    if (packetFresh && (webGazeX != 0 || webGazeY != 0)) {
        if (!webGazeActive) {
            webGazeActive = true;
        }
        int targetX = map(webGazeX, -100, 100, EYE_X_MIN, EYE_X_MAX);
        int targetY = map(webGazeY, -100, 100, EYE_Y_MIN, EYE_Y_MAX);
        eyes.lookAt(targetX, targetY);
    } else if (webGazeActive && !packetFresh) {
        // Web connection lost — hand back to idle wander
        webGazeActive = false;
        eyes.resumeIdle();
    }

    // ----------------------------------------------------------------
    // Web command buttons
    // ----------------------------------------------------------------
    if (webCommandId > 0) {
        Serial.printf("🎭 Web Cmd: %d\n", webCommandId);
        switch (webCommandId) {
            case 1: eyes.setExpression(EXPR_BLINK);  break;
            case 2: eyes.setExpression(EXPR_ALERT);  break;
            case 3: eyes.setExpression(EXPR_SLEEPY); break;
            case 4: eyes.setExpression(EXPR_IDLE);   break;
            case 5: eyes.setExpression(EXPR_CENTER); break;
        }
        webCommandId = 0;
    }

    // ----------------------------------------------------------------
    // Advance the eye state machine
    // ----------------------------------------------------------------
    eyes.update();

    delay(5); // ~200 Hz cap; eye state machine uses millis() internally
}
