#pragma once

#include <glm/vec2.hpp>

#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"

class ZombieAISystem
{
public:
    void update(World& world, const TileMap& tileMap, const NoiseModel& noiseModel, Entity playerEntity, float dtSeconds, double gameHours = 12.0, int currentDay = 1) const;

private:
    static constexpr float kSpriteSize = 32.0f;
    static constexpr float kZombieAttackRange = 28.0f;
    static constexpr float kZombieAttackDamage = 12.0f;
    static constexpr float kZombieAttackCooldownSeconds = 0.85f;
    static constexpr float kZombieRepathSeconds = 0.45f;
    static constexpr float kZombieNoiseInvestigateThreshold = 0.08f;

    [[nodiscard]] static glm::vec2 tileCenterWorld(const TileMap& tileMap, const glm::ivec2& tile);
    [[nodiscard]] static bool hasLineOfSight(const TileMap& tileMap, const glm::vec2& worldStart, const glm::vec2& worldEnd);
};
