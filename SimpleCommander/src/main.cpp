#include <Arduino.h>
#include "hp_espnow.h"

// -------------------------------------------------------------------------
// ⚙️ Config
// -------------------------------------------------------------------------
#define COMMANDER_ID 0x00 // 0 is always Commander
#define TARGET_GROUP 0x01 // Default Group
#define BROADCAST_ID 0xFF

// Pin Definitions (Adjust for your hardware)
// Note: These are standard pins for many ESP32 dev boards, but check yours!
#define PIN_JOY_X 34
#define PIN_JOY_Y 35
#define PIN_BTN_1 0  // Boot button
#define PIN_BTN_2 4  // Example button

// -------------------------------------------------------------------------
// 🌍 Globals
// -------------------------------------------------------------------------
HeroPropsProtocol radio;

// Input State
struct InputState {
    int16_t throttle;
    int16_t turn;
    bool btn1Pressed;
    bool btn2Pressed;
};

InputState currentInput;

// -------------------------------------------------------------------------
// 🎮 Input Abstraction Layer
// -------------------------------------------------------------------------
// This is where we can swap in DMX, CRMX, or other inputs later.
void readInputs() {
    // 1. Read Analog Joysticks (0-4095)
    // Note: ADC2 pins cannot be used when WiFi is active. 
    // GPIO 34 and 35 are on ADC1, so they are safe.
    int rawX = analogRead(PIN_JOY_X);
    int rawY = analogRead(PIN_JOY_Y);

    // Map to -100 to 100
    // Note: Adjust mapping based on your joystick orientation
    // Assuming Center is ~2048
    currentInput.throttle = map(rawY, 0, 4095, -100, 100);
    currentInput.turn     = map(rawX, 0, 4095, -100, 100);

    // Deadzone
    if (abs(currentInput.throttle) < 10) currentInput.throttle = 0;
    if (abs(currentInput.turn) < 10) currentInput.turn = 0;

    // 2. Read Buttons (Active Low)
    currentInput.btn1Pressed = (digitalRead(PIN_BTN_1) == LOW);
    currentInput.btn2Pressed = (digitalRead(PIN_BTN_2) == LOW);
}

// -------------------------------------------------------------------------
// 🚀 Setup
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n🎮 SimpleCommander Booting...");

    // Init GPIO
    pinMode(PIN_BTN_1, INPUT_PULLUP);
    pinMode(PIN_BTN_2, INPUT_PULLUP);
    // Analog pins don't need pinMode on ESP32

    // Init Radio
    // begin(deviceId, groupId, isCommander)
    if (radio.begin(COMMANDER_ID, TARGET_GROUP, true)) { 
        Serial.println("✅ Radio Initialized (Commander Mode)");
    } else {
        Serial.println("❌ Radio Failed!");
        while(1);
    }
}

// -------------------------------------------------------------------------
// 🔄 Loop
// -------------------------------------------------------------------------
void loop() {
    static unsigned long lastTx = 0;
    const int TX_RATE_MS = 50; // 20Hz Update Rate

    // 1. Read Inputs
    readInputs();

    // 2. Transmit Loop
    if (millis() - lastTx > TX_RATE_MS) {
        lastTx = millis();

        // Send Drive Command (Broadcast to Group)
        // Target: 0xFF (Broadcast), Group logic handled by receiver
        // Or specific ID if needed.
        
        // For now, we broadcast to everyone.
        // In v2, we can target specific IDs.
        
        radio.sendDrive(BROADCAST_ID, currentInput.throttle, currentInput.turn);
        
        // Debug
        // Serial.printf("Tx: T%d R%d\n", currentInput.throttle, currentInput.turn);

        // Handle Buttons (Simple Edge Detection could be added)
        if (currentInput.btn1Pressed) {
            radio.sendAction(BROADCAST_ID, 1, 100); // Action 1
            Serial.println("Tx: Action 1");
        }
        
        if (currentInput.btn2Pressed) {
            radio.sendEStop();
            Serial.println("Tx: E-STOP!");
        }
    }

    // 3. Update Radio (Watchdogs, etc)
    radio.update();
}
