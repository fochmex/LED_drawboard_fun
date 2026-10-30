# LED_drawboard_fun

A local, browser-driven 16x16 RGB LED matrix project built around an ESP32-S3. The system is designed to validate the display hardware first, then expand into a web-controlled drawing surface with no cloud dependency.

## Project goal

This project is meant to evolve from a hardware validation platform into a compact embedded visual display system that can be controlled from a local browser.

The software goal is straightforward:
- validate the LED matrix hardware and electrical design before adding network features
- provide a reliable way to address each RGB pixel in a 16x16 grid
- build a foundation for local web control, drawing tools, animation, and persistence
- keep the final system self-hosted and local-only, without requiring cloud services

The repository currently focuses on Stage 1: proving the hardware is correct and stable before moving to Wi‑Fi, the web UI, and later product features.

## Hardware overview

### Controller
- ESP32-S3 DevKitC-1
- Arduino framework in PlatformIO
- FastLED library for low-level LED control

### Display
- 16x16 RGB LED matrix
- WS2812B / SK6812-compatible LED type
- 256 total pixels
- 3-channel RGB LEDs, one pixel per LED

### Electrical design
- ESP32-S3 GPIO used for the LED data line: GPIO 4
- Status indicator pin: GPIO 33
- Matrix powered from a regulated 5 V supply
- ESP32 and matrix share a common ground
- Data signal passes through a 3.3 V to 5 V logic level shifter
- 300 Ω to 500 Ω series resistor recommended on the LED data line
- LED power is not sourced through the ESP32 board headers
- Startup brightness is set to 32 for safe validation tests

### Matrix configuration
The current project configuration matches the hardware and software assumptions in the code:
- Width: 16 pixels
- Height: 16 pixels
- Total pixels: 256
- Wiring mode: serpentine
- Rotation: none
- Mirror: disabled on X and Y

This configuration is defined in:
- `include/ProjectConfig.h`
- `include/MatrixConfig.h`
- `include/PinConfig.h`

## Software goals

The software is intentionally structured to verify the hardware before building the application layer. The goal is to keep the firmware simple, readable, and test-driven.

Current software responsibilities:
- drive LED output using FastLED
- map x/y coordinates to the physical matrix layout
- support serpentine wiring and correct indexing
- run staged hardware validation patterns
- print detailed serial diagnostics for debugging and verification
- prepare the platform for later Wi‑Fi and browser control features

The firmware currently performs a validation routine that checks:
1. single LED RGB output
2. full matrix sweep
3. coordinate mapping verification
4. repeatable behavior under low brightness conditions

## Current implementation status

This repository is currently focused on Stage 1 of the development plan: Basic LED control and hardware validation.

The design principle is to avoid adding Wi‑Fi, browser features, or storage functionality until the display hardware is proven stable. Only after the matrix is electrically sound and correctly mapped should the system advance to networking and local web control.

## Stage 1 success criteria

The firmware and hardware are considered valid for Stage 1 when all of the following are confirmed:
- a single RGB LED can display red, green, blue, and white reliably
- the selected ESP32-S3 GPIO is valid for the LED data signal
- the 3.3 V to 5 V level-shifter path functions correctly
- all 256 pixels can be addressed without dead pixels or wiring faults
- coordinate mapping matches the physical matrix layout
- the system remains stable during repeated testing at low brightness

## Stage-based roadmap

### Stage 1: Basic LED control
- test a single RGB LED
- verify the selected data GPIO
- verify the logic-level shifter
- test red, green, blue, and white outputs
- connect and validate the full 16x16 matrix
- verify coordinate mapping across all pixels

### Stage 2: Power system validation
- regulated 5 V power supply
- common ground integrity
- fuse and protection layout
- bulk capacitance and injection points
- current draw and voltage-drop testing
- safe brightness limits

### Stage 3: Wi‑Fi and networking
- connect to the local router
- display assigned IP address
- reconnect behavior and resilience
- DHCP reservation and network stability

### Stage 4: Basic web server
- device status page
- device information and Wi‑Fi state
- matrix health and reporting endpoints

### Stage 5: Drawing interface
- 16x16 browser grid
- color selection
- touch and mouse drawing
- clear, fill, erase, and brightness controls

### Stage 6: Live communication
- WebSocket updates
- pixel and full-frame synchronization
- connection health monitoring

### Stage 7: Persistence and configuration
- save and restore frames
- configuration storage
- versioned persistence validation

### Stage 8: Enclosure and commissioning
- finalize hardware installation
- safety checks
- documentation and deployment

## Hardware safety and wiring notes

- Never power the matrix directly from ESP32 GPIO or board headers.
- Use a proper common ground between the ESP32 and the LED matrix.
- Route the data line through the correct level-shifting path.
- Keep the LED data line short and include a 300 Ω to 500 Ω series resistor.
- Keep brightness low during initial tests and only raise it after the wiring is verified.
- Treat the LED matrix as a power system as well as a digital display.

## Repository structure

Key project files:
- `platformio.ini` — PlatformIO project configuration
- `include/ProjectConfig.h` — matrix size, brightness, test values
- `include/PinConfig.h` — GPIO assignments
- `include/MatrixConfig.h` — wiring and rotation settings
- `include/secrets.example.h` — example secrets template
- `src/main.cpp` — main firmware and validation logic
- `README_STAGE1.md` — stage-specific validation documentation
- `README.md` — project overview and hardware/software context

## Development environment

This project is built with:
- PlatformIO
- ESP32 Arduino core
- FastLED library
- ESP32-S3 target board

## Build and test workflow

1. Install PlatformIO.
2. Open the repository in PlatformIO.
3. Confirm the LED data pin in `include/PinConfig.h` matches your hardware.
4. Confirm the matrix is powered from a regulated 5 V source with a common ground.
5. Build and upload the firmware.
6. Watch the serial monitor for the Stage 1 validation sequence.

## Technical summary

This project is a practical embedded electronics build with a clear progression:
- first, verify the LED matrix and its electrical interface
- second, confirm pixel addressing and physical mapping
- third, add network connectivity and a local browser UI
- finally, turn the display into a useful drawing and information system

The current codebase reflects a disciplined engineering approach: validate the hardware first, then layer software features on top of a proven foundation.

## Related docs
- `README_STAGE1.md`
- `include/ProjectConfig.h`
- `include/PinConfig.h`
- `include/MatrixConfig.h`
