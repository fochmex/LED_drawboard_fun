# LED_drawboard_fun - ESP32-S3 16x16 RGB LED Matrix

## Project overview
This project is a 16x16 RGB LED matrix controlled by an ESP32-S3. The long-term goal is a local, browser-driven display that can draw and animate without any cloud dependency.

This repository is now intentionally following a phased implementation plan. The first milestone is not the full app; it is a hardware validation stage that proves the LED driver, GPIO wiring, level shifting, and power behavior before the project adds higher-level features.

## Current implementation status
The repository is currently focused on Stage 1: Basic LED Control.

The design is intentionally tight and test-driven:
- verify one RGB LED works on the selected data line
- confirm the ESP32-S3 GPIO and level shifter can drive the matrix safely
- validate the 5 V power setup and common ground
- test serpentine or straight-row mapping across all 256 pixels
- enforce a safe maximum brightness so the matrix cannot exceed the 12 A 5 V supply limit
- prevent adding Wi‑Fi or browser work until the hardware is proven stable

## Stage 1 goals
- Test a single RGB LED with red, green, blue, and white output
- Verify the chosen ESP32-S3 GPIO is valid for the LED data line
- Confirm the 3.3 V to 5 V logic conversion path is working
- Connect the full 16x16 panel and validate every pixel
- Check coordinate mapping for row order and serpentine wiring
- Establish a repeatable baseline before Stage 2 power and Stage 3 Wi‑Fi work
- Keep the software brightness below the safe ceiling dictated by the LPV-100-5 5 V / 12 A supply

## Power-limit note
The project uses a Mean Well LPV-100-5 5 V power supply with a 12 A maximum output. A full-white 16x16 WS2812B matrix can theoretically require:

256 LEDs × 60 mA = 15.36 A

This is above the supply capability, so the firmware enforces a software brightness limit of 78% of full scale. The configured maximum brightness is `MAX_ALLOWED_BRIGHTNESS = 198/255`, which keeps the display operating below the 12 A limit while still allowing useful brightness for validation and normal use.

## Files created for Stage 1
- platformio.ini
- .gitignore
- include/ProjectConfig.h
- include/PinConfig.h
- include/MatrixConfig.h
- include/secrets.example.h
- src/main.cpp
- README_STAGE1.md

## Stage 1 validation sequence
1. Power the board and LED matrix from a regulated 5 V source with a common ground.
2. Run the single-LED RGB test to confirm the line is driving proper colors.
3. Run the full-matrix sweep to check that every LED can be addressed correctly.
4. Run the coordinate verification pattern to confirm row/column mapping is correct.
5. Keep the matrix brightness at or below the firmware limit during validation and increase only after the wiring is proven stable.

## Hardware notes
- Never power the matrix directly through ESP32-S3 GPIO or board headers.
- Use a proper level shifter for 3.3 V logic driving a 5 V LED matrix.
- Keep the data line close to the first LED and use a 300 to 500 ohm series resistor.
- Treat the matrix as a power system, not just a digital display.
- The firmware must clamp brightness to the configured maximum to prevent exceeding the 12 A supply limit.

## Full roadmap
### Stage 1: Basic LED control
- one LED smoke test
- GPIO validation
- level shifter verification
- full matrix test
- coordinate mapping validation
- safe brightness limit enforcement

### Stage 2: Power system
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
6. Watch the serial monitor for the Stage 1 validation sequence.

## Important design principle
The overall product is ambitious, but the best way to make it reliable is to validate the hardware first. This repository now reflects that principle directly, including a programmable brightness ceiling that keeps the matrix safe under the power supply's 12 A limit.

## Related docs
- README_STAGE1.md
- include/ProjectConfig.h
- include/MatrixConfig.h
- include/PinConfig.h
