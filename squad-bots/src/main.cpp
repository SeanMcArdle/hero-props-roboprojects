#include <Arduino.h>
#include "config.h"
#include "hp_espnow.h"
#include <ESP32Servo.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include "web_server.h"

// -------------------------------------------------------------------------
// 🌍 Globals
// -------------------------------------------------------------------------
#define NUM_LEDS 4
CRGB leds[NUM_LEDS];

HeroPropsProtocol radio;

unsigned long lastHeartbeat = 0;
bool isConnected = false; 

Servo leftMotor;
Servo rightMotor;
Servo domeServo;

// -------------------------------------------------------------------------
// ⚡ RC Interrupts (Non-Blocking)
// -------------------------------------------------------------------------
// 0=Throttle, 1=Steering, 2=Spin
volatile unsigned long rc_start[3] = {0, 0, 0};
volatile int rc_val[3] = {0, 0, 0}; // 0 = Invalid

void IRAM_ATTR isrThrottle() {
    unsigned long now = micros();
    if (digitalRead(PIN_RC_THROTTLE) == HIGH) {
        rc_start[0] = now;
    } else {
        if (rc_start[0] > 0) rc_val[0] = now - rc_start[0];
    }
}

void IRAM_ATTR isrSteering() {
    unsigned long now = micros();
    if (digitalRead(PIN_RC_STEERING) == HIGH) {
        rc_start[1] = now;
    } else {
        if (rc_start[1] > 0) rc_val[1] = now - rc_start[1];
    }
}

void IRAM_ATTR isrSpin() {
    unsigned long now = micros();
    if (digitalRead(PIN_RC_SPIN) == HIGH) {
        rc_start[2] = now;
    } else {
        if (rc_start[2] > 0) rc_val[2] = now - rc_start[2];
    }
}

// -------------------------------------------------------------------------
// ⚙️ Motor Control Logic
// -------------------------------------------------------------------------
void setMotorSpeed(Servo& motor, int speed, int trim) {
    int output = map(speed, -100, 100, MOTOR_MIN, MOTOR_MAX);
    output += trim;
    output = constrain(output, MOTOR_MIN, MOTOR_MAX);
    motor.write(output);
}

void drive(int throttle, int turn) {
    int left = throttle + turn;
    int right = throttle - turn;
    left = constrain(left, -100, 100);
    right = constrain(right, -100, 100);
    setMotorSpeed(leftMotor, left, TRIM_LEFT);
    setMotorSpeed(rightMotor, -right, TRIM_RIGHT); 
}

// -------------------------------------------------------------------------
// 📡 ESP-NOW Callback (The Swarm Layer)
// -------------------------------------------------------------------------
void onDataReceived(const HpHeader& header, const uint8_t* payload, size_t len) {
    // Only process Critical Events (Like E-Stop) if RC is active
    if (header.msgType == HP_MSG_ESTOP) {
        drive(0, 0); 
        Serial.println("🚨 E-STOP via ESP-NOW");
    }
    // We can add "Swarm Follow" logic here later
}

// -------------------------------------------------------------------------
// 🚀 Setup
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n🤖 OMNI-NODE V1 (SquadBot) Booting...");

    // 1. Init Hardware
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    leftMotor.setPeriodHertz(50);
    leftMotor.attach(PIN_MOTOR_LEFT, 500, 2500);
    rightMotor.setPeriodHertz(50);
    rightMotor.attach(PIN_MOTOR_RIGHT, 500, 2500);
    domeServo.setPeriodHertz(50);
    domeServo.attach(PIN_DOME, 500, 2500);
    domeServo.write(90); 

    // 1b. Init LEDs
    FastLED.addLeds<NEOPIXEL, PIN_NEOPIXEL>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    fill_solid(leds, NUM_LEDS, CRGB::Blue); // Boot Color
    FastLED.show();

    // 2. Attach Interrupts for RC (The Pilot)
    pinMode(PIN_RC_THROTTLE, INPUT);
    pinMode(PIN_RC_STEERING, INPUT);
    pinMode(PIN_RC_SPIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_RC_THROTTLE), isrThrottle, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_RC_STEERING), isrSteering, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_RC_SPIN), isrSpin, CHANGE);

    drive(0, 0); 

    // 3. Init WiFi & Web Server (The Interface)
    WiFi.mode(WIFI_AP);
    // Force specific channel for ESP-NOW compatibility
    WiFi.softAP(WIFI_SSID, WIFI_PASS, 1, 0, 4); // Channel 1, Hidden 0, Max 4 clients
    Serial.print("📡 WiFi AP: "); Serial.println(WIFI_SSID);
    Serial.print("👉 http://"); Serial.println(WiFi.softAPIP());

    setupWebServer();

    // 4. Init OTA
    ArduinoOTA.setHostname(BOT_NAME);
    ArduinoOTA.setPassword("heroprops");
    // SAFETY: Stop motors before OTA update starts (PWM keeps running during core blocking)
    ArduinoOTA.onStart([]() {
        drive(0, 0);
    });
    ArduinoOTA.begin();

    // 5. Init ESP-NOW (The Nervous System)
    if (radio.begin(BOT_ID)) {
        Serial.println("✅ ESP-NOW Ready");
        radio.onReceive(onDataReceived);
    } else {
        Serial.println("❌ ESP-NOW Init Failed");
    }
}

