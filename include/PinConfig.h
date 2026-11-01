#include <Arduino.h>
#include <FastLED.h>
#include <cstring>

#include "ProjectConfig.h"
#include "PinConfig.h"
#include "MatrixConfig.h"

namespace
{
    CRGB leds[PIXEL_COUNT];
    uint8_t currentTest = 0;
    uint32_t lastTransitionMs = 0;
    uint32_t lastActivityMs = 0;

    static RTC_DATA_ATTR CRGB rtcLastFrame[PIXEL_COUNT];
    static RTC_DATA_ATTR bool rtcFrameValid = false;

    void markLedActivity()
    {
        lastActivityMs = millis();
    }

    void clearMatrix()
    {
        FastLED.clear(true);
        markLedActivity();
    }

    void saveCurrentFrameToRtc()
    {
        std::memcpy(rtcLastFrame, leds, sizeof(leds));
        rtcFrameValid = true;
    }

    void restoreLastFrameFromRtc()
    {
        if (!rtcFrameValid)
        {
            clearMatrix();
            FastLED.show();
            return;
        }

        std::memcpy(leds, rtcLastFrame, sizeof(leds));
        FastLED.show();
        markLedActivity();
    }

    uint8_t clampBrightness(uint8_t requestedBrightness)
    {
        if (requestedBrightness > MAX_ALLOWED_BRIGHTNESS)
        {
            return MAX_ALLOWED_BRIGHTNESS;
        }
        return requestedBrightness;
    }

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
        markLedActivity();
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
            markLedActivity();
            Serial.printf("Color: R=%u G=%u B=%u\n", color.r, color.g, color.b);
            delay(STAGE1_TEST_DELAY_MS);
        }

        clearMatrix();
        FastLED.show();
        markLedActivity();
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
        markLedActivity();
        delay(STAGE1_TEST_DELAY_MS * 2U);
        clearMatrix();
        FastLED.show();
        markLedActivity();
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
        markLedActivity();
        delay(STAGE1_TEST_DELAY_MS * 2U);
        clearMatrix();
        FastLED.show();
        markLedActivity();
    }

    void printStartupSummary()
    {
        Serial.println("========================================");
        Serial.println("Stage 1: LED matrix hardware validation");
        Serial.println("========================================");
        Serial.printf("Board: ESP32-S3\n");
        Serial.printf("Matrix size: %ux%u\n", MATRIX_WIDTH, MATRIX_HEIGHT);
        Serial.printf("Pixel count: %u\n", PIXEL_COUNT);
        Serial.printf("Startup brightness: %u\n", clampBrightness(STARTUP_BRIGHTNESS));
        Serial.printf("Max safe brightness: %u/%u (%u%%)\n",
                      MAX_ALLOWED_BRIGHTNESS,
                      MAX_BRIGHTNESS_VALUE,
                      SAFE_BRIGHTNESS_LIMIT_PERCENT);
        Serial.printf("Power source: Mean Well LPV-100-5, 5 V / 12 A max\n");
        Serial.printf("Data pin: %d\n", LED_DATA_PIN);
        Serial.printf("Status pin: %d\n", LED_STATUS_PIN);
        Serial.printf("Wake button pin: %d (active %s, 5 s hold to sleep)\n",
                      WAKE_BUTTON_PIN,
                      WAKE_BUTTON_ACTIVE_LEVEL == LOW ? "LOW" : "HIGH");
        Serial.printf("Idle shutdown timeout: %lu ms (%.1f minutes)\n",
                      IDLE_SLEEP_TIMEOUT_MS,
                      static_cast<float>(IDLE_SLEEP_TIMEOUT_MS) / (60.0F * 1000.0F));
        Serial.printf("Wiring mode: %s\n",
                      MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine ? "serpentine" : "straight");
        Serial.println("Validation sequence: single LED -> matrix sweep -> coordinate check");
        Serial.println("Wake/sleep: long-press button for 5 s triggers deep sleep and state restore");
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
                markLedActivity();
                break;
        }
    }

    bool isWakeButtonPressed()
    {
        return digitalRead(WAKE_BUTTON_PIN) == WAKE_BUTTON_ACTIVE_LEVEL;
    }

    void enterDeepSleep()
    {
        Serial.println("Button hold detected: entering deep sleep.");
        saveCurrentFrameToRtc();
        clearMatrix();
        FastLED.show();
        digitalWrite(LED_STATUS_PIN, LOW);

        esp_err_t err = esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(WAKE_BUTTON_PIN), WAKE_BUTTON_ACTIVE_LEVEL);
        if (err != ESP_OK)
        {
            Serial.printf("Failed to configure wake button: %d\n", err);
        }

        esp_deep_sleep_start();
    }

    void handleWakeButtonSleepInput()
    {
        static bool buttonWasPressed = false;
        static uint32_t buttonPressStartMs = 0;

        const bool buttonPressed = isWakeButtonPressed();

        if (buttonPressed && !buttonWasPressed)
        {
            buttonPressStartMs = millis();
        }

        if (!buttonPressed && buttonWasPressed)
        {
            buttonPressStartMs = 0;
        }

        buttonWasPressed = buttonPressed;

        if (buttonPressed && (millis() - buttonPressStartMs) >= WAKE_BUTTON_LONG_PRESS_MS)
        {
            enterDeepSleep();
        }
    }

    void handleIdleSleepTimeout()
    {
        if ((millis() - lastActivityMs) >= IDLE_SLEEP_TIMEOUT_MS)
        {
            Serial.printf("Idle timeout reached (%lu ms): entering deep sleep.\n", IDLE_SLEEP_TIMEOUT_MS);
            enterDeepSleep();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    pinMode(LED_STATUS_PIN, OUTPUT);
    pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_STATUS_PIN, HIGH);

    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, PIXEL_COUNT);
    FastLED.setBrightness(clampBrightness(STARTUP_BRIGHTNESS));
    FastLED.setCorrection(TypicalLEDStrip);
    clearMatrix();
    FastLED.show();

    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    if (wakeCause == ESP_SLEEP_WAKEUP_EXT0)
    {
        Serial.println("Wake reason: external wake button");
        restoreLastFrameFromRtc();
    }
    else
    {
        Serial.println("Wake reason: normal boot / power cycle");
    }

    printStartupSummary();
    lastTransitionMs = millis();
    lastActivityMs = millis();
}

void loop()
{
    static const uint32_t kStepDelayMs = 2500UL;

    handleWakeButtonSleepInput();

    if ((millis() - lastTransitionMs) > kStepDelayMs)
    {
        runCurrentTest();
        currentTest = (currentTest + 1U) % 3U;
        lastTransitionMs = millis();
    }

    handleIdleSleepTimeout();
}

