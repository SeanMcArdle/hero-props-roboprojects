#include <Arduino.h>
#include "config.h"
#include "hp_espnow.h"
#include <ESP32Servo.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include "web_server.h"

// -------------------------------------------------------------------------
// 🌍 Globals
// -------------------------------------------------------------------------
// NOTE: NUM_LEDS and PIN_NEOPIXEL are in config.h
Adafruit_NeoPixel strip(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

HeroPropsProtocol radio;

// Persistent droid name (loaded from NVS in setup(), falls back to BOT_NAME)
Preferences preferences;
char activeBotName[MAX_BOT_NAME_LEN + 1];

#ifndef MDNS_HOST
static const char *MDNS_HOST = "l00n";
#endif

unsigned long lastHeartbeat = 0;
bool isConnected = false;
unsigned long lastWsCleanup = 0;

// 🎆 Animation Globals
int ledMode = 10; // Default: PULSE
unsigned long lastLedUpdate = 0;
int aniStep = 0;
int aniPhase = 0;
int lastManualRed = -1;
int lastManualGreen = -1;
int lastManualBlue = -1;
int webPixelIdx = -1;
int webPixelR = 0;
int webPixelG = 0;
int webPixelB = 0;

Servo leftMotor;
Servo rightMotor;
Servo domeServo;

// -------------------------------------------------------------------------
// ⚡ RC Interrupts (Non-Blocking)
// -------------------------------------------------------------------------
// 0=Throttle, 1=Steering, 2=Spin
#if USE_RC_INPUT
volatile unsigned long rc_start[3] = {0, 0, 0};
volatile int rc_val[3] = {0, 0, 0};            // 0 = Invalid
volatile unsigned long rc_time[3] = {0, 0, 0}; // Timeout Watchdog

void IRAM_ATTR isrThrottle()
{
    unsigned long now = micros();
    if (digitalRead(PIN_RC_THROTTLE) == HIGH)
    {
        rc_start[0] = now;
    }
    else
    {
        if (rc_start[0] > 0)
        {
            rc_val[0] = now - rc_start[0];
            rc_time[0] = millis();
        }
    }
}

void IRAM_ATTR isrSteering()
{
    unsigned long now = micros();
    if (digitalRead(PIN_RC_STEERING) == HIGH)
    {
        rc_start[1] = now;
    }
    else
    {
        if (rc_start[1] > 0)
        {
            rc_val[1] = now - rc_start[1];
            rc_time[1] = millis();
        }
    }
}

void IRAM_ATTR isrSpin()
{
    unsigned long now = micros();
    if (digitalRead(PIN_RC_SPIN) == HIGH)
    {
        rc_start[2] = now;
    }
    else
    {
        if (rc_start[2] > 0)
        {
            rc_val[2] = now - rc_start[2];
            rc_time[2] = millis();
        }
    }
}
#endif

// -------------------------------------------------------------------------
// ⚙️ Motor Control Logic (With Physics Smoothing)
// -------------------------------------------------------------------------
// Smooth State Variables
float currentLeft = 0;
float currentRight = 0;
float currentDome = 0; // 360 continuous servo: normalized speed (-1.0 to +1.0)
float targetLeft = 0;
float targetRight = 0;
float targetDome = 0; // normalized speed target (-1.0 to +1.0)

// State to track if servos are electrically active
bool driveActive = true;
bool domeActive = true;

void outputPhysics()
{
    // 1. Smooth the Drive
    currentLeft = currentLeft * (1.0 - DRIVE_SMOOTH) + targetLeft * DRIVE_SMOOTH;
    currentRight = currentRight * (1.0 - DRIVE_SMOOTH) + targetRight * DRIVE_SMOOTH;

    // Snap to zero if very close (Deadzone cleaning)
    // Increased to 0.05 (5%) to ensure motor fully sleeps and noise doesn't wake it
    if (abs(currentLeft) < 0.05)
        currentLeft = 0;
    if (abs(currentRight) < 0.05)
        currentRight = 0;

    // 2. Smooth the Dome (360 continuous servo)
    currentDome = currentDome * (1.0 - DOME_SMOOTH) + targetDome * DOME_SMOOTH;
    if (abs(currentDome) < 0.02)
        currentDome = 0; // Snap to stop if very slow

    // ---------------------------------------------------------
    // 3A. DRIVE MOTOR SLEEP LOGIC
    // ---------------------------------------------------------
    bool driveIdle = (currentLeft == 0 && currentRight == 0);

    if (driveIdle)
    {
        if (driveActive)
        {
            leftMotor.detach();
            rightMotor.detach();
            driveActive = false;
        }
        // Don't return here! We need to process Dome independently.
    }
    else
    {
        if (!driveActive)
        {
            // DRIVE WAKEUP: Strict 5% input required to wake drive motors
            bool substantialDriveInput = (abs(targetLeft) > 0.05 || abs(targetRight) > 0.05);

            if (substantialDriveInput)
            {
                leftMotor.attach(PIN_MOTOR_LEFT, 500, 2500);
                rightMotor.attach(PIN_MOTOR_RIGHT, 500, 2500);
                driveActive = true;
            }
            else
            {
                // Stay asleep if input is tiny noise
                currentLeft = 0;
                currentRight = 0;
            }
        }
    }

    // ---------------------------------------------------------
    // 3B. DOME SLEEP LOGIC (360° Continuous Servo)
    // ---------------------------------------------------------
    bool domeIdle = (currentDome == 0);

    if (domeIdle)
    {
        if (domeActive)
        {
            domeServo.detach();
            domeActive = false;
        }
    }
    else
    {
        if (!domeActive)
        {
            // DOME WAKEUP: Strict inputs required
            bool substantialDomeInput = (abs(targetDome) > 0.05);

            if (substantialDomeInput)
            {
                domeServo.attach(PIN_DOME, 500, 2500);
                domeActive = true;
            }
            else
            {
                // Stay asleep if input is tiny noise
                currentDome = 0;
            }
        }
    }

    // 4. Output to Motors (Only if active)
    if (driveActive)
    {
        // Center is 1500. Range is +/- DRIVE_SPEED_MAX (400)
        int leftUs = 1500 + TRIM_LEFT + (int)(currentLeft * DRIVE_SPEED_MAX);
        int rightUs = 1500 + TRIM_RIGHT - (int)(currentRight * DRIVE_SPEED_MAX); // Inverted right

        // Safety Clamp
        leftUs = constrain(leftUs, 1000, 2000);
        rightUs = constrain(rightUs, 1000, 2000);

        leftMotor.writeMicroseconds(leftUs);
        rightMotor.writeMicroseconds(rightUs);
    }

    if (domeActive)
    {
        // Dome output: 360° servo uses speed (microseconds)
        // 1500µs = STOP, <1500 = CCW, >1500 = CW
        // currentDome is normalized speed (-1.0 to +1.0)
        int domeUs = 1500 + (int)(currentDome * DOME_SPEED_MAX);
        domeUs = constrain(domeUs, 1000, 2000);
        domeServo.writeMicroseconds(domeUs);
    }
}

void setDriveTarget(float throttle, float turn)
{
    // Inputs are -1.0 to 1.0
    // Simple Tank Mix for Joystick
    targetLeft = constrain(throttle + turn, -1.0f, 1.0f);
    targetRight = constrain(throttle - turn, -1.0f, 1.0f);

    // Immediate stop if inputs are zero (Optional, but BB-R2 usually lets it ramp down naturally.
    // If we want "snappy" stop but smooth accel, we can check for 0.
    // For now, let's keep it fully smoothed as requested ("don't destroy servos"))
    if (throttle == 0 && turn == 0)
    {
        // Optional: Uncomment to make stopping faster than starting
        // targetLeft = 0; targetRight = 0;
        // currentLeft = 0; currentRight = 0; // Hard stop
    }
}

// 🎆 Non-Blocking LED Animator
// -------------------------------------------------------------------------
// Modes: 10=Pulse, 11=Party, 12=Rainbow, 13=Scanner, 14=Manual
void handleLEDs()
{
    unsigned long now = millis();
    int interval = 30; // Default Frame Rate ~30fps

    if (ledMode == 10)
    { // 💜 PULSE (Prince Idle)
        interval = 50;
        if (now - lastLedUpdate > interval)
        {
            float breath = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
            int b = map(breath, 0, 255, 20, 150);
            strip.fill(strip.Color(b, 0, b)); // Purple-ish
            strip.show();
            lastLedUpdate = now;
        }
    }
    else if (ledMode == 11)
    { // 🎉 PARTY (Go Crazy - More Purple!)
        interval = 40;
        if (now - lastLedUpdate > interval)
        {
            // Random Sparkles with Heavy Purple Bias
            int i = random(NUM_LEDS);
            int dice = random(10); // 0-9

            if (dice < 6)
                strip.setPixelColor(i, strip.Color(255, 0, 255)); // 60% Purple
            else if (dice < 8)
                strip.setPixelColor(i, strip.Color(138, 43, 226)); // 20% Violet
            else if (dice == 8)
                strip.setPixelColor(i, strip.Color(255, 255, 255)); // 10% White Flash
            else
                strip.setPixelColor(i, strip.Color(255, 215, 0)); // 10% Gold

            // Fade all slightly
            for (int j = 0; j < NUM_LEDS; j++)
            {
                uint32_t c = strip.getPixelColor(j);
                uint8_t r = (uint8_t)(c >> 16);
                uint8_t g = (uint8_t)(c >> 8);
                uint8_t b = (uint8_t)c;
                if (r > 20)
                    r -= 20;
                else
                    r = 0; // Faster fade for punchiness
                if (g > 20)
                    g -= 20;
                else
                    g = 0;
                if (b > 20)
                    b -= 20;
                else
                    b = 0;
                strip.setPixelColor(j, r, g, b);
            }
            strip.show();
            lastLedUpdate = now;
        }
    }
    else if (ledMode == 12)
    { // 🌈 RAINBOW (Confetti Spin)
        interval = 30;
        if (now - lastLedUpdate > interval)
        {
            aniStep += 2; // Speed up
            for (int i = 0; i < NUM_LEDS; i++)
            {
                // "Theater Chase" style Rainbow
                if ((i + aniStep / 4) % 3 == 0)
                {
                    int pixelHue = (aniStep * 20) + (i * 65536L / NUM_LEDS);
                    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
                }
                else
                {
                    strip.setPixelColor(i, 0); // Off for chase effect
                }
            }
            strip.show();
            lastLedUpdate = now;
        }
    }
    else if (ledMode == 13)
    {                  // 🔦 SCANNER (Beefy 5-pixel Eyes)
        interval = 40; //
        if (now - lastLedUpdate > interval)
        {
            strip.clear();

            // Draw 5-pixel wide eyes
            for (int k = 0; k < 5; k++)
            {
                // Brightness taper for the "tail" of the eye
                // Center pixel (k=2) is brightest
                int dim = 0;
                if (k == 0 || k == 4)
                    dim = 150; // Dim edges

                // Eye 1 (Red)
                int pos1 = (aniStep + k) % NUM_LEDS;
                strip.setPixelColor(pos1, strip.Color(255 - dim, 0, 0));

                // Eye 2 (Blue - Opposite)
                int pos2 = (aniStep + (NUM_LEDS / 2) + k) % NUM_LEDS;
                strip.setPixelColor(pos2, strip.Color(0, 0, 255 - dim));
            }

            strip.show();
            aniStep++;
            lastLedUpdate = now;
        }
    }
    else if (ledMode == 14)
    { // 🎛️ MANUAL (Sliders)
        // Update only when RGB values change to avoid unnecessary bus traffic.
        if (webRed != lastManualRed || webGreen != lastManualGreen || webBlue != lastManualBlue)
        {
            strip.fill(strip.Color(webRed, webGreen, webBlue));
            strip.show();
            lastManualRed = webRed;
            lastManualGreen = webGreen;
            lastManualBlue = webBlue;
        }
    }
    else if (ledMode == 15)
    { // 🖊️ PER-PIXEL (Command Line)
        if (webPixelIdx >= 0)
        {
            strip.setPixelColor(webPixelIdx, strip.Color(webPixelR, webPixelG, webPixelB));
            strip.show();
            webPixelIdx = -1;
        }
    }
}

// -------------------------------------------------------------------------
// Wrapper for legacy calls (int -100 to 100)
void drive(int throttle, int turn)
{
    setDriveTarget(throttle / 100.0f, turn / 100.0f);
}

void runBootLedSelfTest()
{
    // Quick visual check: each color should appear cleanly on all 8 pixels.
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
    delay(180);

    strip.fill(strip.Color(0, 255, 0));
    strip.show();
    delay(180);

    strip.fill(strip.Color(0, 0, 255));
    strip.show();
    delay(180);

    strip.fill(0);
    strip.show();
}

// -------------------------------------------------------------------------
// 📡 ESP-NOW Callback (The Swarm Layer)
// -------------------------------------------------------------------------
void onDataReceived(const HpHeader &header, const uint8_t *payload, size_t len)
{
    // Only process Critical Events (Like E-Stop) if RC is active
    if (header.msgType == HP_MSG_ESTOP)
    {
        drive(0, 0);
        Serial.println("🚨 E-STOP via ESP-NOW");
    }
    // We can add "Swarm Follow" logic here later
}

// -------------------------------------------------------------------------
// 🚀 Setup
// -------------------------------------------------------------------------
void setup()
{
    // 0. Safety Delay & Serial Init
    delay(2000); // Give power time to stabilize and USB to enumerate
    Serial.begin(115200);

    // Load persistent droid name (falls back to compiled-in BOT_NAME on first boot)
    preferences.begin("botcfg", false);
    String storedName = preferences.getString("name", "");
    if (storedName.length() > 0)
    {
        storedName.toCharArray(activeBotName, sizeof(activeBotName));
    }
    else
    {
        strncpy(activeBotName, BOT_NAME, sizeof(activeBotName) - 1);
        activeBotName[sizeof(activeBotName) - 1] = '\0';
    }

    // Dynamic Title
    Serial.printf("\n\n🤖 OMNI-NODE V1 (%s) Booting...\n", activeBotName);
    Serial.printf("🔧 Hardware Mapping: L=%d, R=%d, Dome=%d, NeoPixel=%d\n",
                  PIN_MOTOR_LEFT, PIN_MOTOR_RIGHT, PIN_DOME, PIN_NEOPIXEL);

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
    domeServo.writeMicroseconds(1500); // 360 continuous: 1500µs = STOP

    // 1b. Init LEDs (Adafruit NeoPixel RGB)
    strip.begin();
    strip.setBrightness(120); // Brighter for the Teacher Demo!
    runBootLedSelfTest();

    // 2. Attach Interrupts for RC (optional, disabled in Web-only baseline)
#if USE_RC_INPUT
    // Use pulldowns to prevent floating pin noise if receiver is off/disconnected.
    pinMode(PIN_RC_THROTTLE, INPUT_PULLDOWN);
    pinMode(PIN_RC_STEERING, INPUT_PULLDOWN);
    pinMode(PIN_RC_SPIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PIN_RC_THROTTLE), isrThrottle, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_RC_STEERING), isrSteering, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_RC_SPIN), isrSpin, CHANGE);
