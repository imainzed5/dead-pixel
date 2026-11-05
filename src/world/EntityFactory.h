#pragma once

#include <glm/vec2.hpp>

#include "ecs/World.h"

class SpriteSheet;

// Creates fully-formed gameplay entities. Keeps component recipes in one place
// instead of scattered through Game.cpp.
namespace EntityFactory
{
struct ZombieParams
{
    glm::vec2 topLeftWorld{0.0f, 0.0f}; // entity Transform position (sprite top-left)
    float health = 50.0f;
    float baseMoveSpeed = 112.0f;
    int spawnDay = 1;
};

Entity spawnZombie(World& world, const SpriteSheet& spriteSheet, const ZombieParams& params);
} // namespace EntityFactory
