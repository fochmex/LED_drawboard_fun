#pragma once

// -----------------------------------------------------------------------------
// LED matrix geometry
// -----------------------------------------------------------------------------

// Number of LEDs in the horizontal direction.
#define MATRIX_WIDTH 16

// Number of LEDs in the vertical direction.
#define MATRIX_HEIGHT 16

// Total number of addressable LEDs in the matrix.
// For a 16 x 16 matrix: 16 * 16 = 256 LEDs.
#define PIXEL_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

// -----------------------------------------------------------------------------
// Power-supply and LED current assumptions
// -----------------------------------------------------------------------------

// Mean Well LPV-100-5 power supply ratings used for the safety calculation:
// nominal output voltage = 5 V, maximum output current = 12 A.
#define SUPPLY_VOLTAGE_V 5
#define SUPPLY_MAX_CURRENT_MA 12000

// Conservative maximum-current assumption for one WS2812B LED at full white.
// Full white activates the red, green, and blue channels simultaneously.
#define LED_FULL_WHITE_CURRENT_MA 60

// Theoretical current if every LED is set to full white at maximum brightness.
// For 256 LEDs: 256 * 60 mA = 15,360 mA = 15.36 A.
// This is higher than the power supply's 12 A maximum rating.
#define THEORETICAL_FULL_WHITE_CURRENT_MA \
    (PIXEL_COUNT * LED_FULL_WHITE_CURRENT_MA)

// -----------------------------------------------------------------------------
// Software brightness limits
// -----------------------------------------------------------------------------

// The electrical ceiling is 78%; the owner's lower operating cap is 70/255.
// This is a software safeguard, not a replacement for suitable wiring, fusing,
// power injection, cooling, and electrical protection.
#define SAFE_BRIGHTNESS_LIMIT_PERCENT 78

// FastLED brightness uses an 8-bit value from 0 to 255.
#define MAX_BRIGHTNESS_VALUE 255

// Keep the operating cap below the independently calculated electrical ceiling.
#define POWER_BRIGHTNESS_CEILING \
    ((MAX_BRIGHTNESS_VALUE * SAFE_BRIGHTNESS_LIMIT_PERCENT) / 100U)
#define MAX_ALLOWED_BRIGHTNESS 70
static_assert(MAX_ALLOWED_BRIGHTNESS <= POWER_BRIGHTNESS_CEILING,
              "Operating brightness must stay within the electrical ceiling");

// Initial global brightness after startup.
// 15 of 255 is approximately 5.9%.
#define STARTUP_BRIGHTNESS 15

// Recommended maximum brightness during the Stage 1 hardware tests.
// 64 of 255 is approximately 25%.
#define MAX_TEST_BRIGHTNESS 64

// -----------------------------------------------------------------------------
// Effects and power-management timing
// -----------------------------------------------------------------------------

// Render at up to 50 frames per second; change effects every 20 seconds.
#define EFFECT_FRAME_INTERVAL_MS 20UL
#define EFFECT_DURATION_MS 20000UL

// Enter deep sleep after one hour without registered LED activity.
// UL forces unsigned long arithmetic, which prevents 16-bit integer overflow
// on Arduino-compatible targets when calculating milliseconds.
#define IDLE_SLEEP_TIMEOUT_MS (60UL * 60UL * 1000UL)

// -----------------------------------------------------------------------------
// Local Wi-Fi networking
// -----------------------------------------------------------------------------

#define WIFI_HOSTNAME "led-drawboard"
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#define WIFI_RETRY_INITIAL_MS 5000UL
#define WIFI_RETRY_MAX_MS 60000UL
