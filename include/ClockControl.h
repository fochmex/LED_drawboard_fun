#pragma once

#include <stdint.h>
#include <time.h>

namespace ClockControl
{
    enum class Style : uint8_t
    {
        kAnalog,
        kBinary
    };

    bool available();
    bool active();
    bool setActive(bool enabled);
    Style style();
    bool setStyle(Style value);
    bool readLocalTime(struct tm& value);
}
