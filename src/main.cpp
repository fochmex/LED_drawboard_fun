#include <Arduino.h>
#include <FastLED.h>
#include <cstring>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "ProjectConfig.h"
#include "PinConfig.h"
#include "MatrixConfig.h"
#include "NetworkManager.h"
#include "EffectControl.h"
#include "WebDashboard.h"

namespace
{
    // LED framebuffer. FastLED sends this array to the physical matrix when
    // FastLED.show() is called.
    CRGB leds[PIXEL_COUNT];

    enum class Effect : uint8_t
    {
        kRainbow,
        kConfetti,
        kComet,
        kPlasma,
        kScanner,
        kColorWaves,
        kCount
    };

    const char* const effectNames[] = {
        "Rainbow", "Confetti", "Comet", "Plasma", "Bouncing scanner", "Ocean waves"};
    static_assert(sizeof(effectNames) / sizeof(effectNames[0]) ==
                  static_cast<uint8_t>(Effect::kCount), "Every effect needs a name");
    Effect currentEffect = Effect::kRainbow;
    uint32_t effectStartMs = 0;
    uint32_t lastEffectChangeMs = 0;
    uint32_t lastFrameMs = 0;
    bool autoPlaylist = true;

    // Updated whenever the LED content changes. Used for the automatic
    // idle-to-deep-sleep timeout.
    uint32_t lastActivityMs = 0;
    bool waitForWakeButtonRelease = false;

