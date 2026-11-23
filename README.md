# LED Drawboard

A local, browser-controlled **16×16 RGB LED display** built with an ESP32-S3. Draw on the board, select animated effects, adjust brightness, display clocks, and control the panel from any device on your local network.

[![PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/framework-Arduino-blue)](https://www.arduino.cc/)
[![LED library](https://img.shields.io/badge/library-FastLED-red)](https://fastled.io/)

## What It Can Do

- Draw or erase pixels using a mouse or touchscreen
- Choose from nine animated LED effects
- Use an automatic effect playlist
- Adjust brightness within a safe limit
- Display analog or binary clocks using network time
- Synchronize the panel and settings across multiple browser sessions
- Retain the active display through deep sleep

## Features

### Drawing Studio

- 16×16 drawing grid
- 1×1, 2×2, and 3×3 brush sizes
- Brush and eraser modes
- Clear, fill, and apply controls
- Mouse, touch, and keyboard support
- Live updates through WebSockets

### Animated Effects

The project includes:

- Rainbow
- Confetti
- Comet
- Plasma
- Bouncing scanner
- Ocean waves
- Fireworks
- Color-changing fish
- Walking cat

The effects change automatically every 20 seconds when the playlist is enabled.

### Clock Displays

After connecting to Wi-Fi and synchronizing network time, the board can display:

- **Analog clock** — hour and minute hands with a moving second indicator
- **Binary clock** — 24-hour time represented in colored columns

Clock displays use Europe/Berlin time, including CET/CEST daylight-saving changes.

> Binary clock mode is implemented but should be verified on the physical hardware.

## Quick Start

### Requirements

- ESP32-S3 development board
- 16×16 WS2812B/SK6812-compatible LED matrix
- PlatformIO
- 2.4 GHz Wi-Fi network
- Suitable 5 V power supply
- 74AHCT125 logic-level shifter
- Resistor, capacitor, wiring, and fuse

See [ListOfMaterials.md](ListOfMaterials.md) for the complete component and wiring overview.

### 1. Configure Wi-Fi

Copy:

```text
include/secrets.example.h
```

to:

```text
include/secrets.h
```

Enter the network credentials:

```cpp
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
```

This local `secrets.h` file is ignored by Git.

### 2. Build and Upload

Open the repository in PlatformIO and upload it to the ESP32-S3.

```bash
pio run --target upload
```

Open the serial monitor at **115200 baud**. After connecting to Wi-Fi, the board reports its local IP address.

### 3. Open the Dashboard

On a device connected to the same local network, open:

```text
http://<ESP32-IP-ADDRESS>/
```

For example:

```text
http://192.168.178.152/
```

Use the IP address printed in the serial monitor if it has changed.

> The dashboard uses HTTP and has no authentication or HTTPS. Keep the board on a trusted network and do not expose it directly to the internet.

## Dashboard Controls

- **Play selected effect** — keep one effect active
- **Automatic playlist** — advance every 20 seconds
- **Apply drawing** — send the complete drawing to the panel
- **Set brightness** — adjust the LED brightness
- **Show clock** — select a clock after time synchronization
- **Clear draft** — clear the browser drawing
- **Fill with color** — fill the browser grid

## Physical Button

Connect a normally-open momentary button between **GPIO14** and **GND**.

- **Short press** — advance to the next effect
- **Hold for 5 seconds, then release** — enter deep sleep
- **Press while asleep** — wake the board

The display can also enter deep sleep after 60 minutes of static activity.

## Hardware and Wiring

| Function | Connection |
|---|---|
| LED data | ESP32-S3 GPIO4 → level shifter → resistor → matrix DIN |
| Matrix power | 5 V supply → matrix 5 V input |
| Power injection | 5 V and ground to the matrix at suitable points |
| Common ground | ESP32, level shifter, and matrix |
| Wake/sleep button | GPIO14 → momentary button → GND |
| Status indicator | GPIO33 |

The project uses an **SN74AHCT125N** or equivalent 74AHCT-family logic-level shifter to convert the ESP32-S3's 3.3 V output to a 5 V-compatible LED data signal.

## Safety

- Never power the matrix from an ESP32 GPIO or board header.
- Use a suitably rated, fused 5 V power supply.
- Connect all grounds together.
- Place a 300–500 Ω resistor in the LED data line.
- Keep the data line short.
- Add a bulk capacitor close to the matrix power input.
- Start at low brightness and verify the wiring before increasing output.

The firmware starts at **15/255 brightness** and limits output to **70/255**.

A 256-pixel WS2812B matrix may theoretically draw up to approximately **15.36 A** at full-white brightness. This exceeds the documented 5 V / 12 A supply rating. Correct wiring and the firmware brightness limit are essential.

## Project Status

| Stage | Status |
|---|---|
| 1. LED control | Completed |
| 2. Power system | Completed |
| 3. Wi-Fi networking | Completed |
| 4. Web dashboard | Completed |
| 5. Drawing and brightness | Completed |
| 6. Live WebSocket updates | Completed |
| 6.5. Clock displays | Implemented |
| 7. Persistent storage | Planned |
| 8. Enclosure and commissioning | Planned |

## Repository Structure

| Path | Purpose |
|---|---|
| `src/main.cpp` | Main firmware, effects, button handling, and state |
| `src/NetworkManager.cpp` | Wi-Fi connection and reconnection |
| `src/WebDashboard.cpp` | HTTP and WebSocket communication |
| `include/DashboardPage.h` | Browser dashboard interface |
| `include/ProjectConfig.h` | Dimensions and timing settings |
| `include/PinConfig.h` | GPIO assignments |
| `include/MatrixConfig.h` | Matrix layout configuration |
| `include/secrets.example.h` | Wi-Fi credentials template |
| `platformio.ini` | PlatformIO build configuration |
| `ListOfMaterials.md` | Parts and wiring summary |
| `README_STAGE1.md` | Effects and button documentation |
| `PROJECT_STAGES.md` | Detailed project stages and tests |

## Development

### Built With

- PlatformIO
- ESP32 Arduino framework
- FastLED
- Arduino WebSockets library
- HTML, CSS, and JavaScript

### Documentation

- [Materials and wiring](ListOfMaterials.md)
- [LED effects and controls](README_STAGE1.md)
- [Project stages and tests](PROJECT_STAGES.md)
- [Electrical diagram](LED_Drawboard_V1.pdf)

## Known Limitations

- The dashboard has no authentication or HTTPS.
- Drawings and settings may not survive a power cycle.
- The HTTP server is designed for a small, trusted local network.
- Binary clock hardware verification is pending.
- No formal release package or license is currently provided.

## License

No license is currently specified. Until a license is added, reuse, modification, and redistribution of the repository contents are not automatically granted.

## Contributing

Bug reports and improvement suggestions are welcome. Please include:

- Board and matrix details
- Firmware version
- Serial monitor output
- Steps needed to reproduce the issue