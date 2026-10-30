# LED_drawboard_fun - ESP32-S3 Web-Controlled 16 x 16 RGB LED Matrix

## Project Overview

This project implements a web-controlled **16 x 16 RGB LED matrix** using an **ESP32-S3**.

The ESP32-S3 connects to a local Wi-Fi network and hosts a browser-based drawing interface. From a smartphone, tablet, or computer, the user can select colors and draw on a virtual 16 x 16 grid. Each browser pixel corresponds to one physical RGB LED.

The user interface is hosted entirely by the ESP32-S3. No cloud service, external server, internet connection, or mobile application is required during normal operation.

A fixed local IP address is assigned through a DHCP reservation in the router. This address can be encoded in a QR code and attached to the enclosure for easy access from a smartphone.

## Main Features

- ESP32-S3 controller with 2.4 GHz Wi-Fi
- 16 x 16 RGB LED matrix
- 256 individually addressable 5 V RGB LEDs
- Responsive browser-based drawing interface
- Touch and mouse drawing
- RGB color selection and preset colors
- Eraser, Clear, Fill, Undo, and Redo
- Global brightness control with a firmware safety limit
- Live-update and manual Apply modes
- Save and restore the current image
- Fixed local IP address through DHCP reservation
- QR-code access
- Automatic Wi-Fi and browser reconnection
- No cloud dependency or mobile app required

## 1. System Concept

The system consists of:

1. Mean Well 5 V power supply
2. ESP32-S3 development board
3. 3.3 V-to-5 V logic-level shifter
4. 16 x 16 individually addressable RGB LED matrix
5. Fused 5 V power distribution
6. Protective enclosure
7. Local 2.4 GHz Wi-Fi network
8. Browser-based drawing interface

```text
Smartphone, tablet, or computer
              |
              | Wi-Fi
              v
         Local router
              |
              | Wi-Fi
              v
        ESP32-S3 web server
              |
              | 3.3 V data
              v
       Logic-level shifter
              |
              | 5 V data
              v
       16 x 16 RGB matrix
```

## 2. Hardware Specification

### 2.1 ESP32-S3 Controller

Recommended controller:

- ESP32-S3-DevKitC-1 or a comparable ESP32-S3 development board
- USB connection for programming and debugging
- At least one suitable free digital GPIO
- Flash memory for firmware and web files
- Nonvolatile configuration storage
- Optional PSRAM

PSRAM is not required for a 256-pixel framebuffer, but it may be useful for future extensions.

The selected LED data pin must not conflict with:

- Flash or PSRAM connections
- USB functions
- Boot-strapping pins
- Onboard peripherals

The final pin assignment must be verified against the exact board variant.

Example configuration:

```cpp
#define LED_DATA_PIN 4
```

### 2.2 RGB LED Matrix

The initial display consists of:

- Width: 16 pixels
- Height: 16 pixels
- Total: 256 pixels
- Supply voltage: 5 V DC
- Individually addressable RGB LEDs
- WS2812B, SK6812, or compatible protocol

Initial software definition:

```cpp
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define PIXEL_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)
```

Logical coordinates:

- `X = 0`: leftmost column
- `X = 15`: rightmost column
- `Y = 0`: top row
- `Y = 15`: bottom row

The browser interface and physical matrix must use the same orientation.

### 2.3 Matrix Wiring and Coordinate Mapping

The physical matrix may use serpentine wiring:

```text
Row 0:  left  -> right
Row 1:  right -> left
Row 2:  left  -> right
Row 3:  right -> left
...
Row 15: right -> left
```

The browser shall use logical X/Y coordinates. The firmware shall translate these coordinates into the physical LED index.

Example serpentine mapping:

```cpp
uint16_t xyToIndex(uint8_t x, uint8_t y)
{
    if ((y % 2) == 0)
    {
        return y * MATRIX_WIDTH + x;
    }

    return y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
}
```

The final coordinate mapper should support:

- Serpentine or straight-row wiring
- First-pixel corner selection
- Rotation
- Horizontal mirroring
- Vertical mirroring

## 3. Power-Supply Specification

### 3.1 Worst-Case Current Estimate

A conservative design value for a standard RGB addressable LED is approximately 60 mA at full-brightness white.

```text
256 LEDs x 0.060 A = 15.36 A
5 V x 15.36 A = 76.8 W
```

