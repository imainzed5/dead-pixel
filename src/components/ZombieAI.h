#pragma once

#include <glm/vec2.hpp>

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
    float moveSpeed = 112.0f;
    float visionRangeTiles = 8.0f;

    // Decomposition / aging
    DecompStage decomp = DecompStage::Fresh;
    int spawnDay = 1; // in-game day this zombie was created
};
