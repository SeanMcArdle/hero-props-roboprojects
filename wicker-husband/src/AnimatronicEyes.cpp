// -------------------------------------------------------------------------
// 👁️ AnimatronicEyes.cpp
//
// Non-blocking ESP32 adaptation of mklements/AIChatbot EyeMovement.py
//
// Key translation table:
//   Python set_servo_angle()        → _writeServo()
//   Python move_servos_together()   → _moving flag + _stepMove() called from update()
//   Python blink_eyes()             → _blinkPhase state machine in _stepBlink()
//   Python main loop random moves   → _idleEnabled + _pickRandomGaze()
//   Python next_blink_time          → _nextAutoBlinkMs
// -------------------------------------------------------------------------

#include "AnimatronicEyes.h"
#include "config.h"

// -------------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------------
AnimatronicEyes::AnimatronicEyes() {
    // Servo direction multipliers — mirrors DIR_* in EyeMovement.py
    _dir[CH_LEFT_X]      = DIR_LEFT_X;
    _dir[CH_LEFT_Y]      = DIR_LEFT_Y;
    _dir[CH_LEFT_BLINK]  = DIR_LEFT_BLINK;
    _dir[CH_RIGHT_X]     = DIR_RIGHT_X;
    _dir[CH_RIGHT_Y]     = DIR_RIGHT_Y;
    _dir[CH_RIGHT_BLINK] = DIR_RIGHT_BLINK;
}

// -------------------------------------------------------------------------
// begin() — mirrors the INITIALIZATION block in EyeMovement.py
// -------------------------------------------------------------------------
void AnimatronicEyes::begin() {
    // Allocate PWM timers before attaching servos (ESP32Servo requirement)
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    // Attach servos to GPIO pins
    const int pins[NUM_SERVOS] = {
        PIN_LEFT_X, PIN_LEFT_Y, PIN_LEFT_BLINK,
        PIN_RIGHT_X, PIN_RIGHT_Y, PIN_RIGHT_BLINK
    };
    for (int i = 0; i < NUM_SERVOS; i++) {
        _servos[i].setPeriodHertz(50);           // 50 Hz servo standard
        _servos[i].attach(pins[i], 500, 2500);   // 500–2500 µs pulse range
    }

    // Initial positions — mirrors Python initialization block:
    // neutral_x = (X_LIMITS[0] + X_LIMITS[1]) // 2  → 90°
    // neutral_y = (Y_LIMITS[0] + Y_LIMITS[1]) // 2  → 90°
    // blink open = BLINK_OPEN_LEFT / BLINK_OPEN_RIGHT + trim
    int neutralX = (EYE_X_MIN + EYE_X_MAX) / 2;
    int neutralY = (EYE_Y_MIN + EYE_Y_MAX) / 2;

    _current[CH_LEFT_X]      = neutralX;
    _current[CH_LEFT_Y]      = neutralY;
    _current[CH_LEFT_BLINK]  = _blinkOpenLeft();
    _current[CH_RIGHT_X]     = neutralX;
    _current[CH_RIGHT_Y]     = neutralY;
    _current[CH_RIGHT_BLINK] = _blinkOpenRight();

    for (int i = 0; i < NUM_SERVOS; i++) {
        _target[i] = _current[i];
        _writeServo(i, _current[i]);
    }

    // Schedule the first auto-blink
    _scheduleNextBlink();

    Serial.println("👁️  AnimatronicEyes ready");
}

// -------------------------------------------------------------------------
// update() — call every loop()
// -------------------------------------------------------------------------
void AnimatronicEyes::update() {
    unsigned long now = millis();

    // 1. Advance gaze movement (non-blocking step)
    if (_moving && (now - _lastMoveStep >= MOVE_DELAY_MS)) {
        _stepMove();
        _lastMoveStep = now;
    }

    // 2. Advance blink state machine
    if (_blinkPhase > 0) {
        _stepBlink();
        return; // Prioritise blink over idle scheduling
    }

    // 3. Auto-blink trigger
    if (now >= _nextAutoBlinkMs) {
        triggerBlink();
        return;
    }

    // 4. Idle random gaze — mirrors Python main loop:
    //    "Pick a new random target position" + time.sleep(random 0.5–2.0)
    if (_idleEnabled && !_moving && (now >= _idlePauseUntil)) {
        _pickRandomGaze();
    }
}

