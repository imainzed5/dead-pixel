#include "rendering/FurnitureView.h"

#include <cmath>
#include <cstdlib>

#include "rendering/Camera.h"
#include "rendering/SpriteSheet.h"
#include "rendering/StructureView.h"

namespace
{
constexpr float kTile = 32.0f;
constexpr float kIsoTileW = 64.0f;
constexpr float kIsoTileH = 32.0f;
} // namespace

void FurnitureView::build(const std::vector<FurniturePiece>& pieces)
{
    mEntries.clear();
    mEntries.reserve(pieces.size());
    for (const FurniturePiece& piece : pieces)
    {
        Entry entry;
        entry.tile = piece.tile;
        entry.sprite = furniture::spriteName(piece);
        entry.tall = furniture::info(piece.kind).tall;
        mEntries.push_back(std::move(entry));
    }
    mUvReady = false;
}

bool FurnitureView::hidesPlayer(const glm::ivec2& piece, const glm::ivec2& player)
{
    // One or two tile diagonals in front of the player, overlapping them on
    // screen: east, south, or south-east of them.
    const glm::ivec2 d = piece - player;
    return (d.x == 1 && d.y == 0) || (d.x == 0 && d.y == 1) || (d.x == 1 && d.y == 1);
}

void FurnitureView::collect(const Camera& camera, const StructureView& structures, const glm::ivec2& playerTile,
                            const SpriteSheet& sheet, std::vector<StaticSprite>& out)
{
    if (!mUvReady)
    {
        for (Entry& entry : mEntries)
        {
            entry.uv = sheet.uvRect(entry.sprite);
        }
        mUvReady = true;
    }

    const float zoom = camera.zoom();
    const glm::vec2 viewport = camera.viewportSize();
    const float tileW = kIsoTileW * zoom;
    const float tileH = kIsoTileH * zoom;
    const float up = kSpriteUp * zoom;

    for (const Entry& entry : mEntries)
    {
        if (structures.isHiddenAt(entry.tile.x, entry.tile.y))
        {
            continue;
        }
        const glm::vec2 ground = camera.worldToScreen(glm::vec2(
            static_cast<float>(entry.tile.x) * kTile + kTile * 0.5f,
            static_cast<float>(entry.tile.y) * kTile + kTile * 0.5f));
        const float left = std::round(ground.x - tileW * 0.5f);
        const float top = std::round(ground.y - tileH * 0.5f);
        if (left + tileW < 0.0f || left > viewport.x || top - up > viewport.y || top + tileH < 0.0f)
        {
            continue;
        }
        const float alpha = entry.tall && hidesPlayer(entry.tile, playerTile) ? kFadedAlpha : 1.0f;
        out.push_back(StaticSprite{
            glm::vec2(left, top - up), glm::vec2(tileW, tileH + up), entry.uv, glm::vec4(1.0f, 1.0f, 1.0f, alpha),
            static_cast<float>(entry.tile.x + entry.tile.y) * kTile});
    }
}
