#pragma once

#include "ecs/World.h"
#include "map/TileMap.h"

class CollisionSystem
{
public:
    void update(World& world, const TileMap& tileMap, float dtSeconds) const;

private:
    static constexpr float kSpriteSize = 32.0f;
    static constexpr float kCollisionWidth = 20.0f;
    static constexpr float kCollisionHeight = 20.0f;
    static constexpr float kCollisionCenterYOffset = 16.0f;
    static constexpr float kStructureCollisionSize = 28.0f;
};
