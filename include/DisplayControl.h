#pragma once

#include <stddef.h>
#include <stdint.h>

namespace DisplayControl
{
    bool drawing();
    uint8_t brightness();
    bool setBrightness(uint16_t value);
    bool applyFrame(const char* hex, size_t length);
}