This is a worst-case design estimate. Actual consumption depends on LED type, color, brightness, animation, and firmware limits.

### 3.2 Recommended Power Supply

Recommended power class:

- Manufacturer: Mean Well or equivalent
- Output voltage: 5 V DC
- Output current: at least 20 A
- Output power: at least 100 W
- Regulated DC output
- Suitable certifications and enclosure rating

The LEDs must not be powered through the ESP32-S3 board.

The ESP32-S3 may be powered from the same regulated 5 V supply through a supported 5 V input. The matrix, ESP32-S3, level shifter, and power supply must share a common ground.

### 3.3 Brightness and Power Limiting

The firmware shall enforce a global brightness limit.

Recommended initial setting:

```text
Maximum physical brightness: 25-30%
```

The limit may be increased only after verifying:

- Total input current
- Voltage at all matrix injection points
- Power-supply temperature
- Cable and connector temperatures
- Enclosure temperature
- LED temperature
- Stable ESP32-S3 operation

The browser may display a 0-100% brightness slider, but 100% in the interface shall correspond to the configured safe firmware maximum.

The firmware safety limit must also apply to WebSocket and REST commands.

## 4. Power Distribution

A 16 x 16 matrix shall not be powered from only one corner.

Recommended minimum injection points:

- Top-left
- Top-right
- Bottom-left
- Bottom-right

An additional center injection point may be added if voltage-drop measurements require it.

```text
5 V distribution block
    |
    +-- Fuse -- Top-left injection
    +-- Fuse -- Top-right injection
    +-- Fuse -- Bottom-left injection
    +-- Fuse -- Bottom-right injection
```

The entire LED current must not pass through:

- ESP32-S3 headers
- Breadboard contacts
- Thin jumper wires
- The connector of the first LED
- Undersized PCB traces

Recommended protective components:

- Main fuse after the 5 V power supply
- Individually fused branches where appropriate
- Main power switch
- 1,000-2,200 uF electrolytic capacitor near the matrix
- 100 nF local decoupling where required
- 300-500 ohm resistor in the LED data line
- High-current-rated terminals or connectors
- Correctly sized wiring
- Cable glands and strain relief

Fuse values, wire cross-sections, connectors, and terminals must be selected for the expected current and installation conditions.

## 5. Logic-Level Conversion

The ESP32-S3 uses 3.3 V GPIO logic, while the 5 V LED matrix may require a 5 V data signal.

Recommended level-shifter components:

- 74AHCT125
- 74HCT125
- 74AHCT245
- Comparable HCT or AHCT buffer

Recommended signal path:

```text
ESP32-S3 GPIO
      |
      v
3.3 V-to-5 V level shifter
      |
      v
300-500 ohm series resistor
      |
      v
First LED data input
```

The level shifter should be mounted close to the first LED. Slow bidirectional MOSFET level-shifter boards intended for I2C should not be used unless their suitability has been verified.

## 6. Enclosure Requirements

The enclosure shall provide space for:

- ESP32-S3 development board
- Logic-level shifter
- Mean Well 5 V power supply
- Main and optional branch fuses
- Power-distribution terminals
- Main power switch
- Cable glands and strain relief
- Ventilation openings
- Programming access
- Status indicators
- QR-code label

The enclosure shall provide:

- Mechanical protection
- Protection against accidental contact
- Safe separation of mains and low-voltage wiring
- Covered mains terminals
- Adequate airflow
- Secure cable entry
- Service access requiring a tool

If the selected power supply exposes mains terminals, those terminals must not be accessible during normal operation. Mains wiring shall only be installed by a suitably qualified person.

## 7. Network Specification

### 7.1 Normal Wi-Fi Operation

The ESP32-S3 shall operate in station mode and connect to an existing 2.4 GHz Wi-Fi network.

Required functions:

- Store Wi-Fi settings in nonvolatile memory
- Connect automatically after startup
- Reconnect automatically after interruption
- Report connection status and IP address
- Preserve the current LED image during network loss
- Enter configuration mode if no valid network is available

Wi-Fi credentials must not be committed to the repository.

### 7.2 Fixed IP Address

The preferred method is a DHCP reservation in the router using the ESP32-S3 MAC address.

Advantages:

- Prevents accidental address conflicts
- Keeps network administration centralized
- Allows the ESP32-S3 to remain a DHCP client
- Keeps the QR-code URL stable

Example local URL:

```text
http://192.168.1.50/
```

