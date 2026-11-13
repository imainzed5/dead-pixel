#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "map/TileMap.h"

// One distance field out from a goal tile (the player), shared by every
// zombie chasing it: each steps downhill instead of running its own A*.
// Built over a square window of `radius` tiles round the goal, with the same
// step costs the dead pay on a path (closed doors 25, windows 30, blocked
// doorways 40, boards 30 each).
class FlowField
{
public:
    static constexpr int kDefaultRadius = 40;

    void build(const TileMap& tileMap, const glm::ivec2& goal, int radius = kDefaultRadius);
    void clear();

    [[nodiscard]] bool valid() const { return mSize > 0; }
    [[nodiscard]] const glm::ivec2& goal() const { return mGoal; }
    // Cost to reach the goal from `tile`, or a negative number off the field
    // or where the goal cannot be reached.
    [[nodiscard]] float distance(const glm::ivec2& tile) const;
    // The neighbouring tile (of eight, no corner cutting) one step downhill,
    // or false when there is none (at the goal, or off the field).
    [[nodiscard]] bool nextStep(const TileMap& tileMap, const glm::ivec2& from, glm::ivec2& out) const;

private:
    [[nodiscard]] int index(const glm::ivec2& tile) const;

    glm::ivec2 mGoal{0};
    glm::ivec2 mOrigin{0}; // top-left tile of the window
    int mSize = 0;         // window side, tiles
    std::vector<float> mDistance;
};
