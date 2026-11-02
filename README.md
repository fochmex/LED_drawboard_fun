# LED_drawboard_fun

A local, browser-driven 16x16 RGB LED matrix project built around an ESP32-S3. The system is designed to validate the display hardware first, then expand into a web-controlled drawing surface with animation and persistence.

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
- The supply is a Mean Well LPV-100-5, 5 V / 12 A max. A full-white 16x16 WS2812B matrix can theoretically draw 256 × 60 mA = 15.36 A, which exceeds the supply limit, so the firmware enforces a software brightness ceiling to protect the system.

## 74AHCT125 Logic-Level Shifter

### Purpose

The ESP32-S3 uses 3.3 V logic, while the 5 V addressable LED matrix may require a data signal closer to 5 V for reliable operation.

A 74AHCT125 is used as a non-inverting logic-level shifter:

```text
ESP32-S3 3.3 V data
        |
        v
74AHCT125 level shifter
        |
        v
5 V LED data signal
        |
        v
16 x 16 RGB LED matrix
```

The AHCT logic family is important because its inputs are compatible with TTL voltage levels. This allows the 3.3 V output from the ESP32-S3 to be reliably interpreted as a logical high while the IC is powered from 5 V.

Use one of the following parts:

- 74AHCT125
- SN74AHCT125
- Equivalent AHCT-family quad buffer with active-low output enable

Do not substitute it with a 74HC125 unless the input-voltage compatibility has been verified. A 74AHC125 is also not automatically equivalent because its input thresholds differ from the TTL-compatible AHCT version.

---

### 74AHCT125 Pinout

The following pinout applies to common DIP-14, SOIC-14, and TSSOP-14 packages.

```text
                   74AHCT125
                +-------------+
        1OE   1 |             | 14  VCC
         1A   2 |             | 13  4OE
         1Y   3 |             | 12  4A
        2OE   4 |             | 11  4Y
         2A   5 |             | 10  3OE
         2Y   6 |             |  9  3A
        GND   7 |             |  8  3Y
                +-------------+
```

The pin numbers run counterclockwise when the IC is viewed from above.

The notch or orientation marking is located between pins 1 and 14.

---

### Channel Function

The 74AHCT125 contains four independent non-inverting buffers.

Each channel contains:

- One data input named A
- One data output named Y
- One active-low output-enable input named OE

The function is:

```text
OE = LOW:   Output enabled, Y follows A
OE = HIGH:  Output disabled, Y is high impedance
```

Because OE is active-low, the output is enabled by connecting its OE input to ground.

Only one of the four channels is required for this project.

## Recommended Connections

### Power Connections

Connect the 74AHCT125 as follows:

```text
Pin 14, VCC  -> +5 V
Pin 7, GND   -> Common ground
```

The 74AHCT125 shall be powered from the same regulated 5 V supply as the LED matrix.

Do not power the 74AHCT125 from the ESP32-S3 3.3 V supply for this application, because its output must provide a 5 V logic signal to the LED matrix.

---

### Channel 1 LED Data Connections

Use channel 1 for the LED data signal:

```text
Pin 1, 1OE  -> GND
Pin 2, 1A   -> ESP32-S3 LED data GPIO
Pin 3, 1Y   -> 330 ohm resistor -> LED matrix DIN
```

Connecting pin 1 to ground permanently enables channel 1.

The data entering pin 2 is reproduced as a non-inverted 5 V logic signal on pin 3.

---

## Complete Wiring Diagram

```text
                  Mean Well LPV-100-5
                  regulated 5 V output
                         |
            +------------+------------------+
            |                               |
            |                               |
            v                               v
     LED matrix +5 V                 Pin 14, VCC
                                            |
                                         +--+--+
                                         |100nF|
                                         +--+--+
                                            |
Common ground ------------------------------+------ Pin 7, GND
       |                                    |
       |                                    +------ Pin 1, 1OE
       |
       +------------------------------------------- ESP32-S3 GND
       |
       +------------------------------------------- LED matrix GND


ESP32-S3 LED data GPIO
       |
       +------------------------------------------- Pin 2, 1A


Pin 3, 1Y
       |
       v
  330 ohm resistor
       |
       v
LED matrix DIN
```

---

## Pin Connection List

### Used Channel

```text
Pin 1, 1OE  -> GND
Pin 2, 1A   -> ESP32-S3 LED data GPIO
Pin 3, 1Y   -> 330 ohm resistor -> LED matrix DIN
```

### Unused Channel 2

```text
Pin 4, 2OE  -> +5 V
Pin 5, 2A   -> GND
Pin 6, 2Y   -> Not connected
```

### Power

```text
Pin 7, GND  -> Common ground
```

### Unused Channel 3