The actual address must match the local network.

Manual static IP configuration may also be supported with:

- IP address
- Subnet mask
- Default gateway
- Primary DNS server
- Secondary DNS server

### 7.3 Device Name and mDNS

Default device name:

```text
led-matrix
```

Optional mDNS address:

```text
http://led-matrix.local/
```

The QR code should use the fixed IP address because mDNS support may vary between networks and client devices.

## 8. QR-Code Access

The QR code shall contain the complete local URL, for example:

```text
http://192.168.1.50/
```

The QR code may be:

- Attached to the enclosure
- Printed on an instruction card
- Stored in the repository documentation
- Generated on a commissioning page

The phone must be connected to a network that can reach the ESP32-S3.

## 9. Wi-Fi Configuration Mode

If no valid Wi-Fi settings are available, or repeated connection attempts fail, the ESP32-S3 shall start a temporary configuration access point.

The configuration page shall support:

- Wi-Fi SSID
- Wi-Fi password
- DHCP or static IP selection
- Static IP address
- Subnet mask
- Gateway
- DNS server
- Device name

Recommended workflow:

1. Start a password-protected temporary access point.
2. Connect from a phone or computer.
3. Open the local configuration page.
4. Select or enter the target Wi-Fi network.
5. Enter the password and optional network settings.
6. Save the configuration.
7. Restart the ESP32-S3.
8. Connect to the configured network.
9. Disable the temporary access point.

A hardware button may be used to enter configuration mode or reset network settings. A long press should be required to avoid accidental activation.

## 10. Browser Interface

### 10.1 General Requirements

The interface shall:

- Run on smartphones, tablets, and desktop computers
- Support touch and mouse input
- Work in portrait and landscape orientation
- Adapt to different display sizes
- Be hosted entirely by the ESP32-S3
- Work without internet access
- Require no external JavaScript libraries or CDN
- Require no mobile app installation

### 10.2 Drawing Grid

The page shall display a responsive 16 x 16 grid with 256 square cells.

Required functions:

- Tap or click one pixel
- Draw by dragging the mouse
- Draw by moving a finger across the grid
- Erase individual pixels
- Prevent unwanted page scrolling while drawing
- Display the current framebuffer colors
- Preserve the physical matrix orientation

### 10.3 Color Selection

The interface shall provide:

- Standard browser color picker
- Current-color preview
- Hexadecimal color value
- Preset color buttons
- Black/off selection
- Optional recent-color history

Suggested presets:

- Red
- Green
- Blue
- Yellow
- Orange
- Purple
- Cyan
- White
- Black/off

### 10.4 Drawing Commands

Required controls:

- Clear
- Fill
- Eraser
- Undo
- Redo
- Apply
- Live mode
- Save image
- Load image
- Brightness slider

Optional controls:

- Rotate clockwise or counterclockwise
- Mirror horizontally or vertically
- Shift left, right, up, or down

### 10.5 Update Modes

#### Live Mode

Every drawing action is transmitted immediately to the ESP32-S3. Live mode shall be the default.

#### Apply Mode

Changes are prepared in the browser and transmitted as a complete frame after pressing **Apply**.

The selected update mode shall always be visible.

## 11. Communication

### 11.1 WebSocket

WebSocket communication is preferred for live control.

Required functions:

- Initial state synchronization
- Individual pixel updates
- Complete-frame updates
- Brightness updates
- Clear and Fill commands
- Save and Load commands
- Connection-state reporting
- Automatic reconnect

### 11.2 Example Pixel Message

```json
{
  "type": "pixel",
  "x": 3,
  "y": 5,
  "r": 255,
  "g": 80,
  "b": 0
}
```

### 11.3 Example Brightness Message

```json
{
  "type": "brightness",
  "value": 30
}
```

### 11.4 Example Clear Message

```json
{
  "type": "clear"
}
```

### 11.5 Example Fill Message

```json
{
  "type": "fill",
  "r": 0,
  "g": 100,
  "b": 255
}
```

### 11.6 Complete-Frame Message

A complete frame contains 256 RGB values.

```json
{
  "type": "frame",
  "width": 16,
  "height": 16,
  "pixels": [
    [255, 0, 0],
    [0, 255, 0],
    [0, 0, 255]
  ]
}
```

The final protocol may use a compact binary format if required.

## 12. Optional REST API

Suggested endpoints:

