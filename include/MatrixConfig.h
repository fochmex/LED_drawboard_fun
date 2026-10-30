#pragma once

#include "ProjectConfig.h"

enum class MatrixWiringMode : uint8_t
{
    kStraight = 0,
    kSerpentine = 1
};

enum class RotationMode : uint8_t
{
    kNone = 0,
    k90 = 1,
    k180 = 2,
    k270 = 3
};

constexpr MatrixWiringMode MATRIX_WIRING_MODE = MatrixWiringMode::kSerpentine;
constexpr RotationMode MATRIX_ROTATION = RotationMode::kNone;
constexpr bool MATRIX_MIRROR_X = false;
constexpr bool MATRIX_MIRROR_Y = false;
