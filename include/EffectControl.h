#pragma once

#include <stdint.h>

namespace EffectControl
{
    uint8_t count();
    const char* name(uint8_t index);
    uint8_t selected();
    bool playlistEnabled();
    bool select(uint8_t index);
    void setPlaylistEnabled(bool enabled);
}
