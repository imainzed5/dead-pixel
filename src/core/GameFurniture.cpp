// Furniture: placing it in the town, and keeping run spawns off it.
#include "core/GameShared.h"

#include <string_view>

#include "world/BuildingKinds.h"

using namespace gameshared;

void Game::furnishWorld()
{
    mFurniture = furniture::furnish(mMapGenerator.layoutData(), static_cast<std::uint32_t>(mSaveManager.worldSeed()));
    // What earlier lives moved and broke up.
    // A save from before phase 6: shops are furnished by the new rules, so
    // what was moved or broken up in them no longer applies (houses keep it).
    const bool migrating = mSaveManager.migratedFromVersion() > 0 && mSaveManager.migratedFromVersion() < 3;
    const LayoutData& layout = mMapGenerator.layoutData();
    const auto inShop = [&](int x, int y)
    {
        const int building = buildingkinds::buildingAt(layout, glm::ivec2(x, y));
        return building > 0 && buildingkinds::kindOf(layout.buildings[static_cast<std::size_t>(building - 1)]) != BuildingKind::House;
    };
    furnitureuse::Memory memory;
    for (const FurnitureMoveRecord& move : mSaveManager.furnitureMemory().moves)
    {
        if (migrating && inShop(move.fromX, move.fromY)) continue;
        memory.moves.push_back(furnitureuse::Move{glm::ivec2(move.fromX, move.fromY), glm::ivec2(move.toX, move.toY)});
    }
    for (const auto& [x, y] : mSaveManager.furnitureMemory().removed)
    {
        if (migrating && inShop(x, y)) continue;
        memory.removed.emplace_back(x, y);
    }
    furnitureuse::apply(mFurniture, memory);
    mRemovedFurniture = memory.removed;
    refreshFurnitureLayer();
    if (migrating) migrateScavenging();
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
    // A car or a pickup is longer than its tile: every tile its body covers (phase 11).
    mWorld.forEach<Vehicle, Transform>([&](Entity, Vehicle& vehicle, Transform& transform)
    {
        const glm::vec2 centre(transform.x + 16.0f, transform.y + 16.0f);
        const vehiclerules::KindInfo& k = vehiclerules::info(vehicle.kind);
        const glm::vec2 f = vehiclerules::forward(vehicle.heading);
        for (float along = -k.length * 0.5f; along <= k.length * 0.5f; along += 8.0f)
        {
            for (const float across : {-k.width * 0.5f, 0.0f, k.width * 0.5f})
            {
                const glm::vec2 p = centre + f * along + glm::vec2(-f.y, f.x) * across;
                const int x = static_cast<int>(std::floor(p.x / 32.0f));
                const int y = static_cast<int>(std::floor(p.y / 32.0f));
                if (mTileMap.inBounds(x, y)) taken[static_cast<std::size_t>(y * mTileMap.width() + x)] = 1;
            }
        }
    });
    const LayoutData::GateSite& gateSite = mMapGenerator.layoutData().gate;
    std::erase_if(pieces, [&](const SceneryPiece& piece)
    {
        const glm::ivec2 tile = scenery::tileOf(piece);
        // Nothing grows in the guard hut's yard (phase 9).
        if (gateSite.placed && gate::inYard(gateSite, tile)) return true;
        // Nor in the cemetery (phase 10).
        if (cemetery::inside(mMapGenerator.layoutData().cemetery, tile)) return true;
        return piece.blocker && taken[static_cast<std::size_t>(tile.y * mTileMap.width() + tile.x)] != 0;
    });

    // Where there is something to forage (phase 6): bushes, the foot of a
    // living tree, tufts of tall grass.
    mForageSpots.clear();
    for (const SceneryPiece& piece : pieces)
    {
        const glm::ivec2 tile = scenery::tileOf(piece);
        if (piece.blocker)
        {
            mTileMap.setScenery(tile.x, tile.y, true);
        }
        const std::string_view sprite = piece.sprite;
        forage::Spot spot = forage::Spot::None;
        if (sprite.starts_with("scn_bush")) spot = forage::Spot::Bush;
        else if (sprite.starts_with("scn_tree") && sprite != "scn_tree_dead") spot = forage::Spot::TreeBase;
        else if (sprite == "scn_stump") spot = forage::Spot::TreeBase;
        else if (sprite.starts_with("scn_tuft") || sprite == "scn_weeds") spot = forage::Spot::Tuft;
        if (spot != forage::Spot::None) mForageSpots.emplace_back(tile, spot);
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
    // The player too: the start is the middle of the house, where a floor
    // lamp may now stand (phase 6), and a start inside a piece cannot move.
    std::vector<glm::ivec2> start{mSpawnData.playerStart};
    nudge(start);
    mSpawnData.playerStart = start.front();
}
