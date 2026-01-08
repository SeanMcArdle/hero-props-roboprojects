#ifndef CHOPPER_CONFIG_H
#define CHOPPER_CONFIG_H

#include <Arduino.h>

// ==========================================
// SYSTEM WIDE CONFIG
// ==========================================
#define WIFI_AP_SSID        "CHOPPER_NET"
#define WIFI_AP_PASS        "droid1234"
#define BROADCAST_CHANNEL   1

// ==========================================
// PIN DEFINITIONS (ADJUST AS NEEDED)
// ==========================================

// --- ROLE: CAPTAIN (Motor Controller) ---
#ifdef ROLE_CAPTAIN
    #define PIN_MOTOR_L_PWM     25
    #define PIN_MOTOR_R_PWM     26
    #define PIN_DOME_SPIN       27
    // UART to Bard
    #define PIN_SERIAL_BARD_TX  17
    #define PIN_SERIAL_BARD_RX  16
    // Inputs (Example)
    #define PIN_HALL_SENSOR     34 
#endif

// --- ROLE: BARD (Audio) ---
#ifdef ROLE_BARD
    // I2S Pins are defined in platformio.ini to allow flexibility
    // UART from Captain
    #define PIN_SERIAL_CMD_RX   16
    #define PIN_SERIAL_CMD_TX   17 
    #define PIN_SD_CS           5 
#endif

// --- ROLE: LOOKOUT (Dome) ---
#ifdef ROLE_LOOKOUT
    #define PIN_NEOPIXEL        13
    #define NUM_NEOPIXELS       3
    #define PIN_SERVO_ARM_L     25
    #define PIN_SERVO_ARM_R     26
    #define PIN_SERVO_TILT      27
#endif

#endif
