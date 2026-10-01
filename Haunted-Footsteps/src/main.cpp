#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "web_server.h"
#include "effects.h"

// ===== LED STRIP (Adafruit NeoPixel — same library/pattern as squad-bots) =====
// WS2812B strips are almost always wired GRB internally, matching the
// proven squad-bots pattern on this same hardware/pin. (NEO_RGB was tried
// first and produced swapped red/green -- e.g. a "red" slider setting
// rendering as green -- which is the classic symptom of this exact mistake.)
Adafruit_NeoPixel strip(NUM_LEDS_TOTAL, PIN_LED_STRIP, NEO_GRB + NEO_KHZ800);

// ===== CHANNEL STRUCTURE =====
struct FootChannel
{
    uint16_t ledStart;
    uint16_t ledCount;
    uint8_t buttonPin;
    float envelope;
    uint32_t noiseSeed;  // fixed per-channel identity, keeps heel/toe shimmer decorrelated
    uint32_t noisePhase; // advances over time at a rate set by settings.flickerSpeed
    volatile bool *telemetryPressed;
    volatile uint8_t *telemetryEnvelope;
};

// Compile-time guard: catches pixel-map configuration mistakes (e.g. a
// channel's start+count running past the physical strip length) at build
// time instead of silently writing out-of-bounds pixels at runtime.
static_assert(HEEL_LED_START + HEEL_LED_COUNT <= NUM_LEDS_TOTAL, "Heel channel exceeds NUM_LEDS_TOTAL");
static_assert(TOE_LED_START + TOE_LED_COUNT <= NUM_LEDS_TOTAL, "Toe channel exceeds NUM_LEDS_TOTAL");

// ===== TELEMETRY (extern from web_server.h, defined here) =====
volatile bool telemetryToePressed = false;
volatile bool telemetryHeelPressed = false;
volatile uint8_t telemetryToeEnvelope = 0;
volatile uint8_t telemetryHeelEnvelope = 0;

// ===== SETTINGS (extern from web_server.h, defined here) =====
FootstepSettings settings;

// ===== CHANNELS =====
FootChannel toeChannel = {
    TOE_LED_START,
    TOE_LED_COUNT,
    PIN_BUTTON_TOE,
    0.0f,
    0x1701BEEFu,
    0u,
    &telemetryToePressed,
    &telemetryToeEnvelope};

FootChannel heelChannel = {
    HEEL_LED_START,
    HEEL_LED_COUNT,
    PIN_BUTTON_HEEL,
    0.0f,
    0x1337C0DEu,
    0u,
    &telemetryHeelPressed,
    &telemetryHeelEnvelope};

unsigned long lastLedUpdate = 0;
const unsigned long LED_UPDATE_INTERVAL = 20; // ~50Hz

// ===== ENVELOPE, FLICKER & RENDER (thin wrapper around effects.h's pure logic) =====
void updateChannel(FootChannel &ch, unsigned long dtMs)
{
    bool buttonPressed = digitalRead(ch.buttonPin) == LOW;
    *ch.telemetryPressed = buttonPressed;

    ch.envelope = advanceEnvelope(ch.envelope, buttonPressed,
                                  settings.fadeInMs, settings.fadeOutMs,
                                  MIN_FADE_OUT_RATIO, (uint32_t)dtMs);

    *ch.telemetryEnvelope = (uint8_t)(ch.envelope * 255.0f);

    ch.noisePhase = advanceNoisePhase(ch.noisePhase, settings.flickerSpeed, (uint32_t)dtMs);

    for (uint16_t i = 0; i < ch.ledCount; i++)
    {
        uint8_t noiseVal = noise8(ch.noiseSeed, i, ch.noisePhase);

        uint8_t r, g, b;
        computePixelColor(ch.envelope, settings.intensity, noiseVal,
                          settings.colorR, settings.colorG, settings.colorB,
                          MAX_BRIGHTNESS_CEILING, r, g, b);

        strip.setPixelColor(ch.ledStart + i, strip.Color(r, g, b));
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n=== Haunted Footsteps Bootup ===");

    // ===== BUTTON GPIO =====
    pinMode(PIN_BUTTON_TOE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_HEEL, INPUT_PULLUP);

    // ===== NEOPIXEL INIT (matches squad-bots: strip.begin() owns the data pin) =====
    strip.begin();
    strip.clear();
    strip.show();

    Serial.println("GPIO & NeoPixel initialized");

    // ===== WEB SERVER (also loads settings from NVS) =====
    setupWebServer();
    Serial.println("Web server initialized & settings loaded");

    Serial.println("=== Bootup Complete ===\n");
}

void loop()
{
    unsigned long now = millis();
    unsigned long dtMs = (lastLedUpdate > 0) ? (now - lastLedUpdate) : 0;

    if (now - lastLedUpdate >= LED_UPDATE_INTERVAL)
    {
        if (!otaInProgress)
        {
            strip.clear();
            updateChannel(toeChannel, dtMs);
            updateChannel(heelChannel, dtMs);
            strip.show();
        }

        // Telemetry is only meaningful at the same cadence the envelope/LEDs
        // actually update — broadcasting it on every loop() spin (potentially
        // thousands of times/sec) would flood the WebSocket and heap for no
        // visual benefit.
        broadcastTelemetry();

        lastLedUpdate = now;
    }

    serviceWebServer();
}
