# Chopper Commander — Hero Props Modular Robotics Camp

---

## Welcome

**Chopper Commander** is the next-generation robotics camp and teaching platform from Hero Props and collaborators.  
This subproject is dedicated to the full-week "Chopper Commander" camp curriculum:  
- Featuring a multi-ESP32 droid platform (Commander/Chopper, squad BB robots, dome cam, LED FX & more)
- Modular, scalable, and designed for both educators and advanced makers
- Developed fully in the open for continuous improvement and community collaboration

---

## Project Structure & Philosophy

- **/bakken-workshop/** — Archived: The original BB-R2 workshop, for historical reference only, NOT actively maintained.
- **/bb-workshop-v2/** — For the improved one-day or two-day BB bot curriculum. Less complex, optimized for short workshops and easy droid builds.
- **/Chopper-Commander/** — This folder: Full summer camp (multi-day/week) project focused on advanced swarm robotics, customization, and creativity.

**In this folder:**
- `CHOPPER_COMMANDER_HANDOFF.md`: The full technical handoff used as a reference for the camp's architecture (generated via Claude — for context, not as current prescription)
- `src/`: All "Big Brain" Chopper Commander source code — this is the main controller.
- `squad-bots/`: Derived but distinct codebase for BB squad robots to participate in full-camp swarm; modular and designed for easy upgrades.
- `dome-cam/`, `dome-leds/`: Subsystems for dome camera and dome effects controllers.
- `docs/`: Diagrams, lesson plans, and technical reference material specific to Chopper Commander.
- `platformio.ini`: Build and library config for Chopper modules.

---

## Project Principles & Best Practices

- **Documentation-First:** Every module and major iteration is documented up front. Keep READMEs, block diagrams, and wiring guides up-to-date with code.
- **Open Collaboration:** Community, educators, and students are encouraged to contribute. Early commits, messy prototypes, and questions are welcome — perfection comes later.
- **Clear Separation of Scope:**  
    - *Workshops*: /bb-workshop-v2/ for simple, time-limited builds  
    - *Camp*: /Chopper-Commander/ for advanced, multi-day, multi-MCU, creative builds  
- **Historical Context:** All legacy, experimental, or previous AI-generated handoffs are preserved for learning but are not prescriptive for new code.
- **Modular Code (`src/`):** Keep each hardware module, communication protocol, or UI enhancement separate and well-documented for easy testing and upgrades.
- **Issue-Driven Development:** Use Issues and Discussions to propose features, request help, report bugs, or share educational feedback.

---

## Getting Started

1. **Clone This Repo and Explore the Tree:**  
   - `bakken-workshop/` — learn from the past  
   - `bb-workshop-v2/` — see our new short-form BB workshop  
   - `Chopper-Commander/` — the main camp development area (start here for multi-bot, Chopper-driven projects)
2. **Read `CHOPPER_COMMANDER_HANDOFF.md`:**  
   - Reference for architecture, intended features, protocols, and lessons learned.
3. **Review `/src/` and `/squad-bots/`** for the latest modular code.
4. **See `/docs/`** for diagrams, best wiring guides, and teaching aids.

---

## Contributing & Community

- **Be open:** Early commits, rough branches, “here’s what I tried” notes, and even failed workshops are valued.
- **Preserve authorship:** Commit early and often. Use Issues to log brainstorms, TODOs, and errors as they come up.
- **Educational focus:** All educational and outreach efforts (like curriculum, lesson plans, and presentations) may be subject to different licensing; see respective folders for licensing and brand notes.

---

## Future Growth

- Final goal: a robust, modular, teacher- and student-friendly swarm robotics platform that’s ready for both code camps and STEAM classrooms.
- Planned features include:  
    - Robust swarm/commander protocols  
    - WiFi/OTA upgrades  
    - Video streaming  
    - LED and animation FX  
    - Easy team customization  
    - Real-world safety (E-STOP, watchdogs)
    - Multi-week “challenge modules” for advanced camps or extracurriculars

---

## Legal & IP

- **Code:** MIT License (see `LICENSE`)
- **Teaching & Presentation Materials:** See specific attributions. Some Hero Props camp curricula, imaging, and presentations may have restricted reuse or require permission for commercial/camp use.

---

**Questions? Want to collaborate?**  
Open an issue, send a PR, or reach out (contact info in main project README).

---

*“Build boldly, teach bravely, share generously!”*