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
    [[nodiscard]] bool isSolid(int tileX, int tileY) const;
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
};
