# LED_drawboard_fun

A 16x16 RGB LED matrix controlled by an ESP32-S3.

## Project goal
This project is being built in phases so the hardware is proven before features like Wi‑Fi, a web interface, and persistence are added.

The final system is intended to be a local, browser-driven LED display with no cloud dependency, but the first milestone is to validate the LED hardware itself.

## Current focus: Stage 1
Stage 1 is hardware validation only. The goal is to prove that the LED driver, GPIO wiring, level shifter, and matrix mapping work correctly before the rest of the system is implemented.

### Stage 1 success criteria
- A single RGB LED can display red, green, blue, and white reliably
- The selected ESP32-S3 GPIO is valid for the LED data line
- The 3.3 V to 5 V level-shifter path works correctly
- The full 16x16 matrix can be addressed without dead pixels or wiring issues
- Coordinate mapping matches the physical matrix layout
- The system remains stable during repeated testing with low brightness

## Stage-based development plan

### Stage 1: Basic LED control
- test one RGB LED
- verify data GPIO
- verify logic-level shifter
- test red, green, blue, and white
- connect the full matrix
- validate coordinate mapping
- verify all 256 pixels

### Stage 2: Power system validation
- 5 V power supply and common ground
- fuses and protective layout
- bulk capacitor and injection points
- current draw and voltage-drop testing
- safe brightness limit

### Stage 3: Wi‑Fi and networking
- connect to local router
- display assigned IP address
- reconnect handling
- DHCP reservation and network stability

### Stage 4: Basic web server
- status page
- device info and Wi‑Fi state
- matrix status reporting

### Stage 5: Drawing interface
- 16x16 browser grid
- color selection
- touch and mouse drawing
- clear/fill/erase and brightness controls

### Stage 6: Live communication
- WebSocket updates
- pixel and full-frame synchronization
- connection health handling

### Stage 7: Persistence and configuration
- save/restore frames
- configuration storage
- versioned storage validation

### Stage 8: Enclosure and commissioning
- hardware installation
- safety checks
- final deployment and documentation

## Recommended hardware setup
- ESP32-S3 board
- 16x16 RGB matrix using WS2812B/SK6812-style LEDs or compatible
- 3.3 V to 5 V logic level shifter
- regulated 5 V power source
- proper common ground
- 300 to 500 ohm series resistor on the LED data line
- no LED power through ESP32-S3 headers

## Device and software setup
This project is structured for PlatformIO with the Arduino framework and FastLED.

Project files include:
- `platformio.ini`
- `include/ProjectConfig.h`
- `include/PinConfig.h`
- `include/MatrixConfig.h`
- `include/secrets.example.h`
- `src/main.cpp`
- `README_STAGE1.md`

## Stage 1 testing instructions
Before continuing, confirm the following:
1. The matrix is powered from a regulated 5 V supply.
2. The ESP32-S3 and matrix share a common ground.
3. The data line is wired through the correct level shifter path.
4. The LED data GPIO matches `include/PinConfig.h`.
5. Brightness is kept low while testing.

Then build and upload the Stage 1 firmware and watch the serial output.

## Start testing now
This repo is ready for Stage 1 validation. Do not move to Wi‑Fi or web work until the hardware tests are stable and repeatable.

## Related docs
- `README_STAGE1.md`
- `include/ProjectConfig.h`
- `include/PinConfig.h`
- `include/MatrixConfig.h`

