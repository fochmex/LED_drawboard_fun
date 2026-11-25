#include <Arduino.h>
#include <FastLED.h>
#include <cstring>
#include <math.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <time.h>

#include "ProjectConfig.h"
#include "PinConfig.h"
#include "MatrixConfig.h"
#include "NetworkManager.h"
#include "EffectControl.h"
#include "ClockControl.h"
#include "DisplayControl.h"
#include "WebDashboard.h"
#include "TetrisControl.h"
#include "ScrollingTextControl.h"

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
        kFireworks,
        kFish,
        kCat,
        kCount
    };

    const char* const effectNames[] = {
        "Rainbow", "Confetti", "Comet", "Plasma", "Bouncing scanner", "Ocean waves",
        "Fireworks", "Color-changing fish", "Walking cat"};
    static_assert(sizeof(effectNames) / sizeof(effectNames[0]) ==
                  static_cast<uint8_t>(Effect::kCount), "Every effect needs a name");
    Effect currentEffect = Effect::kRainbow;
    uint32_t effectStartMs = 0;
    uint32_t lastEffectChangeMs = 0;
    uint32_t lastFrameMs = 0;
    bool autoPlaylist = true;
    bool drawingMode = false;
    bool clockMode = false;
    ClockControl::Style clockStyle = ClockControl::Style::kAnalog;
    int lastClockSecond = -1;

    // Updated whenever the LED content changes. Used for the automatic
    // idle-to-deep-sleep timeout.
    uint32_t lastActivityMs = 0;
    bool waitForWakeButtonRelease = false;

    // RTC memory survives ESP32 deep sleep. This allows the last displayed
    // LED frame to be restored after waking with the button.
    // CRGB's startup constructor clears retained pixels; RTC storage must be raw bytes.
    static RTC_DATA_ATTR uint8_t rtcLastFrame[sizeof(leds)];
    static RTC_DATA_ATTR bool rtcFrameValid = false;
    static RTC_DATA_ATTR uint8_t rtcEffect = 0;
    static RTC_DATA_ATTR uint32_t rtcEffectElapsedMs = 0;
    static RTC_DATA_ATTR bool rtcAutoPlaylist = true;
    static RTC_DATA_ATTR bool rtcDrawingMode = false;
    static RTC_DATA_ATTR bool rtcClockMode = false;
    static RTC_DATA_ATTR bool rtcTetrisMode = false;
    static RTC_DATA_ATTR uint8_t rtcClockStyle = 0;
    static RTC_DATA_ATTR uint8_t rtcBrightness = STARTUP_BRIGHTNESS;

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
        rtcDrawingMode = drawingMode;
        rtcClockMode = clockMode;
        rtcTetrisMode = TetrisControl::active();
        rtcClockStyle = static_cast<uint8_t>(clockStyle);
        rtcBrightness = FastLED.getBrightness();
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
        drawingMode = rtcDrawingMode;
        clockMode = rtcClockMode;
        clockStyle = rtcClockStyle <= static_cast<uint8_t>(ClockControl::Style::kBinary)
                         ? static_cast<ClockControl::Style>(rtcClockStyle)
                         : ClockControl::Style::kAnalog;
        if (rtcTetrisMode)
        {
            TetrisControl::start();
            drawingMode = false;
            clockMode = false;
            autoPlaylist = false;
        }
        lastClockSecond = -1;
        FastLED.setBrightness(rtcBrightness > MAX_ALLOWED_BRIGHTNESS
                                  ? MAX_ALLOWED_BRIGHTNESS : rtcBrightness);
        effectStartMs = millis() - rtcEffectElapsedMs;
        FastLED.show();
        uint16_t coloredPixels = 0;
        for (const CRGB& pixel : leds)
        {
            if (pixel.r != 0 || pixel.g != 0 || pixel.b != 0) ++coloredPixels;
        }
        Serial.printf("Restored RTC display: %s | brightness: %u | colored pixels: %u/%u\n",
                      drawingMode ? "drawing" : "effect", FastLED.getBrightness(),
                      coloredPixels, PIXEL_COUNT);
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

        if (MATRIX_MIRROR_X) x = MATRIX_WIDTH - 1U - x;
        if (MATRIX_MIRROR_Y) y = MATRIX_HEIGHT - 1U - y;

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
        TetrisControl::stop();
        ScrollingTextControl::stop();
        drawingMode = false;
        clockMode = false;
        currentEffect = static_cast<Effect>(index);
        effectStartMs = millis();
        lastEffectChangeMs = effectStartMs;
        FastLED.clear(false);
        Serial.printf("Effect: %s\n", effectNames[static_cast<uint8_t>(currentEffect)]);
        WebDashboard::broadcastStatus();
        WebDashboard::broadcastFrame();
    }

    void nextEffect()
    {
        selectEffect((static_cast<uint8_t>(currentEffect) + 1U) %
                     static_cast<uint8_t>(Effect::kCount));
    }

    uint32_t effectRandom(uint32_t value)
    {
        // Repeatable waypoints/bursts let elapsed-time restoration also restore motion.
        value ^= value >> 16;
        value *= 0x7feb352dU;
        value ^= value >> 15;
        value *= 0x846ca68bU;
        return value ^ (value >> 16);
    }

    void plotEffectPixel(int16_t x, int16_t y, const CRGB& color)
    {
        if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT)
        {
            setPixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y), color);
        }
    }

    void renderFireworks(uint32_t elapsedMs)
    {
        FastLED.clear(false);
        const uint32_t burst = elapsedMs / 2800U;
        const uint32_t age = elapsedMs % 2800U;
        const uint32_t seed = effectRandom(burst + 137U);
        const int16_t centerX = 4 + seed % (MATRIX_WIDTH - 8U);
        const int16_t centerY = 3 + (seed >> 8) % (MATRIX_HEIGHT - 8U);
        const uint8_t hue = static_cast<uint8_t>(seed >> 16);
        if (age < 700U)
        {
            const int16_t y = MATRIX_HEIGHT - 1 -
                static_cast<int32_t>(MATRIX_HEIGHT - 1 - centerY) * age / 700;
            plotEffectPixel(centerX, y, CHSV(hue, 100, 255));
            plotEffectPixel(centerX, y + 1, CHSV(hue, 255, 100));
        }
        else if (age < 2300U)
        {
            const uint32_t expansion = age - 700U;
            const uint8_t value = 255U - expansion * 230U / 1600U;
            for (uint8_t spark = 0; spark < 16; ++spark)
            {
                const uint8_t angle = spark * 16U + static_cast<uint8_t>(seed);
                const int32_t radius = expansion * (4U + (effectRandom(seed + spark) % 4U));
                const int16_t x = centerX +
                    (static_cast<int32_t>(cos8(angle)) - 128) * radius / 128000;
                const int16_t y = centerY +
                    (static_cast<int32_t>(sin8(angle)) - 128) * radius / 128000 +
                    static_cast<int32_t>(expansion * expansion / 1600000U);
                plotEffectPixel(x, y, CHSV(hue + spark * 3U, 230, value));
            }
        }
    }

    void renderFish(uint32_t elapsedMs)
    {
        FastLED.clear(false);
        const uint32_t segment = elapsedMs / 1800U;
        const uint32_t progress = elapsedMs % 1800U;
        const uint32_t from = effectRandom(segment + 401U);
        const uint32_t to = effectRandom(segment + 402U);
        const int16_t fromX = 3 + from % (MATRIX_WIDTH - 6U);
        const int16_t fromY = 2 + (from >> 8) % (MATRIX_HEIGHT - 4U);
        const int16_t toX = 3 + to % (MATRIX_WIDTH - 6U);
        const int16_t toY = 2 + (to >> 8) % (MATRIX_HEIGHT - 4U);
        const int16_t centerX = fromX + (toX - fromX) * static_cast<int32_t>(progress) / 1800;
        const int16_t centerY = fromY + (toY - fromY) * static_cast<int32_t>(progress) / 1800;
        const bool right = toX >= fromX;
        const uint8_t hue = static_cast<uint8_t>(elapsedMs / 100U);
        static const char* const sprite[] = {
            "....#..", "..####.", ".######", "..####.", "....#.."};
        for (int16_t y = 0; y < 5; ++y)
        {
            for (int16_t x = 0; x < 7; ++x)
            {
                const bool tail = x == 0 &&
                    ((elapsedMs / 180U) % 2U == 0U ? (y == 1 || y == 3) : y == 2);
                if (sprite[y][x] != '#' && !tail) continue;
                const CRGB color = x == 5 && y == 2
                    ? CRGB::White : CRGB(CHSV(hue, 240, 220));
                plotEffectPixel(centerX + (right ? x - 3 : 3 - x), centerY + y - 2, color);
            }
        }
    }

    void renderCat(uint32_t elapsedMs)
    {
        FastLED.clear(false);
        const uint32_t segment = elapsedMs / 2200U;
        const uint32_t progress = elapsedMs % 2200U;
        const uint32_t from = effectRandom(segment + 701U);
        const uint32_t to = effectRandom(segment + 702U);
        const int16_t fromX = from % (MATRIX_WIDTH - 8U);
        const int16_t fromY = (from >> 8) % (MATRIX_HEIGHT - 6U);
        const int16_t toX = to % (MATRIX_WIDTH - 8U);
        const int16_t toY = (to >> 8) % (MATRIX_HEIGHT - 6U);
        const int16_t left = fromX + (toX - fromX) * static_cast<int32_t>(progress) / 2200;
        const int16_t top = fromY + (toY - fromY) * static_cast<int32_t>(progress) / 2200;
        const bool right = toX >= fromX;
        const bool walking = progress < 1900U;
        const bool alternateStep = (elapsedMs / 180U) % 2U != 0U;
        const CRGB fur = CHSV(static_cast<uint8_t>(24U + (elapsedMs / 160U) % 18U), 210, 230);
        static const char* const sprite[] = {
            "......#.#",
            "......###",
            ".....####",
            "..######.",
            ".########",
            ".........",
            "........."};

        for (int16_t y = 0; y < 7; ++y)
        {
            for (int16_t x = 0; x < 9; ++x)
            {
                bool lit = sprite[y][x] == '#';
                if (y == 5)
                {
                    lit = walking
                        ? (x == (alternateStep ? 2 : 3) || x == (alternateStep ? 6 : 7))
                        : (x == 2 || x == 6);
                }
                else if (y == 6)
                {
                    lit = walking
                        ? (x == (alternateStep ? 1 : 2) || x == (alternateStep ? 5 : 6))
                        : (x == 1 || x == 2 || x == 5 || x == 6);
                }
                // Keep the tail attached at its base while its tip swishes.
                if ((x == 1 && y == 3) ||
                    (walking && ((alternateStep && x == 0 && y == 3) ||
                                 (!alternateStep && x <= 1 && y == 2))))
                {
                    lit = true;
                }
                if (!lit) continue;

                int16_t pixelX = right ? left + x : left + 8 - x;
                int16_t pixelY = top + y;
                CRGB color = fur;
                if (y == 0 && (x == 6 || x == 8))
                {
                    color = CRGB(255, 190, 210);
                }
                else if (x == 8 && y == 2)
                {
                    color = CRGB::White;
                }
                plotEffectPixel(pixelX, pixelY, color);
            }
        }
    }

    void drawClockLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, const CRGB& color)
    {
        const int16_t dx = abs(x1 - x0);
        const int16_t sx = x0 < x1 ? 1 : -1;
        const int16_t dy = -abs(y1 - y0);
        const int16_t sy = y0 < y1 ? 1 : -1;
        int16_t error = dx + dy;
        while (true)
        {
            plotEffectPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            const int16_t twiceError = 2 * error;
            if (twiceError >= dy)
            {
                error += dy;
                x0 += sx;
            }
            if (twiceError <= dx)
            {
                error += dx;
                y0 += sy;
            }
        }
    }

    void drawClockHand(int16_t centerX, int16_t centerY, float angle,
                       float length, const CRGB& color)
    {
        const int16_t endX = static_cast<int16_t>(lroundf(centerX + sinf(angle) * length));
        const int16_t endY = static_cast<int16_t>(lroundf(centerY - cosf(angle) * length));
        drawClockLine(centerX, centerY, endX, endY, color);
    }

    void renderClock(const struct tm& localTime)
    {
        if (clockStyle == ClockControl::Style::kBinary)
        {
            FastLED.clear(false);
            const uint8_t values[] = {
                static_cast<uint8_t>(localTime.tm_hour),
                static_cast<uint8_t>(localTime.tm_min),
                static_cast<uint8_t>(localTime.tm_sec)};
            const int16_t columnX[] = {1, 7, 13};
            const CRGB columnColors[] = {
                CRGB(255, 125, 45), CRGB(40, 190, 255), CRGB(220, 70, 190)};

            for (uint8_t column = 0; column < 3; ++column)
            {
                for (uint8_t bit = 0; bit < 6; ++bit)
                {
                    const bool isSet = (values[column] & (1U << (5U - bit))) != 0;
                    CRGB markerColor = columnColors[column];
                    markerColor.nscale8_video(isSet ? 255 : 24);
                    const int16_t y = 2 + bit * 2;
                    plotEffectPixel(columnX[column], y, markerColor);
                    plotEffectPixel(columnX[column] + 1, y, markerColor);
                }
            }

            FastLED.show();
            markLedActivity();
            return;
        }

        FastLED.clear(false);
        const int16_t centerX = MATRIX_WIDTH / 2 - 1;
        const int16_t centerY = MATRIX_HEIGHT / 2 - 1;
        const float pi = 3.14159265F;
        const float twoPi = 2.0F * pi;

        for (int16_t hour = 0; hour < 12; ++hour)
        {
            const float angle = hour * twoPi / 12.0F;
            const int16_t x = static_cast<int16_t>(lroundf(centerX + sinf(angle) * 6.0F));
            const int16_t y = static_cast<int16_t>(lroundf(centerY - cosf(angle) * 6.0F));
            plotEffectPixel(x, y, hour % 3 == 0 ? CRGB(255, 170, 45) : CRGB(70, 95, 120));
        }

        const float minuteAngle = (localTime.tm_min + localTime.tm_sec / 60.0F) * twoPi / 60.0F;
        const float hourAngle = ((localTime.tm_hour % 12) + localTime.tm_min / 60.0F) * twoPi / 12.0F;
        const float secondAngle = localTime.tm_sec * twoPi / 60.0F;
        drawClockHand(centerX, centerY, minuteAngle, 5.0F, CRGB(210, 230, 255));
        drawClockHand(centerX, centerY, hourAngle, 3.5F, CRGB(255, 125, 45));

        const int16_t secondX = static_cast<int16_t>(lroundf(centerX + sinf(secondAngle) * 6.0F));
        const int16_t secondY = static_cast<int16_t>(lroundf(centerY - cosf(secondAngle) * 6.0F));
        plotEffectPixel(secondX, secondY, CRGB(40, 220, 180));
        plotEffectPixel(centerX, centerY, CRGB(255, 255, 255));

        FastLED.show();
        markLedActivity();
    }

    void renderTetris()
    {
        FastLED.clear(false);
        for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
        {
            for (uint8_t x = 0; x < 10U; ++x)
            {
                const uint32_t packed = TetrisControl::boardColor(x, y);
                if (packed != 0)
                {
                    setPixel(x, y, CRGB(static_cast<uint8_t>((packed >> 16) & 0xff),
                                        static_cast<uint8_t>((packed >> 8) & 0xff),
                                        static_cast<uint8_t>(packed & 0xff)));
                }
            }
        }

        // Next-piece preview in columns 11-14.
        const uint32_t nextType = TetrisControl::nextPieceType();
        const uint32_t previewColor = TetrisControl::nextColor();
        int8_t previewX[4], previewY[4];
        switch (nextType)
        {
            case 0: previewX[0]=0; previewY[0]=1; previewX[1]=1; previewY[1]=1; previewX[2]=2; previewY[2]=1; previewX[3]=3; previewY[3]=1; break;
            case 1: previewX[0]=1; previewY[0]=0; previewX[1]=2; previewY[1]=0; previewX[2]=1; previewY[2]=1; previewX[3]=2; previewY[3]=1; break;
            case 2: previewX[0]=1; previewY[0]=0; previewX[1]=0; previewY[1]=1; previewX[2]=1; previewY[2]=1; previewX[3]=2; previewY[3]=1; break;
            case 3: previewX[0]=1; previewY[0]=0; previewX[1]=2; previewY[1]=0; previewX[2]=0; previewY[2]=1; previewX[3]=1; previewY[3]=1; break;
            case 4: previewX[0]=0; previewY[0]=0; previewX[1]=1; previewY[1]=0; previewX[2]=1; previewY[2]=1; previewX[3]=2; previewY[3]=1; break;
            case 5: previewX[0]=0; previewY[0]=0; previewX[1]=0; previewY[1]=1; previewX[2]=1; previewY[2]=1; previewX[3]=2; previewY[3]=1; break;
            default: previewX[0]=2; previewY[0]=0; previewX[1]=0; previewY[1]=1; previewX[2]=1; previewY[2]=1; previewX[3]=2; previewY[3]=1; break;
        }
        for (uint8_t i = 0; i < 4; ++i)
        {
            setPixel(static_cast<uint8_t>(11 + previewX[i]), static_cast<uint8_t>(1 + previewY[i]),
                     CRGB(static_cast<uint8_t>((previewColor >> 16) & 0xff),
                          static_cast<uint8_t>((previewColor >> 8) & 0xff),
                          static_cast<uint8_t>(previewColor & 0xff)));
        }

        // Level is a six-column vertical indicator in the remaining panel.
        const uint8_t level = TetrisControl::level();
        const uint8_t lit = level > 8 ? 8 : level;
        for (uint8_t y = 14; y >= 7 && y < MATRIX_HEIGHT; --y)
        {
            if (y >= 15U - lit)
            {
                for (uint8_t x = 10; x < 16; ++x) setPixel(x, y, CRGB(70, 180, 255));
            }
        }

        if (TetrisControl::gameOver())
        {
            for (uint8_t x = 10; x < 16; ++x) setPixel(x, 15, CRGB(255, 20, 20));
        }
        else if (TetrisControl::paused())
        {
            for (uint8_t x = 10; x < 16; ++x) setPixel(x, 15, CRGB(255, 190, 30));
        }

        FastLED.show();
        markLedActivity();
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
            case Effect::kFireworks:
                renderFireworks(elapsedMs);
                break;
            case Effect::kFish:
                renderFish(elapsedMs);
                break;
            case Effect::kCat:
                renderCat(elapsedMs);
                break;
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
        Serial.println("Stage 6: Live drawing and LED effect synchronization");
        Serial.println("========================================");
        Serial.printf("Board: ESP32-S3\n");
        Serial.printf("Matrix size: %ux%u\n", MATRIX_WIDTH, MATRIX_HEIGHT);
        Serial.printf("Pixel count: %u\n", PIXEL_COUNT);
        Serial.printf("Startup brightness: %u\n", clampBrightness(STARTUP_BRIGHTNESS));
        Serial.printf("Current brightness: %u | Display: %s\n",
                      FastLED.getBrightness(), drawingMode ? "drawing" : "effects");
        Serial.printf("Maximum operating brightness: %u/%u (electrical ceiling: %u%%)\n",
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
    if (enabled && (drawingMode || clockMode))
    {
        selectEffect(static_cast<uint8_t>(currentEffect));
    }
    autoPlaylist = enabled;
    lastEffectChangeMs = millis();
    Serial.printf("Automatic playlist: %s\n", enabled ? "enabled" : "disabled");
    WebDashboard::broadcastStatus();
}

bool ClockControl::available()
{
    return time(nullptr) >= 1700000000;
}

bool ClockControl::active()
{
    return clockMode;
}

bool ClockControl::setActive(bool enabled)
{
    if (enabled)
    {
        if (!available()) return false;
        TetrisControl::stop();
        ScrollingTextControl::stop();
        drawingMode = false;
        clockMode = true;
        autoPlaylist = false;
        lastEffectChangeMs = millis();
        lastClockSecond = -1;
        FastLED.clear(false);
        Serial.println("[Clock] Clock enabled.");
    }
    else if (clockMode)
    {
        clockMode = false;
        selectEffect(static_cast<uint8_t>(currentEffect));
        autoPlaylist = false;
        Serial.println("[Clock] Returning to selected effect.");
    }
    WebDashboard::broadcastStatus();
    WebDashboard::broadcastFrame();
    return true;
}

ClockControl::Style ClockControl::style()
{
    return clockStyle;
}

bool ClockControl::setStyle(ClockControl::Style value)
{
    if (value != Style::kAnalog && value != Style::kBinary) return false;
    if (!available()) return false;

    clockStyle = value;
    if (clockMode)
    {
        struct tm localTime;
        if (readLocalTime(localTime))
        {
            lastClockSecond = localTime.tm_sec;
            renderClock(localTime);
            WebDashboard::broadcastFrame();
        }
    }
    WebDashboard::broadcastStatus();
    return true;
}

bool ClockControl::readLocalTime(struct tm& value)
{
    if (!available()) return false;
    const time_t now = time(nullptr);
    return localtime_r(&now, &value) != nullptr;
}

bool DisplayControl::drawing()
{
    return drawingMode;
}

void DisplayControl::leaveDrawingMode()
{
    ScrollingTextControl::stop();
    drawingMode = false;
    clockMode = false;
    autoPlaylist = false;
}

uint8_t DisplayControl::brightness()
{
    return FastLED.getBrightness();
}

bool DisplayControl::setBrightness(uint16_t value)
{
    if (value > MAX_ALLOWED_BRIGHTNESS)
    {
        Serial.println("Cannot set brightness: above safe ceiling.");
        return false;
    }
    FastLED.setBrightness(static_cast<uint8_t>(value));
    FastLED.show();
    markLedActivity();
    WebDashboard::broadcastStatus();
    return true;
}

bool DisplayControl::frameHex(char* output, size_t length)
{
    if (output == nullptr || length != static_cast<size_t>(PIXEL_COUNT) * 6U + 1U)
    {
        return false;
    }
    static const char digits[] = "0123456789abcdef";
    size_t offset = 0;
    for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
    {
        for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
        {
            const CRGB& color = leds[xyToIndex(x, y)];
            output[offset++] = digits[color.r >> 4];
            output[offset++] = digits[color.r & 0x0f];
            output[offset++] = digits[color.g >> 4];
            output[offset++] = digits[color.g & 0x0f];
            output[offset++] = digits[color.b >> 4];
            output[offset++] = digits[color.b & 0x0f];
        }
    }
    output[offset] = '\0';
    return true;
}

bool DisplayControl::applyFrame(const char* hex, size_t length)
{
    const auto nibble = [](char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    if (hex == nullptr || length != static_cast<size_t>(PIXEL_COUNT) * 6U)
    {
        Serial.println("Cannot apply drawing: incorrect frame length.");
        return false;
    }
    for (size_t i = 0; i < length; ++i)
    {
        if (nibble(hex[i]) < 0)
        {
            Serial.println("Cannot apply drawing: invalid RGB hex data.");
            return false;
        }
    }
    for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
    {
        for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
        {
            const size_t offset = (static_cast<size_t>(y) * MATRIX_WIDTH + x) * 6U;
            const auto channel = [&](size_t i) -> uint8_t
            {
                return static_cast<uint8_t>(nibble(hex[i]) * 16 + nibble(hex[i + 1U]));
            };
            setPixel(x, y, CRGB(channel(offset), channel(offset + 2U), channel(offset + 4U)));
        }
    }
    TetrisControl::stop();
    ScrollingTextControl::stop();
    drawingMode = true;
    clockMode = false;
    autoPlaylist = false;
    FastLED.show();
    markLedActivity();
    Serial.println("Drawing applied; effects paused.");
    WebDashboard::broadcastStatus();
    WebDashboard::broadcastFrame();
    return true;
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
    if (!ScrollingTextControl::active() && !TetrisControl::active() && !clockMode && autoPlaylist &&
        (nowMs - lastEffectChangeMs) >= EFFECT_DURATION_MS)
    {
        nextEffect();
        nowMs = millis();
    }

    if (ScrollingTextControl::active())
    {
        ScrollingTextControl::update(nowMs);
        if ((nowMs - lastFrameMs) >= 40U)
        {
            lastFrameMs = nowMs;
            FastLED.clear(false);
            for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
            {
                for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
                {
                    const uint32_t packed = ScrollingTextControl::pixelColor(x, y);
                    setPixel(x, y, CRGB(static_cast<uint8_t>((packed >> 16) & 0xff),
                                        static_cast<uint8_t>((packed >> 8) & 0xff),
                                        static_cast<uint8_t>(packed & 0xff)));
                }
            }
            FastLED.show();
        }
    }
    else if (TetrisControl::active())
    {
        TetrisControl::update(nowMs);
        if ((nowMs - lastFrameMs) >= 40U)
        {
            lastFrameMs = nowMs;
            renderTetris();
        }
    }
    else if (clockMode)
    {
        struct tm localTime;
        if (ClockControl::readLocalTime(localTime) && localTime.tm_sec != lastClockSecond)
        {
            lastClockSecond = localTime.tm_sec;
            renderClock(localTime);
        }
    }
    else if (!drawingMode && (nowMs - lastFrameMs) >= EFFECT_FRAME_INTERVAL_MS)
    {
        lastFrameMs = nowMs;
        renderEffect(nowMs);
    }

    handleIdleSleepTimeout();
    delay(1);
}
