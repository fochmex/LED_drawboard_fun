# LED_drawboard_fun - ESP32-S3 16x16 RGB LED Matrix

## Project overview
This project is a 16x16 RGB LED matrix controlled by an ESP32-S3. The long-term goal is a local, browser-driven display that can draw and animate without any cloud dependency.

This repository is now intentionally following a phased implementation plan. The first milestone is not the full app; it is a hardware validation stage that proves the LED driver, GPIO wiring, level-shifting path, power distribution, and matrix mapping before any later network features are added.

## Current implementation status
**Stage 1: Basic LED Control - COMPLETED.**
**Stage 2: Power System - COMPLETED.**

Completion confirmed by the hardware owner on 2026-11-17: the LED panel and
sleep/wake work, the fuse and 5 V supply are connected and working, and the
existing brightness limit is accepted. This records the owner's acceptance,
not new current or voltage-drop measurements by the firmware.

The current firmware is an offline LED effects playground. Stage 3 networking
has not started.

The design is intentionally tight and test-driven:
- verify one RGB LED works on the selected data line
- confirm the ESP32-S3 GPIO and level shifter can drive the matrix safely
- validate the 5 V power setup and common ground
- test serpentine or straight-row mapping across all 256 pixels
- enforce a safe maximum brightness so the matrix cannot exceed the 12 A 5 V supply limit
- add a wake/sleep button and idle timeout for the board to enter deep sleep without damaging the panel
- prevent adding Wi‑Fi or browser work until the hardware is proven stable

## Stage 1 goals (completed)
- Test a single RGB LED with red, green, blue, and white output
- Verify the chosen ESP32-S3 GPIO is valid for the LED data line
- Confirm the 3.3 V to 5 V logic conversion path is working
- Connect the full 16x16 panel and validate every pixel
- Check coordinate mapping for row order and serpentine wiring
- Establish a repeatable baseline before Stage 2 power and Stage 3 Wi‑Fi work
- Keep the software brightness below the safe ceiling dictated by the LPV-100-5 5 V / 12 A supply
- Provide a reliable hardware sleep path using a momentary button and a 60-minute idle timer

## Wake and sleep behavior
The effects build retains the validated power-saving behavior:

- A momentary button on GPIO 14 is used as a wake/sleep input.
- Hold the button for 5 seconds, then release it to clear the display and enter ESP32 deep sleep. Waiting for release prevents immediate wake-up from the still-active LOW signal.
- GPIO14 uses the internal input pull-up while awake and the RTC pull-up during deep sleep. Button polling continues during LED pattern display times.
- Press again to wake. The board restores the saved LED frame and selected effect from RTC memory, returns GPIO14 to digital input mode, and resumes animation. Release the wake press before beginning another sleep gesture; this release does not skip effects.
- If the display remains unchanged for more than 60 minutes, the device also enters deep sleep using the same wake routine.

The firmware stores the current frame, effect selection, and animation elapsed
time in RTC memory. This survives deep sleep, not power removal.

## LED effects playground (before Stage 3)

The old repeating hardware test is replaced by six animated effects:
rainbow, confetti, comet, plasma, bouncing scanner, and ocean waves.

- Effects change automatically every 20 seconds.
- A short GPIO14 button press advances on release and restarts the 20-second timer.
- A 5-second hold requests sleep instead of skipping an effect.
- Rendering is scheduled every 20 ms without blocking animation delays.
- Startup brightness remains 32/255; the existing 198/255 ceiling is unchanged.
- Animations count as LED activity, so the unchanged 60-minute idle timeout
  does not expire while effects are playing.

See [effects and button testing](README_STAGE1.md) for controls and checks.

## Power-limit note
The project uses a Mean Well LPV-100-5 5 V power supply with a 12 A maximum output. A full-white 16x16 WS2812B matrix can theoretically require:

256 LEDs × 60 mA = 15.36 A

This is above the supply capability, so the firmware enforces a software brightness limit of 78% of full scale. The configured maximum brightness is `MAX_ALLOWED_BRIGHTNESS = 198/255`, which keeps the matrix below the 12 A ceiling while still providing strong output.

