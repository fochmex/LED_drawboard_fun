#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ScrollingTextControl
{
    enum class RepeatMode : uint8_t { kOnce, kCount, kLoop };

    void setText(const char* text);
    void setColors(uint32_t textColor, uint32_t backgroundColor);
    void setFont(uint8_t font);
    void setDirection(bool scrollLeft);
    void setSpeed(uint8_t speed);
    void setPause(uint16_t beforeMs, uint16_t afterMs);
    void setRepeat(RepeatMode mode, uint8_t count);
    void start();
    void stop();
    void clear();
    bool active();
    bool stopped();
    void update(uint32_t nowMs);
    void render();
    uint32_t pixelColor(uint8_t x, uint8_t y);

    const char* text();
    uint32_t textColor();
    uint32_t backgroundColor();
    uint8_t font();
    bool scrollLeft();
    uint8_t speed();
    uint16_t beforePause();
    uint16_t afterPause();
    RepeatMode repeatMode();
    uint8_t repeatCount();
    uint32_t estimatedDurationMs();
}
