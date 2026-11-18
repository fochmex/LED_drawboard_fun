# Agent Guide for LED_drawboard_fun

## Repository identity

- Repository: `fochmex/LED_drawboard_fun`
- Description: `a 16x16 LED board is controlled via an internet connection`
- Primary language: C++ (93.7%), with a small amount of C (6.3%)
- Hardware target: ESP32-S3 DevKitC-1
- Firmware framework: Arduino via PlatformIO
- Display: 16x16 RGB LED matrix, 256 pixels total
- Primary library: FastLED

## Project goal

This project is a staged embedded electronics and firmware effort to build a browser-controlled 16x16 RGB LED matrix. The engineering approach is deliberately conservative:

1. Validate the hardware first.
2. Prove the electrical design and matrix mapping are correct.
3. Only then add networking, UI, persistence, and product features.

The repository has progressed through Stage 6.5. Stage 6 and the analog NTP clock are accepted; the added binary clock style awaits hardware acceptance.

## Current status

Implemented / active:
- 16x16 ESP32-S3 matrix control, effects, and walking cat
- Wi-Fi, local HTTP dashboard, and live WebSocket status/frame synchronization
- Browser drawing studio and brightness controls
- Optional Europe/Berlin NTP analog and binary clock styles (analog accepted; binary hardware check pending)
- Deep-sleep/wake display restoration

Not yet implemented:
- frame persistence or configuration storage
- enclosure / commissioning work

## Stage roadmap

This repository is organized around these stages:

- Stage 1: Basic LED control
- Stage 2: Power system validation
- Stage 3: Wi-Fi and networking
- Stage 4: Basic web server
- Stage 5: Drawing interface
- Stage 6: Live communication
- Stage 6.5: Optional NTP analog and binary clock
- Stage 7: Persistence and configuration
- Stage 8: Enclosure and commissioning

Current repository status: Stages 1-6 and the Stage 6.5 analog clock are accepted; the Stage 6.5 binary clock style is implemented and awaiting hardware acceptance.

## Hardware architecture

### Controller
- ESP32-S3 DevKitC-1
- Arduino framework in PlatformIO
- GPIO-driven LED data signal

### Display
- 16x16 RGB matrix
- 256 total LED pixels
- WS2812B / SK6812-compatible LEDs
- 3 channels per LED (RGB)

### Electrical assumptions
- LED data pin: GPIO 4
- Status indicator pin: GPIO 33
- Matrix powered from regulated 5 V supply
- ESP32 and matrix share a common ground
- Data line passes through a 3.3 V to 5 V logic-level shifter
- 300 Ω to 500 Ω series resistor recommended on the LED data line
- Do not power the matrix from ESP32 board headers
- Keep brightness low during early hardware validation

## Matrix configuration

The project assumes:
- Width: 16 pixels
- Height: 16 pixels
- Total pixels: 256
- Wiring mode: serpentine
- Rotation: none
- Mirror: disabled on X and Y

This is defined in:
- `include/ProjectConfig.h`
- `include/MatrixConfig.h`
- `include/PinConfig.h`

## Project structure

```text
.
├── README.md
├── README_STAGE1.md
├── PROJECT_STAGES.md
├── platformio.ini
├── .gitignore
├── include/
│   ├── MatrixConfig.h
│   ├── PinConfig.h
│   ├── ProjectConfig.h
│   └── secrets.example.h
└── src/
    └── main.cpp
```

### Important files

- `platformio.ini` — PlatformIO project configuration
- `include/ProjectConfig.h` — matrix dimensions, brightness, timing constants
- `include/PinConfig.h` — GPIO assignments
- `include/MatrixConfig.h` — wiring and rotation configuration
- `include/secrets.example.h` — template for secrets used by future Wi-Fi or network stages
- `src/main.cpp` — main firmware and validation routines
- `README.md` — project overview and roadmap
- `README_STAGE1.md` — stage-specific documentation
- `PROJECT_STAGES.md` — detailed stage-by-stage engineering plan

## Build and run

### Tooling
- PlatformIO
- ESP32 Arduino core
- FastLED library

### PlatformIO project config
`platformio.ini` contains:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps =
    fastled/FastLED@^3.9.0
```

### Typical workflow
1. Install PlatformIO.
2. Open the repo in PlatformIO.
3. Confirm `include/PinConfig.h` matches the actual wiring.
4. Confirm the matrix is powered from a correct 5 V source with a common ground.
5. Build the firmware.
6. Upload to the ESP32-S3.
7. Watch the serial monitor at 115200 baud.

## Core firmware behavior

The firmware in `src/main.cpp` does the following in `setup()`:

- starts serial output at 115200 baud
- configures the status LED pin
- initializes FastLED with the LED strip array
- sets brightness to `STARTUP_BRIGHTNESS` (32)
- sets LED color correction to `TypicalLEDStrip`
- clears the matrix
- prints a startup summary to serial
- begins the stage validation cycle

In `loop()`, the device repeatedly runs Stage 1 validation routines in sequence.

### Validation sequence
The current code cycles through three tests:

1. `runSingleLedColorTest()`
   - shows red, green, blue, and white on a single LED
   - validates basic output and LED indexing

2. `runMatrixSweepTest()`
   - fills the full 16x16 grid with gradient values
   - verifies the entire matrix can be addressed

3. `runCoordinateMappingTest()`
   - fills the grid with alternating cyan/magenta colors
   - verifies mapping consistency and serpentine addressing

The firmware rotates through these tests every ~2.5 seconds.

## Key constants

Defined in `include/ProjectConfig.h`:

```c
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define PIXEL_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