```text
Pin 8, 3Y   -> Not connected
Pin 9, 3A   -> GND
Pin 10, 3OE -> +5 V
```

### Unused Channel 4

```text
Pin 11, 4Y  -> Not connected
Pin 12, 4A  -> GND
Pin 13, 4OE -> +5 V
```

### Supply

```text
Pin 14, VCC -> +5 V
```

Connecting the unused OE pins to +5 V disables the unused outputs.

The unused A inputs should be connected to ground so they remain at a defined logic level and do not float.

Unused Y outputs may remain unconnected.

---

## Decoupling Capacitors

A 100 nF ceramic capacitor shall be installed directly between the supply pins of the 74AHCT125:

```text
100 nF ceramic capacitor:

One side -> Pin 14, VCC
Other side -> Pin 7, GND
```

The capacitor should be placed physically as close as possible to pins 14 and 7.

An additional 1 µF ceramic capacitor may be installed near the IC:

```text
1 uF ceramic capacitor:

One side -> +5 V
Other side -> GND
```

Recommended components:

```text
C1: 100 nF ceramic, mandatory
C2: 1 uF ceramic, optional
```

---

## LED Data Series Resistor

Install a resistor between the output of the 74AHCT125 and the data input of the first LED.

Recommended initial value:

```text
R1 = 330 ohm
```

Connection:

```text
74AHCT125 pin 3, 1Y
        |
        v
330 ohm resistor
        |
        v
LED matrix DIN
```

A value between approximately 300 and 500 ohms is normally suitable.

The resistor should be placed close to the 74AHCT125 output or close to the first LED data input. The connection between the resistor and the first LED should be kept short.

---

## Optional Input Pull-Down Resistor

If pin 1, 1OE, is permanently connected to ground, a 10 kΩ pull-down resistor may be installed between the data input and ground.

```text
Pin 2, 1A
     |
     +---- 10 kohm ---- GND
```

This keeps the level-shifter input low if the ESP32-S3 data GPIO is temporarily floating during startup or reset.

Recommended value:

```text
R2 = 10 kohm
```

This pull-down resistor is optional but recommended for predictable startup behavior.

---

## Common Ground Requirement

All components must use the same ground reference.

The following connections are mandatory:

```text
LPV-100-5 negative output
        |
        +---- ESP32-S3 GND
        |
        +---- 74AHCT125 pin 7
        |
        +---- LED matrix GND
```

The data connection will not operate reliably without a common ground.

Possible symptoms of a missing or poor ground connection include:

- Random LED colors
- Flickering
- Intermittent operation
- Incorrect pixels
- Complete loss of communication
- ESP32-S3 resets

The ground connection between the ESP32-S3, level shifter, and first LED should be short and reliable.

---

## Recommended Physical Placement

The 74AHCT125 should be installed close to the LED matrix data input.

Recommended placement:

```text
ESP32-S3
   |
   | 3.3 V data
   v
74AHCT125
   |
   | short 5 V data connection
   v
330 ohm resistor
   |
   v
First LED DIN
```

Installation recommendations:

- Place the 74AHCT125 near the first LED.
- Place the 100 nF capacitor directly beside the IC.
- Keep the 5 V data line short.
- Keep the data wiring away from mains wiring.
- Keep the data wiring away from high-current LED power cables.
- Route a ground conductor together with the data signal.
- Avoid using long breadboard jumper wires in the final installation.
- Use a PCB, perfboard, or securely mounted terminal arrangement.

If the ESP32-S3 is located farther from the matrix, the 3.3 V connection may run to the level shifter, while the final 5 V connection from the level shifter to the first LED should remain short.

---

## Simple Permanently Enabled Configuration

The simplest recommended configuration permanently enables channel 1.

```text
Pin 1, 1OE  -> GND
Pin 2, 1A   -> ESP32-S3 LED data GPIO
Pin 3, 1Y   -> 330 ohm resistor -> Matrix DIN
```

Add a 10 kΩ pull-down resistor from pin 2 to ground to keep the input low during ESP32-S3 startup.

This configuration requires only one ESP32-S3 GPIO.

---

## Optional Controlled-Enable Configuration

For more controlled startup behavior, the output-enable input can be controlled by a second ESP32-S3 GPIO.

In this configuration:

```text
Pin 1, 1OE -> ESP32-S3 enable GPIO
Pin 1, 1OE -> 10 kohm pull-up resistor -> +5 V
```

Because OE is active-low:

```text
Enable GPIO HIGH -> LED data output disabled
Enable GPIO LOW  -> LED data output enabled
```

The 10 kΩ pull-up keeps the output disabled while the ESP32-S3 is starting or its GPIO is not yet configured.

### Controlled-enable connection

```text
                         +5 V
                          |
                        10 kohm
                          |
ESP32-S3 enable GPIO -----+----- Pin 1, 1OE
```

