
# Bill of Materials (BOM)

## Core Electronics

| Qty | Item | Notes |
|------|------|-------|
| 1 | ESP32-S3 Development Board | Main controller |
| 1 | 16×16 WS2812B LED Matrix (256 LEDs) | Main display |
| 1 | SN74AHCT125N | 3.3 V to 5 V level shifter |
| 1 | Mean Well LRS-100-5 | 5 V / 18 A power supply |
| 1 | Momentary Push Button | Wake/Sleep button on GPIO 14 |

## Passive Components

| Qty | Item | Notes |
|------|------|-------|
| 1 | 330 Ω Resistor | Series resistor on LED data line |
| 1 | 1000 µF Capacitor (≥ 6.3 V) | Across matrix power input |
| 1 | 0.1 µF Capacitor | SN74AHCT125N decoupling capacitor |

## Wiring

| Qty | Item | Notes |
|------|------|-------|
| As required | 16 AWG Wire | Main power feed |
| As required | 16-18 AWG Wire | Power injection |
| As required | 22-24 AWG Wire | Data and logic signals |

## Connectors and Assembly

| Qty | Item | Notes |
|------|------|-------|
| As required | WAGO Connectors or Terminal Blocks | Power distribution |
| 1 | Enclosure | Houses electronics |
| As required | Heat Shrink Tubing | Wire insulation |
| As required | Mounting Hardware | Screws, standoffs, spacers |

---

# Wiring Connections

## Power Supply

### Mean Well LRS-100-5

| Power Supply Terminal | Connects To |
|----------------------|-------------|
| +V | Matrix 5V Input |
| +V | Matrix 5V Power Injection |
| +V | ESP32 VIN / 5V |
| +V | SN74AHCT125N VCC |
| -V | Matrix GND |
| -V | Matrix GND Injection |
| -V | ESP32 GND |
| -V | SN74AHCT125N GND |

---

## Level Shifter (SN74AHCT125N)

| SN74AHCT125N Pin | Connects To |
|------------------|-------------|
| VCC | 5V |
| GND | GND |
| 1OE | GND (always enabled) |
| 1A | ESP32 Data GPIO |
| 1Y | 330 Ω resistor -> Matrix DIN |

Add a **0.1 µF capacitor** between VCC and GND near the SN74AHCT125N.

---

## LED Matrix

| Matrix Connection | Connects To |
|-------------------|-------------|
| 5V Input | Power Supply +5V |
| GND Input | Power Supply GND |
| DIN | SN74AHCT125N Output via 330 Ω resistor |
| DOUT | Optional connection to another matrix |
| 5V Injection | Power Supply +5V |
| GND Injection | Power Supply GND |

Add a **1000 µF capacitor** between 5V and GND at the matrix power input.

---

## ESP32-S3

| ESP32 Pin | Connects To |
|------------|-------------|
| VIN / 5V | Power Supply +5V |
| GND | Common Ground |
| Data GPIO | SN74AHCT125N 1A |
| GPIO 14 | Wake/Sleep Button |
| GND | Wake/Sleep Button |

---

## Wake/Sleep Button

| Button Terminal | Connects To |
|-----------------|-------------|
| Terminal 1 | GPIO 14 |
| Terminal 2 | GND |

Configure GPIO 14 with the internal pull-up resistor and as an RTC wake source.

---

# Power Distribution Diagram

```text
Mean Well LRS-100-5
        +5V
         |
         +-----> ESP32-S3 VIN
         |
         +-----> SN74AHCT125N VCC
         |
         +-----> Matrix 5V Input
         |
         +-----> Matrix 5V Injection

        GND
         |
         +-----> ESP32-S3 GND
         |
         +-----> SN74AHCT125N GND
         |
         +-----> Matrix GND Input
         |
         +-----> Matrix GND Injection

ESP32 GPIO
      |
      v
 SN74AHCT125N
      |
    330Ω
      |
      v
 Matrix DIN

GPIO14
   |
Button
   |
  GND
```

---

# Wake and Sleep Behavior

This Stage 1 build includes basic power-saving functionality through a dedicated wake/sleep button and automatic idle shutdown.

- A momentary push button connected to **GPIO 14** serves as the wake/sleep control.
- While the LED matrix is active, holding the button for **5 seconds** clears the display and places the ESP32-S3 into **deep sleep**.
- The same button acts as the wake source, allowing the device to resume operation from deep sleep.
- On wake, the firmware restores the previously displayed LED frame from **RTC memory** and continues using the last active configuration.
- If the display remains unchanged for more than **60 minutes**, the firmware automatically enters deep sleep using the same shutdown and wake mechanism.
- To keep Stage 1 focused and lightweight, only the currently displayed LED frame is stored in RTC memory. This enables restoration of the last visible pattern without introducing a broader persistence or state-management layer.