// -------------------------------------------------------------------------
// 🔄 Loop
// -------------------------------------------------------------------------
void loop() {
    ArduinoOTA.handle();
    radio.update();

    // ----------------------
    // 1. Process Logic
    // ----------------------

    // SAFETY: Web Watchdog
    // If no packet received for 500ms, force stop.
    if (millis() - lastWebPacket > 500) {
        webDriveX = 0;
        webDriveY = 0;
    }

    // A. Dome Logic (Always Web/Perfomer for now)
    // webDomeX is 0-200. Map to 0-180 for servo.
    int domeTarget = map(webDomeX, 0, 200, 0, 180);
    domeServo.write(domeTarget);

    // B. Handle Web Commands (Sound/Actions)
    if (webCommandId > 0) {
        Serial.printf("🔊 Web Cmd: %d\n", webCommandId);
        
        if (webCommandId == 1) fill_solid(leds, NUM_LEDS, CRGB::Green);  // Happy
        if (webCommandId == 2) fill_solid(leds, NUM_LEDS, CRGB::Blue);   // Sad
        if (webCommandId == 3) fill_solid(leds, NUM_LEDS, CRGB::Red);    // Angry
        if (webCommandId == 4) fill_solid(leds, NUM_LEDS, CRGB::Yellow); // Dance (Party)
        FastLED.show();

        // Reset command
        webCommandId = 0;
    }

    // C. Drive Logic (RC Priority)
    int throttle = 0;
    int turn = 0;
    bool rcActive = false;

    // Check recent RC valid pulses (must be within last 100ms to be valid)
    // Since interrupts update variables instantly, we just check values.
    // NOTE: We should implement a "stale data" check in a real interrupt system, 
    // but for now we assume if the values are in range, they are good.
    // To match PulseIn logic: RC_MIN 1000, RC_MAX 2000.
    
    // Simple verification (Check range)
    if (rc_val[0] > 900 && rc_val[0] < 2100) {
        // Valid Throttle
        int val = rc_val[0];
        // Deadzone
        if (val > 1480 && val < 1520) val = 1500;
        int t = map(val, 1000, 2000, -100, 100);
        if (t != 0) { throttle = t; rcActive = true; }
    }

    if (rc_val[1] > 900 && rc_val[1] < 2100) {
        // Valid Steering
        int val = rc_val[1];
        if (val > 1480 && val < 1520) val = 1500;
        int s = map(val, 1000, 2000, -100, 100);
        if (s != 0) { turn = s; rcActive = true; }
    }

    if (rc_val[2] > 900 && rc_val[2] < 2100) {
        // Valid Spin
        int val = rc_val[2];
        if (val > 1480 && val < 1520) val = 1500;
        int r = map(val, 1000, 2000, -100, 100);
        if (r != 0) { 
            throttle = 0; Safety: Watchdog prevents runaway)
        drive(webDriveY, webDriveX
    }

    if (!rcActive) {
        // Fallback to Web Drive (if implemented)
        // drive(webDriveX, webDriveY);
        // For now, auto-stop
        drive(0, 0);
    } else {
        drive(throttle, turn);
    }

    delay(5); // Small breather for Core 1
}
