#ifndef EFFECTS_H
#define EFFECTS_H

// -------------------------------------------------------------------------
// Pure, hardware-independent effects logic.
// -------------------------------------------------------------------------
// No Arduino.h, no NeoPixel/FastLED, no GPIO calls anywhere in this file.
// This is the entire "brain" of the ghostly-flicker effect, deliberately
// isolated so it can be compiled and unit tested on a desktop machine
// (see tests/test_effects.cpp) without any ESP32 hardware attached.
// main.cpp is just a thin hardware layer that calls into these functions
// and pushes the resulting RGB values to the strip.
// -------------------------------------------------------------------------

#include <stdint.h>

// Advances an envelope (0.0-1.0) by dtMs, ramping toward 1.0 while pressed
// and back toward 0.0 while released. fadeOutMs is always treated as at
// least minFadeOutRatio * fadeInMs, regardless of what's passed in, so a
// caller can never (even by bug or bad input) make release faster than press.
inline float advanceEnvelope(float envelope, bool pressed,
                             uint32_t fadeInMs, uint32_t fadeOutMs,
                             float minFadeOutRatio, uint32_t dtMs)
{
    if (fadeInMs == 0)
        fadeInMs = 1; // guard against div-by-zero

    if (pressed)
    {
        envelope += dtMs / (float)fadeInMs;
        if (envelope > 1.0f)
            envelope = 1.0f;
    }
    else
    {
        float ratio = fadeOutMs / (float)fadeInMs;
        if (ratio < minFadeOutRatio)
            ratio = minFadeOutRatio;
        envelope -= dtMs / (float)(fadeInMs * ratio);
        if (envelope < 0.0f)
            envelope = 0.0f;
    }
    return envelope;
}

// Clamps fadeOutMs so it can never be faster than minFadeOutRatio * fadeInMs.
// Same rule as advanceEnvelope's internal ratio floor, exposed separately so
// the web UI can echo back the *actual* (possibly-clamped) value to clients.
inline uint32_t enforceFadeSafety(uint32_t fadeInMs, uint32_t fadeOutMs, float minFadeOutRatio)
{
    uint32_t minFadeOut = (uint32_t)((float)fadeInMs * minFadeOutRatio);
    return (fadeOutMs < minFadeOut) ? minFadeOut : fadeOutMs;
}

// Deterministic, dependency-free pseudo-noise, addressed by (seed, pixel
// index, time phase) rather than a mutated running state. Being a pure
// function of its inputs (no hidden state advanced once per call) means the
// *rate* at which the pattern changes is entirely controlled by how fast the
// caller advances `phase` — see advanceNoisePhase() below — instead of being
// tied to how often this function happens to get called per frame.
inline uint8_t noise8(uint32_t seed, uint16_t pixelIndex, uint32_t phase)
{
    uint32_t h = seed;
    h ^= (uint32_t)pixelIndex * 0x9E3779B1u;
    h ^= phase * 0x85EBCA77u;
    h ^= (h >> 15);
    h *= 0xC2B2AE35u;
    h ^= (h >> 13);
    return (uint8_t)(h & 0xFF);
}

// Advances a per-channel noise phase accumulator based on elapsed time and
// the user-configurable flickerSpeed (0-255). This is what makes the
// "Flicker Speed" slider actually do something: higher speed advances the
// phase faster, so noise8() samples move through their pattern faster. The
// "+1" floor keeps a slow drift alive even at flickerSpeed=0 rather than
// completely freezing the shimmer.
inline uint32_t advanceNoisePhase(uint32_t phase, uint8_t flickerSpeed, uint32_t dtMs)
{
    uint32_t stepPerMs = (uint32_t)flickerSpeed + 1u;
    return phase + stepPerMs * dtMs;
}

// Turns (envelope, per-pixel noise, flicker intensity, base color, brightness
// ceiling) into the final RGB to write to a pixel. All inputs/outputs are
// plain numeric types — no hardware calls.
inline void computePixelColor(float envelope, uint8_t flickerIntensity, uint8_t noiseVal,
                              uint8_t baseR, uint8_t baseG, uint8_t baseB,
                              uint8_t brightnessCeiling,
                              uint8_t &outR, uint8_t &outG, uint8_t &outB)
{
    float flicker = 1.0f - ((flickerIntensity / 255.0f) * ((noiseVal - 128) / 256.0f));
    if (flicker < 0.0f)
        flicker = 0.0f;
    if (flicker > 1.0f)
        flicker = 1.0f;

    float brightness = envelope * flicker;
    if (brightness < 0.0f)
        brightness = 0.0f;
    if (brightness > 1.0f)
        brightness = 1.0f;

    // brightnessCeiling is already on a 0-255 scale (e.g. 128 = "half power"
    // cap). Multiplying it directly by the 0.0-1.0 brightness fraction keeps
    // the result on that same 0-255 scale. (Previous version divided
    // brightnessCeiling by 255 *before* multiplying and truncating to
    // uint8_t, which collapsed every non-full-ceiling value to 0 -- that was
    // the "buttons work, telemetry works, but LEDs never light" bug.)
    uint8_t scaledBrightness = (uint8_t)(brightness * (float)brightnessCeiling);

    outR = (uint8_t)((baseR * scaledBrightness) / 255);
    outG = (uint8_t)((baseG * scaledBrightness) / 255);
    outB = (uint8_t)((baseB * scaledBrightness) / 255);
}

#endif
