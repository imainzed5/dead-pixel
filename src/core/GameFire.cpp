// Fire (phase 7): a thin wrapper over the fire grid in systems/FireSystem.
// The grid keeps the burning tiles; this file tells it what each tile holds
// and applies what it reports: scorch marks, burned furniture and trees,
// gutted houses, damage, light, noise and smoke.
#include "core/GameShared.h"
#include "world/WeatherRules.h"

#include <iostream>
#include <map>

using namespace gameshared;

namespace
{
glm::vec2 centreOfTile(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

int baseGid(const TileMap& map, int x, int y)
{
    return ((std::max(1, map.groundGid(x, y)) - 1) % TileGid::kPerRow) + 1;
}
} // namespace

bool Game::tileBurned(int tileX, int tileY) const
{
    return mBurnedLookup.count(FireSystem::key(glm::ivec2(tileX, tileY))) != 0;
}

void Game::markTileBurned(int tileX, int tileY)
{
    if (!mBurnedLookup.insert(FireSystem::key(glm::ivec2(tileX, tileY))).second) return;
    mBurnedTiles.emplace_back(tileX, tileY);
}

float Game::tileFuel(int tileX, int tileY)
{
    if (!mTileMap.inBounds(tileX, tileY) || tileBurned(tileX, tileY)) return 0.0f;
    firerules::TileFacts facts;
    const int base = baseGid(mTileMap, tileX, tileY);
    const StructureTile& st = mStructureView.tile(tileX, tileY);
    const bool outdoors = !mTileMap.isIndoors(tileX, tileY);
    if (st.kind != StructureKind::None)
    {
        // The walls and door frames of a wooden house burn; brick and metal do not.
        if (st.building > 0 && mStructureView.buildingMaterial(st.building) == StructureMaterial::Plaster)
        {
            facts.ground = firerules::Ground::WoodWall;
        }
    }
    else if (mSceneryView.treeAt(glm::ivec2(tileX, tileY)))
    {
        facts.ground = firerules::Ground::Tree;
    }
    else if (TileGid::isGrass(base))
    {
        facts.ground = firerules::Ground::Grass;
    }
    else if (base == TileGid::kWood)
    {
        facts.ground = firerules::Ground::WoodFloor;
    }
    else if (base == TileGid::kCarpet || mTileMap.getSurfaceType(tileX, tileY) == SurfaceType::Carpet)
    {
        facts.ground = firerules::Ground::Carpet;
    }
    const WeatherCondition weather = mWeatherState.condition;
    facts.heatwave = weatherrules::heatwave(weather);
    facts.wet = outdoors && (weatherrules::rainy(weather) || mHoursSinceRain < 2.0f);
    facts.snow = outdoors && mWeatherState.snowCover >= weatherrules::kSnowDeepEnough;
    if (const FurniturePiece* piece = furnitureAt(glm::ivec2(tileX, tileY)))
    {
        const furnitureuse::Yield yield = furnitureuse::breakYield(piece->kind, false);
        facts.woodFurniture = yield.planks > 0;
        facts.metalFurniture = !facts.woodFurniture;
    }
    facts.boards = mDoors.boards(glm::ivec2(tileX, tileY)) > 0 || mWindows.boards(glm::ivec2(tileX, tileY)) > 0;
    return firerules::fuelOf(facts);
}

FireSystem::Env Game::fireEnv()
{
    FireSystem::Env env;
    env.fuelAt = [this](int x, int y) { return tileFuel(x, y); };
    env.outdoors = [this](int x, int y) { return mTileMap.inBounds(x, y) && !mTileMap.isIndoors(x, y); };
    env.grass = [this](int x, int y)
    { return mTileMap.inBounds(x, y) && TileGid::isGrass(baseGid(mTileMap, x, y)); };
    const WeatherCondition weather = mWeatherState.condition;
    env.spreadScale = weatherrules::fireSpreadScale(weather, false);
    env.grassScale = weatherrules::fireSpreadScale(weather, true) / std::max(0.01f, env.spreadScale);
    env.outdoorDieSeconds = weatherrules::outdoorFireSeconds(weather);
    env.windTo = glm::vec2(mWeatherState.windX, mWeatherState.windY);
    env.windStrength = mWeatherState.windStrength;
    env.seed = mSaveManager.currentRunSeed();
    return env;
}

bool Game::igniteFire(const glm::ivec2& tile, float heat, bool byPlayer)
{
    if (!mTileMap.inBounds(tile.x, tile.y)) return false;
    const bool was = mFireSystem.burning(tile);
    if (!mFireSystem.ignite(tile, heat, byPlayer, fireEnv())) return false;
    if (!was) std::cout << "[Fire] lit " << tile.x << "," << tile.y << "\n";
    return true;
}

// A molotov coming down: the tile it lands on and the two beside it across
// the line of the throw.
bool Game::igniteTile(const glm::ivec2& targetTile, const glm::vec2& from)
{
    const glm::vec2 landing = centreOfTile(mTileMap, targetTile);
    const glm::vec2 d = landing - from;
    const glm::ivec2 across = std::abs(d.x) >= std::abs(d.y) ? glm::ivec2(0, 1) : glm::ivec2(1, 0);
    bool any = false;
    for (const glm::ivec2& tile : {targetTile, targetTile + across, targetTile - across})
    {
        any = igniteFire(tile, firerules::kMolotovHeat, true) || any;
    }
    mNoiseModel.emitNoise(landing, NoiseTier::Explosive, 0.8f, mPlayerEntity);
    return any;
}

// Water on a fire within reach in front (F with a Water Bottle in hand).
bool Game::douseFireAhead()
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity) ||
        !mWorld.hasComponent<Facing>(mPlayerEntity) || mFireSystem.count() == 0)
    {
        return false;
    }
    const glm::vec2 playerCenter = playerWorldCenter();
    const float facing = mWorld.getComponent<Facing>(mPlayerEntity).angleRadians;
    const glm::vec2 ahead(std::cos(facing), std::sin(facing));
    const float reach = 3.0f * static_cast<float>(mTileMap.tileWidth());

    bool found = false;
    glm::ivec2 nearest{0};
    float bestDistSq = reach * reach;
    for (const FireSystem::Burning& b : mFireSystem.burningNear(playerTile(), 3))
    {
        const glm::vec2 d = centreOfTile(mTileMap, b.tile) - playerCenter;
        const float distSq = glm::dot(d, d);
        // Facing it: within a wide cone, or standing on top of it.
        const bool faced = distSq < 400.0f || glm::dot(d, ahead) > std::sqrt(distSq) * 0.5f;
        if (faced && distSq < bestDistSq)
        {
            bestDistSq = distSq;
            nearest = b.tile;
            found = true;
        }
    }
    if (!found) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const int activeSlot = std::clamp(inv.activeSlot, 0, Inventory::kMaxSlots - 1);
    FireSystem::Events events;
    mFireSystem.douse(nearest, firerules::kDouseHeat, &events);
    for (const glm::ivec2& tile : events.scorched)
    {
        // Put out after it had burned a while: a scar, and a tree on it
        // charred (as the next life will find it).
        markTileBurned(tile.x, tile.y);
        spawnScorchMark(tile.x, tile.y);
        mSceneryView.burnTree(tile);
    }
    InventorySystem::consumeOne(inv, mItemDatabase, activeSlot);
    mInteractionMessage = mFireSystem.burning(nearest) ? "Water on the fire. It hisses down." : "Water on the fire. It goes out.";
    mInteractionMessageTimer = 1.8f;
    return true;
}