// -------------------------------------------------------------------------
// lookAt() — stop idle, drive both eyes to an explicit position
// -------------------------------------------------------------------------
void AnimatronicEyes::lookAt(int x, int y) {
    x = constrain(x, EYE_X_MIN, EYE_X_MAX);
    y = constrain(y, EYE_Y_MIN, EYE_Y_MAX);

    _target[CH_LEFT_X]  = x;
    _target[CH_LEFT_Y]  = y;
    _target[CH_RIGHT_X] = x;
    _target[CH_RIGHT_Y] = y;
    _moving = true;
    _lastMoveStep = millis();
}

// -------------------------------------------------------------------------
// triggerBlink() — mirrors blink_eyes() in EyeMovement.py
// Starts the blink state machine (phase 1 = closing)
// -------------------------------------------------------------------------
void AnimatronicEyes::triggerBlink() {
    if (_blinkPhase != 0) return; // Already blinking

    // Max steps needed = max(left_range, right_range)
    int leftRange  = BLINK_CLOSED - _blinkOpenLeft();
    int rightRange = BLINK_CLOSED - _blinkOpenRight();
    _blinkMaxSteps = max(leftRange, rightRange);

    if (_blinkMaxSteps <= 0) {
        _scheduleNextBlink();
        return;
    }

    _blinkStep  = 0;
    _blinkPhase = 1; // Closing
    _blinkTimer = millis();
}

// -------------------------------------------------------------------------
// setExpression() — action presets
// -------------------------------------------------------------------------
void AnimatronicEyes::setExpression(EyeExpression expr) {
    switch (expr) {
        case EXPR_IDLE:
            resumeIdle();
            break;

        case EXPR_BLINK:
            triggerBlink();
            break;

        case EXPR_ALERT: {
            // Wide-open eyes, rapid look to a random far corner
            _idleEnabled = false;
            _alertMode   = true;
            // Snap eyelids fully open
            _current[CH_LEFT_BLINK]  = _blinkOpenLeft();
            _current[CH_RIGHT_BLINK] = _blinkOpenRight();
            _writeServo(CH_LEFT_BLINK,  _current[CH_LEFT_BLINK]);
            _writeServo(CH_RIGHT_BLINK, _current[CH_RIGHT_BLINK]);
            // Look toward a random corner quickly
            int alertX = (random(2) == 0) ? EYE_X_MIN : EYE_X_MAX;
            int alertY = (random(2) == 0) ? EYE_Y_MIN : EYE_Y_MAX;
            lookAt(alertX, alertY);
            break;
        }

        case EXPR_SLEEPY:
            // Half-close eyelids
            _idleEnabled = false;
            _target[CH_LEFT_BLINK]  = _blinkOpenLeft()  + (BLINK_CLOSED - _blinkOpenLeft())  / 2;
            _target[CH_RIGHT_BLINK] = _blinkOpenRight() + (BLINK_CLOSED - _blinkOpenRight()) / 2;
            _moving = true;
            _lastMoveStep = millis();
            break;

        case EXPR_CENTER:
            centerAndStop();
            break;
    }
}

// -------------------------------------------------------------------------
// resumeIdle()
// -------------------------------------------------------------------------
void AnimatronicEyes::resumeIdle() {
    _idleEnabled = true;
    _alertMode   = false;
    _idlePauseUntil = millis(); // Start immediately
}

