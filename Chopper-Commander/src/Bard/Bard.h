#ifndef BARD_H
#define BARD_H

#include <Arduino.h>

#ifdef ROLE_BARD
#include "Audio.h" // ESP32-audioI2S library
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#endif

class Bard {
public:
    void setup();
    void loop();

private:
#ifdef ROLE_BARD
    void setupAudio();
    void setupSD();
    void handleSerialCommands();
    void playSound(int id);
    
    Audio audio;
    bool sdReady = false;
    unsigned long lastStatus = 0;
#endif
};

#endif
