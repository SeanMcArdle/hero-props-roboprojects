#include "Lookout.h"
#include "../config.h"

#ifdef ROLE_LOOKOUT

void Lookout::setup() {
    Serial.println(">> LOOKOUT (Dome) Initializing...");
    
    // 1. Setup Lights
    setupLights();
    
    // 2. Setup Servos
    setupServos();
    
    // 3. Setup Comms
    comms.onReceive([this](const HpHeader& header, const uint8_t* payload, size_t len) {
        this->onPacketReceived(header, payload, len);
    });
    
    if(comms.begin(2)) { // Role 2 = Dome
        Serial.println("- ESP-NOW Listening");
        // Flash Green to indicate sync
        lights.fill(lights.Color(0, 255, 0));
        lights.show();
        delay(500);
        lights.clear();
        lights.show();
    } else {
        Serial.println("! ESP-NOW Failed");
        lights.fill(lights.Color(255, 0, 0));
        lights.show();
    }
}

void Lookout::setupLights() {
    lights = Adafruit_NeoPixel(NUM_NEOPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
    lights.begin();
    lights.setBrightness(150);
    lights.clear(); // Initialize all pixels to 'off'
    lights.show(); 
}

void Lookout::setupServos() {
    Serial.println("- Attaching Dome Servos...");
    armLeft.attach(PIN_SERVO_ARM_L);
    armRight.attach(PIN_SERVO_ARM_R);
    headTilt.attach(PIN_SERVO_TILT);

    // Initial Positions (Arms down, head level)
    armLeft.write(0);
    armRight.write(180); // Assuming mirrored
    headTilt.write(90);  // Center
}

void Lookout::loop() {
    comms.update();
    updateAnimations();
}

void Lookout::onPacketReceived(const HpHeader& header, const uint8_t* payload, size_t len) {
    // Handle Message Types
    switch(header.msgType) {
        case HP_MSG_DRIVE: {
            // Captain sends Tilt info on Drive channel (Throttle)
            if (len == sizeof(HpPayloadDrive)) {
                HpPayloadDrive* drv = (HpPayloadDrive*)payload;
                // Map -100..100 to Servo 0..180
                // Invert? Let's assume input y is Up=Positive
                int tilt = map(drv->throttle, -100, 100, 45, 135); // Limit range mechanically?
                headTilt.write(tilt);
            }
            break;
        }
        case HP_MSG_ACTION: {
            if (len == sizeof(HpPayloadAction)) {
                HpPayloadAction* act = (HpPayloadAction*)payload;
                triggerAction(act->actionId);
            }
            break;
        }
        case HP_MSG_ESTOP: {
            Serial.println("!!! E-STOP !!!");
            armLeft.write(0);
            armRight.write(180);
            headTilt.write(90);
            lights.fill(lights.Color(255, 0, 0)); // Red Alert
            lights.show();
            currentAnimation = 0;
            break;
        }
    }
}

void Lookout::triggerAction(uint8_t actionId) {
    Serial.printf("Action Triggered: %d\n", actionId);
    currentAnimation = actionId;
    lastActionTime = millis();
    
    // Immediate response for some actions
    if(actionId == 1) { // Happy
        lights.fill(lights.Color(255, 100, 0)); // Orange
        lights.show();
    }
    else if(actionId == 2) { // Angry
        lights.fill(lights.Color(255, 0, 0)); // Red
        lights.show();
    }
}

void Lookout::updateAnimations() {
    if (currentAnimation == 0) return;

    unsigned long elapsed = millis() - lastActionTime;

    // Example Animation: Waving Arms for Action 1
    if (currentAnimation == 1) {
        if (elapsed < 2000) {
            // Simple sine wave
            int angle = 90 + 45 * sin(elapsed * 0.01); 
            armLeft.write(map(angle, 45, 135, 0, 90));  // Flap left
            armRight.write(map(angle, 45, 135, 180, 90)); // Flap right
        } else {
            // Reset
            currentAnimation = 0;
            armLeft.write(0);
            armRight.write(180);
            lights.clear();
            lights.show();
        }
    }
    // Example Animation: Shake Head for Action 2 (Angry)
    else if(currentAnimation == 2) {
        if (elapsed < 2000) {
            // Fast Shake
            int angle = 90 + 30 * sin(elapsed * 0.02);
            headTilt.write(angle);
        } else {
             currentAnimation = 0;
             headTilt.write(90);
             lights.clear();
             lights.show();
        }
    }
    else {
        // Unknown action, just reset after 1s
        if (elapsed > 1000) {
            currentAnimation = 0; 
            lights.clear();
            lights.show();
        }
    }
}

#endif
