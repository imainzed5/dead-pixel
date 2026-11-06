#include "map/TileMap.h"

#include <algorithm>
#include <array>
#include <cmath>

void TileMap::resize(int width, int height, int tileWidth, int tileHeight)
{
    mWidth = width;
    mHeight = height;
    mTileWidth = tileWidth;
    mTileHeight = tileHeight;

    const std::size_t size = static_cast<std::size_t>(mWidth * mHeight);
    mGroundLayer.assign(size, 0);
    mCollisionLayer.assign(size, 0);
    mSurfaceLayer.assign(size, 0);
    mDistrictLayer.assign(size, 0);
    mBuildingLayer.assign(size, 0);
    mDoorLayer.assign(size, DoorState::None);
    mDoorBoards.assign(size, 0);
    mFurnitureLayer.assign(size, 0);
}

void TileMap::setTileset(const TilesetInfo& tileset)
{
    mTileset = tileset;
}

void TileMap::setGroundLayer(std::vector<int> layerData)
{
    mGroundLayer = std::move(layerData);
}

void TileMap::setCollisionLayer(std::vector<int> layerData)
{
    mCollisionLayer = std::move(layerData);
}

void TileMap::setSurfaceLayer(std::vector<int> layerData)
{
    mSurfaceLayer = std::move(layerData);
}

void TileMap::setDistrictLayer(std::vector<int> layerData)
{
    mDistrictLayer = std::move(layerData);
}

void TileMap::setBuildingLayer(std::vector<int> layerData)
{
    mBuildingLayer = std::move(layerData);
}

bool TileMap::gidToUvRect(int gid, glm::vec4& outUvRect) const
{
    if (gid < mTileset.firstGid)
    {
        return false;
    }

    const int localId = gid - mTileset.firstGid;
    if (localId < 0 || localId >= mTileset.tileCount || mTileset.columns <= 0)
    {
        return false;
    }

    const int tileX = localId % mTileset.columns;
    const int tileY = localId / mTileset.columns;

    const float uvWidth = static_cast<float>(mTileset.tileWidth) / static_cast<float>(mTileset.imageWidth);
    const float uvHeight = static_cast<float>(mTileset.tileHeight) / static_cast<float>(mTileset.imageHeight);

    outUvRect.x = static_cast<float>(tileX) * uvWidth;
    outUvRect.y = static_cast<float>(tileY) * uvHeight;
    outUvRect.z = uvWidth;
    outUvRect.w = uvHeight;
    return true;
}

int TileMap::groundGid(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return 0;
    }

    return mGroundLayer[static_cast<std::size_t>(tileIndex(tileX, tileY))];
}

bool TileMap::isSolid(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return true;
    }

    const auto index = static_cast<std::size_t>(tileIndex(tileX, tileY));
    return blocksSight(tileX, tileY) || (index < mFurnitureLayer.size() && mFurnitureLayer[index] != 0);
}

bool TileMap::blocksSight(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return true;
    }

    const auto index = static_cast<std::size_t>(tileIndex(tileX, tileY));
    return mCollisionLayer[index] != 0 || (index < mDoorLayer.size() && mDoorLayer[index] == DoorState::Closed);
}

void TileMap::setFurniture(int tileX, int tileY, bool present)
{
    if (!inBounds(tileX, tileY))
    {
        return;
    }
    mFurnitureLayer.resize(static_cast<std::size_t>(mWidth * mHeight), 0);
    mFurnitureLayer[static_cast<std::size_t>(tileIndex(tileX, tileY))] = present ? 1 : 0;
}

bool TileMap::hasFurniture(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return false;
    }
    const auto index = static_cast<std::size_t>(tileIndex(tileX, tileY));
    return index < mFurnitureLayer.size() && mFurnitureLayer[index] != 0;
}

void TileMap::clearFurniture()
{
    std::fill(mFurnitureLayer.begin(), mFurnitureLayer.end(), std::uint8_t{0});
}

void TileMap::setDoorState(int tileX, int tileY, DoorState state)
{
    if (!inBounds(tileX, tileY))
    {
        return;
    }
    mDoorLayer.resize(static_cast<std::size_t>(mWidth * mHeight), DoorState::None);
    mDoorLayer[static_cast<std::size_t>(tileIndex(tileX, tileY))] = state;
}

void TileMap::setDoorBoards(int tileX, int tileY, int boards)
{
    if (!inBounds(tileX, tileY))
    {
        return;
    }
    mDoorBoards.resize(static_cast<std::size_t>(mWidth * mHeight), 0);
    mDoorBoards[static_cast<std::size_t>(tileIndex(tileX, tileY))] =
        static_cast<std::uint8_t>(std::clamp(boards, 0, 255));
}