void Game::countBuildingFloors()
{
    const int count = std::max(0, mStructureView.buildingCount()) + 1;
    mBuildingFloors.assign(static_cast<std::size_t>(count), 0);
    mBuildingBurned.assign(static_cast<std::size_t>(count), 0);
    mBuildingBurning.assign(static_cast<std::size_t>(count), 0);
    mBuildingGutted.assign(static_cast<std::size_t>(count), 0);
    for (int y = 0; y < mTileMap.height(); ++y)
    {
        for (int x = 0; x < mTileMap.width(); ++x)
        {
            const int b = mTileMap.getBuildingId(x, y);
            if (b <= 0 || b >= count || mTileMap.isSolid(x, y)) continue;
            ++mBuildingFloors[static_cast<std::size_t>(b)];
            if (tileBurned(x, y)) ++mBuildingBurned[static_cast<std::size_t>(b)];
        }
    }
}

bool Game::guttedInMemory(int building) const
{
    if (building <= 0) return false;
    for (const auto& [x, y] : mSaveManager.guttedBuildings())
    {
        if (mTileMap.inBounds(x, y) && mTileMap.getBuildingId(x, y) == building) return true;
    }
    return false;
}

void Game::avoidGuttedStart()
{
    const glm::ivec2 start = mSpawnData.playerStart;
    if (!mTileMap.inBounds(start.x, start.y) || !guttedInMemory(mTileMap.getBuildingId(start.x, start.y))) return;
    // The nearest room in a house that still stands.
    glm::ivec2 best(-1);
    float bestDistance = 0.0f;
    for (int y = 0; y < mTileMap.height(); ++y)
    {
        for (int x = 0; x < mTileMap.width(); ++x)
        {
            const int b = mTileMap.getBuildingId(x, y);
            if (b <= 0 || !mTileMap.isIndoors(x, y) || mTileMap.isSolid(x, y) || mTileMap.hasFurniture(x, y)) continue;
            if (mStructureView.buildingMaterial(b) != StructureMaterial::Plaster || guttedInMemory(b)) continue;
            const float distance = glm::length(glm::vec2(glm::ivec2(x, y) - start));
            if (best.x < 0 || distance < bestDistance)
            {
                best = glm::ivec2(x, y);
                bestDistance = distance;
            }
        }
    }
    if (best.x < 0) return;
    const int building = mTileMap.getBuildingId(best.x, best.y);
    mSpawnData.playerStart = best;
    std::erase_if(mSpawnData.zombieSpawns, [&](const glm::ivec2& tile) { return mTileMap.getBuildingId(tile.x, tile.y) == building; });
    std::cout << "[World] the start house is gutted; starting in building " << building << "\n";
}

