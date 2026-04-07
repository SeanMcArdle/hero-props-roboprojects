# Animatronic Eyes Research

This document contains research on open-source animatronic eye mechanisms for the Wicker Husband puppetry project.

## 1. James Bruton's Animatronic Eyes
James Bruton has designed several iterations of 3D-printed animatronic eyes on his YouTube channel.
*   **Design:** Typically uses a compact 3D-printed mechanism with micro servos (like SG90 or MG90S) and spherical/universal joints.
*   **Features:** 2-axis movement (pan/tilt) for each eye, plus eyelid blinking. He often controls them via an Arduino or ESP32, sometimes paired with an RC receiver or a custom controller.
*   **Where to find:** His CAD files and code are usually hosted on his GitHub (`XRobots`) or his Patreon page.

## 2. Nilheim Mechatronics (Will Cogley)
Will Cogley is currently considered the gold standard for open-source 3D-printed animatronic eyes.
*   **Design:** He offers several versions (Simplified, Advanced, Compact) that are highly realistic.
*   **Features:** 6-servo designs (pan, tilt, individual eyelids), very compact, uses standard micro servos.
*   **Where to find:** His STLs are available on Thingiverse/Printables, and his code is on GitHub. He has excellent build tutorials on his YouTube channel.

## 3. Adafruit "Uncanny Eyes" (Digital/Screen-Based)
If mechanical eyes are too complex or fragile, digital eyes are a great alternative.
*   **Design:** Uses two small TFT or OLED screens (e.g., 1.44" or 1.54") driven by a fast microcontroller (Teensy, ESP32, or RP2040).
*   **Features:** Extremely customizable (human, dragon, robot, etc.). Blinking and looking around are handled entirely in software.
*   **Where to find:** Adafruit Learning System and the Adafruit GitHub repository.

## 4. Brian Roe's Animatronic Eyes (Make: 3D Printing Projects)
A classic, robust project featured in Make magazine.
*   **Design:** Sturdy, easy to print, uses standard hobby servos.
*   **Where to find:** GitHub repository: `Make3DPrintingProjects/Animatronic-Eyes`

## 5. InMoov Robot Eyes
The open-source 3D-printed life-size robot, InMoov, has a well-documented eye mechanism.
*   **Design:** Uses standard servos and includes camera mounts inside the eyes for computer vision.
*   **Where to find:** inmoov.fr

## 6. Other Notable GitHub Projects
*   `mklements/AIChatbot`: A Raspberry Pi 5 project with a 6-servo set of animatronic eyes and a NeoPixel mouth.
*   `kunalsmh/animatronic-eyes`: 3D-printed animatronic eyes that track you.
*   `ripred/EyesAndBrows`: Animatronic desktop toy with eyeballs and eyebrows.
*   `rickymedrano/doorman`: 3D-printed face with animatronic eyes.

## Next Steps for Wicker Husband
1.  **Determine Requirements:** Do we need mechanical realism (Nilheim) or digital flexibility (Adafruit)?
2.  **Hardware Selection:** Select servos (SG90/MG90S) and a microcontroller (ESP32 is recommended since we use it across the HeroProps ecosystem).
3.  **Prototyping:** Print a test mechanism (e.g., Nilheim's simplified version) and write a basic test script using the `HeroPropsProtocol` for remote control.