    // RTC memory survives ESP32 deep sleep. This allows the last displayed
    // LED frame to be restored after waking with the button.
    static RTC_DATA_ATTR CRGB rtcLastFrame[PIXEL_COUNT];
    static RTC_DATA_ATTR bool rtcFrameValid = false;
    static RTC_DATA_ATTR uint8_t rtcEffect = 0;
    static RTC_DATA_ATTR uint32_t rtcEffectElapsedMs = 0;
    static RTC_DATA_ATTR bool rtcAutoPlaylist = true;

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
        rtcEffect = static_cast<uint8_t>(currentEffect);
        rtcEffectElapsedMs = millis() - effectStartMs;
        rtcAutoPlaylist = autoPlaylist;
        rtcFrameValid = true;
    }

    void restoreLastFrameFromRtc()
    {
        // A normal power cycle does not guarantee valid RTC frame data.
        if (!rtcFrameValid || rtcEffect >= static_cast<uint8_t>(Effect::kCount))
        {
            clearMatrix();
            FastLED.show();
            return;
        }

        std::memcpy(leds, rtcLastFrame, sizeof(leds));
        currentEffect = static_cast<Effect>(rtcEffect);
        autoPlaylist = rtcAutoPlaylist;
        effectStartMs = millis() - rtcEffectElapsedMs;
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

    void selectEffect(uint8_t index)
    {
        currentEffect = static_cast<Effect>(index);
        effectStartMs = millis();
        lastEffectChangeMs = effectStartMs;
        FastLED.clear(false);
        Serial.printf("Effect: %s\n", effectNames[static_cast<uint8_t>(currentEffect)]);
    }

    void nextEffect()
    {
        selectEffect((static_cast<uint8_t>(currentEffect) + 1U) %
                     static_cast<uint8_t>(Effect::kCount));
    }

    void renderEffect(uint32_t nowMs)
    {
        const uint32_t elapsedMs = nowMs - effectStartMs;
        const uint8_t phase = static_cast<uint8_t>(elapsedMs / 20U);
        switch (currentEffect)
        {
            case Effect::kRainbow:
            {
                for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
                {
                    for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
                    {
                        setPixel(x, y, CHSV(phase + x * 8U + y * 8U, 255, 255));
                    }
                }
                break;
            }
            case Effect::kConfetti:
                fadeToBlackBy(leds, PIXEL_COUNT, 18);
                leds[random16(PIXEL_COUNT)] += CHSV(phase + random8(64), 200, 255);
                break;
            case Effect::kComet:
            {
                fadeToBlackBy(leds, PIXEL_COUNT, 35);
                const uint16_t position = (elapsedMs / EFFECT_FRAME_INTERVAL_MS) % PIXEL_COUNT;
                const uint8_t y = position / MATRIX_WIDTH;
                const uint8_t x = (y % 2U == 0U)
                    ? position % MATRIX_WIDTH
                    : MATRIX_WIDTH - 1U - position % MATRIX_WIDTH;
                setPixel(x, y, CHSV(phase, 230, 255));
                break;
            }
            case Effect::kPlasma:
            case Effect::kColorWaves:
                for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
                {
                    for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
                    {
                        const uint8_t wave = (static_cast<uint16_t>(sin8(x * 16U + phase)) +
                                              sin8(y * 16U - phase)) / 2U;
                        if (currentEffect == Effect::kPlasma)
                        {
                            setPixel(x, y, CHSV(wave + phase, 255, 255));
                        }
                        else
                        {
                            setPixel(x, y, CHSV(130U + scale8(wave, 40), 240, wave));
                        }
                    }
                }
                break;
            case Effect::kScanner:
            {
                fadeToBlackBy(leds, PIXEL_COUNT, 60);
                const uint32_t travel = (elapsedMs / 90U) % (2U * (MATRIX_WIDTH - 1U));
                const uint8_t x = travel < MATRIX_WIDTH ? travel : 2U * (MATRIX_WIDTH - 1U) - travel;
                for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
                {
                    setPixel(x, y, CHSV(phase + y * 6U, 255, 255));
                }
                break;
            }
            case Effect::kCount:
                Serial.println("Invalid effect index.");
                ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
                return;
        }
        FastLED.show();
        markLedActivity();
    }

    void printStartupSummary()
    {
        Serial.println("========================================");
        Serial.println("Stage 4: Local web dashboard and LED effects");
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
        Serial.printf("Effects: %u | Frame interval: %lu ms | Auto cycle: %lu ms\n",
                      static_cast<unsigned>(Effect::kCount), EFFECT_FRAME_INTERVAL_MS, EFFECT_DURATION_MS);
        Serial.printf("Effect: %s\n", effectNames[static_cast<uint8_t>(currentEffect)]);
        Serial.println("Short button press: next effect");
        Serial.printf("Automatic playlist: %s\n", autoPlaylist ? "enabled" : "disabled");
        Serial.println("Wake/sleep: hold for 5 seconds, then release to sleep; press again to wake");
        Serial.printf("Button raw level: %s (released=HIGH, pressed=LOW)\n",
                      digitalRead(WAKE_BUTTON_PIN) == LOW ? "LOW" : "HIGH");
        Serial.println("========================================");
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
        const gpio_num_t wakePin = static_cast<gpio_num_t>(WAKE_BUTTON_PIN);
        esp_err_t error = esp_sleep_enable_ext0_wakeup(wakePin, WAKE_BUTTON_ACTIVE_LEVEL);
        if (error == ESP_OK)
        {
            error = rtc_gpio_pullup_en(wakePin);
        }
        if (error == ESP_OK)
        {
            error = rtc_gpio_pulldown_dis(wakePin);
        }
        if (error != ESP_OK)
        {
            Serial.printf("Cannot configure button wake-up: %s. Staying awake.\n", esp_err_to_name(error));
            return;
        }

        // EXT0 is level-triggered: sleeping while pressed wakes immediately.
        Serial.println("Sleep requested. Release the button to enter deep sleep.");
        while (readDebouncedButtonState() || isWakeButtonPressed())
        {
            delay(1);
        }

        // Save the visible frame before clearing the LEDs. The frame can then
        // be restored after the ESP32 wakes from the external button signal.
        saveCurrentFrameToRtc();
        clearMatrix();
        FastLED.show();

        // Never reroute GPIO4 while the RMT peripheral is still transmitting.
        if (!FastLED.wait(1000))
        {
            Serial.println("LED transmission did not finish; staying awake instead of changing GPIO4.");
            return;
        }
        const gpio_num_t dataPin = static_cast<gpio_num_t>(LED_DATA_PIN);
        ESP_ERROR_CHECK(rtc_gpio_init(dataPin));
        ESP_ERROR_CHECK(rtc_gpio_set_level(dataPin, 0));
        ESP_ERROR_CHECK(rtc_gpio_set_direction(dataPin, RTC_GPIO_MODE_OUTPUT_ONLY));
        // The powered level shifter must see LOW, not a floating input in sleep.
        ESP_ERROR_CHECK(rtc_gpio_hold_en(dataPin));
        digitalWrite(LED_STATUS_PIN, LOW);

        WebDashboard::stop();
        NetworkManager::stop();
        Serial.println("Entering deep sleep. Press GPIO14 button to wake.");
        Serial.flush();
        esp_deep_sleep_start();
    }

    void handleWakeButtonSleepInput()
    {
        // Detect the start of a press and enter deep sleep only if the button
        // remains pressed for the configured long-press duration.
        static bool buttonWasPressed = false;
        static uint32_t buttonPressStartMs = 0;

        const bool buttonPressed = readDebouncedButtonState();
        if (waitForWakeButtonRelease)
        {
            if (!buttonPressed && !isWakeButtonPressed())
            {
                waitForWakeButtonRelease = false;
                Serial.println("Wake button released; sleep input ready.");
            }
            return;
        }

        if (buttonPressed && !buttonWasPressed)
        {
            buttonPressStartMs = millis();
            Serial.println("Button pressed. Hold for 5 seconds to request sleep.");
        }

        if (!buttonPressed && buttonWasPressed)
        {
            if ((millis() - buttonPressStartMs) < WAKE_BUTTON_LONG_PRESS_MS)
            {
                nextEffect();
            }
            buttonPressStartMs = 0;
            Serial.println("Button released.");
        }

        buttonWasPressed = buttonPressed;

        if (buttonPressed && ((millis() - buttonPressStartMs) >= WAKE_BUTTON_LONG_PRESS_MS))
        {
            // Also require a new press if configuring sleep fails.
            buttonWasPressed = false;
            waitForWakeButtonRelease = true;
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

uint8_t EffectControl::count()
{
    return static_cast<uint8_t>(Effect::kCount);
}

const char* EffectControl::name(uint8_t index)
{
    return index < count() ? effectNames[index] : nullptr;
}

uint8_t EffectControl::selected()
{
    return static_cast<uint8_t>(currentEffect);
}

bool EffectControl::playlistEnabled()
{
    return autoPlaylist;
}

bool EffectControl::select(uint8_t index)
{
    if (index >= count())
    {
        Serial.println("Cannot select effect: invalid index.");
        return false;
    }
    selectEffect(index);
    setPlaylistEnabled(false);
    return true;
}

void EffectControl::setPlaylistEnabled(bool enabled)
{
    autoPlaylist = enabled;
    lastEffectChangeMs = millis();
    Serial.printf("Automatic playlist: %s\n", enabled ? "enabled" : "disabled");
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    if (wakeCause == ESP_SLEEP_WAKEUP_EXT0)
    {
        // EXT0 leaves the wake pin in RTC mode until explicitly released.
        const esp_err_t error = rtc_gpio_deinit(static_cast<gpio_num_t>(WAKE_BUTTON_PIN));
        if (error != ESP_OK)
        {
            Serial.printf("Cannot release wake GPIO from RTC mode: %s\n", esp_err_to_name(error));
        }
        waitForWakeButtonRelease = true;
    }

    // GPIO 33: status output. GPIO 14: active-low wake/sleep button.
    pinMode(LED_STATUS_PIN, OUTPUT);
    pinMode(WAKE_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(LED_STATUS_PIN, HIGH);

    // Determine whether this is a normal boot or a wake from deep sleep.
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
    const gpio_num_t dataPin = static_cast<gpio_num_t>(LED_DATA_PIN);
    ESP_ERROR_CHECK(rtc_gpio_hold_dis(dataPin));
    ESP_ERROR_CHECK(rtc_gpio_deinit(dataPin));
    pinMode(LED_DATA_PIN, OUTPUT);
    digitalWrite(LED_DATA_PIN, LOW);
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, PIXEL_COUNT);
    FastLED.setBrightness(clampBrightness(STARTUP_BRIGHTNESS));
    FastLED.setCorrection(TypicalLEDStrip);
    effectStartMs = millis();

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
    lastEffectChangeMs = millis();
    lastFrameMs = millis();
    lastActivityMs = millis();
    WebDashboard::begin();
    NetworkManager::begin();
}

void loop()
{
    handleWakeButtonSleepInput();
    NetworkManager::update();
    WebDashboard::update();

    uint32_t nowMs = millis();
    if (autoPlaylist && (nowMs - lastEffectChangeMs) >= EFFECT_DURATION_MS)
    {
        nextEffect();
        nowMs = millis();
    }

    if ((nowMs - lastFrameMs) >= EFFECT_FRAME_INTERVAL_MS)
    {
        lastFrameMs = nowMs;
        renderEffect(nowMs);
    }

    handleIdleSleepTimeout();
    delay(1);
}
