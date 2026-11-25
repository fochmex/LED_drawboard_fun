#include "TetrisControl.h"

#include <Arduino.h>

namespace
{
constexpr uint8_t kWidth = 10;
constexpr uint8_t kHeight = 16;
constexpr uint8_t kTypes = 7;

uint8_t board[kHeight][kWidth] = {};
uint8_t bag[kTypes];
bool bagReady = false;
bool isActive = false;
bool isPaused = false;
bool isGameOver = false;
uint8_t pieceType = 0;
uint8_t nextType = 0;
uint8_t rotation = 0;
int8_t pieceX = 3;
int8_t pieceY = 0;
uint32_t lastDropMs = 0;
uint16_t totalLines = 0;
uint16_t gameScore = 0;

uint32_t rngState = 0x13579bdu;

uint32_t random32()
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

void refillBag()
{
    for (uint8_t i = 0; i < kTypes; ++i) bag[i] = i;
    for (uint8_t i = kTypes - 1; i > 0; --i)
    {
        uint8_t j = random32() % (i + 1);
        uint8_t tmp = bag[i]; bag[i] = bag[j]; bag[j] = tmp;
    }
    bagReady = true;
}

uint8_t takeFromBag()
{
    static uint8_t index = kTypes;
    if (!bagReady || index >= kTypes)
    {
        refillBag();
        index = 0;
    }
    return bag[index++];
}

void getCells(uint8_t type, uint8_t rot, int8_t cellsX[4], int8_t cellsY[4])
{
    rot &= 3;
    // Coordinates are in a 4 x 4 tetromino box.
    static const int8_t shapes[kTypes][4][4][2] = {
        // I
        {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
        // O
        {{{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{2,1}}},
        // T
        {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
        // S
        {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}}, {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
        // Z
        {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}}, {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}},
        // J
        {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
        // L
        {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}}, {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}}
    };
    for (uint8_t i = 0; i < 4; ++i)
    {
        cellsX[i] = static_cast<int8_t>(shapes[type][rot][i][0]);
        cellsY[i] = static_cast<int8_t>(shapes[type][rot][i][1]);
    }
}

bool canPlace(int8_t x, int8_t y, uint8_t rot)
{
    int8_t cx[4], cy[4];
    getCells(pieceType, rot, cx, cy);
    for (uint8_t i = 0; i < 4; ++i)
    {
        int16_t bx = x + cx[i];
        int16_t by = y + cy[i];
        if (bx < 0 || bx >= kWidth || by >= kHeight) return false;
        if (by >= 0 && board[by][bx] != 0) return false;
    }
    return true;
}

void spawnPiece()
{
    pieceType = nextType;
    nextType = takeFromBag();
    rotation = 0;
    pieceX = 3;
    pieceY = 0;
    if (!canPlace(pieceX, pieceY, rotation))
    {
        isGameOver = true;
        isPaused = false;
    }
}

void clearLines()
{
    uint8_t cleared = 0;
    for (int16_t y = kHeight - 1; y >= 0; --y)
    {
        bool full = true;
        for (uint8_t x = 0; x < kWidth; ++x)
            if (board[y][x] == 0) full = false;
        if (full)
        {
            ++cleared;
            for (int16_t yy = y; yy > 0; --yy)
                for (uint8_t x = 0; x < kWidth; ++x) board[yy][x] = board[yy - 1][x];
            for (uint8_t x = 0; x < kWidth; ++x) board[0][x] = 0;
            ++y;
        }
    }
    if (cleared != 0)
    {
        const uint16_t awards[] = {0, 100, 300, 500, 800};
        uint8_t level = 1 + totalLines / 10;
        gameScore += awards[cleared <= 4 ? cleared : 4] * level;
        totalLines += cleared;
    }
}

void lockPiece()
{
    int8_t cx[4], cy[4];
    getCells(pieceType, rotation, cx, cy);
    for (uint8_t i = 0; i < 4; ++i)
    {
        int16_t bx = pieceX + cx[i];
        int16_t by = pieceY + cy[i];
        if (by >= 0) board[by][bx] = pieceType + 1;
    }
    clearLines();
    spawnPiece();
}

uint32_t typeColor(uint8_t type)
{
    switch (type)
    {
        case 0: return 0x00c8ff; // I cyan
        case 1: return 0xffd000; // O yellow
        case 2: return 0xb45cff; // T purple
        case 3: return 0x38d878; // S green
        case 4: return 0xff4050; // Z red
        case 5: return 0x428cff; // J blue
        case 6: return 0xff8a28; // L orange
        default: return 0;
    }
}

bool isCurrentCell(int8_t x, int8_t y)
{
    int8_t cx[4], cy[4];
    getCells(pieceType, rotation, cx, cy);
    for (uint8_t i = 0; i < 4; ++i)
        if (pieceX + cx[i] == x && pieceY + cy[i] == y) return true;
    return false;
}
}

