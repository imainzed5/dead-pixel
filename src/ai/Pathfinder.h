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
    // A window with glass or boards is battered like a door, a little dearer
    // so a door into the same room wins.
    static constexpr float kWindowCost = 30.0f;
    // A broken window with no boards is climbed: slow, but no fight.
    static constexpr float kOpenWindowCost = 6.0f;
    // Each board across a door or window: the dead go for the weakest way.
    static constexpr float kBoardCost = 30.0f;
    // Furniture pushed into a doorway must be broken first.
    static constexpr float kBlockedDoorwayCost = 40.0f;

    // Who walks the path. The dead batter and climb their way in; the living
    // (the bot) open doors but go round windows and blocked doorways.
    enum class Mover
    {
        Dead,
        Living
    };

    // Steps a tile costs to enter, or a negative number when it cannot be
    // entered at all.
    [[nodiscard]] static float stepCost(const TileMap& tileMap, const glm::ivec2& tile, Mover mover);

    static std::vector<glm::ivec2> findPath(const TileMap& tileMap, const glm::ivec2& start, const glm::ivec2& goal,
                                            Mover mover = Mover::Dead);

private:
    static float heuristic(const glm::ivec2& from, const glm::ivec2& to);
};
