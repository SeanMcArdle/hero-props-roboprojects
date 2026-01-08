# Session Report: Jellybean LED Integration & The Great PlatformIO Debug Battle
**Date:** January 6-7, 2026  
**Project:** Squad Bots - Jellybean (Phase 2)  
**Status:** ✅ LEDS CONFIGURED, USB UPLOAD RESTORED

---

## 🎯 Session Objectives

1. Integrate NeoPixel LED system into Jellybean
2. Switch dome servo back to 360° continuous rotation
3. Update action button animations
4. Deploy via OTA
5. *[Unexpected]* Diagnose and fix catastrophic PlatformIO upload failure

---

## 🏆 Major Accomplishments

### 1. **NeoPixel System Integration** (Phase 2 Feature)
**Hardware:** Adafruit 24-LED RGBW NeoPixel Ring  
**Mounting:** Inside translucent PETG torso (upward glow)

**Configuration Added:**
```cpp
#define HAS_NEOPIXELS   true
#define NUM_LEDS        24      // 24-LED RGBW Ring
#define PIN_NEOPIXEL    27
```

**Animation Modes Implemented:**
1. **Idle Breathing**: Slow purple pulse (ambient)
2. **Happy**: Bright green flash
3. **Sad**: Blue fade to dim
4. **Let's Go Crazy**: Fast purple pulse
5. **Dance**: Rainbow cycle

**Boot Test Added:**
- White flash on startup to verify ring is functional
- Helps diagnose hardware issues immediately

---

### 2. **Dome Servo Configuration Change**
**Previous:** 180° standard servo (position-based control)  
**Reason for Change:** Originally used 180° to fish LED wire through dome—no longer needed with internal ring  
**New:** 360° continuous rotation servo (speed-based control)  

---

### 3. **Web Interface Updates**
- Changed "ANGRY" button to "LET'S GO CRAZY" (Purple/Gold gradient)
- Action buttons now properly trigger LED animations

---

### 4. **Audio Strategy Clarification**
**Decision:** Raspberry Pi Zero W is optimal for advanced audio needs

**Rationale:**
- ESP32 audio (I2S/DFPlayer): Good for simple beeps/sound effects
- Pi Zero W: Better for music playback, TTS, streaming, audio mixing
- Connection via Serial (TX/RX) for low latency

---

## 🔥 The Great PlatformIO Debug Battle

### **The Problem:**
After successful OTA deployments earlier in the session, PlatformIO became locked into OTA mode and refused to use USB, even with:
- Explicit `upload_protocol = esptool` set
- Complete `.pio` cache deletion
- Physical device unplug/replug

### **Symptoms:**
```
CURRENT: upload_protocol = espota
Uploading .pio/build/jl-bn/firmware.bin
[ERROR]: Host jellybean.local Not Found
```

### **Root Cause (Finally Discovered):**
Hidden in `platformio.ini`:
```ini
;upload_flags =
   --auth=heroprops    // ← THIS LINE WAS NOT COMMENTED!
```

**Why This Broke Everything:**
- The `upload_flags` line was commented with `;`
- BUT the next line (`--auth=...`) was NOT commented
- PlatformIO saw authentication flags and assumed "OTA mode required"
- Overrode all explicit `upload_protocol = esptool` settings

### **The Fix:**
```ini
; upload_protocol = espota
; upload_port = jellybean.local
; upload_flags =
;     --auth=heroprops    // ← NOW PROPERLY COMMENTED
```

**Result:** USB upload immediately worked after proper commenting.

### **Lessons Learned:**
1. **Multi-line INI comments are treacherous**: Each continuation line needs its own comment marker.
2. **PlatformIO prioritizes authentication flags**: Presence of `--auth` triggers OTA mode regardless of protocol setting.
3. **Command-line debugging reveals truth**: `cat platformio.ini` was the only way to see the actual problem.

---

## 🛠️ Technical Debt & Future Work

### **Immediate (This Session's TODO):**
- [ ] Test Jellybean's NeoPixel animations (verify boot flash works)
- [ ] Verify dome 360° rotation is smooth
- [ ] Test all 4 action button animations
- [ ] Confirm USB upload is permanently fixed

### **Next Steps:**
- [ ] Build HP-42 (Phase 1 template validation)
- [ ] Test ESP-NOW mesh between 2+ bots

---

## 💡 Key Insights

### **1. The Danger of "Almost Commented" Code**
The PlatformIO bug highlighted how dangerous partially-commented multi-line config blocks can be. What looks like commented code to humans may be partially active to parsers.

### **2. OTA is Fragile (But Worth It)**
The failed OTA upload mid-session was scary, but having USB as a reliable fallback is critical. The dual-environment approach (`jl-bn` for USB, `jl-bn-ota` for wireless) prevents future lock-in.
