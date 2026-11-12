#pragma once

#include <vector>

#include <glm/vec2.hpp>

class TileMap;

// Things that stand or lie about outdoors: trees, bushes, street furniture,
// litter. Planted from the tile map alone, so the same town always grows the
// same scenery and none of it needs saving.
struct SceneryPiece
{
    glm::vec2 position{0.0f}; // where it touches the ground, world pixels
    const char* sprite = "";  // atlas name (tools/scenery.py)
    bool flat = false;        // lies on the ground, under everything that stands
    bool blocker = false;     // fills its tile: nothing walks through it
    bool tall = false;        // fades when the player is behind it
};

namespace scenery
{
// A blocker may only stand where all eight tiles around it are open ground
// outdoors with no other blocker, so scenery can never close a path.
[[nodiscard]] bool canBlock(const TileMap& map, const std::vector<unsigned char>& blocked, int tileX, int tileY);

[[nodiscard]] std::vector<SceneryPiece> plant(const TileMap& map);

[[nodiscard]] inline glm::ivec2 tileOf(const SceneryPiece& piece)
{
    return glm::ivec2(static_cast<int>(piece.position.x) / 32, static_cast<int>(piece.position.y) / 32);
}
} // namespace scenery
