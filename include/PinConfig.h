#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// LED matrix connections
// -----------------------------------------------------------------------------

// GPIO 4 sends the one-wire data signal to the WS2812B LED matrix.
// Recommended signal path:
// ESP32 GPIO 4 -> level shifter -> 300-500 ohm resistor -> matrix DIN.
// Deep sleep holds this RTC-capable pin LOW to prevent spurious LED data.
#define LED_DATA_PIN 4

// GPIO 33 controls the separate status LED/output used by the application.
// The program sets this output HIGH while running and LOW before deep sleep.
// Note: this is not necessarily the built-in RGB LED of the ESP32-S3 board.
#define LED_STATUS_PIN 33

// -----------------------------------------------------------------------------
// Wake/sleep pushbutton
// -----------------------------------------------------------------------------

// GPIO 14 is connected to a normally-open momentary pushbutton.
// Connect the button between GPIO 14 and GND.
// main.cpp configures this pin as INPUT_PULLUP, therefore:
//   - Button released: GPIO reads HIGH
//   - Button pressed:  GPIO reads LOW
// No external pull-up resistor is required while INPUT_PULLUP is enabled.
// Deep sleep uses the RTC pull-up on this RTC-capable ESP32-S3 pin.
// If the internal pull-up is disabled, add an external 10 kohm pull-up to 3.3 V.
#define WAKE_BUTTON_PIN 14

// The button is active-low because pressing it connects GPIO 14 to GND.
#define WAKE_BUTTON_ACTIVE_LEVEL LOW

// A state change must remain stable for 25 ms before it is accepted.
// This suppresses false transitions caused by mechanical contact bounce.
#define WAKE_BUTTON_DEBOUNCE_MS 25

// Short press: next effect. Hold for 5000 ms, then release to sleep.
// Press again to wake; the wake press does not skip an effect.
#define WAKE_BUTTON_LONG_PRESS_MS 5000
