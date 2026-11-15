#pragma once

#include <glm/vec2.hpp>

#include <vector>

#include "ecs/World.h"
#include "map/TileMap.h"

// Thrown things in flight (phase 3): a rock, a bottle, a clock, a flare, a
// molotov. Each flies at the aimed spot on a low arc, stops at walls (open
// doors and empty window frames let it through) and comes down on the first
// of the dead it meets near the end of its flight. What a landing does is
// the game's to decide; this system only says where and on whom.
struct ThrowLanding
{
    int itemId = -1;
    glm::vec2 position{0.0f};      // world px, where it came down
    Entity hitZombie = kInvalidEntity;
    Entity thrower = kInvalidEntity;
    float fuseSeconds = -1.0f;     // an alarm clock's time left before it rings
    bool hitWall = false;          // stopped short by a wall
};

class ThrownSystem
{
public:
    // Launch `itemId` from `from` toward `to` (already clamped to its range).
    static Entity launch(World& world, int itemId, const glm::vec2& from, const glm::vec2& to, Entity thrower,
                         float fuseSeconds, unsigned int textureId, const glm::vec4& uvRect);

    // Moves everything in flight; fills `landed` with what came down this tick.
    void update(World& world, const TileMap& tileMap, float dtSeconds, std::vector<ThrowLanding>& landed);

    // Thrown things pass this tile: open ground, an open or broken door, an
    // empty window frame. Walls, closed doors and glass stop them.
    [[nodiscard]] static bool passable(const TileMap& tileMap, int tileX, int tileY);
};