#endif

    drive(0, 0);

    // 3. Init WiFi & Web Server (The Interface)
    if (USE_WIFI_AP)
    {
        WiFi.mode(WIFI_AP);
        // REMOVED: WiFi.setSleep(false); -> Reverting to save power, using Detach logic instead.
        WiFi.softAP(WIFI_SSID_NAME, WIFI_PASS, 1, 0, 1); // Channel 1, Hidden 0, Max 1 client

        // REDUCE TX POWER to 11dBm (approx 12mW) to prevent brownouts and servo interference
        // Default is 19.5dBm (almost 100mW). Must be called AFTER WiFi is started.
        WiFi.setTxPower(WIFI_POWER_11dBm);

        Serial.print("📡 WiFi AP: ");
        Serial.println(WIFI_SSID_NAME);
        Serial.print("👉 http://");
        Serial.println(WiFi.softAPIP());

        if (MDNS.begin(MDNS_HOST))
        {
            MDNS.addService("http", "tcp", 80);
            Serial.print("🌐 mDNS URL: http://");
            Serial.print(MDNS_HOST);
            Serial.println(".local");
        }
        else
        {
            Serial.println("⚠️ mDNS start failed");
        }

        setupWebServer();
    }
    else
    {
        Serial.println("📡 WiFi AP disabled (USE_WIFI_AP=0)");
    }

    // 4. Init OTA
    ArduinoOTA.setHostname(MDNS_HOST);
    ArduinoOTA.setPassword("heroprops");
    // SAFETY: Stop motors before OTA update starts (PWM keeps running during core blocking)
    ArduinoOTA.onStart([]()
                       { drive(0, 0); });
    ArduinoOTA.begin();

    // 5. Init ESP-NOW (The Nervous System)
    if (radio.begin(BOT_ID))
    {
        Serial.println("✅ ESP-NOW Ready");
        radio.onReceive(onDataReceived);
    }
    else
    {
        Serial.println("❌ ESP-NOW Init Failed");
    }
}