// -------------------------------------------------------------------------
// centerAndStop() — used for E-STOP; mirrors Python pca.deinit() safety
// -------------------------------------------------------------------------
void AnimatronicEyes::centerAndStop() {
    _idleEnabled = false;
    _alertMode   = false;
    _blinkPhase  = 0;
    _moving      = false;

    int neutralX = (EYE_X_MIN + EYE_X_MAX) / 2;
    int neutralY = (EYE_Y_MIN + EYE_Y_MAX) / 2;

    _current[CH_LEFT_X]      = neutralX;
    _current[CH_LEFT_Y]      = neutralY;
    _current[CH_LEFT_BLINK]  = _blinkOpenLeft();
    _current[CH_RIGHT_X]     = neutralX;
    _current[CH_RIGHT_Y]     = neutralY;
    _current[CH_RIGHT_BLINK] = _blinkOpenRight();

    for (int i = 0; i < NUM_SERVOS; i++) {
        _target[i] = _current[i];
        _writeServo(i, _current[i]);
    }
}

// -------------------------------------------------------------------------
// isBusy()
// -------------------------------------------------------------------------
bool AnimatronicEyes::isBusy() const {
    return _moving || (_blinkPhase != 0);
}

// =========================================================================
// Private helpers
// =========================================================================

// -------------------------------------------------------------------------
// _writeServo() — mirrors set_servo_angle() in EyeMovement.py
//
// Python applies direction inversion as: if direction == -1: angle = 180 - angle
// We replicate exactly the same logic here.
// -------------------------------------------------------------------------
void AnimatronicEyes::_writeServo(int ch, int angle) {
    int a = (_dir[ch] == -1) ? (180 - angle) : angle;
    a = constrain(a, 0, 180);
    _servos[ch].write(a);
}

// -------------------------------------------------------------------------
// _stepMove() — one non-blocking step of move_servos_together()
//
// Python computes t = step / max_steps and lerps each servo.
// We advance by MOVE_STEP_DEG per call (called every MOVE_DELAY_MS).
// Only gaze channels (X and Y) are stepped here; eyelids are handled
// separately by the blink state machine.
// -------------------------------------------------------------------------
void AnimatronicEyes::_stepMove() {
    bool anyMoving = false;

    for (int ch = 0; ch < NUM_SERVOS; ch++) {
        if (_current[ch] == _target[ch]) continue;

        int diff = _target[ch] - _current[ch];
        int step = constrain(diff, -MOVE_STEP_DEG, MOVE_STEP_DEG);
        _current[ch] += step;
        _writeServo(ch, _current[ch]);

        if (_current[ch] != _target[ch]) anyMoving = true;
    }

    if (!anyMoving) {
        _moving = false;
        // If in alert mode after a rapid move, pick another random corner shortly
        if (_alertMode) {
            _idlePauseUntil = millis() + random(300, 800);
            _idleEnabled    = true;
        } else {
            // Pause before picking next idle position
            // (mirrors time.sleep(random.uniform(0.5, 2.0)) in Python)
            _idlePauseUntil = millis() + random(IDLE_PAUSE_MIN_MS, IDLE_PAUSE_MAX_MS);
        }
    }
}