#define STARTUP_BRIGHTNESS 32
#define MAX_TEST_BRIGHTNESS 64
#define STAGE1_TEST_DELAY_MS 500
```

Defined in `include/PinConfig.h`:

```c
#define LED_DATA_PIN 4
#define LED_STATUS_PIN 33
```

Defined in `include/MatrixConfig.h`:

```cpp
enum class MatrixWiringMode : uint8_t
{
    kStraight = 0,
    kSerpentine = 1
};

constexpr MatrixWiringMode MATRIX_WIRING_MODE = MatrixWiringMode::kSerpentine;
constexpr RotationMode MATRIX_ROTATION = RotationMode::kNone;
constexpr bool MATRIX_MIRROR_X = false;
constexpr bool MATRIX_MIRROR_Y = false;
```

## Important implementation details

### Mapping logic
`xyToIndex()` converts x/y coordinates to a flat LED index while supporting serpentine wiring. The logic is intentionally simple and suited for hardware validation.

Important behavior:
- if `MATRIX_WIRING_MODE == kSerpentine`, even rows are left-to-right and odd rows are right-to-left
- invalid coordinates return index 0
- `setPixel()` rejects coordinates outside the matrix bounds

This is essential for correcting the physical orientation of the panel.

### FastLED setup
The board uses:

```cpp
FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, PIXEL_COUNT);
FastLED.setBrightness(STARTUP_BRIGHTNESS);
FastLED.setCorrection(TypicalLEDStrip);
```

This means the project expects a GRB order, not RGB, for the WS2812B hardware.

## Safety and engineering constraints

These are critical and should be respected by any AI or contributor:

- Never power the matrix from ESP32 GPIO pins or board headers
- Keep a proper common ground between ESP32 and matrix
- Route LED data through the proper logic level shifting stage
- Use a series resistor on the data line
- Keep brightness low during early testing
- Only increase brightness after wiring and mapping are confirmed
- Treat this as an embedded hardware project, not a generic software app

## What not to assume

Do not assume later project stages already exist.

This repo is not yet a Wi-Fi web app. The current code only covers:
- hardware validation
- matrix topology
- LED output testing
- serial debugging

Any work on networking, web UI, or frame persistence should be treated as new functionality, not incremental edits to a mature app.

## Suggested tasks for an AI assistant

When working in this repo, favor tasks that align with Stage 1 and the disciplined progression:

- verify or adjust GPIO pin config
- inspect matrix mapping and serpentine indexing
- add extra validation patterns for hardware debugging
- improve serial diagnostics
- document test procedure or wiring notes
- prepare for future Wi-Fi integration by adding clear abstraction boundaries

Avoid tasks like:
- building a full browser app before hardware is validated
- adding cloud services or remote hosting
- introducing network code before Stage 2/3 progress is planned
- refactoring without preserving the Stage 1 validation focus

## Troubleshooting guide

### LEDs do not light
- Check 5 V power to matrix
- Verify common ground
- Confirm GPIO 4 is the actual data pin in hardware
- Confirm logic level shifter is in the signal path
- Ensure `WS2812B` and `GRB` order match the LED type

### Coordinates appear mirrored or rotated
- Check `MATRIX_WIRING_MODE`
- Confirm serpentine order is correct for the actual panel wiring
- Review `xyToIndex()` logic
- Confirm the physical matrix orientation matches assumptions

### Full matrix shows dead or inconsistent pixels
- Check solder joints and connectors
- Verify signal integrity and power injection
- Reduce brightness and retry
- Inspect serial output to confirm test patterns render consistently

### Serial monitor is blank
- Confirm `Serial.begin(115200)` is set
- Confirm upload succeeded
- Confirm monitor speed matches 115200

## Future project evolution

The next meaningful progression after Stage 1 is:

- Stage 2: validate power system and safe operating limits
- Stage 3: connect to local Wi-Fi and show address information
- Stage 4: expose a local web status page
- Stage 5: allow browser-based drawing on a 16x16 grid
- Stage 6: synchronize updates over WebSockets
- Stage 6.5: add opt-in NTP-synchronized analog and binary clock styles
- Stage 7: save and restore board state
- Stage 8: finalize enclosure and production deployment

## Contributor guidance

This repo rewards simple, testable, hardware-aware changes.

Prefer:
- direct, readable firmware code
- explicit constants and configuration values
- well-documented hardware assumptions
- staged validation behavior

Avoid:
- abstracting away the LED matrix wiring too early
- introducing network features before the hardware baseline is stable
- making broad refactors that obscure the hardware logic

## Quick summary

This is a staged ESP32-S3 LED matrix validation project. The current implementation already proves the core idea: the board can drive a 16x16 matrix, map coordinates correctly, and run structured hardware validation sequences. The repository is intentionally held at this early stage so that later features can be built on a reliable foundation.

If you are making changes, default to preserving the Stage 1 validation-first philosophy.