bool Game::buildingGutted(int building) const
{
    return building > 0 && building < static_cast<int>(mBuildingGutted.size()) &&
           mBuildingGutted[static_cast<std::size_t>(building)] != 0;
}

int Game::burningIn(int building) const
{
    if (building <= 0 || building >= static_cast<int>(mBuildingBurning.size())) return 0;
    return mBuildingBurning[static_cast<std::size_t>(building)];
}

void Game::tileBurnedOut(const glm::ivec2& tile)
{
    markTileBurned(tile.x, tile.y);
    spawnScorchMark(tile.x, tile.y);
    mSceneryView.burnTree(tile);

    // What stood on it burned with what it held.
    if (FurniturePiece* piece = furnitureAt(tile))
    {
        std::vector<Entity> gone;
        mWorld.forEach<Interactable, Transform>(
            [&](Entity e, Interactable&, Transform& t)
            {
                if (!mWorld.hasComponent<Sprite>(e) && worldToTile(mTileMap, glm::vec2(t.x, t.y)) == tile) gone.push_back(e);
            });
        for (Entity e : gone)
        {
            if (e == mOpenContainerEntity) closeContainer();
            mWorld.destroyEntity(e);
        }
        mSaveManager.removeContainerRecord(tile.x, tile.y);
        std::erase(mFirstDays.homeContainers, std::make_pair(tile.x, tile.y));
        mRemovedFurniture.push_back(piece->origin);
        mFurniture.erase(mFurniture.begin() + (piece - mFurniture.data()));
        refreshFurnitureLayer();
        rememberFurniture();
    }

    const int building = mTileMap.getBuildingId(tile.x, tile.y);
    if (building <= 0 || building >= static_cast<int>(mBuildingFloors.size()) || mTileMap.isSolid(tile.x, tile.y)) return;
    const auto b = static_cast<std::size_t>(building);
    ++mBuildingBurned[b];
    if (mBuildingGutted[b] == 0 && mStructureView.buildingMaterial(building) == StructureMaterial::Plaster &&
        static_cast<float>(mBuildingBurned[b]) >= firerules::kGuttedShare * static_cast<float>(std::max(1, mBuildingFloors[b])))
    {
        gutBuilding(building, true);
    }
}

