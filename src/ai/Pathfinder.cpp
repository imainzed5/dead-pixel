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

float Pathfinder::stepCost(const TileMap& tileMap, const glm::ivec2& tile, Mover mover)
{
    if (!tileMap.inBounds(tile.x, tile.y))
    {
        return -1.0f;
    }
    // A closed door blocks like a wall but can be opened or broken, so a
    // route through it costs extra instead of being ruled out.
    if (tileMap.isClosedDoor(tile.x, tile.y))
    {
        return kClosedDoorCost + kBoardCost * static_cast<float>(tileMap.doorBoards(tile.x, tile.y));
    }
    if (!tileMap.isSolid(tile.x, tile.y))
    {
        return 1.0f;
    }
    if (mover == Mover::Living)
    {
        return -1.0f;
    }
    if (tileMap.hasWindow(tile.x, tile.y))
    {
        if (tileMap.windowOpen(tile.x, tile.y)) return kOpenWindowCost;
        return kWindowCost + kBoardCost * static_cast<float>(tileMap.windowBoards(tile.x, tile.y));
    }
    if (tileMap.doorwayBlocked(tile.x, tile.y))
    {
        return kBlockedDoorwayCost;
    }
    return -1.0f;
}

std::vector<glm::ivec2> Pathfinder::findPath(const TileMap& tileMap, const glm::ivec2& start, const glm::ivec2& goal,
                                             Mover mover)
{
    std::vector<glm::ivec2> path;

    if (!inBounds(tileMap, start) || !inBounds(tileMap, goal))
    {
        return path;
    }

    // A closed door (or a window, or a blocked doorway) can be the goal: it
    // is reached by battering it.
    if (stepCost(tileMap, goal, mover) < 0.0f)
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
            if (!inBounds(tileMap, nextPos))
            {
                continue;
            }
            const float stepCost = Pathfinder::stepCost(tileMap, nextPos, mover);
            if (stepCost < 0.0f)
            {
                continue;
            }

            const int nextIndex = tileIndex(nextPos.x, nextPos.y, width);
            if (closed[static_cast<std::size_t>(nextIndex)])
            {
                continue;
            }

            const float tentativeG = gScore[static_cast<std::size_t>(current.index)] + stepCost;
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
