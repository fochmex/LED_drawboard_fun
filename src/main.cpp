#include <Arduino.h>
#include <FastLED.h>

#include "ProjectConfig.h"
#include "PinConfig.h"
#include "MatrixConfig.h"

namespace
{
    CRGB leds[PIXEL_COUNT];
    uint8_t currentTest = 0;
    uint32_t lastTransitionMs = 0;

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
        FastLED.clear(true);
    }

    void printStageHeader(const char* label)
    {
        Serial.println();
        Serial.println(label);
        Serial.printf("Matrix: %ux%u | Pixels: %u | Data pin: %d\n",
                      MATRIX_WIDTH,
                      MATRIX_HEIGHT,
                      PIXEL_COUNT,
                      LED_DATA_PIN);
    }

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

        clearMatrix();
        FastLED.show();
    }

    void runMatrixSweepTest()
    {
        printStageHeader("[Stage 1B] Full matrix sweep");

        for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
            {
                const uint8_t r = (x * 255U) / (MATRIX_WIDTH - 1U);
                const uint8_t g = (y * 255U) / (MATRIX_HEIGHT - 1U);
                const uint8_t b = 255U - ((x + y) * 4U);
                setPixel(x, y, CRGB(r, g, b));
            }
        }

        FastLED.show();
        delay(STAGE1_TEST_DELAY_MS * 2U);
        clearMatrix();
        FastLED.show();
    }

    void runCoordinateMappingTest()
    {
        printStageHeader("[Stage 1C] Coordinate mapping verification");

        for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
            {
                const bool oddCell = ((x + y) % 2U) == 1U;
                setPixel(x, y, oddCell ? CRGB::Cyan : CRGB::Magenta);
            }
        }

        FastLED.show();
        delay(STAGE1_TEST_DELAY_MS * 2U);
        clearMatrix();
        FastLED.show();
    }

    void printStartupSummary()
    {
        Serial.println("========================================");
        Serial.println("Stage 1: LED matrix hardware validation");
        Serial.println("========================================");
        Serial.printf("Board: ESP32-S3\n");
        Serial.printf("Matrix size: %ux%u\n", MATRIX_WIDTH, MATRIX_HEIGHT);
        Serial.printf("Pixel count: %u\n", PIXEL_COUNT);
        Serial.printf("Startup brightness: %u\n", STARTUP_BRIGHTNESS);
        Serial.printf("Data pin: %d\n", LED_DATA_PIN);
        Serial.printf("Status pin: %d\n", LED_STATUS_PIN);
        Serial.printf("Wiring mode: %s\n",
                      MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine ? "serpentine" : "straight");
        Serial.println("Validation sequence: single LED -> matrix sweep -> coordinate check");
        Serial.println("========================================");
    }

    void runCurrentTest()
    {
        switch (currentTest)
        {
            case 0:
                runSingleLedColorTest();
                break;
            case 1:
                runMatrixSweepTest();
                break;
            case 2:
                runCoordinateMappingTest();
                break;
            default:
                clearMatrix();
                FastLED.show();
                break;
        }
    }
}

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

void loop()
{
    static const uint32_t kStepDelayMs = 2500UL;

    if ((millis() - lastTransitionMs) > kStepDelayMs)
    {
        runCurrentTest();
        currentTest = (currentTest + 1U) % 3U;
        lastTransitionMs = millis();
    }
}

]} ,