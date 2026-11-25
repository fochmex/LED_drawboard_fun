#include "ScrollingTextControl.h"

#include <Arduino.h>
#include <string.h>

namespace
{
constexpr uint8_t kWidth = 16;
constexpr uint8_t kHeight = 16;
constexpr uint8_t kMaxText = 64;
char message[kMaxText + 1] = "";
uint32_t foreground = 0x75e4ca;
uint32_t background = 0x000000;
uint8_t selectedFont = 1;
bool scrollToLeft = true;
uint8_t scrollSpeed = 5;
uint16_t pauseBefore = 500;
uint16_t pauseAfter = 500;
ScrollingTextControl::RepeatMode repeat = ScrollingTextControl::RepeatMode::kLoop;
uint8_t requestedRepeats = 1;
bool isActive = false;
uint32_t phaseStartMs = 0;
uint32_t scrollStartMs = 0;
int32_t scrollOffset = 0;
uint8_t completedRepeats = 0;
uint8_t phase = 0; // 0 before, 1 scrolling, 2 after

struct Glyph
{
    uint8_t rows[7];
};

uint8_t glyphIndex(char c)
{
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return static_cast<uint8_t>(c - 'A');
    if (c >= '0' && c <= '9') return 26 + static_cast<uint8_t>(c - '0');
    if (c == ' ') return 36;
    if (c == '!') return 37;
    if (c == '?') return 38;
    if (c == '.') return 39;
    if (c == ':') return 40;
    if (c == '-') return 41;
    if (c == '+') return 42;
    if (c == '/') return 43;
    if (c == '#') return 44;
    if (c == '_') return 45;
    return 38;
}

const Glyph& glyph(char c)
{
    static const Glyph fonts[46] = {
        {{0x00,0x7c,0x12,0x11,0x12,0x7c,0x00}}, // A
        {{0x7f,0x49,0x49,0x49,0x49,0x36,0x00}}, // B
        {{0x3e,0x41,0x41,0x41,0x41,0x22,0x00}}, // C
        {{0x7f,0x41,0x41,0x41,0x22,0x1c,0x00}}, // D
        {{0x7f,0x49,0x49,0x49,0x49,0x41,0x00}}, // E
        {{0x7f,0x09,0x09,0x09,0x09,0x01,0x00}}, // F
        {{0x3e,0x41,0x49,0x49,0x49,0x32,0x00}}, // G
        {{0x7f,0x08,0x08,0x08,0x08,0x7f,0x00}}, // H
        {{0x41,0x41,0x7f,0x41,0x41,0x41,0x00}}, // I
        {{0x20,0x40,0x41,0x3f,0x01,0x01,0x00}}, // J
        {{0x7f,0x08,0x14,0x22,0x41,0x00,0x00}}, // K
        {{0x7f,0x40,0x40,0x40,0x40,0x40,0x00}}, // L
        {{0x7f,0x02,0x04,0x08,0x04,0x02,0x7f}}, // M
        {{0x7f,0x02,0x04,0x08,0x10,0x7f,0x00}}, // N
        {{0x3e,0x41,0x41,0x41,0x41,0x3e,0x00}}, // O
        {{0x7f,0x09,0x09,0x09,0x09,0x06,0x00}}, // P
        {{0x3e,0x41,0x51,0x21,0x41,0x5e,0x00}}, // Q
        {{0x7f,0x09,0x19,0x29,0x49,0x46,0x00}}, // R
        {{0x46,0x49,0x49,0x49,0x49,0x31,0x00}}, // S
        {{0x01,0x01,0x7f,0x01,0x01,0x01,0x00}}, // T
        {{0x3f,0x40,0x40,0x40,0x40,0x3f,0x00}}, // U
        {{0x1f,0x20,0x40,0x20,0x1f,0x00,0x00}}, // V
        {{0x7f,0x40,0x38,0x40,0x7f,0x00,0x00}}, // W
        {{0x63,0x14,0x08,0x14,0x63,0x00,0x00}}, // X
        {{0x03,0x04,0x78,0x04,0x03,0x00,0x00}}, // Y
        {{0x61,0x51,0x49,0x45,0x43,0x00,0x00}}, // Z
        {{0x3e,0x51,0x49,0x45,0x3e,0x00,0x00}}, // 0
        {{0x00,0x42,0x7f,0x40,0x00,0x00,0x00}}, // 1
        {{0x42,0x61,0x51,0x49,0x46,0x00,0x00}}, // 2
        {{0x21,0x41,0x45,0x4b,0x31,0x00,0x00}}, // 3
        {{0x18,0x14,0x12,0x7f,0x10,0x00,0x00}}, // 4
        {{0x27,0x45,0x45,0x45,0x39,0x00,0x00}}, // 5
        {{0x3c,0x46,0x49,0x49,0x30,0x00,0x00}}, // 6
        {{0x01,0x71,0x09,0x05,0x03,0x00,0x00}}, // 7
        {{0x36,0x49,0x49,0x49,0x36,0x00,0x00}}, // 8
        {{0x06,0x49,0x49,0x29,0x1e,0x00,0x00}}, // 9
        {{0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, // space
        {{0x00,0x00,0x5f,0x00,0x00,0x00,0x00}}, // !
        {{0x00,0x03,0x05,0x03,0x00,0x00,0x00}}, // ?
        {{0x00,0x60,0x60,0x00,0x00,0x00,0x00}}, // .
        {{0x36,0x36,0x00,0x00,0x00,0x00,0x00}}, // :
        {{0x08,0x08,0x08,0x08,0x08,0x08,0x08}}, // -
        {{0x08,0x08,0x3e,0x08,0x08,0x08,0x00}}, // +
        {{0x20,0x10,0x08,0x04,0x02,0x00,0x00}}, // /
        {{0x14,0x14,0x7f,0x14,0x14,0x7f,0x14}}, // #
        {{0x40,0x40,0x40,0x40,0x40,0x40,0x40}}  // _
    };
    return fonts[glyphIndex(c)];
}

uint8_t scaleX()
{
    return selectedFont == 2 ? 2 : 1;
}

uint8_t charWidth()
{
    if (selectedFont == 0) return 5;
    return selectedFont == 2 ? 14 : 7;
}

uint8_t charSpacing()
{
    return charWidth() + 1;
}

int32_t textWidth()
{
    size_t len = strlen(message);
    if (len == 0) return 0;
    return static_cast<int32_t>(len) * charSpacing();
}

bool glyphPixel(char c, uint8_t localX, uint8_t localY)
{
    const Glyph& g = glyph(c);
    if (localX >= charWidth()) return false;
    const uint8_t sx = scaleX();
    const uint8_t sourceY = static_cast<uint8_t>(localY / sx);
    if (sourceY >= 7) return false;

    const uint8_t rowMask = 1U << sourceY;
    if (selectedFont == 0)
    {
        for (uint8_t sourceX = 0; sourceX < 7; ++sourceX)
        {
            if ((sourceX * charWidth()) / 7U == localX &&
                (g.rows[sourceX] & rowMask) != 0)
                return true;
        }
        return false;
    }

    const uint8_t sourceX = localX / sx;
    return sourceX < 7 && (g.rows[sourceX] & rowMask) != 0;
}

void beginScroll(uint32_t nowMs)
{
    scrollOffset = 0;
    scrollStartMs = nowMs;
    phase = 1;
}

void finishRepeat(uint32_t nowMs)
{
    ++completedRepeats;
    if (repeat == ScrollingTextControl::RepeatMode::kOnce ||
        (repeat == ScrollingTextControl::RepeatMode::kCount && completedRepeats >= requestedRepeats))
    {
        isActive = false;
        return;
    }
    phase = 0;
    phaseStartMs = nowMs;
}
}

void ScrollingTextControl::setText(const char* text)
{
    if (text == nullptr) text = "";
    strncpy(message, text, kMaxText);
    message[kMaxText] = '\0';
}

void ScrollingTextControl::setColors(uint32_t textColor, uint32_t backgroundColor)
{
    foreground = textColor;
    background = backgroundColor;
}

void ScrollingTextControl::setFont(uint8_t font)
{
    selectedFont = font <= 2 ? font : 1;
}

void ScrollingTextControl::setDirection(bool scrollLeft)
{
    scrollToLeft = scrollLeft;
}

void ScrollingTextControl::setSpeed(uint8_t speed)
{
    scrollSpeed = speed == 0 ? 1 : (speed > 10 ? 10 : speed);
}

void ScrollingTextControl::setPause(uint16_t beforeMs, uint16_t afterMs)
{
    pauseBefore = beforeMs > 5000 ? 5000 : beforeMs;
    pauseAfter = afterMs > 5000 ? 5000 : afterMs;
}

void ScrollingTextControl::setRepeat(RepeatMode mode, uint8_t count)
{
    repeat = mode;
    requestedRepeats = count == 0 ? 1 : (count > 20 ? 20 : count);
}

void ScrollingTextControl::start()
{
    if (message[0] == '\0') { isActive = false; return; }
    isActive = true;
    completedRepeats = 0;
    scrollOffset = 0;
    phase = pauseBefore == 0 ? 1 : 0;
    phaseStartMs = millis();
    scrollStartMs = millis();
}

void ScrollingTextControl::stop()
{
    isActive = false;
}

void ScrollingTextControl::clear()
{
    message[0] = '\0';
    isActive = false;
    scrollOffset = 0;
}

bool ScrollingTextControl::active() { return isActive; }
bool ScrollingTextControl::stopped() { return !isActive; }

void ScrollingTextControl::update(uint32_t nowMs)
{
    if (!isActive) return;
    if (phase == 0)
    {
        if (nowMs - phaseStartMs >= pauseBefore)
            beginScroll(nowMs);
        return;
    }
    if (phase == 1)
    {
        uint32_t interval = 180U - static_cast<uint32_t>(scrollSpeed - 1U) * 15U;
        if (nowMs - scrollStartMs >= interval)
        {
            scrollStartMs = nowMs;
            ++scrollOffset;
            int32_t travel = textWidth() + kWidth;
            if (scrollOffset > travel)
            {
                phase = pauseAfter == 0 ? 3 : 2;
                phaseStartMs = nowMs;
            }
        }
        return;
    }
    if (phase == 2)
    {
        if (nowMs - phaseStartMs >= pauseAfter)
        {
            phase = 3;
            finishRepeat(nowMs);
        }
    }
    else if (phase == 3)
    {
        finishRepeat(nowMs);
    }
}

void ScrollingTextControl::render() {}

uint32_t ScrollingTextControl::pixelColor(uint8_t x, uint8_t y)
{
    if (x >= kWidth || y >= kHeight) return background;
    if (!isActive || message[0] == '\0') return background;

    int32_t baseX;
    if (scrollToLeft)
        baseX = kWidth - scrollOffset;
    else
        baseX = -textWidth() + scrollOffset;

    const uint8_t scale = scaleX();
    const uint8_t spacing = charSpacing();
    const int32_t yOffset = selectedFont == 2 ? 1 : 4;
    int32_t localColumn = static_cast<int32_t>(x) - baseX;
    if (localColumn < 0) return background;

    size_t charIndex = static_cast<size_t>(localColumn / spacing);
    if (charIndex >= strlen(message)) return background;
    uint8_t within = static_cast<uint8_t>(localColumn % spacing);
    if (within >= charWidth()) return background;

    int32_t localY = static_cast<int32_t>(y) - yOffset;
    if (localY < 0 || localY >= 7 * scale) return background;
    if (glyphPixel(message[charIndex], within, static_cast<uint8_t>(localY))) return foreground;
    return background;
}

const char* ScrollingTextControl::text() { return message; }
uint32_t ScrollingTextControl::textColor() { return foreground; }
uint32_t ScrollingTextControl::backgroundColor() { return background; }
uint8_t ScrollingTextControl::font() { return selectedFont; }
bool ScrollingTextControl::scrollLeft() { return scrollToLeft; }
uint8_t ScrollingTextControl::speed() { return scrollSpeed; }
uint16_t ScrollingTextControl::beforePause() { return pauseBefore; }
uint16_t ScrollingTextControl::afterPause() { return pauseAfter; }
ScrollingTextControl::RepeatMode ScrollingTextControl::repeatMode() { return repeat; }
uint8_t ScrollingTextControl::repeatCount() { return requestedRepeats; }

uint32_t ScrollingTextControl::estimatedDurationMs()
{
    if (message[0] == '\0') return 0;
    int32_t travel = textWidth() + kWidth;
    uint32_t interval = 180U - static_cast<uint32_t>(scrollSpeed - 1U) * 15U;
    uint32_t one = pauseBefore + static_cast<uint32_t>(travel) * interval + pauseAfter;
    uint32_t repeats = repeat == RepeatMode::kLoop ? 1U : (repeat == RepeatMode::kCount ? requestedRepeats : 1U);
    return one * repeats;
}
