#ifdef TEST_PIN_MODE
#include <Arduino.h>

// Quick pin-sanity tester: toggles each pin in sequence and prints to Serial.
// Adjust `testPins` if you want a different set.
const int testPins[] = {15, 2, 4, 16, 17}; // typical DevKit adjacent row

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== PIN TEST MODE ===");
    for (size_t i = 0; i < sizeof(testPins) / sizeof(testPins[0]); ++i)
    {
        pinMode(testPins[i], OUTPUT);
        digitalWrite(testPins[i], LOW);
    }
}

unsigned long lastToggle = 0;
size_t idx = 0;

void loop()
{
    unsigned long now = millis();
    if (now - lastToggle < 700)
        return; // 700ms per step
    lastToggle = now;

    // Turn all pins low, then set the current one high
    for (size_t i = 0; i < sizeof(testPins) / sizeof(testPins[0]); ++i)
    {
        digitalWrite(testPins[i], (i == idx) ? HIGH : LOW);
    }
    Serial.printf("PIN TEST: toggled pin %d (idx=%u)\n", testPins[idx], (unsigned)idx);
    idx = (idx + 1) % (sizeof(testPins) / sizeof(testPins[0]));
}

#endif
