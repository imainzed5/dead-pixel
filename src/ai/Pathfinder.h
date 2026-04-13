#pragma once

#include <glm/vec2.hpp>

#include <vector>

#include "map/TileMap.h"

class Pathfinder
{
public:
    static std::vector<glm::ivec2> findPath(const TileMap& tileMap, const glm::ivec2& start, const glm::ivec2& goal);

private:
    static float heuristic(const glm::ivec2& from, const glm::ivec2& to);
};
