#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

#include "ecs/Types.h"

enum class ZombieState
{
    Wander,
    Investigate,
    Chase,
    Attack
};

enum class DecompStage
{
    Fresh = 0,    // Day 1-3:  fastest, highest threat
    Bloated = 1,  // Day 4-10: slower, durable, bio-hazard on death
    Decayed = 2,  // Day 11-20: slow, low individual threat, disease risk
    Skeletal = 3  // Day 20+: barely mobile, minimal threat
};

struct ZombieAI
{
    ZombieState state = ZombieState::Wander;
    glm::vec2 targetWorld{0.0f, 0.0f};
    float stateTimer = 0.0f;
    float pathRepathTimer = 0.0f;
    float attackCooldown = 0.0f;
    float attackWindup = 0.0f; // > 0: lunging; the hit resolves when it reaches 0

    // Per-zombie base speed (rolled at spawn); threat tuning scales it into moveSpeed.
    float baseMoveSpeed = 112.0f;
    float moveSpeed = 112.0f;
    float visionRangeTiles = 8.0f;

    // Decomposition / aging
    DecompStage decomp = DecompStage::Fresh;
    int spawnDay = 1; // in-game day this zombie was created

    // Wandering: per-zombie RNG and a persistent heading (correlated random walk)
    // so zombies drift across the map instead of jittering in place.
    std::uint32_t rngState = 0; // 0 = seed lazily from the entity id
    float heading = 0.0f;       // radians, world space
    float idleTimer = 0.0f;     // > 0: standing still for a moment

    // Long-range moves (migration, distant noise) follow an A* path.
    std::vector<glm::ivec2> path;
    std::size_t pathIndex = 0;
    glm::vec2 pathGoal{0.0f, 0.0f};
    bool migrating = false; // sent toward the player's area by the threat director

    // Who dealt the most recent damage (player, projectile shooter, fire,
    // survivor, vehicle). Kills are credited from this.
    Entity lastDamagedBy = kInvalidEntity;
};
