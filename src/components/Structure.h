#pragma once

#include <string>

enum class StructureType
{
    Barricade,
    NoiseTrap,
    Campfire,
    SleepingArea,
    SupplyCache,
    RainCollector
};

enum class StructureCondition
{
    Intact,
    Damaged,
    Breaking
};

struct Structure
{
    StructureType type = StructureType::Barricade;
    StructureCondition condition = StructureCondition::Intact;
    float hp = 100.0f;
    float maxHp = 100.0f;
    float decayRatePerHour = 0.5f;
    bool triggered = false; // for noise trap
    float noiseTimer = 0.0f; // for campfire periodic noise
    float waterGenTimer = 0.0f; // for rain collector
};
