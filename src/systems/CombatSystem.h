#pragma once

#include <glm/vec2.hpp>

#include <random>
#include <vector>

#include "components/Combat.h"
#include "core/Input.h"
#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"

class CombatSystem
{
public:
    CombatSystem();

    void update(World& world, const Input& input, const TileMap& tileMap, NoiseModel& noiseModel, float dtSeconds);

private:
    static constexpr float kSpriteSize = 32.0f;
    static constexpr float kWindupSeconds = 0.20f;
    static constexpr float kActiveSeconds = 0.10f;
    static constexpr float kRecoverySeconds = 0.30f;
    static constexpr float kRecoveryDelayAfterAttackSeconds = 1.00f;
    static constexpr float kExhaustedMissChance = 0.35f;
    static constexpr float kExhaustedThreshold = 0.20f;
    static constexpr float kKnockbackPixels = 24.0f;
    static constexpr float kBackstabMultiplier = 1.5f;

    static constexpr float kGrabStaminaCost = 35.0f;
    static constexpr float kGrabLungeSeconds = 0.15f;
    static constexpr float kGrabHoldSeconds = 1.0f;
    static constexpr float kGrabExecuteSeconds = 0.6f;
    static constexpr float kGrabExecuteDamage = 200.0f;
    static constexpr float kGrabRange = 36.0f;

    static constexpr float kCollisionWidth = 20.0f;
    static constexpr float kCollisionHeight = 20.0f;
    static constexpr float kCollisionCenterYOffset = 22.0f;

    static void degradeWeapon(Combat& combat);
    static float effectiveRangePixels(const Combat& combat);
    static float effectiveArcDegrees(const Combat& combat);
    static float effectiveDamage(const Combat& combat);
    static bool isPointInsideArc(const glm::vec2& origin, float facingRadians, float arcDegrees, float rangePixels, const glm::vec2& point);

    void resolveSwing(
        World& world,
        const TileMap& tileMap,
        NoiseModel& noiseModel,
        Entity playerEntity,
        const glm::vec2& playerCenter,
        float facingRadians,
        Combat& combat,
        std::vector<Entity>& destroyedEntities);

    std::mt19937 mRng;
    std::uniform_real_distribution<float> mDist;
};
