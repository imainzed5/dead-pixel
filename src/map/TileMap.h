#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

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

    [[nodiscard]] bool gidToUvRect(int gid, glm::vec4& outUvRect) const;
    [[nodiscard]] int groundGid(int tileX, int tileY) const;
    [[nodiscard]] bool isSolid(int tileX, int tileY) const;
    [[nodiscard]] bool isSolidAtWorldRect(float left, float top, float right, float bottom) const;
    [[nodiscard]] SurfaceType getSurfaceType(int tileX, int tileY) const;
    [[nodiscard]] int getDistrictId(int tileX, int tileY) const;

    [[nodiscard]] int width() const { return mWidth; }
    [[nodiscard]] int height() const { return mHeight; }
    [[nodiscard]] int tileWidth() const { return mTileWidth; }
    [[nodiscard]] int tileHeight() const { return mTileHeight; }
    [[nodiscard]] glm::vec2 pixelSize() const;
    [[nodiscard]] const TilesetInfo& tileset() const { return mTileset; }

private:
    [[nodiscard]] bool inBounds(int tileX, int tileY) const;
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
};
