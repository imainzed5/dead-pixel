#pragma once

#include <glm/vec2.hpp>

#include <string>

enum class SurvivorState
{
    Idle,
    Wander,
    Scavenge,
    Flee,
    Fight,
    Sleep,
    Trade,
    Dead
};

enum class SurvivorAttitude
{
    Neutral,
    Organised,
    Desperate,
    Hostile
};

struct SurvivorPersonality
{
    float aggression = 0.5f;   // 0=pacifist, 1=attacks on sight
    float cooperation = 0.5f;  // 0=territorial, 1=willing to trade/share
    float competence = 0.5f;   // 0=poor decisions, 1=smart survivor
};

struct SurvivorAI
{
    SurvivorState state = SurvivorState::Wander;
    SurvivorAttitude attitude = SurvivorAttitude::Neutral;
    SurvivorPersonality personality;

    std::string name;
    int spawnDay = 1;

    // Needs mirroring player (same system drives both)
    float hunger = 80.0f;
    float thirst = 80.0f;
    float fatigue = 0.0f;

    // Decision timers
    float decisionTimer = 0.0f;
    float stateTimer = 0.0f;

    // Movement
    glm::vec2 targetWorld{0.0f, 0.0f};
    float moveSpeed = 96.0f;

    // Combat
    float attackCooldown = 0.0f;

    // District assignment
    int homeDistrictId = 0;

    // Persistence
    bool alive = true;
    int deathDay = 0;
};
