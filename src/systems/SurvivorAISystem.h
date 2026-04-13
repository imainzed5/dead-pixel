#pragma once

#include <glm/vec2.hpp>

#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"
#include "items/ItemDatabase.h"

class SurvivorAISystem
{
public:
    void update(World& world, const TileMap& tileMap, const NoiseModel& noiseModel,
                Entity playerEntity, float dtSeconds, double gameHours, int currentDay) const;

private:
    static constexpr float kSpriteSize = 32.0f;
    static constexpr float kDecisionInterval = 1.5f;        // seconds between decision cycles
    static constexpr float kFleeRange = 160.0f;             // pixel range to detect zombies
    static constexpr float kScavengeRange = 256.0f;         // pixel range to find items
    static constexpr float kWanderRadius = 128.0f;          // pixels from home position
    static constexpr float kAttackRange = 28.0f;
    static constexpr float kAttackCooldown = 1.2f;
    static constexpr float kAttackDamage = 8.0f;

    static constexpr float kNeedsDecayHunger = 0.02f;
    static constexpr float kNeedsDecayThirst = 0.035f;
    static constexpr float kStarvationDamage = 2.5f;
    static constexpr float kDehydrationDamage = 3.0f;
};
