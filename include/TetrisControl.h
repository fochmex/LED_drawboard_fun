#pragma once

#include <stdint.h>

namespace TetrisControl
{
    void start();
    void restart();
    void stop();
    bool active();
    bool paused();
    bool gameOver();
    bool togglePause();
    bool moveLeft();
    bool moveRight();
    bool rotate();
    bool softDrop();
    void hardDrop();
    void update(uint32_t nowMs);
    uint32_t boardColor(uint8_t x, uint8_t y);
    uint32_t currentColor();
    uint32_t nextColor();
    uint32_t nextPieceType();
    uint16_t score();
    uint16_t lines();
    uint8_t level();
}
