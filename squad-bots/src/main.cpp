#include <Arduino.h>
#include "config.h"
#include "hp_espnow.h"
#include <ESP32Servo.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "DFRobotDFPlayerMini.h"

// -------------------------------------------------------------------------
// 🌍 Globals
// -------------------------------------------------------------------------
HeroPropsProtocol radio;
DFRobotDFPlayerMini myDFPlayer;
HardwareSerial dfSerial(2); // Use UART2

unsigned long lastHeartbeat = 0;
bool isConnected = false; // FIX: Track connection state
bool isWiggling = false;  // FIX: Prevent radio interference during wiggle

Servo leftMotor;
Servo rightMotor;
Servo domeServo;

// -------------------------------------------------------------------------
// 🎮 Control State
// -------------------------------------------------------------------------
int wifiThrottle = 0;
int wifiTurn = 0;

// -------------------------------------------------------------------------
// ⚙️ Motor Control Logic
// -------------------------------------------------------------------------
void setMotorSpeed(Servo& motor, int speed, int trim) {
    // Speed input: -100 to 100
    // Servo output: 0 to 180 (90 is stop)
    
    // 1. Apply Trim
    // Trim is added to the center point. 
    // If trim is +10, "stop" becomes 100 instead of 90.
    // But usually trim is added to the final output.
    // Let's stick to: Output = Map(Speed) + Trim
    
    int output = map(speed, -100, 100, MOTOR_MIN, MOTOR_MAX);
    output += trim;
    
    // 2. Clamp
    output = constrain(output, MOTOR_MIN, MOTOR_MAX);
    
    motor.write(output);
}

void drive(int throttle, int turn) {
    // Arcade Drive Mixing
    // Throttle: -100 (Back) to 100 (Fwd)
    // Turn: -100 (Left) to 100 (Right)
    
    int left = throttle + turn;
    int right = throttle - turn;
    
    // Clamp to -100 to 100
    left = constrain(left, -100, 100);
    right = constrain(right, -100, 100);
    
    // Invert one side if motors are mirrored (usually needed for differential drive)
    // Assuming "Forward" means both spin "forward" relative to the bot.
    // Often one servo is mounted mirrored. 
    // Let's assume standard config: Left Normal, Right Inverted.
    // Adjust signs here if bot spins in place when asked to go forward.
    
    setMotorSpeed(leftMotor, left, TRIM_LEFT);
    setMotorSpeed(rightMotor, -right, TRIM_RIGHT); // Invert right side
}

void startupWiggle() {
    isWiggling = true;
    Serial.println("👋 Wiggle Check...");
    // Left Fwd, Right Back
    drive(30, 30); // Spin
    delay(200);
    // Left Back, Right Fwd
    drive(-30, -30); // Spin back
    delay(200);
    // Stop
    drive(0,0);
    isWiggling = false;
}

// -------------------------------------------------------------------------
// 📡 Protocol Callbacks
// -------------------------------------------------------------------------
void onDataReceived(const HpHeader& header, const uint8_t* payload, size_t len) {
    if (isWiggling) return; // FIX: Ignore packets during startup dance

    // 1. Update Watchdog
    lastHeartbeat = millis();
    isConnected = true; // FIX: We are connected

    // 2. Handle Message Types
    switch (header.msgType) {
        case HP_MSG_ESTOP:
            Serial.println("🚨 E-STOP RECEIVED!");
            drive(0, 0); // HARD STOP
            break;

        case HP_MSG_DRIVE:
            if (len == sizeof(HpPayloadDrive)) {
                HpPayloadDrive* cmd = (HpPayloadDrive*)payload;
                // Update target values, let loop() handle the driving
                wifiThrottle = cmd->throttle;
                wifiTurn = cmd->turn;
            }
            break;

        case HP_MSG_ACTION:
            if (len == sizeof(HpPayloadAction)) {
                HpPayloadAction* action = (HpPayloadAction*)payload;
                Serial.printf("🎬 Action: ID %d, Param %d\n", action->actionId, action->parameter);
                
                // Action 10: Dome Control (0-200, Center 100)
                if (action->actionId == 10) {
                    int domeAngle = map(action->parameter, 0, 200, 0, 180);
                    domeServo.write(domeAngle);
                }

                // Action 1: Play Sound (Param = Track Number, 0 = Random)
                if (action->actionId == 1) {
                    if (action->parameter == 0) {
                        myDFPlayer.next();
                    } else {
                        myDFPlayer.play(action->parameter);
                    }
                }
            }
            break;
            
        default:
            break;
    }
}