The design intent is:
- keep the matrix within the safe power envelope of the LPV-100-5
- protect the wiring and power injection from overcurrent stress
- allow a useful brightness range during testing without exceeding the supply limit
- treat the brightness cap as a software safety feature, not a substitute for proper fusing, power injection, and grounding

## Files created for Stage 1
- platformio.ini
- .gitignore
- include/ProjectConfig.h
- include/PinConfig.h
- include/MatrixConfig.h
- include/secrets.example.h
- src/main.cpp
- README_STAGE1.md

## Completed Stage 1 validation sequence (historical)
These are the accepted hardware checks; the current firmware plays effects
instead of repeating these diagnostic patterns.

1. Power the board and LED matrix from a regulated 5 V source with a common ground.
2. Run the single-LED RGB test to confirm the line is driving proper colors.
3. Run the full-matrix sweep to check that every LED can be addressed correctly.
4. Run the coordinate verification pattern to confirm row/column mapping is correct.
5. Test the wake button by holding it for 5 seconds, then releasing it and confirming the matrix clears and sleeps.
6. Wake the board again with the same button and confirm the last LED frame is restored.
7. Keep the matrix brightness at or below the firmware limit during validation and increase only after the wiring is proven stable.

## Hardware notes
- Never power the matrix directly through ESP32-S3 GPIO or board headers.
- Use a proper level shifter for 3.3 V logic driving a 5 V LED matrix.
- Keep the data line close to the first LED and use a 300 to 500 ohm series resistor.
- Treat the matrix as a power system, not just a digital display.
- The firmware must clamp brightness to the configured maximum to prevent exceeding the 12 A supply limit.
- Use a normally-open button to GND with input pull-up so the board can wake reliably from deep sleep.
- Install a fuse on the 5 V rail feeding the matrix and logic circuitry.

## Full roadmap
### Stage 1: Basic LED control - COMPLETED
- one LED smoke test
- GPIO validation
- level shifter verification
- full matrix test
- coordinate mapping validation
- safe brightness limit enforcement
- wake/sleep button and idle shutdown testing

### Stage 2: Power system - COMPLETED
Accepted by the hardware owner: connected fuse, working 5 V supply, and correct
configured maximum brightness. Original power-system checklist for reference:

- 5 V supply and fusing
- bulk capacitor and injection points
- measurement of current and voltage drop
- software and hardware power margin validation

### Stage 3: Wi‑Fi and networking
- connect to router
- display IP address
- reconnect behavior
- DHCP reservation

### Stage 4: Basic web server
- status page
- device information
- matrix status and Wi‑Fi state

### Stage 5: Drawing interface
- 16x16 browser grid
- color and brush tools
- clear/fill/erase and brightness control

### Stage 6: Live communication
- WebSocket updates
- frame synchronization
- connection health handling

### Stage 7: Storage and persistence
- save/restore image state
- configuration persistence
- versioned storage validation

### Stage 8: Enclosure and commissioning
- hardware installation
- safety checks
- final documentation and deployment

## Tools and environment
This Stage 1 implementation is designed for PlatformIO with the Arduino framework and the FastLED library.

## Quick start
1. Install PlatformIO.
2. Open the project folder.
3. Confirm the data pin in `include/PinConfig.h` matches your board and wiring.
4. Build and upload the firmware.
5. Confirm the serial monitor reports the safe brightness ceiling for the LPV-100-5 5 V / 12 A supply.
6. Watch the effects playlist; the serial monitor prints each selected effect.
7. Hold the wake button for 5 seconds, release to sleep, then press again to test wake recovery.

## Important design principle
The overall product is ambitious, but the best way to make it reliable is to validate the hardware first. This repository now reflects that principle directly, including a programmable brightness ceiling, proper fusing, and conservative power budgeting for the LPV-100-5 supply.

## Related docs
- README_STAGE1.md
- include/ProjectConfig.h
- include/MatrixConfig.h
- include/PinConfig.h