void Game::gutBuilding(int building, bool fresh)
{
    if (building <= 0 || building >= static_cast<int>(mBuildingGutted.size())) return;
    mBuildingGutted[static_cast<std::size_t>(building)] = 1;
    mStructureView.setGutted(building, true);

    // Doors and windows burned out of their frames.
    for (const Doors::Door& door : mDoors.all())
    {
        if (mTileMap.getBuildingId(door.tile.x, door.tile.y) != building &&
            mStructureView.tile(door.tile.x, door.tile.y).building != building)
        {
            continue;
        }
        mDoors.setBoards(mTileMap, door.tile, 0.0f);
        mDoors.setState(mTileMap, door.tile, DoorState::Broken);
    }
    for (const Windows::Window& window : mWindows.all())
    {
        if (mStructureView.tile(window.spot.tile.x, window.spot.tile.y).building != building) continue;
        mWindows.restore(mTileMap, window.spot.tile, true, 0.0f, false);
    }

    // Everything in it burned with what it held.
    std::vector<glm::ivec2> pieces;
    for (const FurniturePiece& piece : mFurniture)
    {
        if (piece.building == building || mTileMap.getBuildingId(piece.tile.x, piece.tile.y) == building) pieces.push_back(piece.tile);
    }
    if (!pieces.empty())
    {
        std::vector<Entity> gone;
        mWorld.forEach<Interactable, Transform>(
            [&](Entity e, Interactable&, Transform& t)
            {
                if (mWorld.hasComponent<Sprite>(e)) return;
                const glm::ivec2 at = worldToTile(mTileMap, glm::vec2(t.x, t.y));
                if (std::find(pieces.begin(), pieces.end(), at) != pieces.end()) gone.push_back(e);
            });
        for (Entity e : gone)
        {
            if (e == mOpenContainerEntity) closeContainer();
            mWorld.destroyEntity(e);
        }
        for (const glm::ivec2& tile : pieces)
        {
            mSaveManager.removeContainerRecord(tile.x, tile.y);
            std::erase(mFirstDays.homeContainers, std::make_pair(tile.x, tile.y));
        }
        for (const FurniturePiece& piece : mFurniture)
        {
            if (std::find(pieces.begin(), pieces.end(), piece.tile) != pieces.end()) mRemovedFurniture.push_back(piece.origin);
        }
        std::erase_if(mFurniture, [&](const FurniturePiece& piece)
                      { return std::find(pieces.begin(), pieces.end(), piece.tile) != pieces.end(); });
        refreshFurnitureLayer();
        rememberFurniture();
    }

    if (!fresh) return;
    // One floor tile names it in world memory.
    for (int y = 0; y < mTileMap.height(); ++y)
    {
        for (int x = 0; x < mTileMap.width(); ++x)
        {
            if (mTileMap.getBuildingId(x, y) != building || mTileMap.isSolid(x, y)) continue;
            mSaveManager.addGuttedBuilding(x, y);
            y = mTileMap.height();
            break;
        }
    }
    mSaveManager.saveWorldMemory();
    std::cout << "[Fire] building " << building << " gutted\n";
    mInteractionMessage = "The house is gutted.";
    mInteractionMessageTimer = 2.5f;
}

