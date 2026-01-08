#include "Captain.h"
#include "../config.h"

#ifdef ROLE_CAPTAIN

// ID for the Dome Peer (Lookout)
#define PEER_ID_LOOKOUT 2 

void Captain::setup() {
    Serial.println(">> CAPTAIN (Main Brain) Initializing...");
    
    // Safety startup delay
    delay(500);

    setupMotors();
    setupWiFi();
    web.begin();
    setupComms();
    
    Serial.println(">> System Ready. Connect to WiFi CHOPPER_NET");
}

void Captain::setupMotors() {
    Serial.println("- Attaching Motors...");
    leftMotor.attach(PIN_MOTOR_L_PWM);
    rightMotor.attach(PIN_MOTOR_R_PWM);
    domeSpinMotor.attach(PIN_DOME_SPIN);

    // Init to Stop
    leftMotor.write(90);
    rightMotor.write(90);
    domeSpinMotor.write(90);
}

void Captain::setupWiFi() {
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    Serial.print("- WiFi AP Started: ");
    Serial.println(WiFi.softAPIP());
}

void Captain::setupComms() {
    // Check Config.h for PIN definitions for UART
    Serial1.begin(115200, SERIAL_8N1, PIN_SERIAL_BARD_RX, PIN_SERIAL_BARD_TX);
    
    // Init ESP-NOW
    if(comms.begin(1)) { // I am ID 1
        Serial.println("- ESP-NOW Started");
    } else {
        Serial.println("! ESP-NOW Failed");
    }
}

void Captain::loop() {
    // 1. Maintain Comms
    comms.update();
    
    // 2. Get User Input
    WebInput input = web.getInput();

    // 3. E-STOP Check
    if(input.eStop) {
        Serial.println("!!! E-STOP TRIGGERED !!!");
        comms.sendEStop();
        leftMotor.write(90);
        rightMotor.write(90);
        domeSpinMotor.write(90);
        sendToBard("STOP");
        return; // Skip rest of loop
    }

    // 4. Drive Logic
    handleDrive(input.driveX, input.driveY);

    // 5. Dome Logic
    handleDome(input.domeX, input.domeY);

    // 6. Action Logic
    if(input.actionId > 0) {
        Serial.printf("Action: %d\n", input.actionId);
        
        // Tell Bard (Wire)
        String cmd = "PLAY:" + String(input.actionId);
        sendToBard(cmd);

        // Tell Lookout (Wireless)
        // Using sendAction(target, actionId, param)
        comms.sendAction(PEER_ID_LOOKOUT, input.actionId, 0);
    }
}

void Captain::handleDrive(int x, int y) {
    // Simple Tank/Arcade mixer
    // Y = Forward/Back (-100 to 100)
    // X = Left/Right (-100 to 100)

    // Invert X for natural steering if needed? Usually Right Stick X>0 means Turn Right.
    // If Logic: Left Motor = Y + X, Right Motor = Y - X
    
    int leftVal = y + x;
    int rightVal = y - x;

    // Constrain to -100 to 100
    leftVal = constrain(leftVal, -100, 100);
    rightVal = constrain(rightVal, -100, 100);

    // Map to Servo (0-180)
    // 90 is Stop. 
    // If Y is positive (Forward), we want > 90.
    
    // Inverse one motor if they are mirrored? 
    // Usually one motor is physically flipped. Assuming code handles it or wiring handles it.
    // If we need to flip, we do it here. For now, assume uniform.

    // Map -100..100 to 0..180
    // -100 -> 0
    // 0 -> 90
    // 100 -> 180
    int leftServo = map(leftVal, -100, 100, 0, 180);
    int rightServo = map(rightVal, -100, 100, 0, 180);
    
    leftMotor.write(leftServo);
    // Right motor usually needs inversion if not handled by hardware
    // rightMotor.write(180 - rightServo); // Uncomment if driving in circles
    rightMotor.write(rightServo);
}

void Captain::handleDome(int x, int y) {
    // X Axis -> Local Spin Motor
    // Deadband
    if(abs(x) < 10) x = 0;
    
    int spinVal = map(x, -100, 100, 0, 180);
    domeSpinMotor.write(spinVal);

    // Y Axis -> Remote Tilt (Lookout)
    // Only send if changed substantially to save airtime?
    // Or send periodically. HeroPropsProtocol handles sequence numbers.
    
    static unsigned long lastSent = 0;
    if(millis() - lastSent > 100) { // Limit to 10Hz
        // We hijack the 'Throttle' channel of sendDrive for Tilt
        // Send to Peer 2 (Lookout)
        comms.sendDrive(PEER_ID_LOOKOUT, y, 0);
        lastSent = millis();
    }
}

void Captain::sendToBard(String cmd) {
    Serial1.println(cmd);
}
#endif
