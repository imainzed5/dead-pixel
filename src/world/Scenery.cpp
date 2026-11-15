#include "world/Scenery.h"

#include <cmath>
#include <cstdint>

#include "map/TileIds.h"
#include "map/TileMap.h"

namespace
{
constexpr float kTile = 32.0f;

// A repeatable number in [0, 1) for a tile and a purpose.
float roll(int x, int y, std::uint32_t salt)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093u ^ static_cast<std::uint32_t>(y) * 19349663u ^
                      salt * 83492791u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

// Smooth noise over the map: where the groves are, and what grows in them.
float drift(int x, int y, int cell, std::uint32_t salt)
{
    const float fx = static_cast<float>(x) / static_cast<float>(cell);
    const float fy = static_cast<float>(y) / static_cast<float>(cell);
    const int ix = static_cast<int>(std::floor(fx));
    const int iy = static_cast<int>(std::floor(fy));
    float tx = fx - static_cast<float>(ix);
    float ty = fy - static_cast<float>(iy);
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    const float a = roll(ix, iy, salt);
    const float b = roll(ix + 1, iy, salt);
    const float c = roll(ix, iy + 1, salt);
    const float d = roll(ix + 1, iy + 1, salt);
    return (a + (b - a) * tx) * (1.0f - ty) + (c + (d - c) * tx) * ty;
}

int baseGid(const TileMap& map, int x, int y)
{
    const int gid = map.groundGid(x, y);
    return gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
}

bool openGround(const TileMap& map, int x, int y)
{
    return map.inBounds(x, y) && !map.isIndoors(x, y) && !map.isSolid(x, y) &&
           map.doorState(x, y) == DoorState::None && baseGid(map, x, y) != TileGid::kWater &&
           baseGid(map, x, y) != 0;
}

glm::vec2 centre(int x, int y)
{
    return glm::vec2((static_cast<float>(x) + 0.5f) * kTile, (static_cast<float>(y) + 0.5f) * kTile);
}

// Somewhere on the tile, kept off its edges.
glm::vec2 scattered(int x, int y, std::uint32_t salt)
{
    return centre(x, y) + glm::vec2((roll(x, y, salt) - 0.5f) * 18.0f, (roll(x, y, salt + 1u) - 0.5f) * 18.0f);
}
} // namespace

bool scenery::canBlock(const TileMap& map, const std::vector<unsigned char>& blocked, int tileX, int tileY)
{
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const int x = tileX + dx;
            const int y = tileY + dy;
            if (!openGround(map, x, y))
            {
                return false;
            }
            if (blocked[static_cast<std::size_t>(y * map.width() + x)] != 0)
            {
                return false;
            }
        }
    }
    return true;
}

