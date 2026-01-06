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
// ⚙️ Motor Control Logic (With Physics Smoothing)
// -------------------------------------------------------------------------
// Smooth State Variables
float currentLeft = 0;
float currentRight = 0;
float currentDome = 90;
float targetLeft = 0;
float targetRight = 0;
float targetDome = 90;

void outputPhysics() {
    // 1. Smooth the Drive
    currentLeft = currentLeft * (1.0 - DRIVE_SMOOTH) + targetLeft * DRIVE_SMOOTH;
    currentRight = currentRight * (1.0 - DRIVE_SMOOTH) + targetRight * DRIVE_SMOOTH;
    
    // 2. Smooth the Dome
    currentDome = currentDome * (1.0 - DOME_SMOOTH) + targetDome * DOME_SMOOTH;

    // 3. Output to Motors (Microseconds for precision)
    // Center is 1500. Range is +/- DRIVE_SPEED_MAX (400)
    int leftUs = 1500 + TRIM_LEFT + (int)(currentLeft * DRIVE_SPEED_MAX);
    int rightUs = 1500 + TRIM_RIGHT - (int)(currentRight * DRIVE_SPEED_MAX); // Inverted right
    
    // Safety Clamp
    leftUs = constrain(leftUs, 1000, 2000);
    rightUs = constrain(rightUs, 1000, 2000);

    leftMotor.writeMicroseconds(leftUs);
    rightMotor.writeMicroseconds(rightUs);
    
    // Dome output (Angle)
    domeServo.write((int)currentDome);
}

void setDriveTarget(float throttle, float turn) {
    // Inputs are -1.0 to 1.0
    // Simple Tank Mix for Joystick
    targetLeft = constrain(throttle + turn, -1.0f, 1.0f);
    targetRight = constrain(throttle - turn, -1.0f, 1.0f);
    
    // Immediate stop if inputs are zero (Optional, but BB-R2 usually lets it ramp down naturally.
    // If we want "snappy" stop but smooth accel, we can check for 0.
    // For now, let's keep it fully smoothed as requested ("don't destroy servos"))
    if (throttle == 0 && turn == 0) {
        // Optional: Uncomment to make stopping faster than starting
        // targetLeft = 0; targetRight = 0; 
        // currentLeft = 0; currentRight = 0; // Hard stop
    }
}

// Wrapper for legacy calls (int -100 to 100)
void drive(int throttle, int turn) {
    setDriveTarget(throttle / 100.0f, turn / 100.0f);
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

    // A. Dome Logic
    // webDomeX is 0-200. Center 100.
    int domeSpeedMapped = map(webDomeX, 0, 200, -DOME_SPEED_MAX, DOME_SPEED_MAX);
    targetDome = 90 + TRIM_DOME + domeSpeedMapped;
    targetDome = constrain(targetDome, 0, 180);

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
    float inputThrottle = 0;
    float inputTurn = 0;
    bool rcActive = false;

    // Check recent RC valid pulses (must be within last 100ms to be valid)
    if (rc_val[0] > 900 && rc_val[0] < 2100) {
        // Valid Throttle
        int val = rc_val[0];
        // Deadzone
        if (val > (1500 - RC_DEADZONE) && val < (1500 + RC_DEADZONE)) val = 1500;
        inputThrottle = map(val, 1000, 2000, -100, 100) / 100.0f;
        if (inputThrottle != 0) rcActive = true;
    }

    if (rc_val[1] > 900 && rc_val[1] < 2100) {
        // Valid Steering
        int val = rc_val[1];
        if (val > (1500 - RC_DEADZONE) && val < (1500 + RC_DEADZONE)) val = 1500;
        inputTurn = map(val, 1000, 2000, -100, 100) / 100.0f;
        if (inputTurn != 0) rcActive = true;
    }

    if (rc_val[2] > 900 && rc_val[2] < 2100) {
        // Valid Spin
        int val = rc_val[2];
        if (val > (1500 - RC_DEADZONE) && val < (1500 + RC_DEADZONE)) val = 1500;
        float r = map(val, 1000, 2000, -100, 100) / 100.0f;
        if (r != 0) { 
            inputThrottle = 0; 
            inputTurn = r; 
            rcActive = true; 
        }
    }

    if (!rcActive) {
        // Fallback to Web Drive (Safety: Watchdog prevents runaway)
        // Web variables are int -100 to 100
        inputThrottle = webDriveY / 100.0f;
        inputTurn = webDriveX / 100.0f;
    } 

    // Set targets based on inputs
    setDriveTarget(inputThrottle, inputTurn);

    // Apply Physics & Output
    outputPhysics();

    delay(5); // Small breather for Core 1
}