```text
GET  /api/status
GET  /api/frame
GET  /api/config
POST /api/pixel
POST /api/frame
POST /api/clear
POST /api/fill
POST /api/brightness
POST /api/save
POST /api/load
```

Example status response:

```json
{
  "deviceName": "led-matrix",
  "width": 16,
  "height": 16,
  "pixelCount": 256,
  "brightness": 30,
  "wifiConnected": true,
  "ipAddress": "192.168.1.50",
  "firmwareVersion": "0.1.0"
}
```

## 13. Framebuffer and Input Validation

The internal framebuffer contains 256 RGB values.

```text
256 pixels x 3 bytes = 768 bytes
```

The framebuffer is the authoritative representation of the displayed image.

The firmware shall validate:

- X coordinate from 0 to 15
- Y coordinate from 0 to 15
- RGB values from 0 to 255
- Brightness inside the allowed range
- Correct matrix dimensions
- Correct complete-frame length
- Known command types
- Maximum message size
- Valid JSON or binary format

Invalid commands shall be rejected without changing the framebuffer.

## 14. Persistent Storage

The following data shall be stored in nonvolatile memory:

- Device name
- Wi-Fi settings
- DHCP or static IP mode
- Static network settings
- Matrix dimensions
- Matrix mapping settings
- Brightness setting
- Last saved frame
- User-saved images
- Configuration format version

The frame should not be written after every pixel change. Recommended behavior:

- Keep drawing changes in RAM.
- Save after a configurable inactivity period.
- Save when the user selects **Save**.
- Save before a controlled restart.
- Restore the last valid frame after startup.

## 15. Startup Sequence

1. Initialize logging and status indicators.
2. Load the saved configuration.
3. Validate the configuration.
4. Initialize the LED driver.
5. Keep the LEDs off or at low brightness.
6. Initialize the framebuffer.
7. Connect to Wi-Fi.
8. Start the web server.
9. Start WebSocket communication.
10. Restore the last saved frame.
11. Apply the firmware brightness limit.
12. Display the restored frame.
13. Indicate that the system is ready.

The matrix shall not produce an uncontrolled full-brightness flash during startup.

## 16. Failure and Recovery Behavior

### Wi-Fi Connection Loss

- Keep the current image displayed.
- Preserve the framebuffer.
- Attempt automatic reconnection.
- Indicate the disconnected state.
- Restore browser communication after reconnection.

### Browser Connection Loss

- Keep the current image displayed.
- Keep the web server running.
- Accept new browser connections.
- Synchronize the framebuffer after reconnecting.

### Invalid Network Configuration

- Keep the last valid image displayed.
- Retry the connection.
- Enter configuration mode after a defined timeout.
- Do not erase saved images automatically.

### Power Interruption

- Restart safely.
- Load the saved configuration.
- Reconnect to Wi-Fi.
- Restore the last saved image.
- Reapply the brightness limit.

### Corrupt Configuration

- Do not crash.
- Use safe default values.
- Keep the matrix off or at low brightness.
- Enter configuration mode.
- Log the error.

## 17. Software Architecture

Recommended modules:

```text
src/
|-- main.cpp
|-- LedMatrixController.cpp
|-- CoordinateMapper.cpp
|-- WifiManager.cpp
|-- WebServerManager.cpp
|-- WebSocketHandler.cpp
|-- ConfigurationManager.cpp
|-- FrameStorage.cpp
|-- PowerLimiter.cpp
`-- StatusIndicator.cpp
```

Recommended headers:

```text
include/
|-- ProjectConfig.h
|-- PinConfig.h
|-- MatrixConfig.h
|-- LedMatrixController.h
|-- CoordinateMapper.h
|-- WifiManager.h
|-- WebServerManager.h
|-- WebSocketHandler.h
|-- ConfigurationManager.h
|-- FrameStorage.h
|-- PowerLimiter.h
`-- StatusIndicator.h
```

Web interface:

