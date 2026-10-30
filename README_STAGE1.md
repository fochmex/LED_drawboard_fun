#include <Arduino.h>
#include <FastLED.h>

#include "ProjectConfig.h"
#include "PinConfig.h"
#include "MatrixConfig.h"

CRGB leds[PIXEL_COUNT];

uint16_t xyToIndex(uint8_t x, uint8_t y)
{
    uint16_t index = 0;

    if (MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine)
    {
        if (y % 2 == 0)
        {
            index = (uint16_t)y * MATRIX_WIDTH + x;
        }
        else
        {
            index = (uint16_t)y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
        }
    }
    else
    {
        index = (uint16_t)y * MATRIX_WIDTH + x;
    }

    if (MATRIX_ROTATION == RotationMode::k90 || MATRIX_ROTATION == RotationMode::k270)
    {
        // Rotation is handled centrally for stage 1 by keeping the mapping simple and predictable.
        // The full implementation can swap from logical X/Y to physical row/column transforms later.
    }

    if (MATRIX_MIRROR_X)
    {
        x = (uint8_t)(MATRIX_WIDTH - 1 - x);
    }

    if (MATRIX_MIRROR_Y)
    {
        y = (uint8_t)(MATRIX_HEIGHT - 1 - y);
    }

    if (MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine && y % 2 == 0)
    {
        return (uint16_t)y * MATRIX_WIDTH + x;
    }

    if (MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine)
    {
        return (uint16_t)y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
    }

    return (uint16_t)y * MATRIX_WIDTH + x;
}

void setPixel(uint8_t x, uint8_t y, const CRGB& color)
{
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
    {
        return;
    }

    leds[xyToIndex(x, y)] = color;
}

void clearMatrix()
{
    fill_solid(leds, PIXEL_COUNT, CRGB::Black);
}

void runSingleLedTest()
{
    clearMatrix();
    setPixel(0, 0, CRGB::Red);
    FastLED.show();
    delay(STAGE1_TEST_DELAY_MS);

    setPixel(0, 0, CRGB::Green);
    FastLED.show();
    delay(STAGE1_TEST_DELAY_MS);

    setPixel(0, 0, CRGB::Blue);
    FastLED.show();
    delay(STAGE1_TEST_DELAY_MS);

    setPixel(0, 0, CRGB::White);
    FastLED.show();
    delay(STAGE1_TEST_DELAY_MS);

    clearMatrix();
    FastLED.show();
}

void runFullMatrixSweep()
{
    clearMatrix();

    for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
    {
        for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
        {
            uint8_t r = (x * 255) / (MATRIX_WIDTH - 1);
            uint8_t g = (y * 255) / (MATRIX_HEIGHT - 1);
            uint8_t b = 255 - ((x + y) * 4);
            setPixel(x, y, CRGB(r, g, b));
        }
    }

    FastLED.show();
    delay(STAGE1_TEST_DELAY_MS * 2);

    clearMatrix();
    FastLED.show();
}

void verifyPixelCoordinates()
{
    clearMatrix();

    for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
    {
        for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
        {
            CRGB color = (x + y) % 2 == 0 ? CRGB::Blue : CRGB::Red;
            setPixel(x, y, color);
        }
    }

    FastLED.show();
    delay(STAGE1_TEST_DELAY_MS * 2);

    clearMatrix();
    FastLED.show();
}

void printMatrixSummary()
{
    Serial.println("Stage 1 baseline configuration");
    Serial.printf("Matrix size: %d x %d (%d pixels)\n", MATRIX_WIDTH, MATRIX_HEIGHT, PIXEL_COUNT);
    Serial.printf("Data pin: %d\n", LED_DATA_PIN);
    Serial.printf("Matrix wiring: %s\n", MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine ? "serpentine" : "straight");
    Serial.println("Recommended validation sequence:");
    Serial.println("  1. Single LED RGBW smoke test");
    Serial.println("  2. Full matrix sweep for all pixels");
    Serial.println("  3. Coordinate verification and routing check");
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    pinMode(LED_STATUS_PIN, OUTPUT);
    digitalWrite(LED_STATUS_PIN, HIGH);

    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, PIXEL_COUNT);
    FastLED.setBrightness(STARTUP_BRIGHTNESS);
    clearMatrix();
    FastLED.show();

    printMatrixSummary();
}

void loop()
{
    static uint8_t state = 0;

    switch (state)
    {
        case 0:
            Serial.println("Running single LED RGB test");
            runSingleLedTest();
            break;
        case 1:
            Serial.println("Running full matrix sweep");
            runFullMatrixSweep();
            break;
        case 2:
            Serial.println("Running coordinate verification");
            verifyPixelCoordinates();
            break;
        default:
            clearMatrix();
            FastLED.show();
            break;
    }

    state = (state + 1) % 3;
    delay(2000);
}
