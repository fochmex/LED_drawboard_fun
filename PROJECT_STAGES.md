# Project Stages

This repository is structured around an incremental, stage-based development plan for a 16x16 RGB LED matrix driven by an ESP32-S3. The project intentionally validates the hardware first before moving on to Wi‑Fi, web control, persistence, and deployment.

## Stage 1: Basic LED control

Status: implemented / active

Goal:
- Confirm a single RGB LED can display red, green, blue, and white
- Validate the ESP32-S3 GPIO and 3.3 V to 5 V level shifter path
- Test the full 16x16 matrix and coordinate mapping
- Establish a stable hardware baseline before networking or UI work

Most important code:

```c
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define PIXEL_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

#define STARTUP_BRIGHTNESS 32
#define MAX_TEST_BRIGHTNESS 64
#define STAGE1_TEST_DELAY_MS 500
```
`include/ProjectConfig.h`

```c
#define LED_DATA_PIN 4
#define LED_STATUS_PIN 33
```
`include/PinConfig.h`

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
`include/MatrixConfig.h`

```cpp
uint16_t xyToIndex(uint8_t x, uint8_t y)
{
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
    {
        return 0;
    }

    if (MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine)
    {
        const uint16_t rowBase = static_cast<uint16_t>(y) * MATRIX_WIDTH;
        if ((y % 2U) == 0U)
        {
            return rowBase + x;
        }
        return rowBase + (MATRIX_WIDTH - 1U - x);
    }

    return static_cast<uint16_t>(y) * MATRIX_WIDTH + x;
}
```
`src/main.cpp`

```cpp
void runSingleLedColorTest()
{
    printStageHeader("[Stage 1A] Single LED RGB test");

    const CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};

    for (const auto& color : colors)
    {
        clearMatrix();
        setPixel(0, 0, color);
        FastLED.show();
        Serial.printf("Color: R=%u G=%u B=%u\n", color.r, color.g, color.b);
        delay(STAGE1_TEST_DELAY_MS);
    }
}
```

```cpp
void setup()
{
    Serial.begin(115200);
    delay(1000);

    pinMode(LED_STATUS_PIN, OUTPUT);
    digitalWrite(LED_STATUS_PIN, HIGH);

    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, PIXEL_COUNT);
    FastLED.setBrightness(STARTUP_BRIGHTNESS);
    FastLED.setCorrection(TypicalLEDStrip);
    clearMatrix();
    FastLED.show();

    printStartupSummary();
    lastTransitionMs = millis();
}
```

Stage 1 validates the board and matrix before any networking features are added.

---

## Stage 2: Power system validation

Status: planned

Goal:
- Confirm correct 5 V power distribution and common ground
- Validate fuses, bulk capacitors, and injection points
- Measure current draw and voltage drop under load
- Define a safe brightness limit for long-term operation

Important note:
- This stage is not implemented in the current codebase yet.
- It is the next step after the hardware test sequence is proven stable.

---

## Stage 3: Wi‑Fi and networking

Status: planned

Goal:
- Connect the ESP32-S3 to the local router
- Show the assigned IP address on the matrix or serial console
- Handle reconnects and DHCP issues gracefully
- Prepare the device for local browser access

Important note:
- No Wi‑Fi implementation is present yet in the current code.
- The repository is intentionally held at hardware validation until Stage 1 is stable.

---

## Stage 4: Basic web server

Status: planned

Goal:
- Expose a local web status page
- Show device information and Wi‑Fi state
- Report matrix status to a user over HTTP

Expected technologies:
- ESP32-S3 web server
- HTML status page
- lightweight JSON/status endpoints

---

## Stage 5: Drawing interface

Status: planned

Goal:
- Provide a 16x16 browser grid
- Allow mouse/touch drawing
- Support colors, brush tools, clear/fill, and brightness controls

This is the first user-facing control layer for the display.

---

## Stage 6: Live communication

Status: planned

Goal:
- Synchronize pixels and full-frame updates over WebSockets
- Keep the browser display in sync with the device
- Handle connection health and refresh logic

This stage connects the board to the browser-based drawing interface.

---

## Stage 7: Persistence and configuration

Status: planned

Goal:
- Save and restore frames or board state
- Store configuration values persistently
- Support versioned storage validation and recovery

This stage makes the display usable over time rather than only as a live test device.

---

## Stage 8: Enclosure and commissioning

Status: planned

Goal:
- Install the matrix in its final physical enclosure
- Validate heat, power, and safety behavior
- Document deployment steps and maintenance notes

This is the final hardware and productization stage.

---

## Current repository status

At the moment, the project is centered on Stage 1: hardware validation. The code present in the repository focuses on proving that the LED matrix, GPIO line, wiring mode, and coordinate mapping are all correct before moving to network and UI layers.

The implementation already includes:
- matrix size and brightness configuration
- data pin configuration
- serpentine coordinate mapping
- test routines for single LED, full matrix sweep, and coordinate verification
- setup/loop scheduling for staged validation

This makes Stage 1 the foundation for the entire project.

---

## Summary

The project follows a disciplined progression:

1. Stage 1 — validate LED hardware and matrix mapping
2. Stage 2 — validate power system and safe operation
3. Stage 3 — add Wi‑Fi connectivity
4. Stage 4 — add a basic web status interface
5. Stage 5 — enable drawing from the browser
6. Stage 6 — make updates live and synchronized
7. Stage 7 — add persistence and configuration storage
8. Stage 8 — finalize enclosure and deployment

The current repository code is primarily aligned with Stage 1, and that is the correct starting point for the project.
