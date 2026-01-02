# Evaluation of hero-props-roboprojects Repository

## Repository Overview

- **Repo:** [SeanMcArdle/hero-props-roboprojects](https://github.com/SeanMcArdle/hero-props-roboprojects)
- **Description:** STEAM robotics education projects by Hero Props Inc.
- **Languages:** Mostly C++ (82.3%), some Shell scripts.
- **Purpose & Style:** Projects are designed for hands-on youth robotics education. The code and docs are structured with workshop users (kids, educators) in mind—clear instructions, modular organization, and thoughtful attention to setup, kit assembly, and workshop workflow.

Your documentation and code style reflect:
- Good educational pedagogy
- Clear project breakdown (each workshop in separate subfolders)
- Emphasis on reproducibility and easy customization for participants
- Attribution to upstream inspirations and open sharing ethos

---

## Bakken Workshop and BB-R2 Review

### 1. Repo Structure & BB-R2 Content

- There’s a top-level `bakken-workshop/` containing:
  - `README.md` (detailed workshop and troubleshooting info)
  - `BB-R2-Workshop/BB-R2-Workshop.ino` (main Arduino controller code)
  - `generate-workshop-inos.sh` (automation for generating customized .ino sketches for the fleet of droids)

#### Key Features:
- Each robot runs its own WiFi AP for direct device control (no shared infrastructure needed—a huge plus).
- Controller sketch supports virtual joystick/web UI, OTA safety via watchdog, smoothing for movement, and per-unit customization.

#### Example: `bakken-workshop/README.md`

This doc is a gold standard for transparency and workshop prep:
- **What’s different:** Explains AP-mode, not infrastructure mode, and why that matters in a museum setting.
- **The Problem:** Clearly details the “servo drift with AP” hardware bug and docs the troubleshooting journey. Kids would see real-world engineering—great pedagogical value!
- **Technical Record:** Tracks steps, settings, dead ends, and pinouts—perfect for educational reflection and reproducibility.

[View bakken-workshop/README.md](https://github.com/SeanMcArdle/hero-props-roboprojects/blob/main/bakken-workshop/README.md)

#### Example: `BB-R2-Workshop.ino`

- Cites inspiration, respects licenses, and explains hardware setup.
- Encodes per-droid identity and credentials.
- Joystick and dome controls via browser; mDNS allows friendly names.
- Watchdog as a safety feature for workshops.
- Hardware: ESP32 with FS90R servos on assigned pins (well documented).
- HTML for the web UI is embedded for portability.

[View BB-R2-Workshop.ino](https://github.com/SeanMcArdle/hero-props-roboprojects/blob/main/bakken-workshop/BB-R2-Workshop/BB-R2-Workshop.ino)

#### Workshop Automation Script: `generate-workshop-inos.sh`

- Bash script generates N per-droid folders & sketches with unique names/SSIDs/passwords.
- Kids get ownership and can personalize their .ino before uploading.
- Good error handling and dependency checks.
- Encourages “Your Name Here” customization for student engagement.

[View generate-workshop-inos.sh](https://github.com/SeanMcArdle/hero-props-roboprojects/blob/main/bakken-workshop/generate-workshop-inos.sh)

---

## Summary & Suggestions

### Strengths:
- **Superb documentation:** Especially around real workshop hardware quirks (servo drift case study!)
- **Customizability:** Kids empowered to edit code and personalize their droids.
- **Safety features:** Watchdog for lost connections.
- **Automation:** Bash script smooths multi-device setup for group events.

### Weaknesses/Next Steps:
- No evidence of unit tests (but this is normal for Arduino/edu workshops).
- Servo drift bug is unresolved; consider posting the issue to ESP32 or Arduino forums with your detailed findings—someone may have a fix.
- For further reproducibility, you could export a BOM (bill of materials), and possibly provide simple wiring diagrams or Fritzing.
- Consider explicit license files if you intend broad reuse.

---

## Direct Links to Core Files

- [Bakken Workshop README (full)](https://github.com/SeanMcArdle/hero-props-roboprojects/blob/main/bakken-workshop/README.md)
- [BB-R2-Workshop.ino (controller code)](https://github.com/SeanMcArdle/hero-props-roboprojects/blob/main/bakken-workshop/BB-R2-Workshop/BB-R2-Workshop.ino)
- [Generator Script](https://github.com/SeanMcArdle/hero-props-roboprojects/blob/main/bakken-workshop/generate-workshop-inos.sh)

[See more results and file contents via GitHub web code search.](https://github.com/SeanMcArdle/hero-props-roboprojects/search)

If you want a deeper code review, specifics about the servo drift hardware issue, or guidance on classroom facilitation, just ask!
