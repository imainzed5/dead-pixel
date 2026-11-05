// Furniture: placing it in the town, and keeping run spawns off it.
#include "core/GameShared.h"

using namespace gameshared;

void Game::furnishWorld()
{
    mTileMap.clearFurniture();
    mFurniture = furniture::furnish(mMapGenerator.layoutData(), static_cast<std::uint32_t>(mSaveManager.worldSeed()));
    for (const FurniturePiece& piece : mFurniture)
    {
        mTileMap.setFurniture(piece.tile.x, piece.tile.y, true);
    }
    mFurnitureView.build(mFurniture);
}

void Game::nudgeSpawnsOffFurniture()
{
    // Spawn tiles are picked from the bare layout; anything that landed on a
    // piece of furniture moves to a free tile beside it, in the same building.
    const auto nudge = [&](std::vector<glm::ivec2>& list)
    {
        for (glm::ivec2& tile : list)
        {
            if (!mTileMap.hasFurniture(tile.x, tile.y)) continue;
            const int building = mTileMap.getBuildingId(tile.x, tile.y);
            constexpr int kDirs[8][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
            for (const auto& d : kDirs)
            {
                const glm::ivec2 next(tile.x + d[0], tile.y + d[1]);
                if (mTileMap.isSolid(next.x, next.y) || mTileMap.getBuildingId(next.x, next.y) != building) continue;
                tile = next;
                break;
            }
        }
    };
    for (std::vector<glm::ivec2>* list :
         {&mSpawnData.zombieSpawns, &mSpawnData.foodSpawns, &mSpawnData.waterSpawns, &mSpawnData.weaponSpawns,
          &mSpawnData.bandageSpawns, &mSpawnData.medicalCacheSpawns, &mSpawnData.supplyCacheSpawns,
          &mSpawnData.sleepSpawns, &mSpawnData.decorIndoorSpawns})
    {
        nudge(*list);
    }
}