### Example initialization

```cpp
constexpr uint8_t LED_DATA_PIN = 4;
constexpr uint8_t LED_ENABLE_PIN = 5;

void initializeLedOutput()
{
    // Disable the level-shifter output during initialization.
    pinMode(LED_ENABLE_PIN, OUTPUT);
    digitalWrite(LED_ENABLE_PIN, HIGH);

    // Prepare the LED data output in a known low state.
    pinMode(LED_DATA_PIN, OUTPUT);
    digitalWrite(LED_DATA_PIN, LOW);

    // Initialize the LED library and clear the framebuffer here.

    // Enable the level-shifter output after initialization.
    digitalWrite(LED_ENABLE_PIN, LOW);
}
```

This controlled-enable circuit is optional. The permanently enabled configuration is sufficient if the extra GPIO is not available.

---

## Recommended Final Configuration

For the initial project version, use the following configuration:

```text
Component:                  74AHCT125 or SN74AHCT125
Supply voltage:             5 V
Used channel:               Channel 1

Pin 1, 1OE:                 GND
Pin 2, 1A:                  ESP32-S3 LED data GPIO
Pin 3, 1Y:                  330 ohm resistor to matrix DIN
Pin 7, GND:                 Common ground
Pin 14, VCC:                +5 V

Decoupling capacitor:       100 nF between pins 14 and 7
Optional local capacitor:   1 uF between +5 V and GND
Input pull-down:            10 kohm from pin 2 to GND

Unused OE inputs:           +5 V
Unused A inputs:            GND
Unused Y outputs:           Not connected
```

---

## Recommended Parts

```text
1 x 74AHCT125 or SN74AHCT125
1 x 100 nF ceramic capacitor
1 x 1 uF ceramic capacitor, optional
1 x 330 ohm resistor
1 x 10 kohm resistor, optional but recommended
```

For hand assembly and prototyping, the DIP-14 version is the easiest to use.

For a compact PCB, SOIC-14 or TSSOP-14 may be used.

---

## Important Notes

- Use an AHCT device for dependable 3.3 V-to-5 V conversion.
- Power the 74AHCT125 from the regulated 5 V supply.
- Always connect all grounds together.
- Do not leave unused inputs floating.
- Install the 100 nF decoupling capacitor close to the IC.
- Keep the connection between the level shifter and the first LED short.
- Ensure that the LED data connection goes to DIN, not DOUT.
- Verify the IC orientation before applying power.
- Verify the exact package pinout against the datasheet of the purchased component.

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
- enforce a safe software brightness ceiling to protect the 5 V supply
- prepare the platform for later Wi‑Fi and browser control features

The firmware currently performs a validation routine that checks:
1. single LED RGB output
2. full matrix sweep
3. coordinate mapping verification
4. repeatable behavior under low brightness conditions

## Current implementation status

This repository is currently focused on Stage 1 of the development plan: Basic LED control and hardware validation.

The design principle is to avoid adding Wi‑Fi, browser features, or storage functionality until the display hardware is proven stable. Only after the matrix is electrically sound and correctly mapped should software complexity increase.

## Stage 1 success criteria

The firmware and hardware are considered valid for Stage 1 when all of the following are confirmed:
- a single RGB LED can display red, green, blue, and white reliably
- the selected ESP32-S3 GPIO is valid for the LED data signal
- the 3.3 V to 5 V level-shifter path functions correctly
- all 256 pixels can be addressed without dead pixels or wiring faults
- coordinate mapping matches the physical matrix layout
- the system remains stable during repeated testing while staying under the power limit

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
- confirm firmware brightness cap stays below the Mean Well 12 A limit

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
- The firmware must never allow the brightness to exceed the configured software ceiling, because the 5 V / 12 A supply cannot safely support a theoretical 15.36 A full-white matrix.

## Repository structure

Key project files:
- `platformio.ini` — PlatformIO project configuration
- `include/ProjectConfig.h` — matrix size, brightness, test values, and power limits
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
6. Verify the serial monitor shows the safe brightness cap and the project remains under the 12 A supply limit.
7. Watch the serial monitor for the Stage 1 validation sequence.

## Technical summary

This project is a practical embedded electronics build with a clear progression:
- first, verify the LED matrix and its electrical interface
- second, confirm pixel addressing and physical mapping
- third, set safe operating limits for power and brightness
- fourth, add network connectivity and a local browser UI
- finally, turn the display into a useful drawing and information system

The current codebase reflects a disciplined engineering approach: validate the hardware first, then layer software features on top of a proven foundation.

## Related docs
- `README_STAGE1.md`
- `include/ProjectConfig.h`
- `include/PinConfig.h`
- `include/MatrixConfig.h`
