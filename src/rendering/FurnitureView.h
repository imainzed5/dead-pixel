#pragma once

#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "rendering/StaticSprite.h"
#include "world/Furniture.h"

class Camera;
class SpriteSheet;
class StructureView;

// Draws furniture as upright sprites that sort with walls and characters.
// Pieces under a closed roof are hidden with the rest of the interior, and
// tall pieces fade while they stand in front of the player.
class FurnitureView
{
public:
    // Must match FURN_UP in tools/generate_sprites.py: sprite pixels above
    // the top corner of the tile diamond.
    static constexpr float kSpriteUp = 48.0f;
    static constexpr float kFadedAlpha = 0.4f;

    void build(const std::vector<FurniturePiece>& pieces);
    void collect(const Camera& camera, const StructureView& structures, const glm::ivec2& playerTile,
                 const SpriteSheet& sheet, std::vector<StaticSprite>& out);

    // A tall piece at `piece` would hide the player standing at `player`.
    [[nodiscard]] static bool hidesPlayer(const glm::ivec2& piece, const glm::ivec2& player);

private:
    struct Entry
    {
        glm::ivec2 tile{0};
        std::string sprite;
        glm::vec4 uv{0.0f};
        bool tall = false;
    };
    std::vector<Entry> mEntries;
    bool mUvReady = false; // the sprite sheet loads after the world
};
