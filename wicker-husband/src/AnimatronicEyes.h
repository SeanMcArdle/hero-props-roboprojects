#ifndef ANIMATRONIC_EYES_H
#define ANIMATRONIC_EYES_H

// -------------------------------------------------------------------------
// 👁️ AnimatronicEyes — ESP32 adaptation of mklements/AIChatbot EyeMovement.py
//
// Original: Raspberry Pi + PCA9685 I2C servo driver, blocking time.sleep() loops
// Adapted:  ESP32 direct-GPIO PWM servos, non-blocking millis() state machine
// -------------------------------------------------------------------------

#include <Arduino.h>
#include <ESP32Servo.h>

// Servo channel indices (mirrors Python channel constants)
#define CH_LEFT_X     0
#define CH_LEFT_Y     1
#define CH_LEFT_BLINK 2
#define CH_RIGHT_X    3
#define CH_RIGHT_Y    4
#define CH_RIGHT_BLINK 5
#define NUM_SERVOS    6

// Expression presets (for HP_MSG_ACTION actionId)
enum EyeExpression : uint8_t {
    EXPR_IDLE    = 0,  // Random wandering gaze (default)
    EXPR_BLINK   = 1,  // Single blink, then return to idle
    EXPR_ALERT   = 2,  // Wide-open eyes, rapid look-around
    EXPR_SLEEPY  = 3,  // Half-closed eyelids
    EXPR_CENTER  = 4   // Lock to center (used with E-STOP)
};

class AnimatronicEyes {
public:
    AnimatronicEyes();

    // Call once from setup()
    void begin();

    // Call every loop() — drives the non-blocking state machine
    void update();

    // -----------------------------------------------------------------------
    // Control API
    // -----------------------------------------------------------------------

    // Set an explicit gaze target (x: EYE_X_MIN..EYE_X_MAX,
    //                               y: EYE_Y_MIN..EYE_Y_MAX)
    // Stops idle auto-wander until resumeIdle() is called.
    void lookAt(int x, int y);

    // Trigger a single blink immediately
    void triggerBlink();

    // Apply a named expression preset
    void setExpression(EyeExpression expr);

    // Resume random idle gaze wandering
    void resumeIdle();

    // Center both eyes and fully open eyelids (for E-STOP)
    void centerAndStop();

    // True while a movement or blink animation is in progress
    bool isBusy() const;

private:
    // -----------------------------------------------------------------------
    // Servo objects (6 total, indexed by CH_* constants above)
    // -----------------------------------------------------------------------
    Servo _servos[NUM_SERVOS];

    // -----------------------------------------------------------------------
    // Per-servo state
    // -----------------------------------------------------------------------
    int _current[NUM_SERVOS];  // Current angle (degrees)
    int _target[NUM_SERVOS];   // Target angle (degrees)
    int _dir[NUM_SERVOS];      // Direction multiplier (+1 or -1)

    // -----------------------------------------------------------------------
    // Gaze movement state (mirrors move_servos_together() in Python)
    // -----------------------------------------------------------------------
    unsigned long _lastMoveStep = 0;
    bool _moving = false;

    // -----------------------------------------------------------------------
    // Blink state machine (mirrors blink_eyes() in Python)
    // Phase: 0=idle, 1=closing, 2=hold, 3=opening
    // -----------------------------------------------------------------------
    int  _blinkPhase    = 0;
    int  _blinkStep     = 0;   // Current step within phase
    int  _blinkMaxSteps = 0;   // Steps needed for full close/open
    unsigned long _blinkTimer     = 0;  // Step-rate limiter (phases 1 & 3)
    unsigned long _blinkHoldStart = 0;  // When phase 2 (hold) began
    unsigned long _nextAutoBlinkMs = 0;  // millis() for next scheduled blink

    // -----------------------------------------------------------------------
    // Idle auto-wander state (mirrors main loop in Python)
    // -----------------------------------------------------------------------
    bool _idleEnabled   = true;
    bool _alertMode     = false;
    unsigned long _idlePauseUntil = 0;

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    // Write a single servo with direction correction applied
    // (mirrors set_servo_angle() in Python)
    void _writeServo(int ch, int angle);

    // Step all gaze servos one increment toward their targets
    // (mirrors one iteration of the step loop in move_servos_together())
    void _stepMove();

    // Advance the blink state machine one tick
    // (mirrors blink_eyes() phases in Python)
    void _stepBlink();

    // Schedule the next auto-blink
    void _scheduleNextBlink();

    // Pick and set a new random gaze target
    // (mirrors random_eye_position() + move call in Python main loop)
    void _pickRandomGaze();

    // Compute the eyelid open angle for each eye (includes trim)
    int _blinkOpenLeft() const;
    int _blinkOpenRight() const;
};

#endif // ANIMATRONIC_EYES_H
