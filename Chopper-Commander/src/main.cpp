#include <Arduino.h>
#include "config.h"

// Conditional Imports based on PlatformIO Environment
#if defined(ROLE_CAPTAIN)
    #include "Captain/Captain.h"
    Captain robot;
#elif defined(ROLE_BARD)
    #include "Bard/Bard.h"
    Bard robot;
#elif defined(ROLE_LOOKOUT)
    #include "Lookout/Lookout.h"
    Lookout robot;
#elif defined(ROLE_EYE)
    #include "Eye/Eye.h"
    Eye robot;
#endif

void setup() {
    Serial.begin(115200);
    // Give serial monitor time to catch up
    delay(1000);
    
    #if defined(ROLE_CAPTAIN)
        Serial.println("BOOTING ROLE: CAPTAIN");
    #elif defined(ROLE_BARD)
        Serial.println("BOOTING ROLE: BARD");
    #elif defined(ROLE_LOOKOUT)
        Serial.println("BOOTING ROLE: LOOKOUT");
    #elif defined(ROLE_EYE)
        Serial.println("BOOTING ROLE: EYE");
    #else
        #error "NO ROLE DEFINED! Check platformio.ini envs"
    #endif

    robot.setup();
}

void loop() {
    robot.loop();
}
