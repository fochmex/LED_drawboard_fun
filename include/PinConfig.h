#pragma once

#include <Arduino.h>

#define LED_DATA_PIN 4
#define LED_STATUS_PIN 33

// Planned wake/sleep button hardware:
// - Use a normally-open momentary pushbutton from WAKE_BUTTON_PIN to GND.
// - Configure the pin with INPUT_PULLUP so the button reads HIGH when idle and LOW when pressed.
// - This means no external pull-up resistor is required in the default design.
// - If the internal pull-up is disabled, add a 10 kΩ pull-up to 3.3 V and treat the signal as active-low.
#define WAKE_BUTTON_PIN 14
#define WAKE_BUTTON_ACTIVE_LEVEL LOW
#define WAKE_BUTTON_DEBOUNCE_MS 25
#define WAKE_BUTTON_LONG_PRESS_MS 5000