```text
data/
|-- index.html
|-- style.css
|-- app.js
`-- icons/
```

## 18. Development Environment

Recommended environment:

- Visual Studio Code
- PlatformIO
- Arduino framework for ESP32

Alternative:

- ESP-IDF

Suggested libraries:

- FastLED or Adafruit NeoPixel
- ESPAsyncWebServer or another maintained web-server library
- ArduinoJson, if JSON messages are used
- LittleFS for hosted web files

Only one LED-control library should be used in the final implementation.

## 19. Repository Structure

```text
esp32-s3-led-matrix/
|-- README.md
|-- LICENSE
|-- platformio.ini
|-- .gitignore
|-- src/
|   |-- main.cpp
|   |-- LedMatrixController.cpp
|   |-- CoordinateMapper.cpp
|   |-- WifiManager.cpp
|   |-- WebServerManager.cpp
|   |-- WebSocketHandler.cpp
|   |-- ConfigurationManager.cpp
|   |-- FrameStorage.cpp
|   |-- PowerLimiter.cpp
|   `-- StatusIndicator.cpp
|-- include/
|   |-- ProjectConfig.h
|   |-- PinConfig.h
|   |-- MatrixConfig.h
|   |-- LedMatrixController.h
|   |-- CoordinateMapper.h
|   |-- WifiManager.h
|   |-- WebServerManager.h
|   |-- WebSocketHandler.h
|   |-- ConfigurationManager.h
|   |-- FrameStorage.h
|   |-- PowerLimiter.h
|   `-- StatusIndicator.h
|-- data/
|   |-- index.html
|   |-- style.css
|   |-- app.js
|   `-- icons/
|-- docs/
|   |-- hardware.md
|   |-- wiring.md
|   |-- network-setup.md
|   |-- software-architecture.md
|   |-- websocket-protocol.md
|   |-- commissioning.md
|   |-- qr-code.png
|   `-- images/
`-- test/
    |-- test_coordinate_mapping/
    |-- test_message_validation/
    `-- test_frame_storage/
```

## 20. Example Project Configuration

```cpp
#pragma once

#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define PIXEL_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

#define LED_DATA_PIN 4

#define MATRIX_SERPENTINE true
#define MATRIX_MIRROR_X false
#define MATRIX_MIRROR_Y false
#define MATRIX_ROTATION 0

#define DEFAULT_BRIGHTNESS_PERCENT 20
#define MAXIMUM_BRIGHTNESS_PERCENT 30

#define DEVICE_NAME "led-matrix"
```

The data GPIO must be verified for the selected board.

## 21. Wi-Fi Credentials

Wi-Fi credentials must not be committed to Git.

Example local file:

```cpp
#pragma once

