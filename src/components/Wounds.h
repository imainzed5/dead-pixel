#pragma once

#include <vector>

enum class WoundType
{
    Cut,
    Bruise,
    Bite
};

struct Wound
{
    WoundType type = WoundType::Bruise;
    float severity = 1.0f;     // 0..1 scale
    bool infected = false;
    float healTimer = 0.0f;    // seconds remaining for treatment to kick in
    float infectionTimer = 0.0f; // hours until infection takes hold (Bite only)
};

struct Wounds
{
    std::vector<Wound> active;
};