void TetrisControl::start()
{
    if (isActive) return;
    memset(board, 0, sizeof(board));
    isActive = true;
    isPaused = false;
    isGameOver = false;
    gameScore = 0;
    totalLines = 0;
    rotation = 0;
    pieceX = 3;
    pieceY = 0;
    bagReady = false;
    nextType = takeFromBag();
    spawnPiece();
    lastDropMs = millis();
}

void TetrisControl::restart()
{
    stop();
    start();
}

void TetrisControl::stop()
{
    isActive = false;
    isPaused = false;
    isGameOver = false;
}

bool TetrisControl::active() { return isActive; }
bool TetrisControl::paused() { return isPaused; }
bool TetrisControl::gameOver() { return isGameOver; }

bool TetrisControl::togglePause()
{
    if (!isActive || isGameOver) return false;
    isPaused = !isPaused;
    lastDropMs = millis();
    return true;
}

bool TetrisControl::moveLeft()
{
    if (!isActive || isPaused || isGameOver) return false;
    if (canPlace(pieceX - 1, pieceY, rotation)) { --pieceX; return true; }
    return false;
}

bool TetrisControl::moveRight()
{
    if (!isActive || isPaused || isGameOver) return false;
    if (canPlace(pieceX + 1, pieceY, rotation)) { ++pieceX; return true; }
    return false;
}

bool TetrisControl::rotate()
{
    if (!isActive || isPaused || isGameOver) return false;
    uint8_t newRot = (rotation + 1) & 3;
    for (int8_t kick = 0; kick >= -2; --kick)
    {
        if (canPlace(pieceX + kick, pieceY, newRot))
        {
            pieceX += kick;
            rotation = newRot;
            return true;
        }
    }
    for (int8_t kick = 1; kick <= 2; ++kick)
    {
        if (canPlace(pieceX + kick, pieceY, newRot))
        {
            pieceX += kick;
            rotation = newRot;
            return true;
        }
    }
    return false;
}

bool TetrisControl::softDrop()
{
    if (!isActive || isPaused || isGameOver) return false;
    if (canPlace(pieceX, pieceY + 1, rotation))
    {
        ++pieceY;
        gameScore += 1;
        lastDropMs = millis();
        return true;
    }
    lockPiece();
    lastDropMs = millis();
    return false;
}

void TetrisControl::hardDrop()
{
    if (!isActive || isPaused || isGameOver) return;
    uint16_t distance = 0;
    while (canPlace(pieceX, pieceY + 1, rotation))
    {
        ++pieceY;
        ++distance;
    }
    gameScore += distance * 2;
    lockPiece();
    lastDropMs = millis();
}

void TetrisControl::update(uint32_t nowMs)
{
    if (!isActive || isPaused || isGameOver) return;
    uint8_t level = 1 + totalLines / 10;
    uint32_t interval = level >= 12 ? 100 : 800 - (level - 1) * 70;
    if (nowMs - lastDropMs >= interval)
    {
        if (canPlace(pieceX, pieceY + 1, rotation)) ++pieceY;
        else lockPiece();
        lastDropMs = nowMs;
    }
}

uint32_t TetrisControl::boardColor(uint8_t x, uint8_t y)
{
    if (x >= kWidth || y >= kHeight) return 0;
    if (board[y][x] != 0) return typeColor(board[y][x] - 1);
    if (isActive && !isGameOver && isCurrentCell(x, y)) return typeColor(pieceType);
    return 0;
}

uint32_t TetrisControl::currentColor() { return typeColor(pieceType); }
uint32_t TetrisControl::nextColor() { return typeColor(nextType); }
uint32_t TetrisControl::nextPieceType() { return nextType; }
uint16_t TetrisControl::score() { return gameScore; }
uint16_t TetrisControl::lines() { return totalLines; }
uint8_t TetrisControl::level() { return 1 + totalLines / 10; }
