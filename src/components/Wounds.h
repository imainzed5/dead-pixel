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
    float healTimer = 0.0f;    // real seconds since the wound was taken; it closes when this reaches the type's heal time
    float infectionTimer = 0.0f; // hours until infection takes hold (Bite only)
    bool contaminated = false;   // a dirty bite: infects unless treated in time
};

struct Wounds
{
    std::vector<Wound> active;
};
