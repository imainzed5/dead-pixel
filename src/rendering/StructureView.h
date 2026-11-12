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

// Where on its tile a wall stands. A building's walls are thin: an outside
// wall runs along the edge of its tile nearest the rooms, a corner is a post
// in that corner of the tile, and a wall between rooms runs down the middle.
// A wall that belongs to no building also runs down the middle of its tiles.
enum class WallPlace : std::uint8_t
{
    Block,
    EdgeN, // along the tile's -y edge
    EdgeS,
    EdgeW, // along the tile's -x edge
    EdgeE,
    PostNW,
    PostNE,
    PostSW,
    PostSE,
    Centre
};

struct StructureTile
{
    StructureKind kind = StructureKind::None;
    StructureRun run = StructureRun::Corner;
    StructureMaterial material = StructureMaterial::Plaster;
    bool perimeter = false; // on the outside edge of a building (or not in one)
    bool window = false;
    int building = 0;       // building id (index + 1), 0 outside
    WallPlace place = WallPlace::Block;
    std::uint8_t arms = 0;  // Centre walls: 1 north, 2 south, 4 west, 8 east
    int underGid = 0;       // ground drawn under a thin wall (0: the tile's own)
    // The roof piece over this tile: how many steps up from the eaves it
    // starts, and which sides of the building it slopes up from (1 west,
    // 2 east, 4 north, 8 south; 0 with roofed set is the flat top).
    bool roofed = false;
    std::uint8_t roofLevel = 0;
    std::uint8_t roofMask = 0;
};

// Thin wall pieces of one finish; [0] runs along x, [1] along y.
struct WallPieces
{
    std::array<glm::vec4, 2> full{};
    std::array<glm::vec4, 2> window{};
    std::array<glm::vec4, 2> door{};
    std::array<glm::vec4, 2> doorClosed{};
    std::array<glm::vec4, 2> doorBroken{};
    std::array<glm::vec4, 2> half{};
    std::array<glm::vec4, 2> low{};
    std::array<glm::vec4, 2> halfLow{};
    std::array<glm::vec4, 2> doorLow{};
    glm::vec4 post{};
    glm::vec4 postLow{};
};

// Atlas UVs for every structure piece, indexed by StructureMaterial.
struct StructureSprites
{
    // Whole blocks, for walls outside buildings.
    std::array<glm::vec4, 3> wall{};
    std::array<glm::vec4, 3> wallLow{};
    // Thin walls: the three outside materials, then the indoor finish.
    static constexpr std::size_t kInterior = 3;
    std::array<WallPieces, 4> pieces{};
    // Planks over a closed door, by count (index 0 = one board).
    std::array<glm::vec4, 3> boardsLeft{};
    std::array<glm::vec4, 3> boardsRight{};
    // A plank across the cut-away stub of a boarded door.
    glm::vec4 boardsLowLeft{};
    glm::vec4 boardsLowRight{};
    // Roof pieces by material and StructureTile::roofMask.
    std::array<std::array<glm::vec4, 16>, 3> roof{};

    [[nodiscard]] static StructureSprites fromSheet(const SpriteSheet& sheet);
};

// Walls with height, doorways, roofs and the cut-away around the player.
//
//  - A building's walls are thin pieces that join into continuous walls, the
//    outside finish on the faces seen from the street and painted plaster on
//    those seen from the rooms. They sort with entities by depth.
//  - Each building has a hipped roof with eaves that hides its interior, and
//    everything in it, until the player is inside or standing at one of its
//    doorways. A roof the player walks behind thins out.
//  - Walls that would hide the player, and the walls of the player's building
//    that are in front of them, drop to low stubs.
class StructureView
{
public:
    // Must match WALL_H / WALL_LOW_H in tools/generate_sprites.py.
    static constexpr float kWallHeight = 56.0f;
    static constexpr float kWallLowHeight = 8.0f;
    // Must match SLAB_T, ROOF_RISE and ROOF_FASCIA in tools/generate_sprites.py.
    static constexpr float kWallThickness = 8.0f;
    static constexpr std::array<float, 3> kRoofRise{12.0f, 5.0f, 8.0f}; // by material
    static constexpr float kRoofFascia = 3.0f;
    static constexpr int kRoofMaxLevel = 4; // steps up before the roof flattens
    static constexpr float kRoofThinAlpha = 0.3f;
    static constexpr float kRoofFadePerSecond = 4.0f;
    // Cut-away band in front of the player, in tile diagonals.
    static constexpr int kCutDepth = 5;
    static constexpr int kCutWidth = 2;

    void build(const TileMap& map, const LayoutData& layout);
    void update(float dtSeconds, const glm::ivec2& playerTile);
    void snapRoofs(const glm::ivec2& playerTile);

    // `doors` gives each doorway's door state; without it doorways draw open.
    void collect(const Camera& camera, const glm::ivec2& playerTile, const StructureSprites& sprites,
                 std::vector<StaticSprite>& out, const TileMap* doors = nullptr) const;

    // A thing standing on this tile is under a closed roof and not drawn.
    [[nodiscard]] bool isHiddenAt(int tileX, int tileY) const;
    // Wall blocks cover their whole tile, so the ground pass can skip it.
    [[nodiscard]] bool coversGround(int tileX, int tileY) const;
    // The ground to draw under a thin wall (0: draw the tile's own).
    [[nodiscard]] int groundUnder(int tileX, int tileY) const;
    // A wall or doorway stands along this edge of the tile (0 the -x edge,
    // 1 -y, 2 +x, 3 +y). A block stands along all four.
    [[nodiscard]] bool wallAtEdge(int tileX, int tileY, int edge) const;
    // A roof piece over `roofTile` would hide the player standing outside.
    [[nodiscard]] static bool roofHidesPlayer(const glm::ivec2& roofTile, const glm::ivec2& player);
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
