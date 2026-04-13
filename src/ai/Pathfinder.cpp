#include "ai/Pathfinder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace
{
struct Node
{
    int index = 0;
    float fScore = 0.0f;

    bool operator<(const Node& other) const
    {
        return fScore > other.fScore;
    }
};

int tileIndex(int x, int y, int width)
{
    return y * width + x;
}

bool inBounds(const TileMap& tileMap, const glm::ivec2& p)
{
    return p.x >= 0 && p.x < tileMap.width() && p.y >= 0 && p.y < tileMap.height();
}
}

std::vector<glm::ivec2> Pathfinder::findPath(const TileMap& tileMap, const glm::ivec2& start, const glm::ivec2& goal)
{
    std::vector<glm::ivec2> path;

    if (!inBounds(tileMap, start) || !inBounds(tileMap, goal))
    {
        return path;
    }

    if (tileMap.isSolid(goal.x, goal.y))
    {
        return path;
    }

    if (start == goal)
    {
        path.push_back(start);
        return path;
    }

    const int width = tileMap.width();
    const int height = tileMap.height();
    const int nodeCount = width * height;

    std::vector<float> gScore(static_cast<std::size_t>(nodeCount), std::numeric_limits<float>::infinity());
    std::vector<float> fScore(static_cast<std::size_t>(nodeCount), std::numeric_limits<float>::infinity());
    std::vector<int> cameFrom(static_cast<std::size_t>(nodeCount), -1);
    std::vector<bool> closed(static_cast<std::size_t>(nodeCount), false);

    const int startIndex = tileIndex(start.x, start.y, width);
    const int goalIndex = tileIndex(goal.x, goal.y, width);

    gScore[startIndex] = 0.0f;
    fScore[startIndex] = heuristic(start, goal);

    std::priority_queue<Node> openSet;
    openSet.push(Node{startIndex, fScore[startIndex]});

    const glm::ivec2 neighbors[4] = {
        glm::ivec2(1, 0),
        glm::ivec2(-1, 0),
        glm::ivec2(0, 1),
        glm::ivec2(0, -1)};

    while (!openSet.empty())
    {
        const Node current = openSet.top();
        openSet.pop();

        if (closed[static_cast<std::size_t>(current.index)])
        {
            continue;
        }

        if (current.index == goalIndex)
        {
            int trace = goalIndex;
            while (trace >= 0)
            {
                const int x = trace % width;
                const int y = trace / width;
                path.push_back(glm::ivec2(x, y));
                trace = cameFrom[static_cast<std::size_t>(trace)];
            }

            std::reverse(path.begin(), path.end());
            return path;
        }

        closed[static_cast<std::size_t>(current.index)] = true;

        const int currentX = current.index % width;
        const int currentY = current.index / width;
        const glm::ivec2 currentPos(currentX, currentY);

        for (const glm::ivec2& delta : neighbors)
        {
            const glm::ivec2 nextPos = currentPos + delta;
            if (!inBounds(tileMap, nextPos) || tileMap.isSolid(nextPos.x, nextPos.y))
            {
                continue;
            }

            const int nextIndex = tileIndex(nextPos.x, nextPos.y, width);
            if (closed[static_cast<std::size_t>(nextIndex)])
            {
                continue;
            }

            const float tentativeG = gScore[static_cast<std::size_t>(current.index)] + 1.0f;
            if (tentativeG < gScore[static_cast<std::size_t>(nextIndex)])
            {
                cameFrom[static_cast<std::size_t>(nextIndex)] = current.index;
                gScore[static_cast<std::size_t>(nextIndex)] = tentativeG;
                fScore[static_cast<std::size_t>(nextIndex)] = tentativeG + heuristic(nextPos, goal);
                openSet.push(Node{nextIndex, fScore[static_cast<std::size_t>(nextIndex)]});
            }
        }
    }

    return path;
}

float Pathfinder::heuristic(const glm::ivec2& from, const glm::ivec2& to)
{
    const float dx = static_cast<float>(to.x - from.x);
    const float dy = static_cast<float>(to.y - from.y);
    return std::fabs(dx) + std::fabs(dy);
}
