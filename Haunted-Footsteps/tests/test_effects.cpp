// Native, hardware-free test harness for src/effects.h.
// Build & run directly with g++ (no PlatformIO/ESP32 toolchain needed):
//   g++ -std=c++17 -I../src -o /tmp/test_effects test_effects.cpp && /tmp/test_effects
//
// This exercises the exact same envelope/fade/brightness math that runs on
// the ESP32, but on the desktop, so the core effect logic can be verified
// before ever touching hardware.

#include "../src/effects.h"
#include <cstdio>
#include <cmath>

static int failures = 0;

#define CHECK(cond, msg)                                   \
    do                                                     \
    {                                                      \
        if (!(cond))                                       \
        {                                                  \
            printf("FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                    \
        }                                                  \
        else                                               \
        {                                                  \
            printf("PASS: %s\n", msg);                     \
        }                                                  \
    } while (0)

int main()
{
    // ---- advanceEnvelope: fade-in ramps 0 -> 1 over exactly fadeInMs ----
    {
        float env = 0.0f;
        // 600ms fade-in, step in 20ms ticks (matches LED_UPDATE_INTERVAL)
        for (int i = 0; i < 30; i++) // 30 * 20ms = 600ms
            env = advanceEnvelope(env, true, 600, 3000, 1.5f, 20);
        CHECK(std::fabs(env - 1.0f) < 0.01f, "fade-in reaches ~1.0 after fadeInMs elapsed");
    }

    // ---- advanceEnvelope: never overshoots 1.0 ----
    {
        float env = 0.0f;
        for (int i = 0; i < 200; i++) // way more than needed
            env = advanceEnvelope(env, true, 600, 3000, 1.5f, 20);
        CHECK(env <= 1.0f, "fade-in envelope never exceeds 1.0");
    }

    // ---- advanceEnvelope: fade-out is always slower than fade-in ----
    {
        // Deliberately configure fadeOutMs FASTER than fadeInMs (user/browser
        // error or malicious input) — the safety ratio must still be enforced.
        uint32_t fadeInMs = 600;
        uint32_t fadeOutMs = 100; // intentionally too fast
        float minRatio = 1.5f;

        float env = 1.0f;
        int ticks = 0;
        while (env > 0.0f && ticks < 10000)
        {
            env = advanceEnvelope(env, false, fadeInMs, fadeOutMs, minRatio, 20);
            ticks++;
        }
        float actualFadeOutMs = ticks * 20.0f;
        float minExpectedMs = fadeInMs * minRatio;
        CHECK(actualFadeOutMs >= minExpectedMs * 0.9f,
              "fade-out never faster than MIN_FADE_OUT_RATIO * fadeInMs, even with bad input");
    }

    // ---- advanceEnvelope: never goes negative ----
    {
        float env = 0.05f;
        for (int i = 0; i < 50; i++)
            env = advanceEnvelope(env, false, 600, 3000, 1.5f, 20);
        CHECK(env >= 0.0f, "fade-out envelope never goes negative");
    }

    // ---- enforceFadeSafety: clamps bad (too-fast) fadeOutMs up to the floor ----
    {
        uint32_t result = enforceFadeSafety(600, 100, 1.5f); // 100ms is way too fast
        CHECK(result == 900, "enforceFadeSafety raises fadeOutMs to fadeInMs*ratio floor");
    }

    // ---- enforceFadeSafety: leaves already-safe values untouched ----
    {
        uint32_t result = enforceFadeSafety(600, 3000, 1.5f);
        CHECK(result == 3000, "enforceFadeSafety leaves compliant fadeOutMs unchanged");
    }

    // ---- noise8: deterministic for a given seed+index+phase ----
    {
        uint8_t a = noise8(0xDEADBEEF, 5, 100);
        uint8_t b = noise8(0xDEADBEEF, 5, 100);
        CHECK(a == b, "noise8 is deterministic given identical seed/index/phase");
    }

    // ---- noise8: different seeds diverge (heel vs toe shouldn't sync) ----
    {
        bool anyDifferent = false;
        for (uint16_t i = 0; i < 26; i++)
        {
            if (noise8(0x1337C0DE, i, 0) != noise8(0x1701BEEFu, i, 0))
            {
                anyDifferent = true;
                break;
            }
        }
        CHECK(anyDifferent, "heel/toe independent noise seeds produce different sequences");
    }

    // ---- noise8: same seed, different phase, diverges (so it animates over time) ----
    {
        uint8_t atPhase0 = noise8(0xABCD1234, 3, 0);
        bool anyDifferent = false;
        for (uint32_t phase = 1; phase <= 2000; phase += 50)
        {
            if (noise8(0xABCD1234, 3, phase) != atPhase0)
            {
                anyDifferent = true;
                break;
            }
        }
        CHECK(anyDifferent, "noise8 output changes as phase advances (drives the visible shimmer)");
    }

    // ---- advanceNoisePhase: flickerSpeed=0 still advances (slow drift floor) ----
    {
        uint32_t phase = 0;
        phase = advanceNoisePhase(phase, 0, 20);
        CHECK(phase > 0, "advanceNoisePhase never fully freezes even at flickerSpeed=0");
    }

    // ---- advanceNoisePhase: higher flickerSpeed advances phase faster ----
    {
        uint32_t slowPhase = advanceNoisePhase(0, 10, 20);
        uint32_t fastPhase = advanceNoisePhase(0, 250, 20);
        CHECK(fastPhase > slowPhase, "higher flickerSpeed advances the noise phase faster per tick");
    }

    // ---- computePixelColor: envelope=0 -> fully off regardless of color ----
    {
        uint8_t r, g, b;
        computePixelColor(0.0f, 80, 128, 40, 255, 120, 128, r, g, b);
        CHECK(r == 0 && g == 0 && b == 0, "envelope=0 produces black pixel");
    }

    // ---- computePixelColor: brightness never exceeds MAX_BRIGHTNESS_CEILING scale ----
    {
        uint8_t r, g, b;
        // envelope=1.0, no flicker dip (noiseVal=128 -> flicker=1.0), full base color
        computePixelColor(1.0f, 0, 128, 255, 255, 255, 128, r, g, b);
        // ceiling=128/255 of 255 base => expect ~128
        CHECK(r <= 129 && g <= 129 && b <= 129, "brightness ceiling caps output even at full envelope+color");
    }

    // ---- computePixelColor: full envelope + full color MUST actually light up ----
    // (Regression test: a previous version divided brightnessCeiling by 255
    // *before* multiplying by the 0-1 brightness fraction, so the product
    // was always < 1.0 and truncated to 0 -- LEDs stayed black no matter what
    // buttons/envelope/telemetry reported. This asserts real, non-zero output.)
    {
        uint8_t r, g, b;
        computePixelColor(1.0f, 0, 128, 255, 255, 255, 128, r, g, b);
        CHECK(r > 0 && g > 0 && b > 0, "full envelope + full color must produce non-zero (visible) output");
        CHECK(r >= 120 && g >= 120 && b >= 120, "full envelope at ceiling=128 should be close to the ceiling, not near-zero");
    }

    // ---- computePixelColor: ceiling=255 (no cap) at full envelope reproduces base color ----
    {
        uint8_t r, g, b;
        computePixelColor(1.0f, 0, 128, 40, 255, 120, 255, r, g, b);
        CHECK(r >= 38 && g >= 250 && b >= 115, "uncapped full-envelope brightness should closely match base color");
    }

    // ---- computePixelColor: output channels never exceed 255 (type safety) ----
    {
        uint8_t r, g, b;
        computePixelColor(1.0f, 255, 255, 255, 255, 255, 255, r, g, b);
        CHECK(r <= 255 && g <= 255 && b <= 255, "output RGB channels always in valid uint8_t range");
    }

    printf("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASSED" : "SOME FAILED", failures);
    return failures == 0 ? 0 : 1;
}
