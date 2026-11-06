#pragma once

#include <glm/vec2.hpp>

#include "components/ZombieAI.h"
#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"

class DistrictInfectionSystem;

class ZombieAISystem
{
public:
    void update(
        World& world,
        const TileMap& tileMap,
        const NoiseModel& noiseModel,
        Entity playerEntity,
        float dtSeconds,
        double gameHours = 12.0,
        int currentDay = 1,
        const DistrictInfectionSystem* infectionSystem = nullptr,
        std::vector<glm::ivec2>* doorBlows = nullptr) const;

    // Night vision depends on the player's light: darkness hides you, a lit
    // flashlight makes you visible from further away than in daylight.
    void setPlayerLit(bool lit) { mPlayerLit = lit; }
    static constexpr float kNightVisionDark = 0.6f;
    static constexpr float kNightVisionLit = 1.3f;

    // Send a zombie toward a world position (used by the threat director).
    // Long trips follow an A* path; `migrating` zombies shamble rather than rush.
    static void sendTo(ZombieAI& zombie, const glm::vec2& targetWorld, bool migrating);

    // Uniform [0, 1) from the zombie's own RNG stream.
    [[nodiscard]] static float nextRandom(ZombieAI& zombie, Entity entity);

    [[nodiscard]] static bool hasLineOfSight(const TileMap& tileMap, const glm::vec2& worldStart, const glm::vec2& worldEnd);

private:
    static constexpr float kSpriteSize = 32.0f;
    static constexpr float kZombieAttackRange = 28.0f;
    // A zombie attack is a telegraphed lunge: it winds up, then hits only if
    // the player is still within reach, and only some of the time.
    static constexpr float kZombieAttackDamage = 7.0f;
    static constexpr float kZombieAttackCooldownSeconds = 1.5f;
    static constexpr float kZombieAttackWindupSeconds = 0.55f;
    static constexpr float kZombieAttackReachScale = 1.25f; // reach at resolve, relative to attack range
    static constexpr float kZombieHitChance = 0.6f;
    static constexpr float kZombieWoundChance = 0.35f;     // per landed hit
    static constexpr float kZombieBiteShare = 0.3f;         // of wounds
    static constexpr float kBiteContaminationChance = 0.5f;
    static constexpr float kZombieRepathSeconds = 0.45f;
    static constexpr float kZombieNoiseInvestigateThreshold = 0.08f;

    // Wandering
    static constexpr float kWanderSpeedFactor = 0.45f;
    static constexpr float kMigrateSpeedFactor = 0.6f;
    static constexpr float kInvestigateSpeedFactor = 0.8f;
    static constexpr float kWanderStuckSeconds = 4.0f;

    // Investigate targets farther than this (tiles) use A* instead of a straight line.
    static constexpr float kPathfindDistanceTiles = 5.0f;

    // Zombies closer than this (world px) push apart.
    static constexpr float kSeparationRadius = 14.0f;

    [[nodiscard]] static glm::vec2 tileCenterWorld(const TileMap& tileMap, const glm::ivec2& tile);

    bool mPlayerLit = false;
};
