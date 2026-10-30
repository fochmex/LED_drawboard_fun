#pragma once

#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16
#define PIXEL_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)

// Mean Well LPV-100-5 is a 5 V / 12 A supply (60 W max).
// A full-white 16x16 WS2812B matrix can theoretically draw:
// 256 LEDs * 60 mA = 15.36 A, which exceeds the 12 A supply limit.
// To keep the project within safe limits, enforce a hard brightness ceiling
// of roughly 78% of full-scale brightness in software.
#define SUPPLY_VOLTAGE_V 5
#define SUPPLY_MAX_CURRENT_MA 12000
#define LED_FULL_WHITE_CURRENT_MA 60
#define THEORETICAL_FULL_WHITE_CURRENT_MA (PIXEL_COUNT * LED_FULL_WHITE_CURRENT_MA)
#define SAFE_BRIGHTNESS_LIMIT_PERCENT 78
#define MAX_BRIGHTNESS_VALUE 255
#define MAX_ALLOWED_BRIGHTNESS ((MAX_BRIGHTNESS_VALUE * SAFE_BRIGHTNESS_LIMIT_PERCENT) / 100)

#define STARTUP_BRIGHTNESS 32
#define MAX_TEST_BRIGHTNESS 64
#define STAGE1_TEST_DELAY_MS 500