int TileMap::doorBoards(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return 0;
    }
    const auto index = static_cast<std::size_t>(tileIndex(tileX, tileY));
    return index < mDoorBoards.size() ? mDoorBoards[index] : 0;
}

DoorState TileMap::doorState(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return DoorState::None;
    }
    const auto index = static_cast<std::size_t>(tileIndex(tileX, tileY));
    return index < mDoorLayer.size() ? mDoorLayer[index] : DoorState::None;
}

bool TileMap::isSolidAtWorldRect(float left, float top, float right, float bottom) const
{
    const int minTileX = static_cast<int>(std::floor(left / static_cast<float>(mTileWidth)));
    const int maxTileX = static_cast<int>(std::floor((right - 0.001f) / static_cast<float>(mTileWidth)));
    const int minTileY = static_cast<int>(std::floor(top / static_cast<float>(mTileHeight)));
    const int maxTileY = static_cast<int>(std::floor((bottom - 0.001f) / static_cast<float>(mTileHeight)));

    for (int tileY = minTileY; tileY <= maxTileY; ++tileY)
    {
        for (int tileX = minTileX; tileX <= maxTileX; ++tileX)
        {
            if (isSolid(tileX, tileY))
            {
                return true;
            }
        }
    }

    return false;
}

bool TileMap::isSolidAtWorldDiamond(float centerX, float centerY, float radiusX, float radiusY) const
{
    const float rx = std::max(1.0f, radiusX);
    const float ry = std::max(1.0f, radiusY);

    // Sample along a diamond footprint to better match isometric movement feel.
    const std::array<glm::vec2, 13> samples = {
        glm::vec2(0.0f, 0.0f),
        glm::vec2(-1.0f, 0.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(0.0f, -1.0f),
        glm::vec2(0.0f, 1.0f),
        glm::vec2(-0.60f, -0.35f),
        glm::vec2(0.60f, -0.35f),
        glm::vec2(-0.60f, 0.35f),
        glm::vec2(0.60f, 0.35f),
        glm::vec2(-0.30f, -0.70f),
        glm::vec2(0.30f, -0.70f),
        glm::vec2(-0.30f, 0.70f),
        glm::vec2(0.30f, 0.70f)
    };

    for (const glm::vec2& sample : samples)
    {
        const float worldX = centerX + sample.x * rx;
        const float worldY = centerY + sample.y * ry;
        const int tileX = static_cast<int>(std::floor(worldX / static_cast<float>(mTileWidth)));
        const int tileY = static_cast<int>(std::floor(worldY / static_cast<float>(mTileHeight)));
        if (isSolid(tileX, tileY))
        {
            return true;
        }
    }

    return false;
}

SurfaceType TileMap::getSurfaceType(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return SurfaceType::Default;
    }

    const int value = mSurfaceLayer[static_cast<std::size_t>(tileIndex(tileX, tileY))];
    switch (value)
    {
    case 1:
        return SurfaceType::Carpet;
    case 2:
        return SurfaceType::Gravel;
    case 3:
        return SurfaceType::Grass;
    case 4:
        return SurfaceType::Metal;
    default:
        return SurfaceType::Default;
    }
}

glm::vec2 TileMap::pixelSize() const
{
    return glm::vec2(
        static_cast<float>(mWidth * mTileWidth),
        static_cast<float>(mHeight * mTileHeight));
}

bool TileMap::inBounds(int tileX, int tileY) const
{
    return tileX >= 0 && tileX < mWidth && tileY >= 0 && tileY < mHeight;
}

int TileMap::tileIndex(int tileX, int tileY) const
{
    return tileY * mWidth + tileX;
}

int TileMap::getDistrictId(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return 0;
    }

    return mDistrictLayer[static_cast<std::size_t>(tileIndex(tileX, tileY))];
}

int TileMap::getBuildingId(int tileX, int tileY) const
{
    if (!inBounds(tileX, tileY))
    {
        return 0;
    }
    const auto index = static_cast<std::size_t>(tileIndex(tileX, tileY));
    return index < mBuildingLayer.size() ? mBuildingLayer[index] : 0;
}

bool TileMap::isIndoorsAtWorld(float worldX, float worldY) const
{
    if (mTileWidth <= 0 || mTileHeight <= 0)
    {
        return false;
    }
    const int tileX = static_cast<int>(std::floor(worldX / static_cast<float>(mTileWidth)));
    const int tileY = static_cast<int>(std::floor(worldY / static_cast<float>(mTileHeight)));
    return isIndoors(tileX, tileY);
}
