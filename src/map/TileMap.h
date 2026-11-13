#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>
#include <vector>

enum class SurfaceType
{
    Default,
    Carpet,
    Gravel,
    Grass,
    Metal
};

// A doorway's door. A closed door is solid like a wall, except that a path
// may still lead through it (to be opened, or battered down).
enum class DoorState : std::uint8_t
{
    None = 0, // not a doorway
    Open,
    Closed,
    Broken
};

// A window in an outside wall. The wall tile stays solid; the window decides
// whether sight passes (glass, an open hole) and how the dead get in.
enum class WindowState : std::uint8_t
{
    None = 0, // no window
    Intact,
    Broken
};

struct TilesetInfo
{
    int firstGid = 1;
    int tileCount = 0;
    int columns = 1;
    int tileWidth = 32;
    int tileHeight = 32;
    int imageWidth = 32;
    int imageHeight = 32;
    std::string imagePath;
};

class TileMap
{
public:
    void resize(int width, int height, int tileWidth, int tileHeight);

    void setTileset(const TilesetInfo& tileset);
    void setGroundLayer(std::vector<int> layerData);
    void setCollisionLayer(std::vector<int> layerData);
    void setSurfaceLayer(std::vector<int> layerData);
    void setDistrictLayer(std::vector<int> layerData);
    // Per-tile building id (index + 1) for building footprints, 0 outside.
    // Covers interior floors, walls and doorways.
    void setBuildingLayer(std::vector<int> layerData);

    [[nodiscard]] bool gidToUvRect(int gid, glm::vec4& outUvRect) const;
    [[nodiscard]] int groundGid(int tileX, int tileY) const;
    // Blocks movement: walls, closed doors, furniture and standing scenery.
    [[nodiscard]] bool isSolid(int tileX, int tileY) const;
    // Blocks thrown things and fire, and counts as a wall for drawing and
    // weather: walls (windows included) and closed doors, not furniture.
    // For what the eye can see through, use blocksView.
    [[nodiscard]] bool blocksSight(int tileX, int tileY) const;
    [[nodiscard]] bool isSolidAtWorldRect(float left, float top, float right, float bottom) const;
    [[nodiscard]] bool isSolidAtWorldDiamond(float centerX, float centerY, float radiusX, float radiusY) const;
    [[nodiscard]] SurfaceType getSurfaceType(int tileX, int tileY) const;
    [[nodiscard]] int getDistrictId(int tileX, int tileY) const;
    [[nodiscard]] int getBuildingId(int tileX, int tileY) const;
    [[nodiscard]] bool isIndoors(int tileX, int tileY) const { return getBuildingId(tileX, tileY) > 0; }
    [[nodiscard]] bool isIndoorsAtWorld(float worldX, float worldY) const;

    void setDoorState(int tileX, int tileY, DoorState state);
    [[nodiscard]] DoorState doorState(int tileX, int tileY) const;
    [[nodiscard]] bool isClosedDoor(int tileX, int tileY) const { return doorState(tileX, tileY) == DoorState::Closed; }
    // Boards nailed across a closed door (0 = none).
    void setDoorBoards(int tileX, int tileY, int boards);
    [[nodiscard]] int doorBoards(int tileX, int tileY) const;

    // Windows: state, boards (0..2) and a drawn curtain. A window with glass
    // or none, no boards and no curtain lets sight through (see blocksView).
    void setWindowState(int tileX, int tileY, WindowState state);
    [[nodiscard]] WindowState windowState(int tileX, int tileY) const;
    [[nodiscard]] bool hasWindow(int tileX, int tileY) const { return windowState(tileX, tileY) != WindowState::None; }
    void setWindowBoards(int tileX, int tileY, int boards);
    [[nodiscard]] int windowBoards(int tileX, int tileY) const;
    void setWindowCovered(int tileX, int tileY, bool covered);
    [[nodiscard]] bool windowCovered(int tileX, int tileY) const;
    // Sight passes this window: no boards, no curtain.
    [[nodiscard]] bool windowClear(int tileX, int tileY) const;
    // A broken window with no boards: it can be climbed through.
    [[nodiscard]] bool windowOpen(int tileX, int tileY) const;
    // Blocks a line of sight: walls (not clear windows) and closed doors.
    [[nodiscard]] bool blocksView(int tileX, int tileY) const;

    // A piece of furniture pushed into a doorway: the dead must break it.
    void setDoorwayBlocked(int tileX, int tileY, bool blocked);
    [[nodiscard]] bool doorwayBlocked(int tileX, int tileY) const;

    // Furniture stands on a whole tile and blocks movement through it.
    void setFurniture(int tileX, int tileY, bool present);
    [[nodiscard]] bool hasFurniture(int tileX, int tileY) const;
    void clearFurniture();

    // Scenery that fills a tile (a tree, a lamp post) blocks movement too.
    void setScenery(int tileX, int tileY, bool present);
    [[nodiscard]] bool hasScenery(int tileX, int tileY) const;
    void clearScenery();

    [[nodiscard]] int width() const { return mWidth; }
    [[nodiscard]] int height() const { return mHeight; }
    [[nodiscard]] int tileWidth() const { return mTileWidth; }
    [[nodiscard]] int tileHeight() const { return mTileHeight; }
    [[nodiscard]] glm::vec2 pixelSize() const;
    [[nodiscard]] const TilesetInfo& tileset() const { return mTileset; }
    [[nodiscard]] bool inBounds(int tileX, int tileY) const;

private:
    [[nodiscard]] int tileIndex(int tileX, int tileY) const;

    int mWidth = 0;
    int mHeight = 0;
    int mTileWidth = 32;
    int mTileHeight = 32;

    TilesetInfo mTileset{};
    std::vector<int> mGroundLayer;
    std::vector<int> mCollisionLayer;
    std::vector<int> mSurfaceLayer;
    std::vector<int> mDistrictLayer;
    std::vector<int> mBuildingLayer;
    std::vector<DoorState> mDoorLayer;
    std::vector<std::uint8_t> mDoorBoards;
    std::vector<WindowState> mWindowLayer;
    std::vector<std::uint8_t> mWindowBoards;
    std::vector<std::uint8_t> mWindowCovered;
    std::vector<std::uint8_t> mBlockedDoorway;
    std::vector<std::uint8_t> mFurnitureLayer;
    std::vector<std::uint8_t> mSceneryLayer;
};
