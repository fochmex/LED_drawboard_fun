#include <Arduino.h>
#include <FastLED.h>
#include <cstring>

#include "ProjectConfig.h"
#include "PinConfig.h"
#include "MatrixConfig.h"

namespace
{
    // LED framebuffer. FastLED sends this array to the physical matrix when
    // FastLED.show() is called.
    CRGB leds[PIXEL_COUNT];

    // Index of the next hardware validation test: RGB, sweep, or mapping.
    uint8_t currentTest = 0;
    uint32_t lastTransitionMs = 0;

    // Updated whenever the LED content changes. Used for the automatic
    // idle-to-deep-sleep timeout.
    uint32_t lastActivityMs = 0;

    // RTC memory survives ESP32 deep sleep. This allows the last displayed
    // LED frame to be restored after waking with the button.
    static RTC_DATA_ATTR CRGB rtcLastFrame[PIXEL_COUNT];
    static RTC_DATA_ATTR bool rtcFrameValid = false;

    void markLedActivity()
    {
        lastActivityMs = millis();
    }

    void clearMatrix()
    {
        // The 'true' argument immediately transmits the cleared framebuffer.
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
        // A normal power cycle does not guarantee valid RTC frame data.
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
        // Enforce the configured safe limit so software cannot accidentally
        // request more current than intended for the LED power supply/wiring.
        if (requestedBrightness > MAX_ALLOWED_BRIGHTNESS)
        {
            return MAX_ALLOWED_BRIGHTNESS;
        }
        return requestedBrightness;
    }

