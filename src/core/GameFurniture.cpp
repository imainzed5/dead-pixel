// Furniture: placing it in the town, and keeping run spawns off it.
#include "core/GameShared.h"

using namespace gameshared;

void Game::furnishWorld()
{
    mFurniture = furniture::furnish(mMapGenerator.layoutData(), static_cast<std::uint32_t>(mSaveManager.worldSeed()));
    // What earlier lives moved and broke up.
    furnitureuse::Memory memory;
    for (const FurnitureMoveRecord& move : mSaveManager.furnitureMemory().moves)
    {
        memory.moves.push_back(furnitureuse::Move{glm::ivec2(move.fromX, move.fromY), glm::ivec2(move.toX, move.toY)});
    }
    for (const auto& [x, y] : mSaveManager.furnitureMemory().removed)
    {
        memory.removed.emplace_back(x, y);
    }
    furnitureuse::apply(mFurniture, memory);
    mRemovedFurniture = memory.removed;
    refreshFurnitureLayer();
}

void Game::plantScenery()
{
    mTileMap.clearScenery();
    std::vector<SceneryPiece> pieces = scenery::plant(mTileMap);

    // Nothing grows where something already is: the player, the dead, loot,
    // camp fires, cars. Their tiles keep whatever lies flat and lose the rest.
    std::vector<unsigned char> taken(static_cast<std::size_t>(mTileMap.width() * mTileMap.height()), 0);
    mWorld.forEach<Transform>([&](Entity, Transform& transform)
    {
        for (const float dx : {4.0f, 28.0f})
        {
            for (const float dy : {4.0f, 28.0f})
            {
                const int x = static_cast<int>(std::floor((transform.x + dx) / 32.0f));
                const int y = static_cast<int>(std::floor((transform.y + dy) / 32.0f));
                if (mTileMap.inBounds(x, y))
                {
                    taken[static_cast<std::size_t>(y * mTileMap.width() + x)] = 1;
                }
            }
        }
    });
    std::erase_if(pieces, [&](const SceneryPiece& piece)
    {
        const glm::ivec2 tile = scenery::tileOf(piece);
        return piece.blocker && taken[static_cast<std::size_t>(tile.y * mTileMap.width() + tile.x)] != 0;
    });

    for (const SceneryPiece& piece : pieces)
    {
        if (piece.blocker)
        {
            const glm::ivec2 tile = scenery::tileOf(piece);
            mTileMap.setScenery(tile.x, tile.y, true);
        }
    }
    mSceneryView.build(pieces);
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
