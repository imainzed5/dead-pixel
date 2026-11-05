#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include <glm/vec2.hpp>

#include "ecs/World.h"
#include "map/TileMap.h"

class DistrictInfectionSystem;

// The slow-burn pressure behind the "world gets worse whether you act or not"
// pillar. Two jobs:
//
//  1. Migration: every so often a few distant wandering zombies drift toward the
//     player's general area (never the exact spot). More often at night and as
//     days pass, and only while few zombies are already nearby. Hiding still
//     works; standing still forever does not.
//  2. Population: the zombie count grows toward a target that rises with days
//     and district infection. New zombies appear far from the player, weighted
//     toward infected districts.
//
// The director never creates entities itself: it returns spawn positions and
// Game builds the zombies through EntityFactory.
class ThreatDirectorSystem
{
public:
    struct Tuning
    {
        float migrationScale = 1.0f;  // >1: zombies are drawn in more often
        float populationScale = 1.0f; // >1: population grows toward a larger target
    };

    struct SpawnRequest
    {
        glm::vec2 topLeftWorld{0.0f, 0.0f};
        float health = 50.0f;
        float baseMoveSpeed = 112.0f;
    };

    // Call at the start of every run.
    void reset(std::uint32_t seed, const Tuning& tuning);

    void update(World& world,
                const TileMap& tileMap,
                Entity playerEntity,
                float dtSeconds,
                double gameHours,
                int currentDay,
                const DistrictInfectionSystem* infectionSystem,
                std::vector<SpawnRequest>& outSpawns);

    [[nodiscard]] int populationTarget() const { return mPopulationTarget; }
    [[nodiscard]] int migrationsSent() const { return mMigrationsSent; }

    // Tunables (public for tests).
    static constexpr float kGracePeriodSeconds = 150.0f;   // calm opening (~2.5 game hours)
    static constexpr float kDayMigrationInterval = 70.0f;  // real seconds between waves
    static constexpr float kNightMigrationInterval = 35.0f;
    static constexpr float kNearRadiusTiles = 14.0f;       // "already nearby"
    static constexpr float kMinSourceDistanceTiles = 16.0f;
    static constexpr float kTargetScatterMinTiles = 6.0f;  // migrants aim at the area, not the player
    static constexpr float kTargetScatterMaxTiles = 14.0f;
    static constexpr float kSpawnInterval = 90.0f;
    static constexpr float kSpawnMinDistanceTiles = 24.0f;
    static constexpr float kDailyGrowth = 0.18f;           // +18% target per day
    // How many zombies the player's area "should" hold: base + per elapsed day.
    static constexpr float kDesiredNearDay = 1.0f;
    static constexpr float kDesiredNearDayGrowth = 0.5f;
    static constexpr float kDesiredNearNight = 3.0f;
    static constexpr float kDesiredNearNightGrowth = 1.0f;
    static constexpr int kMaxPopulation = 320;

private:
    [[nodiscard]] float uniform(float lo, float hi);
    [[nodiscard]] bool findWalkableNear(const TileMap& tileMap, const glm::vec2& center,
                                        float minTiles, float maxTiles, glm::vec2& outWorld);

    std::mt19937 mRng{1u};
    Tuning mTuning{};
    float mElapsed = 0.0f;
    float mMigrationTimer = 0.0f;
    float mSpawnTimer = 0.0f;
    int mInitialPopulation = -1;
    float mInitialInfection = -1.0f; // growth only counts infection above the starting level
    int mPopulationTarget = 0;
    int mMigrationsSent = 0;
};
