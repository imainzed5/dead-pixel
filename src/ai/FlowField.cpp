#include "ai/FlowField.h"

#include <cmath>
#include <limits>
#include <queue>

#include "ai/Pathfinder.h"

namespace
{
constexpr float kUnreached = std::numeric_limits<float>::infinity();
}

void FlowField::clear()
{
    mSize = 0;
    mDistance.clear();
}

int FlowField::index(const glm::ivec2& tile) const
{
    const glm::ivec2 local = tile - mOrigin;
    if (local.x < 0 || local.y < 0 || local.x >= mSize || local.y >= mSize) return -1;
    return local.y * mSize + local.x;
}

void FlowField::build(const TileMap& tileMap, const glm::ivec2& goal, int radius)
{
    mGoal = goal;
    mOrigin = goal - glm::ivec2(radius);
    mSize = radius * 2 + 1;
    mDistance.assign(static_cast<std::size_t>(mSize) * static_cast<std::size_t>(mSize), kUnreached);
    if (!tileMap.inBounds(goal.x, goal.y)) return;

    // Dijkstra outward from the goal. A zombie on tile N stepping to tile C
    // pays C's cost, so N's distance is C's plus the cost of entering C.
    struct Node
    {
        float d;
        int i;
        bool operator<(const Node& o) const { return d > o.d; }
    };
    std::priority_queue<Node> open;
    const int start = index(goal);
    mDistance[static_cast<std::size_t>(start)] = 0.0f;
    open.push({0.0f, start});
    const glm::ivec2 steps[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!open.empty())
    {
        const Node node = open.top();
        open.pop();
        if (node.d > mDistance[static_cast<std::size_t>(node.i)]) continue;
        const glm::ivec2 here = mOrigin + glm::ivec2(node.i % mSize, node.i / mSize);
        // Entering `here` costs this much (the goal itself is free).
        const float enter = here == goal ? 0.0f : Pathfinder::stepCost(tileMap, here, Pathfinder::Mover::Dead);
        if (enter < 0.0f && here != goal) continue;
        for (const glm::ivec2& s : steps)
        {
            const glm::ivec2 next = here + s;
            const int ni = index(next);
            if (ni < 0 || !tileMap.inBounds(next.x, next.y)) continue;
            if (Pathfinder::stepCost(tileMap, next, Pathfinder::Mover::Dead) < 0.0f) continue;
            const float d = node.d + std::max(1.0f, enter);
            if (d < mDistance[static_cast<std::size_t>(ni)])
            {
                mDistance[static_cast<std::size_t>(ni)] = d;
                open.push({d, ni});
            }
        }
    }
}

float FlowField::distance(const glm::ivec2& tile) const
{
    const int i = index(tile);
    if (i < 0 || mSize == 0) return -1.0f;
    const float d = mDistance[static_cast<std::size_t>(i)];
    return std::isinf(d) ? -1.0f : d;
}

bool FlowField::nextStep(const TileMap& tileMap, const glm::ivec2& from, glm::ivec2& out) const
{
    const float here = distance(from);
    if (here <= 0.0f) return false;
    float best = here;
    bool found = false;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0) continue;
            const glm::ivec2 next = from + glm::ivec2(dx, dy);
            float d = distance(next);
            if (d < 0.0f) continue;
            if (dx != 0 && dy != 0)
            {
                // A diagonal only across open ground: no cutting a corner.
                if (tileMap.isSolid(from.x + dx, from.y) || tileMap.isSolid(from.x, from.y + dy) ||
                    tileMap.isSolid(next.x, next.y))
                {
                    continue;
                }
                d += 0.41f;
            }
            if (d < best - 0.01f)
            {
                best = d;
                out = next;
                found = true;
            }
        }
    }
    return found;
}
