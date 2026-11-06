#pragma once

#include <glm/vec2.hpp>

#include <vector>

#include "map/TileMap.h"

class Pathfinder
{
public:
    // Steps a closed door counts for. High enough that the dead walk round a
    // house rather than through two of its doors, but a door into the place
    // they want to reach is still the way in.
    static constexpr float kClosedDoorCost = 25.0f;

    static std::vector<glm::ivec2> findPath(const TileMap& tileMap, const glm::ivec2& start, const glm::ivec2& goal);

private:
    static float heuristic(const glm::ivec2& from, const glm::ivec2& to);
};
