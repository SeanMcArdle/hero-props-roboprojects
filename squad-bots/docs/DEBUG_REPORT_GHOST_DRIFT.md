# Post-Mortem: The "Ghost Drift" Bug (RC3.3)

**Date:** Jan 6, 2026  
**Subject:** Resolution of Random Servo Jitter, Drift, and "Pulsing" on ESP32 Robot  
**Status:** ✅ RESOLVED

## 1. The Symptoms
*   **The Drift:** Upon startup or after driving, the robot would exhibit random, slow rotations or "pulses" of movement without user input.
*   **The Hum:** Servos would hum aggressively when idle, indicating they were fighting to hold a position against fluctuating signals.
*   **The Trigger:** Moving one subsystem (Dome) would often cause the other subsystem (Drive Wheels) to start drifting.
*   **The Regression:** Fixes seemed to work temporarily (e.g., "It works on boot"), but the issue would return after 30-60 seconds or after a heavy drive cycle.

## 2. Root Cause Analysis
The issue was not a single bug, but a "Perfect Storm" of three interacting physical and logical flaws.

### A. The Hardware Culprit: RF Interference (EMI)
*   **Diagnosis:** "It's the WiFi."
*   **Mechanism:** The ESP32's WiFi radio, running at default power (~19.5dBm), generates significant Electro-Magnetic Interference (EMI) during TX bursts.
*   **Effect:** The servo signal wires acted as antennas, picking up this RF energy. The induction created voltage spikes on the PWM lines that the servos interpreted as valid commands (e.g., "Drive forward at 1% speed").

### B. The Logic Culprit: "Hyper-Sensitive Wakeup"
*   **Code Flaw:** The original deadzone verification was: `if (abs(input) < 0.01)`.
*   **Failure Mode:** The RF noise floor was spiking *above* 1%. The code saw this noise as a deliberate user command ("Input is 0.02! Go!"), triggering the motors to wake up (`attach()`) and try to execute the noise.

### C. The State Culprit: "Sympathetic Wakeup"
*   **Code Flaw:** A single boolean `motorsActive` controlled all servos.
*   **Failure Mode:** When the user moved the Dome, `motorsActive` became `true`. This woke up the **Drive Motors** as well. Once awake, the drive motors were immediately exposed to the RF noise mentioned in (A) and began to drift.

## 3. The Resolution (RC3.3)

We implemented a defense-in-depth strategy to kill the ghost.

### Fix 1: Reduce the Noise Source (The Big One)
We lowered the transmission power of the ESP32 WiFi radio.
```cpp
// Reduced from ~19.5dBm (100mW) to 11dBm (~12mW)
// This massive drop in output power stopped the brownouts and huge RF spikes.
WiFi.setTxPower(WIFI_POWER_11dBm);
```

### Fix 2: "Lazy Wakeup" Protocol
We increased the threshold required to wake the system from sleep. The robot now ignores the "Noise Floor".
```cpp
// Old: 0.01 (1%) -> New: 0.05 (5%)
// You must move the stick > 5% to wake the dragon.
if (abs(currentLeft) < 0.05) currentLeft = 0;
```

### Fix 3: Subsystem Decoupling
We split the sleep logic. Moving the head no longer wakes the feet.
*   **Old Logic:** `if (anyInput) enableAllMotors();`
*   **New Logic:** `if (driveInput) enableDrive();` AND `if (domeInput) enableDome();`

### Fix 4: Auto-Detach
When values fall below the 5% threshold, we physically stop sending PWM signals.
```cpp
leftMotor.detach(); // Stops the servo from "seeking" phantom signals
```

## 4. Lessons Learned / Guidelines for Future Bots
1.  **Always use `detached` state for idle servos** on ESP32. The pins are too noisy otherwise.
2.  **WiFi Power Management is critical** for robotics. You rarely need full TX power for a robot you are standing next to.
3.  **Deadbands are your friend.** 1% is too precise for hobby hardware. 5% is the safe floor.
4.  **Split your power domains.** If logically possible, don't let one system wake up another.