    uint16_t xyToIndex(uint8_t x, uint8_t y)
    {
        // Invalid coordinates are rejected by setPixel(). Returning zero here
        // provides a safe fallback for any other future caller.
        if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
        {
            return 0;
        }

        // In serpentine matrices, every second row runs in the opposite
        // direction. Convert logical X/Y coordinates to the physical LED index.
        if (MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine)
        {
            const uint16_t rowBase = static_cast<uint16_t>(y) * MATRIX_WIDTH;
            if ((y % 2U) == 0U)
            {
                return rowBase + x;
            }
            return rowBase + (MATRIX_WIDTH - 1U - x);
        }

        // Straight wiring: all rows run from left to right.
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
        // Verify data communication and RGB/GRB color order using LED (0,0).
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
        // Fill the complete matrix with a position-dependent color gradient.
        // This helps identify dead pixels, interrupted data paths, and wiring issues.
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
        // A checkerboard pattern makes swapped rows, reversed rows, or an
        // incorrect serpentine configuration easy to recognize visually.
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
        Serial.printf("Startup brightness: %u\n", clampBrightness(STARTUP_BRIGHTNESS));
        Serial.printf("Max safe brightness: %u/%u (%u%%)\n",
                      MAX_ALLOWED_BRIGHTNESS,
                      MAX_BRIGHTNESS_VALUE,
                      SAFE_BRIGHTNESS_LIMIT_PERCENT);
        Serial.printf("Power source: Mean Well LPV-100-5, 5 V / 12 A max\n");
        Serial.printf("Data pin: %d\n", LED_DATA_PIN);
        Serial.printf("Status pin: %d\n", LED_STATUS_PIN);
        Serial.printf("Wake button pin: %d\n", WAKE_BUTTON_PIN);
        Serial.printf("Button active level: %s\n", WAKE_BUTTON_ACTIVE_LEVEL == LOW ? "LOW" : "HIGH");
        Serial.printf("Long press threshold: %u ms\n", WAKE_BUTTON_LONG_PRESS_MS);
        Serial.printf("Idle shutdown timeout: %lu ms\n", IDLE_SLEEP_TIMEOUT_MS);
        Serial.printf("Wiring mode: %s\n",
                      MATRIX_WIRING_MODE == MatrixWiringMode::kSerpentine ? "serpentine" : "straight");
        Serial.println("Validation sequence: single LED -> matrix sweep -> coordinate check");
        Serial.println("Wake/sleep: long-press button enters deep sleep and restores the last LED frame on wake");
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

    bool isWakeButtonPressed()
    {
        // PinConfig defines the electrical active level. With INPUT_PULLUP,
        // the usual configuration is a button between the GPIO and GND.
        return digitalRead(WAKE_BUTTON_PIN) == WAKE_BUTTON_ACTIVE_LEVEL;
    }

    bool readDebouncedButtonState()
    {
        // Accept a changed input only after it has remained unchanged for the
        // configured debounce period. This filters mechanical contact bounce.
        static bool stableState = false;
        static bool lastRawState = false;
        static uint32_t lastRawChangeMs = 0;

        const bool rawState = isWakeButtonPressed();
        if (rawState != lastRawState)
        {
            lastRawState = rawState;
            lastRawChangeMs = millis();
        }

        if ((millis() - lastRawChangeMs) >= WAKE_BUTTON_DEBOUNCE_MS)
        {
            stableState = rawState;
        }

        return stableState;
    }

    void enterDeepSleep()
    {
        Serial.println("Long press detected: entering deep sleep...");

        // Save the visible frame before clearing the LEDs. The frame can then
        // be restored after the ESP32 wakes from the external button signal.
        saveCurrentFrameToRtc();
        clearMatrix();
        FastLED.show();
        digitalWrite(LED_STATUS_PIN, LOW);

        // EXT0 wake-up monitors one RTC-capable GPIO for the configured level.
        esp_sleep_enable_ext0_wakeup(
            static_cast<gpio_num_t>(WAKE_BUTTON_PIN),
            WAKE_BUTTON_ACTIVE_LEVEL);
        esp_deep_sleep_start();
    }

    void handleWakeButtonSleepInput()
    {
        // Detect the start of a press and enter deep sleep only if the button
        // remains pressed for the configured long-press duration.
        static bool buttonWasPressed = false;
        static uint32_t buttonPressStartMs = 0;

        const bool buttonPressed = readDebouncedButtonState();
        if (buttonPressed && !buttonWasPressed)
        {
            buttonPressStartMs = millis();
        }

        if (!buttonPressed && buttonWasPressed)
        {
            buttonPressStartMs = 0;
        }

        buttonWasPressed = buttonPressed;

        if (buttonPressed && ((millis() - buttonPressStartMs) >= WAKE_BUTTON_LONG_PRESS_MS))
        {
            enterDeepSleep();
        }
    }

    void handleIdleSleepTimeout()
    {
        // Automatically power down after no LED activity for the configured time.
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

    // GPIO 33: status output. GPIO 14: active-low wake/sleep button.
    pinMode(LED_STATUS_PIN, OUTPUT);
    pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_STATUS_PIN, HIGH);

    // Determine whether this is a normal boot or a wake from deep sleep.
    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    if (wakeCause == ESP_SLEEP_WAKEUP_EXT0)
    {
        Serial.println("Wake reason: external wake button");
    }
    else
    {
        Serial.println("Wake reason: normal boot / power cycle");
    }

    // GPIO 4 carries the one-wire data signal to the LED matrix.
    // GRB defines the byte order expected by WS2812B LEDs.
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, PIXEL_COUNT);
    FastLED.setBrightness(clampBrightness(STARTUP_BRIGHTNESS));
    FastLED.setCorrection(TypicalLEDStrip);

    // Restore the previous image only after a button wake. Start with a blank
    // matrix after a normal boot or power cycle.
    if (wakeCause == ESP_SLEEP_WAKEUP_EXT0)
    {
        restoreLastFrameFromRtc();
    }
    else
    {
        clearMatrix();
        FastLED.show();
    }

    printStartupSummary();
    lastTransitionMs = millis();
    lastActivityMs = millis();
}

void loop()
{
    // Run the three hardware tests cyclically every 2.5 seconds.
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