void Game::holdSurvivorsFromFire()
{
    if (mFireSystem.count() == 0) return;
    mWorld.forEach<SurvivorAI, Transform, Velocity>(
        [&](Entity, SurvivorAI&, Transform& t, Velocity& v)
        {
            if (v.dx * v.dx + v.dy * v.dy < 1.0f) return;
            const glm::vec2 feet(t.x + 16.0f, t.y + 16.0f);
            const glm::vec2 dir = glm::normalize(glm::vec2(v.dx, v.dy));
            const glm::ivec2 ahead = worldToTile(mTileMap, feet + dir * 20.0f);
            if (ahead != worldToTile(mTileMap, feet) && mFireSystem.burning(ahead))
            {
                v.dx = 0.0f;
                v.dy = 0.0f;
            }
        });
}

std::vector<glm::vec2> Game::fireLights() const
{
    // One light per 3x3 block, at most 8: the blocks with most fire nearest
    // the player.
    std::map<std::int64_t, std::pair<glm::vec2, int>> blocks;
    for (const FireSystem::Burning& b : mFireSystem.burningNear(playerTile(), 24))
    {
        const glm::ivec2 block(b.tile.x / 3, b.tile.y / 3);
        auto& [sum, n] = blocks[FireSystem::key(block)];
        sum += centreOfTile(mTileMap, b.tile);
        ++n;
    }
    std::vector<std::pair<float, glm::vec2>> ranked;
    const glm::vec2 player = playerWorldCenter();
    for (const auto& [key, block] : blocks)
    {
        const glm::vec2 at = block.first / static_cast<float>(block.second);
        ranked.emplace_back(glm::length(at - player), at);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<glm::vec2> out;
    for (std::size_t i = 0; i < ranked.size() && i < 8; ++i) out.push_back(ranked[i].second);
    return out;
}

// The burn and spread, and what fire does to whoever stands in it.
void Game::updateFire(float dtSeconds, bool playerAlive, bool)
{
    std::fill(mBuildingBurning.begin(), mBuildingBurning.end(), 0);
    if (mFireSystem.count() == 0) return;

    FireSystem::Events events;
    mFireSystem.update(dtSeconds, fireEnv(), events);
    for (const glm::ivec2& tile : events.burnedOut) tileBurnedOut(tile);
    for (const glm::ivec2& tile : events.scorched)
    {
        // Put out after it had burned a while: a scar, and a tree on it
        // charred (as the next life will find it).
        markTileBurned(tile.x, tile.y);
        spawnScorchMark(tile.x, tile.y);
        mSceneryView.burnTree(tile);
    }

    const std::vector<FireSystem::Burning> burning = mFireSystem.all();
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    for (const FireSystem::Burning& b : burning)
    {
        const int building = mTileMap.getBuildingId(b.tile.x, b.tile.y);
        if (building > 0 && building < static_cast<int>(mBuildingBurning.size())) ++mBuildingBurning[static_cast<std::size_t>(building)];
    }

    // Fire hurts whoever stands in it: 14/s to you, 18/s to the rest, times
    // its heat. The dead burn; kills by a fire you lit are yours.
    if (playerAlive && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        if (const FireSystem::Tile* here = mFireSystem.at(worldToTile(mTileMap, playerWorldCenter())))
        {
            const Traits* traits = playerTraits();
            const float scale = traits != nullptr && traits->has(BloodlineTrait::Firewise) ? firerules::kFirewiseScale : 1.0f;
            Health& health = mWorld.getComponent<Health>(mPlayerEntity);
            if (!mDevGodMode)
            {
                health.current = std::max(0.0f, health.current - firerules::kPlayerDamagePerSecond * here->heat * scale * dtSeconds);
            }
        }
    }
    for (const FireSystem::Burning& b : burning)
    {
        const glm::vec2 centre = centreOfTile(mTileMap, b.tile);
        mZombieAISystem.grid().query(
            centre, tile, SpatialGrid::Zombie | SpatialGrid::Survivor,
            [&](const SpatialGrid::Item& item)
            {
                if (worldToTile(mTileMap, item.position) != b.tile || !mWorld.hasComponent<Health>(item.entity)) return;
                Health& health = mWorld.getComponent<Health>(item.entity);
                if (health.current <= 0.0f) return;
                health.current = std::max(0.0f, health.current - firerules::kOtherDamagePerSecond * b.heat * dtSeconds);
                if (!mWorld.hasComponent<ZombieAI>(item.entity)) return;
                ZombieAI& ai = mWorld.getComponent<ZombieAI>(item.entity);
                ai.burnedLast = true;
                if (b.byPlayer) ai.lastDamagedBy = mPlayerEntity;
                if (!ai.torchRolled)
                {
                    ai.torchRolled = true;
                    if (deterministic01(static_cast<int>(item.entity), b.tile.x * 31 + b.tile.y, 77) < firerules::kWalkingTorchChance)
                    {
                        ai.torchSeconds = firerules::kWalkingTorchSeconds;
                    }
                }
            });
    }

    // The dead set alight keep walking a while and light what they pass.
    mWorld.forEach<ZombieAI, Transform>(
        [&](Entity, ZombieAI& ai, Transform& t)
        {
            if (ai.torchSeconds <= 0.0f) return;
            ai.torchSeconds = std::max(0.0f, ai.torchSeconds - dtSeconds);
            const glm::ivec2 at = worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            if (!mFireSystem.burning(at)) (void)mFireSystem.ignite(at, firerules::kSpreadStartHeat, false, fireEnv());
        });

    // Noise: a fire of 6 tiles or more is Loud every 2 s, a smaller one
    // Medium; the dead that can see a fire walk to it.
    mFireNoiseTimer += dtSeconds;
    if (mFireNoiseTimer >= firerules::kNoiseEverySeconds)
    {
        mFireNoiseTimer = 0.0f;
        const glm::vec2 player = playerWorldCenter();
        glm::vec2 sum(0.0f);
        float bestDist = 1e9f;
        glm::vec2 nearest(0.0f);
        for (const FireSystem::Burning& b : burning)
        {
            const glm::vec2 c = centreOfTile(mTileMap, b.tile);
            sum += c;
            const float d = glm::length(c - player);
            if (d < bestDist)
            {
                bestDist = d;
                nearest = c;
            }
        }
        const NoiseTier tier = static_cast<int>(burning.size()) >= firerules::kLoudTiles ? NoiseTier::Loud : NoiseTier::Medium;
        mNoiseModel.emitNoise(nearest, tier, 0.85f);
        const glm::vec2 centre = sum / static_cast<float>(std::max<std::size_t>(1, burning.size()));
        const float sight = 8.0f * tile * mWeatherState.visibilityMultiplier;
        mZombieAISystem.grid().query(centre, sight, SpatialGrid::Zombie,
                                     [&](const SpatialGrid::Item& item)
                                     {
                                         if (!mWorld.hasComponent<ZombieAI>(item.entity)) return;
                                         ZombieAI& ai = mWorld.getComponent<ZombieAI>(item.entity);
                                         if (ai.dormant || ai.state == ZombieState::Chase || ai.state == ZombieState::Attack) return;
                                         ZombieAISystem::sendTo(ai, centre, false);
                                     });
    }
}

std::vector<FireTileRecord> Game::fireRecords() const
{
    std::vector<FireTileRecord> records;
    for (const FireSystem::Burning& b : mFireSystem.all())
    {
        FireTileRecord record;
        record.tileX = b.tile.x;
        record.tileY = b.tile.y;
        record.intensity = b.heat;
        record.lifeRemaining = b.fuel;
        record.byPlayer = b.byPlayer;
        records.push_back(record);
    }
    return records;
}

void Game::clearFireAndWeatherMarks()
{
    mBurnedTiles.clear();
    mBurnedLookup.clear();
    mFireSystem.clear();
    mFootprints.clear();
    mLastPrint.clear();
    mLightningFlash = 0.0f;
}