// -------------------------------------------------------------------------
// 🚀 Setup
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n🤖 JL-BN 'Jellybean' Booting...");

    // 1. Init Hardware (Motors)
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    leftMotor.setPeriodHertz(50);
    leftMotor.attach(PIN_MOTOR_LEFT, 500, 2500); // Standard Servo Range

    rightMotor.setPeriodHertz(50);
    rightMotor.attach(PIN_MOTOR_RIGHT, 500, 2500);

    domeServo.setPeriodHertz(50);
    domeServo.attach(PIN_DOME, 500, 2500);
    domeServo.write(90); // Center Dome

    // Init RC Input Pins
    pinMode(PIN_RC_THROTTLE, INPUT);
    pinMode(PIN_RC_STEERING, INPUT);
    pinMode(PIN_RC_SPIN, INPUT);

    drive(0, 0); // Ensure stopped
    // startupWiggle(); // FIX: Moved to end of setup to avoid blocking radio init

    // 2. Init WiFi (Hybrid Mode)
    // We create an AP so we can OTA update even without a router
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    Serial.print("📡 AP Created: ");
    Serial.println(WIFI_SSID);
    Serial.print("👉 IP Address: ");
    Serial.println(WiFi.softAPIP());

    // 3. Init OTA
    ArduinoOTA.setHostname(BOT_NAME);
    ArduinoOTA.setPassword("heroprops");
    
    ArduinoOTA.onStart([]() {
        drive(0,0); // Safety stop
        Serial.println("📦 OTA Update Started");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n📦 OTA Update Complete");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // Optional: Blink LED
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("❌ OTA Error[%u]: ", error);
    });
    ArduinoOTA.begin();
    Serial.println("✅ OTA Ready");

    // 4. Init Protocol
    // Note: HeroPropsProtocol will detect we are in AP mode and NOT reset WiFi
    if (radio.begin(BOT_ID)) {
        Serial.println("✅ ESP-NOW Initialized");
        radio.onReceive(onDataReceived);
    } else {
        Serial.println("❌ ESP-NOW Failed!");
        // Blink Error LED
    }

    pinMode(PIN_NEOPIXEL, OUTPUT);
    
    // 5. Init Audio
    dfSerial.begin(9600, SERIAL_8N1, PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
    if (myDFPlayer.begin(dfSerial)) {
        Serial.println("✅ DFPlayer Online");
        myDFPlayer.volume(20); // Set volume (0-30)
        myDFPlayer.play(1);    // Play startup sound
    } else {
        Serial.println("⚠️ DFPlayer Not Found (Check Wiring)");
    }

    // FIX: Init Watchdog & Wiggle
    lastHeartbeat = millis();
    startupWiggle(); 
}

// -------------------------------------------------------------------------
// 🔄 Loop
// -------------------------------------------------------------------------
void loop() {
    // 1. Handle OTA & Protocol
    ArduinoOTA.handle();
    radio.update();

    // 2. Read RC Input (The Pilot)
    // Timeout 25000us (25ms) to avoid blocking too long if receiver is off
    unsigned long rcT_raw = pulseIn(PIN_RC_THROTTLE, HIGH, 25000);
    unsigned long rcS_raw = pulseIn(PIN_RC_STEERING, HIGH, 25000);
    unsigned long rcR_raw = pulseIn(PIN_RC_SPIN, HIGH, 25000); // Spin Input

    int finalThrottle = 0;
    int finalTurn = 0;
    bool rcOverride = false;

    // Check if RC signal is valid (Receiver is ON and connected)
    if (rcT_raw > 900 && rcT_raw < 2100) {
        // Map Pulse (1000-2000) to Speed (-100 to 100)
        int t = map(rcT_raw, RC_MIN_PULSE, RC_MAX_PULSE, -100, 100);
        int s = map(rcS_raw, RC_MIN_PULSE, RC_MAX_PULSE, -100, 100);
        int r = map(rcR_raw, RC_MIN_PULSE, RC_MAX_PULSE, -100, 100);

        // Apply Deadzone
        if (abs(t) < RC_DEADZONE) t = 0;
        if (abs(s) < RC_DEADZONE) s = 0;
        if (abs(r) < RC_DEADZONE) r = 0;

        // Priority Logic: Spin > Drive
        if (r != 0) {
            // Spin in place (Left Stick X)
            finalThrottle = 0;
            finalTurn = r;
            rcOverride = true;
        } else if (t != 0 || s != 0) {
            // Normal Drive (Right Stick)
            finalThrottle = t;
            finalTurn = s;
            rcOverride = true;
        }
    }

    // 3. WiFi / Mixer Logic
    if (!rcOverride) {
        // If RC is not active (or receiver off), use WiFi values
        finalThrottle = wifiThrottle;
        finalTurn = wifiTurn;

        // WiFi Watchdog: Only apply if we are relying on WiFi
        if (isConnected && (millis() - lastHeartbeat > 1000)) {
            finalThrottle = 0;
            finalTurn = 0;
            // Serial.println("⚠️ WiFi Failsafe");
        }
    }

    // 4. Execute Drive
    drive(finalThrottle, finalTurn);
}
