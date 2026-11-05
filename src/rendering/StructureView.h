#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "map/LayoutData.h"
#include "rendering/StaticSprite.h"

class Camera;
class SpriteSheet;
class TileMap;

enum class StructureKind : std::uint8_t
{
    None,
    Wall,
    Doorway
};

// Which way a wall line runs; decides which face carries windows and doorways.
enum class StructureRun : std::uint8_t
{
    Corner,
    AlongX, // features on the left (south-facing) face
    AlongY  // features on the right (east-facing) face
};

enum class StructureMaterial : std::uint8_t
{
    Plaster = 0, // houses
    Brick = 1,   // shops; also the map boundary
    Metal = 2    // industrial
};

struct StructureTile
{
    StructureKind kind = StructureKind::None;
    StructureRun run = StructureRun::Corner;
    StructureMaterial material = StructureMaterial::Plaster;
    bool perimeter = false; // on the outside edge of a building (or not in one)
    bool window = false;
    int building = 0;       // building id (index + 1), 0 outside
};

// Atlas UVs for every structure piece, indexed by StructureMaterial.
struct StructureSprites
{
    std::array<glm::vec4, 3> wall{};
    std::array<glm::vec4, 3> wallLow{};
    std::array<glm::vec4, 3> windowLeft{};
    std::array<glm::vec4, 3> windowRight{};
    std::array<glm::vec4, 3> doorLeft{};
    std::array<glm::vec4, 3> doorRight{};
    std::array<glm::vec4, 3> roof{};

    [[nodiscard]] static StructureSprites fromSheet(const SpriteSheet& sheet);
};

// Walls with height, doorways, roofs and the cut-away around the player.
//
//  - Solid wall tiles are drawn as blocks that sort with entities by depth.
//  - Each building has a flat roof that hides its interior, and everything in
//    it, until the player is inside or standing at one of its doorways.
//  - Walls that would hide the player, and the walls of the player's building
//    that are in front of them, drop to low stubs.
class StructureView
{
public:
    // Must match WALL_H / WALL_LOW_H in tools/generate_sprites.py.
    static constexpr float kWallHeight = 56.0f;
    static constexpr float kWallLowHeight = 8.0f;
    static constexpr float kRoofFadePerSecond = 4.0f;
    // Cut-away band in front of the player, in tile diagonals.
    static constexpr int kCutDepth = 5;
    static constexpr int kCutWidth = 2;

    void build(const TileMap& map, const LayoutData& layout);
    void update(float dtSeconds, const glm::ivec2& playerTile);
    void snapRoofs(const glm::ivec2& playerTile);

    void collect(const Camera& camera, const glm::ivec2& playerTile, const StructureSprites& sprites,
                 std::vector<StaticSprite>& out) const;

    // A thing standing on this tile is under a closed roof and not drawn.
    [[nodiscard]] bool isHiddenAt(int tileX, int tileY) const;
    // Wall blocks cover their whole tile, so the ground pass can skip it.
    [[nodiscard]] bool coversGround(int tileX, int tileY) const;
    // A wall at `wall` is drawn as a low stub while the player is at `player`.
    [[nodiscard]] bool isCutAway(const glm::ivec2& wall, const glm::ivec2& player) const;
    // Building whose roof is open for a player standing here (0 = none).
    [[nodiscard]] int openBuilding(const glm::ivec2& playerTile) const;

    [[nodiscard]] const StructureTile& tile(int tileX, int tileY) const;
    [[nodiscard]] float roofAlpha(int building) const;
    [[nodiscard]] int buildingCount() const { return static_cast<int>(mRoofAlpha.size()) - 1; }

private:
    [[nodiscard]] bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < mWidth && y < mHeight; }
    [[nodiscard]] std::size_t index(int x, int y) const
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(mWidth) + static_cast<std::size_t>(x);
    }

    int mWidth = 0;
    int mHeight = 0;
    std::vector<StructureTile> mTiles;
    std::vector<StructureMaterial> mBuildingMaterial; // per building id, [0] unused
    std::vector<float> mRoofAlpha;                    // per building id, [0] unused
};
