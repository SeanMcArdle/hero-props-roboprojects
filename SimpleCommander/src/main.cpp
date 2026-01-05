#include <Arduino.h>
#include "hp_espnow.h"

// -------------------------------------------------------------------------
// ⚙️ Config
// -------------------------------------------------------------------------
#define COMMANDER_ID 0x00 // 0 is always Commander
#define TARGET_GROUP 0x01 // Default Group
#define BROADCAST_ID 0xFF

// Pin Definitions (Single Stick Drive + Dome)
// Left Stick (Drive)
#define PIN_THROTTLE_AXIS 35  // ADC1_CH7 (VRy)
#define PIN_TURN_AXIS     34  // ADC1_CH6 (VRx)
#define PIN_BTN_L         25  // Optional

// Right Stick (Dome)
#define PIN_DOME_AXIS     33  // ADC1_CH5 (VRx)
#define PIN_BTN_R         26  // Optional

// Calibration & Tuning
const int JOY_MIN = 0;
const int JOY_MAX = 4095;
const int JOY_CENTER = 2048; // Approx center
const int DEADZONE = 150;    // Ignore small movements around center

// Inversion Flags (Set to true if controls are backwards)
const bool INVERT_THROTTLE = true; // Joysticks often read 0 at top, 4095 at bottom
const bool INVERT_TURN     = false;
const bool INVERT_DOME     = false;

// -------------------------------------------------------------------------
// 🌍 Globals
// -------------------------------------------------------------------------
HeroPropsProtocol radio;

// Input State
struct InputState {
    int16_t throttle;
    int16_t turn;
    int16_t dome;
    bool btnL_Pressed;
    bool btnR_Pressed;
};

InputState currentInput;
bool safeStartComplete = false; // Prevent runaway on boot

// -------------------------------------------------------------------------
// 🎮 Input Processing
// -------------------------------------------------------------------------
int processAxis(int rawValue, bool invert) {
    // 1. Center Offset
    int centered = rawValue - JOY_CENTER;

    // 2. Deadzone
    if (abs(centered) < DEADZONE) {
        return 0;
    }

    // 3. Map to -100 to 100
    // We map from (DEADZONE to 2048) -> (0 to 100)
    int output = 0;
    if (centered > 0) {
        output = map(centered, DEADZONE, (JOY_MAX - JOY_CENTER), 0, 100);
    } else {
        output = map(centered, -DEADZONE, -(JOY_CENTER - JOY_MIN), 0, -100);
    }

    // 4. Constrain
    output = constrain(output, -100, 100);

    // 5. Invert
    if (invert) output = -output;

    return output;
}

void readInputs() {
    // 1. Read Analog
    int rawThrottle = analogRead(PIN_THROTTLE_AXIS);
    int rawTurn     = analogRead(PIN_TURN_AXIS);
    int rawDome     = analogRead(PIN_DOME_AXIS);

    // 2. Process
    currentInput.throttle = processAxis(rawThrottle, INVERT_THROTTLE);
    currentInput.turn     = processAxis(rawTurn, INVERT_TURN);
    currentInput.dome     = processAxis(rawDome, INVERT_DOME);

    // 3. Read Buttons (Active Low with Pullup)
    // Note: If pins are not connected, internal pullup keeps them HIGH (false)
    currentInput.btnL_Pressed = (digitalRead(PIN_BTN_L) == LOW);
    currentInput.btnR_Pressed = (digitalRead(PIN_BTN_R) == LOW);
}

// -------------------------------------------------------------------------
// 🚀 Setup
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n🎮 SimpleCommander Booting (Single Stick Mode)...");

    // Init GPIO
    pinMode(PIN_BTN_L, INPUT_PULLUP);
    pinMode(PIN_BTN_R, INPUT_PULLUP);

    // Init Radio
    if (radio.begin(COMMANDER_ID, TARGET_GROUP, true)) { 
        Serial.println("✅ Radio Initialized (Commander Mode)");
    } else {
        Serial.println("❌ Radio Failed!");
        while(1);
    }

    Serial.println("⚠️  WAITING FOR CENTER (SAFE START)...");
}

// -------------------------------------------------------------------------
// 🔄 Loop
// -------------------------------------------------------------------------
void loop() {
    static unsigned long lastTx = 0;
    const int TX_RATE_MS = 50; // 20Hz Update Rate

    // 1. Read Inputs
    readInputs();

    // 2. Safe Start Check
    if (!safeStartComplete) {
        if (currentInput.throttle == 0 && currentInput.turn == 0) {
            safeStartComplete = true;
            Serial.println("✅ Safe Start Complete. Controls Active.");
        } else {
            // Blink LED or print warning?
            static unsigned long lastWarn = 0;
            if (millis() - lastWarn > 1000) {
                Serial.printf("⚠️  Center sticks to start! (T:%d R:%d)\n", currentInput.throttle, currentInput.turn);
                lastWarn = millis();
            }
            return; // Don't transmit yet
        }
    }

    // 3. Transmit Loop
    if (millis() - lastTx > TX_RATE_MS) {
        lastTx = millis();

        // Send Drive Command (Left Stick)
        radio.sendDrive(BROADCAST_ID, currentInput.throttle, currentInput.turn);
        
        // Send Dome Command (Right Stick)
        // Note: Protocol doesn't have explicit Dome message yet, using Action 10 for now
        // Receiver needs to be updated to handle this!
        if (abs(currentInput.dome) > 5) {
             // Map -100..100 to 0..255 for Action Param? 
             // Or just send raw value if we add a custom packet.
             // For now, let's just print it to debug.
             // Serial.printf("Dome: %d\n", currentInput.dome);
             
             // Temporary: Use Action 10 for Dome (Param = 0-200, 100=Center)
             uint8_t domeVal = map(currentInput.dome, -100, 100, 0, 200);
             radio.sendAction(BROADCAST_ID, 10, domeVal);
        }

        // Handle Buttons
        if (currentInput.btnL_Pressed) {
            radio.sendAction(BROADCAST_ID, 1, 100); // Action 1
            Serial.println("Tx: Action 1");
        }
        
        if (currentInput.btnR_Pressed) {
            radio.sendEStop();
            Serial.println("Tx: E-STOP!");
        }
    }

    // 4. Update Radio (Watchdogs, etc)
    radio.update();
}
