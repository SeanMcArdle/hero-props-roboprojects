#include "Bard.h"
#include "../config.h"

#ifdef ROLE_BARD

// --- CONFIGURATION ---
#define SD_CS_PIN 5
#define STARTUP_SOUND "/001.mp3" 

void Bard::setup() {
    Serial.println(">> BARD (Audio) Initializing...");
    
    // 1. Setup Comms with Captain (Pins defined in config.h / PlatformIO)
    Serial1.begin(115200, SERIAL_8N1, PIN_SERIAL_CMD_RX, PIN_SERIAL_CMD_TX);
    
    // 2. Setup SD Card
    setupSD();
    
    // 3. Setup Audio
    if (sdReady) {
        setupAudio();
        audio.connecttoFS(SD, STARTUP_SOUND);
    } else {
        Serial.println("! Audio skipped (No SD)");
    }
}

void Bard::setupSD() {
    Serial.println("- Initializing SD Card...");
    SPI.begin(); // Uses default VSPI pins (SCK=18, MISO=19, MOSI=23, SS=5)
    
    // NOTE: If using custom SPI pins, allow defining them. 
    // Default ESP32 VSPI is usually fine unless I2S Mic conflicts.
    // I2S Mic is using 18, 19, 21.
    // CONFLICT ALERT: VSPI uses 18 (CLK) and 19 (MISO).
    // I2S Mic uses 18 (SCK) and 19 (WS).
    // We must remap SPI or I2S.
    
    // Let's assume we stick to I2S on 14, 12, 27 for Speaker.
    // I2S Mic on 32, 33, 35 (safe alternate) or disable Mic for now.
    // PlatformIO defines Mic on 18, 19, 21. 
    // WE WILL IGNORE MIC FOR NOW TO ALLOW SD CARD TO WORK.
    
    if(!SD.begin(SD_CS_PIN)) {
        Serial.println("! SD Start Failed");
        return;
    }
    
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE) {
        Serial.println("! No SD Card attached");
        return;
    }
    
    Serial.printf("+ SD Card Mounted: %.2f GB\n", SD.cardSize() / (1024.0 * 1024.0 * 1024.0));
    sdReady = true;
}

void Bard::setupAudio() {
    // I2S Connections (Defined in PlatformIO build flags)
    // I2S_SPEAKER_SERIAL_CLOCK (BCLK)
    // I2S_SPEAKER_LEFT_RIGHT_CLOCK (LRC)
    // I2S_SPEAKER_SERIAL_DATA (DOUT)
    
    audio.setPinout(I2S_SPEAKER_SERIAL_CLOCK, I2S_SPEAKER_LEFT_RIGHT_CLOCK, I2S_SPEAKER_SERIAL_DATA);
    audio.setVolume(21); // 0-21 range usually
    Serial.println("+ Audio initialized");
}

void Bard::loop() {
    // 1. Keep Audio Buffer Full
    audio.loop();

    // 2. Check Serial Commands
    handleSerialCommands();
    
    // 3. Debug Status
    if (millis() - lastStatus > 5000) {
        if (audio.isRunning()) {
            Serial.printf("Playing: %s (%d/%d)\n", audio.getAudioFileDuration(), audio.getAudioCurrentTime(), audio.getAudioFileDuration());
        }
        lastStatus = millis();
    }
}

void Bard::handleSerialCommands() {
    // Check Serial1 (From Captain)
    while (Serial1.available()) {
        String cmd = Serial1.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
            Serial.print("RX: "); Serial.println(cmd);
            
            if (cmd.startsWith("PLAY:")) {
                int id = cmd.substring(5).toInt();
                playSound(id);
            } else if (cmd == "STOP") {
                audio.stopSong();
            } else if (cmd.startsWith("VOL:")) {
                int vol = cmd.substring(4).toInt();
                audio.setVolume(vol);
            }
        }
    }
    
    // Check USB Serial (Debug)
    while (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
            if (cmd.startsWith("PLAY:")) {
                int id = cmd.substring(5).toInt();
                playSound(id);
            }
        }
    }
}

void Bard::playSound(int id) {
    if (!sdReady) return;
    
    // Format: /001.mp3, /042.mp3
    char filename[10];
    sprintf(filename, "/%03d.mp3", id);
    
    Serial.printf("Loading %s...\n", filename);
    if(SD.exists(filename)) {
        audio.connecttoFS(SD, filename);
    } else {
        Serial.println("! File not found");
        // Try WAV?
        sprintf(filename, "/%03d.wav", id);
        if(SD.exists(filename)) {
            audio.connecttoFS(SD, filename);
        }
    }
}

// --- AUDIO EVENTS (Optional) ---
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     "); Serial.println(info);
}

#endif