#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"
```

Example `.gitignore`:

```gitignore
include/secrets.h
.pio/
.vscode/
.DS_Store
```

A credential-free template may be committed as:

```text
include/secrets.example.h
```

## 22. Status Indication

Optional indicators:

- Power LED
- Wi-Fi status LED
- Error LED
- Configuration button
- Reset button

Suggested behavior:

- Slow blinking: startup
- Fast blinking: configuration mode
- Solid green: connected and ready
- Yellow: reconnecting
- Red: configuration, storage, or internal error

## 23. Security

The device is intended for operation on a trusted local network.

Minimum requirements:

- Do not commit Wi-Fi credentials.
- Password-protect configuration mode.
- Disable the temporary access point after configuration.
- Validate all commands.
- Limit message sizes.
- Reject malformed messages.
- Protect reset and firmware-update functions.
- Change default passwords during commissioning.
- Do not expose the ESP32-S3 directly to the internet.
- Do not create internet-facing router port forwarding.

Optional improvements:

- Web-interface authentication
- Administrator login
- Session authentication
- Authenticated OTA updates
- Rate limiting
- Separate user and administrator permissions

## 24. Performance Targets

- Web interface available within 15 seconds after Wi-Fi connection
- Normal individual-pixel response below 200 ms
- Smooth mouse and touch drawing
- Stable full-frame updates
- No visible flicker
- No ESP32-S3 reset during full-frame changes
- Automatic Wi-Fi reconnection
- Automatic browser reconnection
- Stable continuous operation for at least 24 hours

## 25. Development Plan

### Stage 1: Basic LED Control

- Test one RGB LED.
- Verify the data GPIO.
- Verify the level shifter.
- Test red, green, blue, and white.
- Connect the full matrix.
- Implement coordinate mapping.
- Verify all 256 pixels.

### Stage 2: Power System

- Connect the Mean Well supply.
- Install fuses and the main switch.
- Add the bulk capacitor.
- Add power injection.
- Measure current and voltage drop.
- Establish a safe brightness limit.

### Stage 3: Wi-Fi

- Connect to the router.
- Display the assigned IP address.
- Test reconnection.
- Configure a DHCP reservation.
- Verify the address after multiple power cycles.

### Stage 4: Basic Web Server

- Serve a status page.
- Display device name, IP address, firmware version, matrix size, and Wi-Fi status.

### Stage 5: Drawing Interface

- Create the 16 x 16 browser grid.
- Add color selection.
- Add touch and mouse drawing.
- Add Clear, Fill, Eraser, and brightness controls.

### Stage 6: Live Communication

- Add WebSocket communication.
- Synchronize the framebuffer.
- Add pixel and frame updates.
- Add automatic reconnect.
- Display connection status.

### Stage 7: Storage

- Store configuration and brightness.
- Save and restore frames.
- Add saved image slots.
- Add storage-version validation.

### Stage 8: Enclosure and Commissioning

- Install all components.
- Add strain relief and terminal protection.
- Attach the QR code.
- Perform electrical and thermal tests.
- Complete a 24-hour stability test.
- Document the final wiring.
- Create the first release.

## 26. Test Requirements

### Hardware Tests

- Verify 5 V before connecting electronics.
- Verify polarity and common ground.
- Verify level-shifter output.
- Verify LED data direction.
- Test the first LED.
- Test every pixel individually.
- Display complete red, green, blue, and limited-white frames.
- Measure total current.
- Measure voltage at all corners and the center.
- Check cable, connector, power-supply, enclosure, and LED temperatures.
- Check for flicker and repeated power-cycle stability.

### Software Tests

- Initial Wi-Fi configuration
- DHCP and DHCP reservation
- Static IP configuration
- Invalid network settings
- Wi-Fi and browser reconnection
- QR-code access
- Mobile portrait and landscape layouts
- Desktop layout
- Pixel, drag, and touch drawing
- Color picker and Eraser
- Clear, Fill, Undo, and Redo
- Brightness control and safety-limit enforcement
- Live and Apply modes
- Complete-frame transfer
- Save and restore
- Invalid and oversized message handling
- Serpentine mapping, rotation, and mirroring
- Configuration reset

## 27. Acceptance Criteria

The project is accepted when:

1. All 256 LEDs are individually addressable.
2. The browser displays a matching 16 x 16 grid.
3. Each browser cell controls the correct physical LED.
4. Browser and matrix orientations match.
5. Mouse and touch drawing work reliably.
6. Color selection, Eraser, Clear, Fill, Undo, and Redo work.
7. Live and Apply modes work correctly.
8. Brightness control works and the firmware limit cannot be bypassed.
9. The ESP32-S3 connects and reconnects automatically.
10. The reserved IP address remains stable.
11. Scanning the QR code opens the interface.
12. The saved image is restored after restart.
13. Invalid messages do not crash the controller.
14. No visible flicker or brownout reset occurs.
15. Voltage remains acceptable across the entire matrix.
16. Wiring, connectors, power supply, LEDs, and enclosure remain within safe temperatures.
17. No mains-voltage terminals are accessible during normal use.
18. The system completes a 24-hour continuous stability test.

## 28. Future Extensions

- Animated patterns
- Scrolling text
- Image or GIF import
- Multiple image slots
- User color palettes
- Timed scenes
- Ambient-light-based brightness
- MQTT and Home Assistant integration
- Authenticated OTA firmware updates
- User and administrator accounts
- Image import and export
- Additional matrix panels
- Larger matrix sizes
- Multiple drawing layers
- Physical control buttons

## 29. Safety Notice

This project may involve exposed AC mains voltage when an industrial power supply with screw terminals is used.

Required precautions include:

- Covered mains terminals
- Correct protective-earth connection where required
- Suitable mains fuse and switch
- Correctly rated wiring
- Wire ferrules
- Cable glands and strain relief
- Physical separation of mains and 5 V wiring
- Enclosure access requiring a tool

Mains-voltage wiring shall only be performed by a suitably qualified person.

## 30. Project Status

```text
Planning and hardware specification
```

Next steps:

- Select the exact ESP32-S3 board.
- Select the exact 16 x 16 matrix.
- Select the exact Mean Well 5 V / 20 A power supply.
- Define the final LED data GPIO.
- Create the wiring diagram.
- Test one LED and then the full matrix at limited brightness.
- Create the initial PlatformIO project.

## License

Select an appropriate license before publishing the repository, for example:

- MIT License
- Apache License 2.0
- GNU General Public License v3.0
- Private or proprietary license