// -------------------------------------------------------------------------
// _stepBlink() — advances the blink state machine
//
// Mirrors the three phases of blink_eyes() in EyeMovement.py:
//   Phase 1 = closing  (step 0 → _blinkMaxSteps)
//   Phase 2 = hold     (BLINK_HOLD_MS)
//   Phase 3 = opening  (step _blinkMaxSteps → 0)
//
// BLINK_SIDE_STEPS replicates the BLINK_SIDE_DELAY stagger from Python:
//   right eyelid lags behind left by BLINK_SIDE_STEPS steps.
//
// Two separate timers are used:
//   _blinkTimer     — step-rate limiter for phases 1 & 3 (BLINK_STEP_MS)
//   _blinkHoldStart — duration timer for phase 2 (BLINK_HOLD_MS)
// -------------------------------------------------------------------------
void AnimatronicEyes::_stepBlink() {
    unsigned long now = millis();

    int leftOpen  = _blinkOpenLeft();
    int rightOpen = _blinkOpenRight();
    int leftRange  = BLINK_CLOSED - leftOpen;
    int rightRange = BLINK_CLOSED - rightOpen;

    // Phase 2 (hold) is handled outside the step-rate limiter
    if (_blinkPhase == 2) {
        if (now - _blinkHoldStart >= (unsigned long)BLINK_HOLD_MS) {
            _blinkStep  = _blinkMaxSteps;
            _blinkPhase = 3; // Transition to opening
            _blinkTimer = now;
        }
        return;
    }

    // Rate-limit steps for phases 1 and 3
    if (now - _blinkTimer < (unsigned long)BLINK_STEP_MS) return;
    _blinkTimer = now;

    if (_blinkPhase == 1) {
        // --- Closing phase ---
        // Left: progress = min(step, leftRange) / leftRange
        float leftProg  = (float)min(_blinkStep, leftRange)  / max(leftRange,  1);
        int leftAngle   = leftOpen  + (int)(leftProg  * leftRange);

        // Right lags by BLINK_SIDE_STEPS steps
        int rightStepIdx = max(0, _blinkStep - BLINK_SIDE_STEPS);
        float rightProg  = (float)min(rightStepIdx, rightRange) / max(rightRange, 1);
        int rightAngle   = rightOpen + (int)(rightProg * rightRange);

        _current[CH_LEFT_BLINK]  = leftAngle;
        _current[CH_RIGHT_BLINK] = rightAngle;
        _writeServo(CH_LEFT_BLINK,  leftAngle);
        _writeServo(CH_RIGHT_BLINK, rightAngle);

        _blinkStep++;
        if (_blinkStep > _blinkMaxSteps) {
            _blinkPhase     = 2; // Transition to hold
            _blinkHoldStart = now;
        }

    } else if (_blinkPhase == 3) {
        // --- Opening phase (mirror of closing) ---
        float leftProg  = (float)min(_blinkStep, leftRange)  / max(leftRange,  1);
        int leftAngle   = leftOpen  + (int)(leftProg  * leftRange);

        int rightStepIdx = max(0, _blinkStep - BLINK_SIDE_STEPS);
        float rightProg  = (float)min(rightStepIdx, rightRange) / max(rightRange, 1);
        int rightAngle   = rightOpen + (int)(rightProg * rightRange);

        _current[CH_LEFT_BLINK]  = leftAngle;
        _current[CH_RIGHT_BLINK] = rightAngle;
        _writeServo(CH_LEFT_BLINK,  leftAngle);
        _writeServo(CH_RIGHT_BLINK, rightAngle);

        _blinkStep--;
        if (_blinkStep < 0) {
            // Blink complete — snap fully open and return to idle
            _current[CH_LEFT_BLINK]  = leftOpen;
            _current[CH_RIGHT_BLINK] = rightOpen;
            _writeServo(CH_LEFT_BLINK,  leftOpen);
            _writeServo(CH_RIGHT_BLINK, rightOpen);
            _blinkPhase = 0;
            _scheduleNextBlink();
        }
    }
}

// -------------------------------------------------------------------------
// _scheduleNextBlink() — mirrors next_blink_time assignment in Python
// -------------------------------------------------------------------------
void AnimatronicEyes::_scheduleNextBlink() {
    _nextAutoBlinkMs = millis() + random(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);
}

// -------------------------------------------------------------------------
// _pickRandomGaze() — mirrors random_eye_position() + move call in Python
// -------------------------------------------------------------------------
void AnimatronicEyes::_pickRandomGaze() {
    int newX = random(EYE_X_MIN, EYE_X_MAX + 1);
    int newY = random(EYE_Y_MIN, EYE_Y_MAX + 1);
    lookAt(newX, newY);
}

// -------------------------------------------------------------------------
// Eyelid open angles with per-eye trim
// (mirrors left_blink_open = BLINK_OPEN_LEFT in Python)
// -------------------------------------------------------------------------
int AnimatronicEyes::_blinkOpenLeft() const {
    return BLINK_OPEN + BLINK_TRIM_LEFT;
}

int AnimatronicEyes::_blinkOpenRight() const {
    return BLINK_OPEN + BLINK_TRIM_RIGHT;
}
