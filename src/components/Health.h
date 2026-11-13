#pragma once

#include <algorithm>
#include <cstddef>

struct Health
{
    float current = 100.0f;
    float maximum = 100.0f;
    // What `maximum` returns to with no wounds (Tough raises it to 120).
    float baseMaximum = 100.0f;
};

// Each open wound costs 5 points of maximum health, down to a floor of 20.
inline float woundedMaximum(float baseMaximum, std::size_t openWounds)
{
    constexpr float kMaxHpPenaltyPerWound = 5.0f;
    return std::max(20.0f, baseMaximum - kMaxHpPenaltyPerWound * static_cast<float>(openWounds));
}