std::vector<SceneryPiece> scenery::plant(const TileMap& map)
{
    std::vector<SceneryPiece> pieces;
    const int w = map.width();
    const int h = map.height();
    std::vector<unsigned char> blocked(static_cast<std::size_t>(w * h), 0);

    const auto stand = [&](int x, int y, const char* sprite, bool tall)
    {
        blocked[static_cast<std::size_t>(y * w + x)] = 1;
        pieces.push_back(SceneryPiece{centre(x, y), sprite, false, true, tall});
    };
    const auto loose = [&](int x, int y, const char* sprite, std::uint32_t salt)
    {
        pieces.push_back(SceneryPiece{scattered(x, y, salt), sprite, false, false, false});
    };
    const auto lying = [&](int x, int y, const char* sprite, std::uint32_t salt)
    {
        pieces.push_back(SceneryPiece{scattered(x, y, salt), sprite, true, false, false});
    };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            if (!openGround(map, x, y))
            {
                continue;
            }
            const int gid = baseGid(map, x, y);
            const float r = roll(x, y, 1u);

            if (TileGid::isGrass(gid))
            {
                // Trees gather in groves and thin out between them.
                const float grove = drift(x, y, 7, 11u);
                const float treeChance = grove > 0.62f ? 0.30f : (grove > 0.48f ? 0.07f : 0.012f);
                if (r < treeChance)
                {
                    if (canBlock(map, blocked, x, y))
                    {
                        const float wood = drift(x, y, 13, 23u);
                        const float kind = roll(x, y, 2u);
                        const char* sprite = "scn_tree_oak_a";
                        if (kind < 0.07f)
                            sprite = "scn_tree_dead";
                        else if (wood > 0.56f)
                            sprite = kind < 0.85f ? "scn_tree_pine" : "scn_tree_oak_b";
                        else if (kind < 0.20f)
                            sprite = "scn_tree_autumn";
                        else if (kind < 0.60f)
                            sprite = "scn_tree_oak_b";
                        stand(x, y, sprite, true);
                    }
                    else if (roll(x, y, 3u) < 0.5f)
                    {
                        loose(x, y, roll(x, y, 4u) < 0.5f ? "scn_bush_a" : "scn_bush_b", 5u);
                    }
                    continue;
                }
                const float s = roll(x, y, 6u);
                const float undergrowth = grove > 0.48f ? 2.0f : 1.0f;
                if (s < 0.030f * undergrowth)
                    loose(x, y, roll(x, y, 4u) < 0.5f ? "scn_bush_a" : "scn_bush_b", 5u);
                else if (s < 0.030f * undergrowth + 0.012f)
                    loose(x, y, roll(x, y, 4u) < 0.4f ? "scn_rock_big" : "scn_rock_small", 5u);
                else if (s < 0.030f * undergrowth + 0.016f)
                    loose(x, y, "scn_stump", 5u);
                else if (s < 0.20f)
                    loose(x, y, roll(x, y, 4u) < 0.5f ? "scn_tuft_a" : "scn_tuft_b", 5u);
                else if (s < 0.24f)
                    loose(x, y, roll(x, y, 4u) < 0.5f ? "scn_flowers_a" : "scn_flowers_b", 5u);
                else if (s < 0.24f + 0.03f * undergrowth)
                    lying(x, y, "scn_leaves", 5u);
            }
            else if (gid == TileGid::kSidewalk)
            {
                if (r < 0.022f && canBlock(map, blocked, x, y))
                    stand(x, y, "scn_lamppost", false);
                else if (r < 0.030f && canBlock(map, blocked, x, y))
                    stand(x, y, "scn_hydrant", false);
                else if (r < 0.042f && canBlock(map, blocked, x, y))
                    stand(x, y, "scn_trash_can", false);
                else if (r < 0.10f)
                    lying(x, y, "scn_weeds", 5u);
                else if (r < 0.125f)
                    lying(x, y, roll(x, y, 4u) < 0.6f ? "scn_paper" : "scn_can", 5u);
                else if (r < 0.14f)
                    lying(x, y, "scn_leaves", 5u);
            }
            else if (gid == TileGid::kAsphalt)
            {
                if (r < 0.045f)
                    lying(x, y, "scn_crack", 5u);
                else if (r < 0.065f)
                    lying(x, y, roll(x, y, 4u) < 0.6f ? "scn_paper" : "scn_can", 5u);
                else if (r < 0.080f)
                    lying(x, y, "scn_puddle", 5u);
                else if (r < 0.086f)
                    pieces.push_back(SceneryPiece{centre(x, y), "scn_manhole", true, false, false});
            }
            else if (gid == TileGid::kDirt || gid == TileGid::kGravel)
            {
                if (r < 0.03f)
                    loose(x, y, "scn_rock_small", 5u);
                else if (r < 0.08f)
                    lying(x, y, "scn_weeds", 5u);
                else if (r < 0.11f)
                    loose(x, y, roll(x, y, 4u) < 0.5f ? "scn_tuft_a" : "scn_tuft_b", 5u);
            }
        }
    }
    return pieces;
}