// -------------------------------------------------------------------------
// 🔄 Loop
// -------------------------------------------------------------------------
void loop()
{
    ArduinoOTA.handle();
    radio.update();

    if (millis() - lastWsCleanup > 1000)
    {
        serviceWebServer();
        lastWsCleanup = millis();
    }

    // Apply any pending rename from the web UI. This is the ONLY place
    // Preferences/NVS gets written (never from the async WS callback).
    if (renameRequested)
    {
        renameRequested = false;
        if (strncmp(activeBotName, pendingBotName, sizeof(activeBotName)) != 0)
        {
            strncpy(activeBotName, pendingBotName, sizeof(activeBotName) - 1);
            activeBotName[sizeof(activeBotName) - 1] = '\0';
            preferences.putString("name", activeBotName);
            broadcastConfig();
            Serial.printf("✏️  Droid renamed to: %s\n", activeBotName);
        }
    }

    // ----------------------
    // 1. Process Logic
    // ----------------------

    // SAFETY: Web Watchdog
    // If no packet received for 500ms, force stop.
    if (millis() - lastWebPacket > 500)
    {
        webDriveX = 0;
        webDriveY = 0;
        webDomeX = 100; // Center dome control on watchdog timeout
    }

    // A. Dome Logic (360° Continuous Rotation)
    // webDomeX is 0-200. Center 100=stop.
    // Map to -100 to +100 (percentage of max speed)
    // 0-100: Counterclockwise (negative), 100-200: Clockwise (positive)
    targetDome = map(webDomeX, 0, 200, -100, 100) / 100.0;

    // B. Handle Web Commands (Sound/Actions)
    if (webCommandId > 0)
    {
        Serial.printf("🔊 Web Cmd: %d\n", webCommandId);

        // Mode Select (Teacher Edition)
        if (webCommandId >= 10 && webCommandId <= 15)
        {
            ledMode = webCommandId;
        }

        // Legacy Support / Reset
        if (webCommandId == 5)
        { // Off
            ledMode = 14;
            webRed = 0;
            webGreen = 0;
            webBlue = 0;
            strip.fill(0);
            strip.show();
        }

        // Reset command
        webCommandId = 0;
    }

    // C. Drive Logic (Web-only baseline, optional RC override)
    float inputThrottle = 0;
    float inputTurn = 0;
    bool rcActive = false;
    unsigned long now = millis();

#if USE_RC_INPUT
    // Check recent RC valid pulses (must be within last 250ms to be valid)
    // This allows for missed packets but fails safe if receiver dies or interference blocks signals
    if (rc_val[0] > 900 && rc_val[0] < 2100 && (now - rc_time[0] < 250))
    {
        // Valid Throttle
        int val = rc_val[0];
        // Deadzone
        if (val > (1500 - RC_DEADZONE) && val < (1500 + RC_DEADZONE))
            val = 1500;
        inputThrottle = map(val, 1000, 2000, -100, 100) / 100.0f;
        if (inputThrottle != 0)
            rcActive = true;
    }

    if (rc_val[1] > 900 && rc_val[1] < 2100 && (now - rc_time[1] < 250))
    {
        // Valid Steering
        int val = rc_val[1];
        if (val > (1500 - RC_DEADZONE) && val < (1500 + RC_DEADZONE))
            val = 1500;
        inputTurn = map(val, 1000, 2000, -100, 100) / 100.0f;
        if (inputTurn != 0)
            rcActive = true;
    }

    if (rc_val[2] > 900 && rc_val[2] < 2100 && (now - rc_time[2] < 250))
    {
        // Valid Spin
        int val = rc_val[2];
        if (val > (1500 - RC_DEADZONE) && val < (1500 + RC_DEADZONE))
            val = 1500;
        float r = map(val, 1000, 2000, -100, 100) / 100.0f;
        if (r != 0)
        {
            inputThrottle = 0;
            inputTurn = r;
            rcActive = true;
        }
    }
#endif

    if (!rcActive)
    {
        // Fallback to Web Drive (Safety: Watchdog prevents runaway)
        // Web variables are int -100 to 100
        inputThrottle = webDriveY / 100.0f;
        inputTurn = webDriveX / 100.0f;
    }

    // Set targets based on inputs
    setDriveTarget(inputThrottle, inputTurn);

    // Apply Physics & Output
    outputPhysics();

    // D. Update LED Animations
    handleLEDs();

    delay(5); // Small breather for Core 1
}
