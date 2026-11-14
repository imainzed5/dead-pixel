// Vehicles (overhaul phase 11): the town's sedans, pickups and bicycles.
// Getting in and out, the key, the wire and the crank, driving them through
// the town and its dead, the trunk, fuel, wear and mending, a pickup's load,
// alarms, and what the world remembers of them. The rules are pure in
// world/VehicleRules; the body moves in systems/VehicleSystem.
#include "core/GameShared.h"

#include <string_view>

#include "world/BuildingKinds.h"
#include "world/Storage.h"
#include "world/WeatherRules.h"

using namespace gameshared;

namespace
{
constexpr float kPi = 3.14159265358979f;
constexpr float kCarFrame = 112.0f;  // tools/generate_sprites.py VEH_CAR
constexpr float kCarSink = 32.0f;    // the frame's bottom is 32 px under the ground centre
constexpr float kBikeFrame = 64.0f;
constexpr float kBikeSink = 16.0f;
constexpr float kBodyRadius = 10.0f; // a person or one of the dead, against a vehicle
constexpr float kExitSpeed = 60.0f;  // px/s: faster, and you stay in
constexpr float kBatterMemorySeconds = 3.0f;

glm::vec2 wasdDirection(const Input& input)
{
    // As the walk (InputSystem): W up the screen, S down, A left, D right.
    glm::vec2 want(0.0f);
    if (input.isKeyDown(SDL_SCANCODE_W)) want += glm::vec2(-1.0f, -1.0f);
    if (input.isKeyDown(SDL_SCANCODE_S)) want += glm::vec2(1.0f, 1.0f);
    if (input.isKeyDown(SDL_SCANCODE_A)) want += glm::vec2(-1.0f, 1.0f);
    if (input.isKeyDown(SDL_SCANCODE_D)) want += glm::vec2(1.0f, -1.0f);
    const float len = glm::length(want);
    return len > 0.01f ? want / len : glm::vec2(0.0f);
}

int baseGid(int gid)
{
    return gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
}

glm::vec2 centreOfTransform(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}

glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

float unitRoll(std::mt19937& rng)
{
    return static_cast<float>(rng() % 100000u) / 100000.0f;
}
} // namespace

// ---------------------------------------------------------------------------
// The fleet: placed once, remembered after

void Game::spawnVehicles(std::mt19937& runRng, float suppliesScale)
{
    mVehicleRng.seed(mSaveManager.currentRunSeed() ^ 0x7EC1u);
    mVehicleBlows.clear();
    mBatterers.clear();
    mRidingCompanion = kInvalidEntity;
    mVehicleTheftNoted = false;
    mWireTimer = 0.0f;
    mWireHeld = false;
    mVehicleHitCooldown = 0.0f;

    std::vector<VehicleRecord> records;
    if (mSaveManager.fleetPlaced())
    {
        records = mSaveManager.vehicles();
    }
    else
    {
        const std::vector<VehicleRecord> old = mSaveManager.vehicles();
        if (old.empty())
        {
            // The draws the old sedans took, so later spawns stay where they were.
            std::uniform_real_distribution<float> fuelDist(8.0f, 24.0f);
            std::uniform_real_distribution<float> conditionDist(45.0f, 95.0f);
            for (std::size_t i = 0; i < mSpawnData.vehicleSpawns.size(); ++i)
            {
                (void)conditionDist(runRng);
                (void)fuelDist(runRng);
            }
        }
        std::mt19937 trunkRng(mSaveManager.currentRunSeed() ^ 0x7A11u);
        const auto rollTrunk = [&](VehicleRecord& record)
        {
            std::vector<std::pair<int, int>> items;
            mLoot.roll("trunk", trunkRng, suppliesScale, items);
            for (const auto& [id, count] : items)
            {
                if (id >= 0 && count > 0) record.trunk.push_back(ContainerItemRecord{id, count, 1.0f, 0});
            }
        };
        // The nearest house's kitchen or bedroom: where the key was left.
        const LayoutData& layout = mMapGenerator.layoutData();
        const auto keyPlace = [&](const glm::vec2& centre, std::uint32_t salt) -> glm::ivec2
        {
            int house = 0;
            float best = 1.0e9f;
            for (std::size_t i = 0; i < layout.buildings.size(); ++i)
            {
                const LayoutData::BuildingRect& b = layout.buildings[i];
                if (buildingkinds::kindOf(b) != BuildingKind::House) continue;
                const glm::vec2 mid((static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f) * kSpriteSize,
                                    (static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f) * kSpriteSize);
                const float d = glm::length(mid - centre);
                if (d < best)
                {
                    best = d;
                    house = static_cast<int>(i) + 1;
                }
            }
            std::vector<glm::ivec2> spots;
            for (const FurniturePiece& piece : mFurniture)
            {
                if (piece.building != house) continue;
                const FurnitureInfo& def = furniture::info(piece.kind);
                if (def.prompt == nullptr) continue;
                if (def.loot != FurnitureLoot::Kitchen && def.loot != FurnitureLoot::Bedroom) continue;
                spots.push_back(piece.origin);
            }
            if (spots.empty()) return glm::ivec2(-1, -1);
            return spots[(salt * 2654435761u >> 7) % spots.size()];
        };

        int oldSedans = 0;
        for (const VehicleRecord& o : old)
        {
            // A sedan from before phase 11: on its old spot, its key lost.
            VehicleRecord record = o;
            record.centred = true;
            if (!o.centred)
            {
                record.x += kSpriteSize * 0.5f;
                record.y += kSpriteSize * 0.5f;
            }
            record.kind = static_cast<int>(VehicleKind::Sedan);
            record.maxFuel = vehiclerules::info(VehicleKind::Sedan).maxFuel;
            for (const auto& [id, count] : o.items) record.trunk.push_back(ContainerItemRecord{id, count, 1.0f, 0});
            record.items.clear();
            records.push_back(record);
            ++oldSedans;
        }
        std::uint32_t salt = static_cast<std::uint32_t>(mSaveManager.worldSeed());
        for (const vehiclerules::Spot& spot : vehiclerules::place(layout, static_cast<std::uint32_t>(mSaveManager.worldSeed())))
        {
            if (spot.kind == VehicleKind::Sedan && oldSedans > 0) continue;
            const vehiclerules::KindInfo& k = vehiclerules::info(spot.kind);
            VehicleRecord record;
            record.centred = true;
            record.x = spot.centre.x;
            record.y = spot.centre.y;
            record.kind = static_cast<int>(spot.kind);
            record.color = spot.color;
            record.heading = spot.heading;
            record.condition = spot.condition;
            record.maxFuel = k.maxFuel;
            record.fuel = std::min(spot.fuel, k.maxFuel);
            record.alarm = spot.alarm;
            if (k.engine)
            {
                rollTrunk(record);
                record.keyInside = spot.keyInside;
                if (!record.keyInside)
                {
                    const glm::ivec2 key = keyPlace(spot.centre, ++salt);
                    if (key.x < 0) record.keyInside = true; // no house near: left in it
                    record.keyTileX = key.x;
                    record.keyTileY = key.y;
                }
            }
            records.push_back(record);
        }
        mSaveManager.setFleetPlaced(true);
        mSaveManager.setVehicleRecords(records);
    }

    for (const VehicleRecord& record : records) (void)spawnVehicle(record);

    // One car is someone's (phase 8): the one nearest a house people live in.
    {
        const LayoutData& layout = mMapGenerator.layoutData();
        Entity owned = kInvalidEntity;
        int ownerBuilding = 0;
        float best = 10.0f * kSpriteSize;
        mWorld.forEach<Vehicle, Transform>(
            [&](Entity e, Vehicle& v, Transform& t)
            {
                v.ownerBuilding = 0;
                if (v.kind == VehicleKind::Bicycle) return;
                const glm::vec2 centre = centreOfTransform(t);
                for (std::size_t i = 0; i < layout.buildings.size(); ++i)
                {
                    if (!survivorLivesIn(static_cast<int>(i) + 1)) continue;
                    const LayoutData::BuildingRect& b = layout.buildings[i];
                    const glm::vec2 mid((static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f) * kSpriteSize,
                                        (static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f) * kSpriteSize);
                    const float d = glm::length(mid - centre);
                    if (d < best)
                    {
                        best = d;
                        owned = e;
                        ownerBuilding = static_cast<int>(i) + 1;
                    }
                }
            });
        if (owned != kInvalidEntity) mWorld.getComponent<Vehicle>(owned).ownerBuilding = ownerBuilding;
    }
}

Entity Game::spawnVehicle(const VehicleRecord& record)
{
    const VehicleKind kind = static_cast<VehicleKind>(std::clamp(record.kind, 0, static_cast<int>(VehicleKind::Count) - 1));
    const vehiclerules::KindInfo& k = vehiclerules::info(kind);
    glm::vec2 centre(record.x, record.y);
    if (!record.centred) centre += glm::vec2(kSpriteSize * 0.5f);

    const Entity e = mWorld.createEntity();
    Transform t{};
    t.x = t.prevX = centre.x - kSpriteSize * 0.5f;
    t.y = t.prevY = centre.y - kSpriteSize * 0.5f;
    Sprite s{};
    s.textureId = mSpriteSheet.textureId();
    s.layer = 5;
    s.drawSize = kind == VehicleKind::Bicycle ? kBikeFrame : kCarFrame;
    s.sink = kind == VehicleKind::Bicycle ? kBikeSink : kCarSink;

    Vehicle v{};
    v.kind = kind;
    v.color = std::clamp(record.color, 0, vehiclerules::colorCount(kind) - 1);
    v.heading = record.heading;
    v.condition = std::clamp(record.condition, 0.0f, 100.0f);
    v.maxFuel = k.maxFuel;
    v.fuel = std::clamp(record.fuel, 0.0f, v.maxFuel);
    v.tyres = std::clamp(record.tyres, 0, 4);
    v.started = record.started;
    v.keyKnown = record.keyKnown;
    v.keyInside = record.keyInside;
    v.keyTile = glm::ivec2(record.keyTileX, record.keyTileY);
    v.alarm = record.alarm;
    v.alarmSpentRun = record.alarmSpentRun;
    v.windowBroken = record.windowBroken;
    v.hasCargo = record.hasCargo;
    v.cargoOrigin = glm::ivec2(record.cargoX, record.cargoY);
    v.usedThisLife = record.usedThisLife;

    mWorld.addComponent<Transform>(e, t);
    mWorld.addComponent<Sprite>(e, s);

    if (k.trunkSlots > 0)
    {
        const Entity trunk = mWorld.createEntity();
        const glm::vec2 rear = vehiclerules::rear(kind, centre, v.heading);
        Transform tt{};
        tt.x = tt.prevX = rear.x - kSpriteSize * 0.5f;
        tt.y = tt.prevY = rear.y - kSpriteSize * 0.5f;
        Sprite ts{};
        ts.textureId = mSpriteSheet.textureId();
        ts.visible = false; // the vehicle draws it
        Interactable inter{};
        inter.type = InteractableType::Container;
        inter.prompt = kind == VehicleKind::Pickup ? "Open the truck bed" : "Open the trunk";
        Inventory inv{};
        inv.maxSlots = std::min(k.trunkSlots, Inventory::kMaxSlots);
        inv.maxWeight = k.trunkKg;
        ContainerRecord contents;
        contents.items = record.trunk;
        for (const auto& [id, count] : record.items) contents.items.push_back(ContainerItemRecord{id, count, 1.0f, 0});
        storage::fillFromRecord(inv, contents, mItemDatabase);
        mWorld.addComponent<Transform>(trunk, tt);
        mWorld.addComponent<Sprite>(trunk, ts);
        mWorld.addComponent<Interactable>(trunk, inter);
        mWorld.addComponent<Inventory>(trunk, inv);
        mWorld.addComponent<TrunkOf>(trunk, TrunkOf{e});
        v.trunk = trunk;
    }
    mWorld.addComponent<Vehicle>(e, v);

    // A pickup's load rides off the map (furniture memory keeps it there).
    if (v.hasCargo)
    {
        const auto piece = std::find_if(mFurniture.begin(), mFurniture.end(),
                                        [&](const FurniturePiece& p) { return p.origin == v.cargoOrigin; });
        if (piece == mFurniture.end()) mWorld.getComponent<Vehicle>(e).hasCargo = false;
    }
    refreshVehicleSprite(e);
    return e;
}

void Game::persistVehicles(bool lifeOver)
{
    std::vector<VehicleRecord> records;
    mWorld.forEach<Vehicle, Transform>(
        [&](Entity, Vehicle& v, Transform& t)
        {
            VehicleRecord record;
            const glm::vec2 centre = centreOfTransform(t);
            record.centred = true;
            record.x = centre.x;
            record.y = centre.y;
            record.kind = static_cast<int>(v.kind);
            record.color = v.color;
            record.heading = v.heading;
            record.condition = v.condition;
            // Left out a life: one stage worse for whoever comes next.
            if (lifeOver && v.usedThisLife) record.condition = std::max(0.0f, v.condition - vehiclerules::kBetweenLivesWear);
            record.fuel = v.fuel;
            record.maxFuel = v.maxFuel;
            record.tyres = v.tyres;
            record.started = v.started;
            record.keyKnown = v.keyKnown;
            record.keyInside = v.keyInside;
            record.keyTileX = v.keyTile.x;
            record.keyTileY = v.keyTile.y;
            record.alarm = v.alarm;
            record.alarmSpentRun = v.alarmSpentRun;
            record.windowBroken = v.windowBroken;
            record.hasCargo = v.hasCargo;
            record.cargoX = v.cargoOrigin.x;
            record.cargoY = v.cargoOrigin.y;
            record.ownerBuilding = v.ownerBuilding;
            record.usedThisLife = lifeOver ? false : v.usedThisLife;
            if (v.trunk != kInvalidEntity && mWorld.hasComponent<Inventory>(v.trunk))
            {
                record.trunk = storage::toRecord(mWorld.getComponent<Inventory>(v.trunk), 0, 0, 0).items;
            }
            records.push_back(record);
        });
    mSaveManager.setVehicleRecords(std::move(records));
    mSaveManager.setFleetPlaced(true);
}

// ---------------------------------------------------------------------------
// Looking at them

std::string Game::vehicleName(const Vehicle& vehicle) const
{
    char buffer[48];
    return vehiclerules::describe(vehicle.kind, vehicle.color, buffer, sizeof(buffer));
}

glm::vec2 Game::vehicleCentre(Entity vehicle) const
{
    if (vehicle == kInvalidEntity || !mWorld.hasComponent<Transform>(vehicle)) return glm::vec2(0.0f);
    return centreOfTransform(mWorld.getComponent<Transform>(vehicle));
}

Entity Game::vehicleNear(const glm::vec2& at, float reach) const
{
    Entity best = kInvalidEntity;
    float bestDistance = reach;
    const_cast<World&>(mWorld).forEach<Vehicle, Transform>(
        [&](Entity e, Vehicle& v, Transform& t)
        {
            const float d = VehicleSystem::distanceToBody(v.kind, centreOfTransform(t), v.heading, at);
            if (d <= bestDistance)
            {
                bestDistance = d;
                best = e;
            }
        });
    return best;
}

void Game::refreshVehicleSprite(Entity vehicle)
{
    if (!mWorld.hasComponent<Vehicle>(vehicle) || !mWorld.hasComponent<Sprite>(vehicle)) return;
    const Vehicle& v = mWorld.getComponent<Vehicle>(vehicle);
    Sprite& s = mWorld.getComponent<Sprite>(vehicle);
    const vehiclerules::Facing8 facing = vehiclerules::facing8(v.heading);
    std::string name;
    switch (v.kind)
    {
    case VehicleKind::Bicycle:
        name = v.playerInside ? "veh_bicycle_rider_" : "veh_bicycle_";
        break;
    case VehicleKind::Pickup:
        name = std::string("veh_pickup_") + vehiclerules::colorName(v.kind, v.color) + (v.hasCargo ? "_load_" : "_");
        break;
    default:
        name = std::string("veh_sedan_") + vehiclerules::colorName(v.kind, v.color) + "_";
        break;
    }
    name += std::to_string(facing.drawn);
    s.uvRect = mSpriteSheet.uvRect(name);
    s.flipX = facing.mirrored;
    const vehiclerules::Band band = vehiclerules::band(v.condition);
    const float shade = band == vehiclerules::Band::Broken ? 0.62f : band == vehiclerules::Band::Damaged ? 0.82f : 1.0f;
    s.color = glm::vec4(shade, shade, shade, 1.0f);
}

bool Game::alarmLive(const Vehicle& vehicle) const
{
    return vehicle.alarm && vehiclerules::info(vehicle.kind).engine && mCurrentDay <= vehiclerules::kAlarmDays &&
           vehicle.alarmSpentRun != mSaveManager.runNumber();
}

int Game::vehicleTileKind(int x, int y) const
{
    using TK = VehicleSystem::TileKind;
    if (!mTileMap.inBounds(x, y)) return static_cast<int>(TK::Wall);
    // A doorway is too narrow for any of them, open or not.
    if (mTileMap.doorState(x, y) != DoorState::None) return static_cast<int>(TK::Wall);
    if (mTileMap.isWater(x, y)) return static_cast<int>(TK::Wall);
    if (!mTileMap.isSolid(x, y)) return static_cast<int>(TK::Open);
    if (mTileMap.hasScenery(x, y))
    {
        // A hydrant or a bin goes down; a lamp post or a tree stops it.
        const std::string_view sprite = mSceneryView.blockerAt(glm::ivec2(x, y));
        if (sprite == "scn_hydrant" || sprite == "scn_trash_can") return static_cast<int>(TK::Thin);
        return static_cast<int>(TK::Wall);
    }
    const LayoutData::CemeterySite& site = mMapGenerator.layoutData().cemetery;
    if (site.placed && mTileMap.isBlocked(x, y) && !mTileMap.blocksSight(x, y) && !mTileMap.hasFurniture(x, y) &&
        x >= site.fenceMin.x && x <= site.fenceMax.x && y >= site.fenceMin.y && y <= site.fenceMax.y)
    {
        return static_cast<int>(TK::Thin); // the cemetery's low fence
    }
    return static_cast<int>(TK::Wall);
}

void Game::knockDownScenery(const glm::ivec2& tile)
{
    if (mSceneryView.knockDown(tile))
    {
        mTileMap.setScenery(tile.x, tile.y, false);
    }
    else if (mTileMap.isBlocked(tile.x, tile.y))
    {
        // A section of the cemetery fence.
        for (auto it = mCemeteryEntities.begin(); it != mCemeteryEntities.end(); ++it)
        {
            if (!mWorld.hasComponent<Transform>(*it)) continue;
            const Transform& t = mWorld.getComponent<Transform>(*it);
            if (worldToTile(mTileMap, centreOfTransform(t)) != tile) continue;
            mWorld.destroyEntity(*it);
            mCemeteryEntities.erase(it);
            break;
        }
        mTileMap.setBlocked(tile.x, tile.y, false);
    }
    mParticleSystem.spawnDust(tileCentre(mTileMap, tile), 8);
}

glm::vec2 Game::cameraTarget(const glm::vec2& playerCenter) const
{
    if (mActiveVehicleEntity == kInvalidEntity || !mWorld.hasComponent<Vehicle>(mActiveVehicleEntity)) return playerCenter;
    const Vehicle& v = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
    // The view runs ahead of a moving vehicle, up to three tiles.
    const float lead = vehiclerules::kCameraLeadTiles * kSpriteSize *
                       std::clamp(v.speed / vehiclerules::kZoomOutSpeed, -0.5f, 1.0f);
    return playerCenter + vehiclerules::forward(v.heading) * lead;
}

// ---------------------------------------------------------------------------
// In and out

void Game::enterVehicle(Entity vehicle)
{
    Vehicle& v = mWorld.getComponent<Vehicle>(vehicle);
    const vehiclerules::KindInfo& k = vehiclerules::info(v.kind);
    std::string message = v.kind == VehicleKind::Bicycle ? "You get on the bicycle." : "You get into " + vehicleName(v) + ".";
    if (k.engine)
    {
        // Opening the door without the key: its alarm, while its battery lasts.
        if (alarmLive(v) && !v.keyKnown && v.alarmSeconds <= 0.0f)
        {
            v.alarmSeconds = vehiclerules::kAlarmSeconds;
            v.alarmSpentRun = mSaveManager.runNumber();
            message = "The alarm goes off.";
        }
        else if (v.keyInside && !v.keyKnown)
        {
            message = "The key is in it.";
        }
        if (v.keyInside) v.keyKnown = true;
        else if (!v.keyKnown && !v.started && v.condition >= 10.0f && message.find("alarm") == std::string::npos)
        {
            message += " No key. Hold E to hot-wire it.";
        }
    }
    v.playerInside = true;
    v.usedThisLife = true;
    mActiveVehicleEntity = vehicle;
    mWireTimer = 0.0f;
    mWireHeld = false;
    mShowCrafting = false;
    mShowInventory = false;
    if (mContainerOpen) closeContainer();
    if (mBaseAction.kind == BaseAction::Kind::Build) cancelBuild("Building interrupted.");
    mBaseAction = BaseAction{};
    if (mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        vel.dx = vel.dy = 0.0f;
    }
    if (mWorld.hasComponent<Sprite>(mPlayerEntity)) mWorld.getComponent<Sprite>(mPlayerEntity).visible = false;

    // Someone travelling with you gets in too.
    const Entity companion = companionEntity();
    if (companion != kInvalidEntity && k.seats >= 2 && companionNear(4.0f))
    {
        SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(companion);
        ai.riding = true;
        ai.path.clear();
        mRidingCompanion = companion;
        if (mWorld.hasComponent<Sprite>(companion)) mWorld.getComponent<Sprite>(companion).visible = false;
    }
    // Someone's car (phase 8).
    if (v.ownerBuilding > 0 && !mVehicleTheftNoted && k.engine)
    {
        mVehicleTheftNoted = true;
        noteTheft(v.ownerBuilding);
    }
    mInteractionMessage = message;
    mInteractionMessageTimer = 2.5f;
    refreshVehicleSprite(vehicle);
    std::cout << "[Vehicle] in " << vehiclerules::kindName(v.kind) << '\n';
}

bool Game::exitVehicle(bool thrown)
{
    if (mActiveVehicleEntity == kInvalidEntity || !mWorld.hasComponent<Vehicle>(mActiveVehicleEntity)) return false;
    const Entity vehicle = mActiveVehicleEntity;
    Vehicle& v = mWorld.getComponent<Vehicle>(vehicle);
    const glm::vec2 centre = vehicleCentre(vehicle);
    if (!thrown && v.kind != VehicleKind::Bicycle && std::fabs(v.speed) > kExitSpeed)
    {
        mInteractionMessage = "Slow down first.";
        mInteractionMessageTimer = 1.5f;
        return false;
    }
    const vehiclerules::KindInfo& k = vehiclerules::info(v.kind);
    const glm::vec2 fwd = vehiclerules::forward(v.heading);
    const std::array<glm::vec2, 4> spots = {vehiclerules::side(v.kind, centre, v.heading, true),
                                            vehiclerules::side(v.kind, centre, v.heading, false),
                                            centre - fwd * (k.length * 0.5f + 18.0f),
                                            centre + fwd * (k.length * 0.5f + 18.0f)};
    const auto clear = [&](const glm::vec2& p)
    {
        const glm::ivec2 tile = worldToTile(mTileMap, p);
        if (mTileMap.isSolid(tile.x, tile.y)) return false;
        bool free = true;
        mWorld.forEach<Vehicle, Transform>(
            [&](Entity other, Vehicle& ov, Transform& ot)
            {
                if (other == vehicle || ov.kind == VehicleKind::Bicycle) return;
                if (VehicleSystem::distanceToBody(ov.kind, centreOfTransform(ot), ov.heading, p) < kBodyRadius) free = false;
            });
        return free;
    };
    int chosen = -1;
    for (int i = 0; i < 4 && chosen < 0; ++i)
    {
        if (clear(spots[static_cast<std::size_t>(i)])) chosen = i;
    }
    if (chosen < 0)
    {
        if (!thrown)
        {
            mInteractionMessage = "No room to get out.";
            mInteractionMessageTimer = 1.5f;
            return false;
        }
        chosen = 0;
    }
    const glm::vec2 out = spots[static_cast<std::size_t>(chosen)];
    v.playerInside = false;
    v.engineOn = false;
    v.headlights = false;
    if (v.kind == VehicleKind::Bicycle) v.speed = 0.0f;
    mActiveVehicleEntity = kInvalidEntity;
    mWireTimer = 0.0f;
    mWireHeld = false;
    mBatterers.clear();
    if (mVehicleZoomed)
    {
        mCamera.setZoom(mCamera.zoom() / 0.85f);
        mVehicleZoomed = false;
    }
    Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
    pt.x = pt.prevX = out.x - kSpriteSize * 0.5f;
    pt.y = pt.prevY = out.y - kSpriteSize * 0.5f;
    if (mWorld.hasComponent<Sprite>(mPlayerEntity)) mWorld.getComponent<Sprite>(mPlayerEntity).visible = true;

    if (mRidingCompanion != kInvalidEntity && mWorld.hasComponent<SurvivorAI>(mRidingCompanion))
    {
        mWorld.getComponent<SurvivorAI>(mRidingCompanion).riding = false;
        glm::vec2 seat = out;
        for (int i = 0; i < 4; ++i)
        {
            if (i != chosen && clear(spots[static_cast<std::size_t>(i)]))
            {
                seat = spots[static_cast<std::size_t>(i)];
                break;
            }
        }
        if (mWorld.hasComponent<Transform>(mRidingCompanion))
        {
            Transform& ct = mWorld.getComponent<Transform>(mRidingCompanion);
            ct.x = ct.prevX = seat.x - kSpriteSize * 0.5f;
            ct.y = ct.prevY = seat.y - kSpriteSize * 0.5f;
        }
        if (mWorld.hasComponent<Sprite>(mRidingCompanion)) mWorld.getComponent<Sprite>(mRidingCompanion).visible = true;
    }
    mRidingCompanion = kInvalidEntity;
    if (!thrown)
    {
        mInteractionMessage = v.kind == VehicleKind::Bicycle ? "You get off the bicycle." : "You get out.";
        mInteractionMessageTimer = 1.5f;
    }
    refreshVehicleSprite(vehicle);
    std::cout << "[Vehicle] out" << (thrown ? " (thrown)" : "") << '\n';
    return true;
}

void Game::fallOffBicycle(const char* why)
{
    if (!exitVehicle(true)) return;
    if (!mDevGodMode && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        Health& h = mWorld.getComponent<Health>(mPlayerEntity);
        h.current = std::max(0.0f, h.current - vehiclerules::kFallDamage);
    }
    mPlayerDownTimer = std::max(mPlayerDownTimer, vehiclerules::kFallDownSeconds);
    mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.35f);
    mAudioManager.playSound(SoundId::PlayerHurt, 0.5f);
    mNoiseModel.emitNoise(playerWorldCenter(), NoiseTier::Medium, 0.8f, mPlayerEntity);
    mInteractionMessage = why;
    mInteractionMessageTimer = 2.5f;
}

// V gets in or out of the nearest vehicle.
void Game::handleVehicleKey(bool playerAlive)
{
    if (!playerAlive || mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity) ||
        !mInput.wasKeyPressed(SDL_SCANCODE_V) || mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        return;
    }
    if (mActiveVehicleEntity != kInvalidEntity)
    {
        (void)exitVehicle(false);
        return;
    }
    if (playerHeld() || playerKnockedDown()) return;
    const Entity vehicle = vehicleNear(playerWorldCenter(), kVehicleReachPixels);
    if (vehicle == kInvalidEntity) return;
    enterVehicle(vehicle);
}

// ---------------------------------------------------------------------------
// In the seat

void Game::updateVehicleSeat(float dtSeconds)
{
    Vehicle& v = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
    if (!vehiclerules::info(v.kind).engine) return;
    const glm::vec2 centre = vehicleCentre(mActiveVehicleEntity);
    const bool pressed = mInput.wasKeyPressed(SDL_SCANCODE_E);
    const bool held = mInput.isKeyDown(SDL_SCANCODE_E);
    const int mechanics = skillLevel(Skill::Mechanics);

    const auto crank = [&]()
    {
        mNoiseModel.emitNoise(centre, NoiseTier::Loud, 1.0f, mPlayerEntity);
        mAudioManager.playSound(SoundId::FootstepLoud, 0.5f);
    };
    const auto tryStart = [&]()
    {
        if (!vehiclerules::runs(v.condition))
        {
            mInteractionMessage = "It will not turn over. It is broken.";
        }
        else if (v.fuel <= 0.01f)
        {
            mInteractionMessage = "The starter turns. The tank is dry.";
            crank();
        }
        else if (vehiclerules::startFails(unitRoll(mVehicleRng), v.condition))
        {
            mInteractionMessage = "It coughs and dies.";
            crank();
        }
        else
        {
            v.engineOn = true;
            v.started = true;
            mInteractionMessage = "The engine turns over.";
            crank();
            std::cout << "[Vehicle] started\n";
        }
        mInteractionMessageTimer = 2.0f;
    };

    if (v.engineOn)
    {
        if (pressed)
        {
            v.engineOn = false;
            mInteractionMessage = "Engine off.";
            mInteractionMessageTimer = 1.5f;
        }
        return;
    }
    if (v.keyKnown || v.started)
    {
        if (pressed) tryStart();
        return;
    }
    // No key: hold E under the wheel.
    if (!held)
    {
        mWireHeld = false;
        mWireTimer = 0.0f;
        return;
    }
    if (!vehiclerules::runs(v.condition))
    {
        if (pressed)
        {
            mInteractionMessage = "It will not turn over. It is broken.";
            mInteractionMessageTimer = 2.0f;
        }
        return;
    }
    if (!mWireHeld)
    {
        mWireHeld = true;
        mWireTimer = 0.0f;
    }
    mWireTimer += dtSeconds / heldActionScale();
    if (mWireTimer < vehiclerules::hotwireSeconds(mechanics)) return;
    mWireTimer = 0.0f;
    if (vehiclerules::hotwireFails(unitRoll(mVehicleRng), mechanics))
    {
        crank();
        mInteractionMessage = "Sparks, and the starter grinds. Nothing. Try again.";
        mInteractionMessageTimer = 2.5f;
        std::cout << "[Vehicle] hot-wire failed\n";
        return;
    }
    mWireHeld = false;
    v.started = true;
    awardXp(Skill::Mechanics, skillrules::kXpHotwire);
    std::cout << "[Vehicle] hot-wired\n";
    tryStart();
}

// Before physics: the driver's legs do nothing while in the seat.
void Game::applyVehicleDriving()
{
    if (mActiveVehicleEntity == kInvalidEntity || mPlayerEntity == kInvalidEntity) return;
    if (mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        vel.dx = vel.dy = 0.0f;
    }
    if (mWorld.hasComponent<Facing>(mPlayerEntity))
    {
        Facing& facing = mWorld.getComponent<Facing>(mPlayerEntity);
        facing.running = false;
    }
}

// ---------------------------------------------------------------------------
// Driving

void Game::driveVehicle(Entity vehicle, float dtSeconds)
{
    Vehicle& v = mWorld.getComponent<Vehicle>(vehicle);
    Transform& t = mWorld.getComponent<Transform>(vehicle);
    const vehiclerules::KindInfo& k = vehiclerules::info(v.kind);
    const bool driver = vehicle == mActiveVehicleEntity;
    glm::vec2 centre = centreOfTransform(t);
    const glm::ivec2 here = worldToTile(mTileMap, centre);
    const int ground = mTileMap.inBounds(here.x, here.y) ? baseGid(mTileMap.groundGid(here.x, here.y)) : 0;
    const bool offRoad = TileGid::isGrass(ground) || ground == TileGid::kDirt;
    const bool outdoors = !mTileMap.isIndoors(here.x, here.y);
    const bool snow = outdoors && mWeatherState.snowCover >= weatherrules::kSnowDeepEnough;
    const bool wet = weatherrules::precipitation(mWeatherState.condition);

    vehiclerules::DriveInput in;
    in.want = driver && !playerKnockedDown() ? wasdDirection(mInput) : glm::vec2(0.0f);
    in.top = vehiclerules::topSpeed(v.kind, vehiclerules::SpeedInput{v.condition, v.tyres, offRoad});
    in.snow = snow;
    in.rain = weatherrules::rainy(mWeatherState.condition);
    if (k.engine)
    {
        in.canThrottle = driver && v.engineOn && v.fuel > 0.01f && vehiclerules::runs(v.condition);
    }
    else
    {
        const bool legs = mWorld.hasComponent<Stamina>(mPlayerEntity) && mWorld.getComponent<Stamina>(mPlayerEntity).current > 1.0f;
        in.canThrottle = driver && legs && vehiclerules::runs(v.condition);
    }

    VehicleSystem::Env env;
    env.tileSize = static_cast<float>(mTileMap.tileWidth());
    env.tile = [this](int x, int y) { return static_cast<VehicleSystem::TileKind>(vehicleTileKind(x, y)); };
    const float before = v.speed;
    const float headingBefore = v.heading;
    std::vector<VehicleSystem::Hit> hits;
    VehicleSystem::step(v.kind, centre, v.heading, v.speed, in, env, dtSeconds, hits);
    t.x = centre.x - kSpriteSize * 0.5f;
    t.y = centre.y - kSpriteSize * 0.5f;

    // Pedalling costs breath; coasting is free.
    if (!k.engine && driver && glm::length(in.want) > 0.0f && v.speed > before - 0.001f && std::fabs(v.speed) > 1.0f &&
        mWorld.hasComponent<Stamina>(mPlayerEntity))
    {
        Stamina& st = mWorld.getComponent<Stamina>(mPlayerEntity);
        st.current = std::max(0.0f, st.current - vehiclerules::kPedalStaminaPerSecond * dtSeconds);
    }

    // Fuel and wear, by the game hour.
    const float hours = static_cast<float>(static_cast<double>(dtSeconds) * kGameHoursPerRealSecond);
    const bool moving = std::fabs(v.speed) > 5.0f;
    if (k.engine && v.engineOn)
    {
        v.fuel = std::max(0.0f, v.fuel - vehiclerules::fuelUsed(v.kind, hours) * (moving ? 1.0f : 0.25f));
    }
    if (moving && driver) v.condition = std::max(0.0f, v.condition - vehiclerules::wearPerGameHour(wet || snow, offRoad) * hours);

    // What it hit.
    for (const VehicleSystem::Hit& hit : hits)
    {
        if (hit.type == VehicleSystem::Hit::Type::Thin)
        {
            knockDownScenery(hit.tile);
            v.condition = std::max(0.0f, v.condition - 2.0f);
            mNoiseModel.emitNoise(tileCentre(mTileMap, hit.tile), NoiseTier::Loud, 1.0f, mPlayerEntity);
            mAudioManager.playSound(SoundId::WeaponHit, 0.6f);
            mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.2f);
            if (v.tyres > 0 && unitRoll(mVehicleRng) < vehiclerules::kFlatChance)
            {
                --v.tyres;
                mInteractionMessage = "A tyre goes flat.";
                mInteractionMessageTimer = 2.0f;
            }
            std::cout << "[Vehicle] ran down scenery at " << hit.tile.x << ',' << hit.tile.y << '\n';
            continue;
        }
        if (driver) ++mVehicleScrapes;
        if (v.kind == VehicleKind::Bicycle)
        {
            if (driver && hit.impact > vehiclerules::kWallDamageSpeed) fallOffBicycle("You hit it and go over the bars.");
            return;
        }
        const vehiclerules::WallHit wall = vehiclerules::wallHit(hit.impact);
        if (!wall.hurts || mVehicleHitCooldown > 0.0f) continue;
        mVehicleHitCooldown = 0.4f;
        v.condition = std::max(0.0f, v.condition - wall.vehicleDamage);
        if (driver && !mDevGodMode && mWorld.hasComponent<Health>(mPlayerEntity))
        {
            Health& h = mWorld.getComponent<Health>(mPlayerEntity);
            h.current = std::max(0.0f, h.current - wall.driverDamage);
        }
        mNoiseModel.emitNoise(centre, NoiseTier::Loud, 1.0f, mPlayerEntity);
        mAudioManager.playSound(SoundId::WeaponHit, 0.7f);
        mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.45f);
        std::cout << "[Vehicle] hit a wall at " << static_cast<int>(hit.impact) << " px/s, condition "
                  << static_cast<int>(v.condition) << '\n';
    }

    // The dead and the living in its way.
    const float speed = v.speed;
    const glm::vec2 fwd = vehiclerules::forward(v.heading);
    const float reach = k.length * 0.5f + 24.0f;
    std::vector<SpatialGrid::Item> near;
    mZombieAISystem.grid().query(centre, reach, SpatialGrid::Zombie | SpatialGrid::Survivor | SpatialGrid::Corpse,
                                 [&](const SpatialGrid::Item& item) { near.push_back(item); });
    for (const SpatialGrid::Item& item : near)
    {
        if (!driver && std::fabs(v.speed) < 1.0f) break;
        const float d = VehicleSystem::distanceToBody(v.kind, centre, v.heading, item.position);
        const glm::vec2 to = item.position - centre;
        const bool ahead = glm::dot(to, fwd) * (speed >= 0.0f ? 1.0f : -1.0f) > 0.0f;
        if (item.kind == SpatialGrid::Corpse)
        {
            // A body under a bicycle's wheel at speed throws the rider.
            if (v.kind == VehicleKind::Bicycle && driver && d < 2.0f && ahead &&
                std::fabs(speed) > vehiclerules::kBodyStopsBikeAbove)
            {
                fallOffBicycle("A body under the wheel throws you.");
                return;
            }
            continue;
        }
        if (d > kBodyRadius - 1.0f) continue;
        if (item.kind == SpatialGrid::Survivor)
        {
            if (mWorld.hasComponent<SurvivorAI>(item.entity) && mWorld.getComponent<SurvivorAI>(item.entity).riding) continue;
            if (ahead) v.speed = 0.0f; // it stops for the living
            continue;
        }
        if (!mWorld.hasComponent<ZombieAI>(item.entity) || !mWorld.hasComponent<Health>(item.entity)) continue;
        ZombieAI& ai = mWorld.getComponent<ZombieAI>(item.entity);
        Health& zh = mWorld.getComponent<Health>(item.entity);
        if (zh.current <= 0.0f) continue;
        const bool down = ZombieBlow::isDown(ai);
        if (v.kind == VehicleKind::Bicycle)
        {
            if (!driver) continue;
            if (down)
            {
                if (ahead && std::fabs(speed) > vehiclerules::kBodyStopsBikeAbove)
                {
                    fallOffBicycle("One of them on the ground throws you.");
                    return;
                }
                continue;
            }
            fallOffBicycle(std::fabs(speed) > vehiclerules::kBodyStopsBikeAbove ? "You hit one of them. Hands pull you off the bicycle."
                                                                                : "Hands pull you off the bicycle.");
            return;
        }
        if (down || !ahead) continue; // over it, or alongside
        const vehiclerules::BodyHit hit = vehiclerules::bodyHit(speed, ai.kind == ZombieKind::Brute);
        if (hit.blocked)
        {
            v.speed = 0.0f;
            continue;
        }
        zh.current = std::max(0.0f, zh.current - hit.damage);
        ai.lastDamagedBy = mPlayerEntity; // only the player drives
        const float len = glm::length(to);
        const glm::vec2 dir = glm::normalize(fwd * (speed >= 0.0f ? 1.0f : -1.0f) + (len > 0.01f ? to / len : glm::vec2(0.0f)) * 0.5f);
        if (hit.thrown)
        {
            ZombieBlow::land(ai, dir, 70.0f);
            ZombieBlow::knockDown(ai, vehiclerules::kThrownDownSeconds);
        }
        else
        {
            ZombieBlow::land(ai, dir, 12.0f);
        }
        v.condition = std::max(0.0f, v.condition - hit.conditionLoss);
        v.speed *= hit.speedKeep;
        if (hit.thrown) ++mVehicleRunDowns;
        else ++mVehicleStops;
        mParticleSystem.spawnBloodSpray(item.position, dir, 10);
        mShakeTrauma = std::min(1.0f, mShakeTrauma + (hit.thrown ? 0.25f : 0.5f));
        mNoiseModel.emitNoise(item.position, NoiseTier::Medium, 0.8f, mPlayerEntity);
        mAudioManager.playSound(SoundId::WeaponHit, 0.6f);
        std::cout << "[Vehicle] " << (hit.thrown ? "ran down" : "stopped by") << " one of the dead at "
                  << static_cast<int>(std::fabs(speed)) << " px/s, now " << static_cast<int>(std::fabs(v.speed)) << '\n';
    }

    if (driver || std::fabs(v.heading - headingBefore) > 0.0001f) refreshVehicleSprite(vehicle);
    // The trunk rides at the back.
    if (v.trunk != kInvalidEntity && mWorld.hasComponent<Transform>(v.trunk))
    {
        const glm::vec2 rear = vehiclerules::rear(v.kind, centre, v.heading);
        Transform& tt = mWorld.getComponent<Transform>(v.trunk);
        tt.x = rear.x - kSpriteSize * 0.5f;
        tt.y = rear.y - kSpriteSize * 0.5f;
    }
}

void Game::keepClearOfVehicles()
{
    // A standing vehicle is in the way: whoever walks into it is put back
    // outside its body. Only near the player (far off nobody would see).
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity)) return;
    const glm::vec2 player = playerWorldCenter();
    const float far = 40.0f * kSpriteSize;
    const bool onFoot = mActiveVehicleEntity == kInvalidEntity;
    mWorld.forEach<Vehicle, Transform>(
        [&](Entity e, Vehicle& v, Transform& t)
        {
            if (v.kind == VehicleKind::Bicycle) return; // stepped over
            const glm::vec2 centre = centreOfTransform(t);
            if (glm::length(centre - player) > far) return;
            const float reach = vehiclerules::info(v.kind).length * 0.5f + 16.0f;
            const auto shove = [&](Entity who, const glm::vec2& at, float radius)
            {
                const glm::vec2 push = VehicleSystem::pushOut(v.kind, centre, v.heading, at, radius);
                if (push.x == 0.0f && push.y == 0.0f) return;
                const glm::ivec2 to = worldToTile(mTileMap, at + push);
                if (mTileMap.isSolid(to.x, to.y)) return;
                Transform& wt = mWorld.getComponent<Transform>(who);
                wt.x += push.x;
                wt.y += push.y;
            };
            mZombieAISystem.grid().query(centre, reach, SpatialGrid::Zombie | SpatialGrid::Survivor,
                                         [&](const SpatialGrid::Item& item)
                                         {
                                             if (!mWorld.hasComponent<Transform>(item.entity)) return;
                                             if (mWorld.hasComponent<SurvivorAI>(item.entity) &&
                                                 mWorld.getComponent<SurvivorAI>(item.entity).riding) return;
                                             const glm::vec2 at = centreOfTransform(mWorld.getComponent<Transform>(item.entity));
                                             shove(item.entity, at, kBodyRadius);
                                         });
            if (onFoot && e != mActiveVehicleEntity) shove(mPlayerEntity, player, 8.0f);
        });
}

void Game::updateVehicleAlarms(float dtSeconds)
{
    mWorld.forEach<Vehicle, Transform>(
        [&](Entity, Vehicle& v, Transform& t)
        {
            if (v.alarmSeconds <= 0.0f) return;
            const float before = v.alarmSeconds;
            v.alarmSeconds = std::max(0.0f, v.alarmSeconds - dtSeconds);
            if (std::floor(before) != std::floor(v.alarmSeconds) || before == vehiclerules::kAlarmSeconds)
            {
                mNoiseModel.emitNoise(centreOfTransform(t), NoiseTier::Loud, 1.0f);
                mAudioManager.playSound(SoundId::FootstepLoud, 0.25f);
            }
        });
}

// After movement: every vehicle on the move, the one you sit in, its noise,
// the dead at its doors, and anyone walking into a standing one.
void Game::updateVehicleAfterPhysics(float dtSeconds)
{
    mVehicleClock += dtSeconds;
    mVehicleHitCooldown = std::max(0.0f, mVehicleHitCooldown - dtSeconds);
    updateVehicleAlarms(dtSeconds);

    // Rolling with nobody in it (got out at speed): it coasts to a stop.
    std::vector<Entity> rolling;
    mWorld.forEach<Vehicle, Transform>(
        [&](Entity e, Vehicle& v, Transform&)
        {
            if (e != mActiveVehicleEntity && std::fabs(v.speed) > 0.5f) rolling.push_back(e);
        });
    for (Entity e : rolling) driveVehicle(e, dtSeconds);

    if (mActiveVehicleEntity != kInvalidEntity &&
        (!mWorld.hasComponent<Vehicle>(mActiveVehicleEntity) || !mWorld.hasComponent<Transform>(mActiveVehicleEntity)))
    {
        mActiveVehicleEntity = kInvalidEntity;
    }
    if (mActiveVehicleEntity != kInvalidEntity && mPlayerEntity != kInvalidEntity)
    {
        const Entity vehicle = mActiveVehicleEntity;
        updateVehicleSeat(dtSeconds);
        driveVehicle(vehicle, dtSeconds);
    }
    if (mActiveVehicleEntity != kInvalidEntity && mPlayerEntity != kInvalidEntity)
    {
        const Entity vehicle = mActiveVehicleEntity;
        Vehicle& v = mWorld.getComponent<Vehicle>(vehicle);
        const vehiclerules::KindInfo& k = vehiclerules::info(v.kind);
        const glm::vec2 centre = vehicleCentre(vehicle);

        // The driver (and whoever rides along) goes where it goes.
        Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
        pt.x = centre.x - kSpriteSize * 0.5f;
        pt.y = centre.y - kSpriteSize * 0.5f;
        if (mWorld.hasComponent<Sprite>(mPlayerEntity)) mWorld.getComponent<Sprite>(mPlayerEntity).visible = false;
        if (mWorld.hasComponent<Facing>(mPlayerEntity)) mWorld.getComponent<Facing>(mPlayerEntity).angleRadians = v.heading;
        if (mRidingCompanion != kInvalidEntity && mWorld.hasComponent<Transform>(mRidingCompanion))
        {
            Transform& ct = mWorld.getComponent<Transform>(mRidingCompanion);
            ct.x = pt.x;
            ct.y = pt.y;
            if (mWorld.hasComponent<Sprite>(mRidingCompanion)) mWorld.getComponent<Sprite>(mRidingCompanion).visible = false;
        }

        // It runs dry or gives out.
        if (k.engine && v.engineOn && v.fuel <= 0.01f)
        {
            v.engineOn = false;
            mInteractionMessage = "The engine dies. The tank is dry.";
            mInteractionMessageTimer = 2.5f;
        }
        if (k.engine && v.engineOn && !vehiclerules::runs(v.condition))
        {
            v.engineOn = false;
            mInteractionMessage = "Something gives under the hood. It will not run.";
            mInteractionMessageTimer = 2.5f;
        }

        // Its noise: the engine running (a Damaged one roars), or the tyres of a bicycle.
        v.engineNoiseTimer += dtSeconds;
        if (k.engine && v.engineOn)
        {
            const bool moving = std::fabs(v.speed) > 5.0f;
            const float interval = moving ? vehiclerules::kEngineNoiseSeconds : vehiclerules::kIdleNoiseSeconds;
            if (v.engineNoiseTimer >= interval)
            {
                v.engineNoiseTimer = 0.0f;
                const bool damaged = vehiclerules::band(v.condition) == vehiclerules::Band::Damaged;
                NoiseTier tier = moving ? NoiseTier::Loud : NoiseTier::Medium;
                if (damaged) tier = moving ? NoiseTier::Explosive : NoiseTier::Loud;
                mNoiseModel.emitNoise(centre, tier, interval + 0.05f, mPlayerEntity, NoiseSource::World, k.hearing);
                mAudioManager.playSound(SoundId::FootstepLoud, moving ? 0.16f : 0.08f);
            }
            mMovementNoiseTier = moving ? NoiseTier::Loud : NoiseTier::Medium;
        }
        else if (!k.engine && std::fabs(v.speed) > 20.0f && v.engineNoiseTimer >= 0.5f)
        {
            v.engineNoiseTimer = 0.0f;
            const glm::ivec2 tile = worldToTile(mTileMap, centre);
            const NoiseTier tier =
                NoiseModel::adjustTier(NoiseTier::Whisper, surfaceNoiseModifier(mTileMap.getSurfaceType(tile.x, tile.y)));
            mNoiseModel.emitNoise(centre, tier, 0.55f, mPlayerEntity);
            mMovementNoiseTier = tier;
        }

        // The dead at the doors: their blows on the body, and a window that gives.
        if (!mVehicleBlows.empty())
        {
            for (Entity zombie : mVehicleBlows)
            {
                v.condition = std::max(0.0f, v.condition - vehiclerules::kBlowDamage);
                const auto it = std::find_if(mBatterers.begin(), mBatterers.end(),
                                             [&](const std::pair<Entity, float>& b) { return b.first == zombie; });
                if (it != mBatterers.end()) it->second = mVehicleClock;
                else mBatterers.emplace_back(zombie, mVehicleClock);
                mAudioManager.playSound(SoundId::WeaponHit, 0.3f);
            }
            mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.08f);
            refreshVehicleSprite(vehicle);
        }
        std::erase_if(mBatterers, [&](const std::pair<Entity, float>& b) { return mVehicleClock - b.second > kBatterMemorySeconds; });
        if (static_cast<int>(mBatterers.size()) >= vehiclerules::kWindowCrowd) v.batterSeconds += dtSeconds;
        else v.batterSeconds = std::max(0.0f, v.batterSeconds - dtSeconds * 0.5f);
        if (k.engine && !v.windowBroken && v.batterSeconds >= vehiclerules::kWindowSeconds)
        {
            v.windowBroken = true;
            mNoiseModel.emitNoise(centre, NoiseTier::Loud, 1.0f, mPlayerEntity);
            mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.4f);
            mInteractionMessage = "Glass gives. Hands reach in.";
            mInteractionMessageTimer = 3.0f;
            std::cout << "[Vehicle] window broken\n";
        }

        // The view pulls back a little at speed.
        const bool fast = std::fabs(v.speed) > vehiclerules::kZoomOutSpeed;
        if (fast != mVehicleZoomed)
        {
            mCamera.setZoom(fast ? mCamera.zoom() * 0.85f : mCamera.zoom() / 0.85f);
            mVehicleZoomed = fast;
        }
    }
    else if (mVehicleZoomed)
    {
        mCamera.setZoom(mCamera.zoom() / 0.85f);
        mVehicleZoomed = false;
    }
    mVehicleBlows.clear();
    keepClearOfVehicles();
}

void Game::fillVehicleTick(ZombieTick& tick) const
{
    if (mActiveVehicleEntity == kInvalidEntity || !mWorld.hasComponent<Vehicle>(mActiveVehicleEntity)) return;
    const Vehicle& v = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
    if (v.kind == VehicleKind::Bicycle) return; // a rider is out in the open
    const vehiclerules::KindInfo& k = vehiclerules::info(v.kind);
    const glm::vec2 centre = vehicleCentre(mActiveVehicleEntity);
    const glm::vec2 f = vehiclerules::forward(v.heading);
    tick.playerInVehicle = true;
    tick.vehicleOpen = v.windowBroken;
    tick.vehicleA = centre - f * k.circleOffset;
    tick.vehicleB = centre + f * k.circleOffset;
    tick.vehicleHalfWidth = k.radius;
    tick.vehicleBlows = const_cast<std::vector<Entity>*>(&mVehicleBlows);
    tick.playerMoving = std::fabs(v.speed) > 2.0f;
    // Crouched low in a parked car with the engine off.
    const bool crouched = mWorld.hasComponent<Facing>(mPlayerEntity) && mWorld.getComponent<Facing>(mPlayerEntity).crouching;
    if (crouched && !v.engineOn && std::fabs(v.speed) < 1.0f)
    {
        tick.playerHidden = true;
        tick.hiddenSightScale = vehiclerules::kHiddenSightScale;
    }
    if (v.headlights && weatherrules::isNight(mCurrentDay, mGameHours))
    {
        tick.lightsSeenTiles = vehiclerules::kHeadlightSeenTiles;
        tick.playerLit = true;
    }
}

// ---------------------------------------------------------------------------
// On foot at a vehicle: E

bool Game::handleVehicleInteraction(const ProximityPrompt& entityPrompt)
{
    if (mPlayerEntity == kInvalidEntity) return false;
    const glm::vec2 here = playerWorldCenter();
    const Entity vehicle = vehicleNear(here, kVehicleReachPixels);
    if (vehicle == kInvalidEntity) return false;
    Vehicle& v = mWorld.getComponent<Vehicle>(vehicle);
    const vehiclerules::KindInfo& k = vehiclerules::info(v.kind);
    const glm::vec2 centre = vehicleCentre(vehicle);
    const bool pressed = mInput.wasKeyPressed(SDL_SCANCODE_E);
    const std::string name = vehicleName(v);
    std::string title = name;
    title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));

    const auto beginHeld = [&](BaseAction::Kind kind, float seconds, const char* label)
    {
        mBaseAction = BaseAction{};
        mBaseAction.kind = kind;
        mBaseAction.tile = worldToTile(mTileMap, centre);
        mBaseAction.total = seconds;
        mBaseAction.label = label;
        mBaseAction.startPos = here;
        mBaseAction.target = vehicle;
        mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
    };
    int active = -1;
    if (mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        if (inv.activeSlot >= 0 && inv.activeSlot < Inventory::kMaxSlots && inv.slots[static_cast<std::size_t>(inv.activeSlot)].count > 0)
        {
            active = inv.slots[static_cast<std::size_t>(inv.activeSlot)].itemId;
        }
    }

    // A fuel can in hand: into the tank.
    if (k.engine && active == mItems.fuelCan)
    {
        const std::string fuel = std::to_string(static_cast<int>(std::round(v.fuel))) + " of " +
                                 std::to_string(static_cast<int>(std::round(v.maxFuel)));
        if (v.fuel >= v.maxFuel - 0.5f)
        {
            mProximityPrompt = title + "   The tank is full.";
            return true; // E is this, here
        }
        mProximityPrompt = title + "   E: Refuel (fuel " + fuel + ")";
        if (pressed) beginHeld(BaseAction::Kind::Refuel, vehiclerules::kRefuelSeconds, "Pouring fuel");
        return true; // E is this, here
    }
    // A toolbox in hand: mend it.
    if (k.engine && active == mItems.toolbox)
    {
        const std::string band = vehiclerules::bandName(vehiclerules::band(v.condition));
        if (v.condition >= 99.5f)
        {
            mProximityPrompt = title + "   " + band + ". Nothing to mend.";
            return true; // E is this, here
        }
        const bool scrap = playerHasItem(mItems.scrapMetal);
        mProximityPrompt = title + "   " + band + (scrap ? ".   E: Mend it (scrap metal)" : ". Mending it takes scrap metal.");
        if (pressed && scrap) beginHeld(BaseAction::Kind::MendVehicle, vehiclerules::kRepairSeconds, "Mending");
        return true; // E is this, here
    }
    // A pickup's tailgate: load a piece pushed up to it, or set it down.
    if (v.kind == VehicleKind::Pickup)
    {
        const glm::vec2 rear = vehiclerules::rear(v.kind, centre, v.heading);
        if (glm::length(rear - here) <= 40.0f)
        {
            if (v.hasCargo)
            {
                const auto piece = std::find_if(mFurniture.begin(), mFurniture.end(),
                                                [&](const FurniturePiece& p) { return p.origin == v.cargoOrigin; });
                const std::string what = piece != mFurniture.end() ? pieceNameOf(piece->kind) : "load";
                mProximityPrompt = title + "   E: Unload the " + what;
                if (pressed) beginHeld(BaseAction::Kind::UnloadCargo, 3.0f, "Unloading");
                return true; // E is this, here
            }
            const FurniturePiece* best = nullptr;
            float bestDistance = 1.7f * kSpriteSize; // pushed up to the tailgate
            for (const FurniturePiece& piece : mFurniture)
            {
                if (!furnitureuse::movable(piece.kind) || piece.tile.x < 0) continue;
                const float d = glm::length(tileCentre(mTileMap, piece.tile) - rear);
                if (d < bestDistance)
                {
                    bestDistance = d;
                    best = &piece;
                }
            }
            if (best != nullptr)
            {
                mProximityPrompt = title + "   E: Load the " + pieceNameOf(best->kind);
                if (pressed) beginHeld(BaseAction::Kind::LoadCargo, 3.0f, "Loading");
                return true; // E is this, here
            }
        }
    }
    // Something else in reach speaks first (the trunk is a container of its own).
    if (entityPrompt.entity != kInvalidEntity) return false;
    std::string state;
    if (vehiclerules::band(v.condition) == vehiclerules::Band::Broken) state = "   Broken.";
    else if (v.windowBroken) state = "   A window is out.";
    mProximityPrompt = title + state + "   V: " + (v.kind == VehicleKind::Bicycle ? "Get on" : "Get in");
    return false;
}

bool Game::handleVehicleAlarm(const ProximityPrompt& entityPrompt)
{
    // Last in the chain, and only when nothing after the vehicle's own prompt
    // offered E (GameUpdate.cpp): a door, a window, someone to talk to, a
    // container or tall grass in reach keeps E, so the alarm is never set
    // off by a hand meant for them.
    if (mPlayerEntity == kInvalidEntity || entityPrompt.entity != kInvalidEntity) return false;
    const Entity vehicle = vehicleNear(playerWorldCenter(), kVehicleReachPixels);
    if (vehicle == kInvalidEntity) return false;
    Vehicle& v = mWorld.getComponent<Vehicle>(vehicle);
    // A live alarm: a red light on the dash. From outside, you can set it off.
    if (!alarmLive(v) || v.alarmSeconds > 0.0f) return false;
    std::string title = vehicleName(v);
    title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    mProximityPrompt = title + "   A red light blinks on the dash.   E: Set it off   V: Get in";
    if (mInput.wasKeyPressed(SDL_SCANCODE_E))
    {
        v.alarmSeconds = vehiclerules::kAlarmSeconds;
        v.alarmSpentRun = mSaveManager.runNumber();
        mInteractionMessage = "You set the alarm off.";
        mInteractionMessageTimer = 2.0f;
        std::cout << "[Vehicle] alarm set off\n";
    }
    return true; // E is this, here
}

void Game::finishVehicleAction()
{
    const BaseAction action = mBaseAction;
    mBaseAction = BaseAction{};
    const Entity vehicle = action.target;
    if (vehicle == kInvalidEntity || !mWorld.hasComponent<Vehicle>(vehicle) || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    Vehicle& v = mWorld.getComponent<Vehicle>(vehicle);
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const glm::vec2 centre = vehicleCentre(vehicle);
    switch (action.kind)
    {
    case BaseAction::Kind::Refuel:
    {
        if (!InventorySystem::removeItem(inv, mItems.fuelCan, 1)) return;
        v.fuel = std::min(v.maxFuel, v.fuel + vehiclerules::kCanFuel);
        awardXp(Skill::Mechanics, skillrules::kXpRefuel);
        mInteractionMessage = "Fuel " + std::to_string(static_cast<int>(std::round(v.fuel))) + " of " +
                              std::to_string(static_cast<int>(std::round(v.maxFuel))) + ".";
        std::cout << "[Vehicle] refuelled to " << v.fuel << '\n';
        break;
    }
    case BaseAction::Kind::MendVehicle:
    {
        if (!playerHasItem(mItems.toolbox) || !InventorySystem::removeItem(inv, mItems.scrapMetal, 1)) return;
        // On a garage's lift (parked by one), twice the work gets done.
        bool lift = false;
        const LayoutData& layout = mMapGenerator.layoutData();
        const glm::ivec2 tile = worldToTile(mTileMap, centre);
        for (const LayoutData::BuildingRect& b : layout.buildings)
        {
            if (buildingkinds::kindOf(b) != BuildingKind::Garage) continue;
            if (tile.x >= b.x - 3 && tile.x < b.x + b.w + 3 && tile.y >= b.y - 3 && tile.y < b.y + b.h + 3) lift = true;
        }
        v.condition = std::min(100.0f, v.condition + vehiclerules::repairAmount(skillLevel(Skill::Mechanics), lift));
        awardXp(Skill::Mechanics, skillrules::kXpVehicleRepair);
        mInteractionMessage = std::string("It runs better. ") + vehiclerules::bandName(vehiclerules::band(v.condition)) + ".";
        refreshVehicleSprite(vehicle);
        std::cout << "[Vehicle] mended to " << v.condition << (lift ? " (lift)" : "") << '\n';
        break;
    }
    case BaseAction::Kind::LoadCargo:
    {
        if (v.hasCargo) return;
        const glm::vec2 rear = vehiclerules::rear(v.kind, centre, v.heading);
        FurniturePiece* best = nullptr;
        float bestDistance = 1.7f * kSpriteSize; // pushed up to the tailgate
        for (FurniturePiece& piece : mFurniture)
        {
            if (!furnitureuse::movable(piece.kind) || piece.tile.x < 0) continue;
            const float d = glm::length(tileCentre(mTileMap, piece.tile) - rear);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = &piece;
            }
        }
        if (best == nullptr) return;
        // Into the bed: the piece leaves the map while it rides.
        const glm::ivec2 aboard(-500 - best->origin.x, -500 - best->origin.y);
        const std::string what = pieceNameOf(best->kind);
        v.hasCargo = true;
        v.cargoOrigin = best->origin;
        moveFurniturePiece(best->tile, aboard);
        mNoiseModel.emitNoise(rear, NoiseTier::Medium, 0.8f, mPlayerEntity);
        mInteractionMessage = "The " + what + " is in the bed.";
        refreshVehicleSprite(vehicle);
        std::cout << "[Vehicle] loaded " << what << '\n';
        break;
    }
    case BaseAction::Kind::UnloadCargo:
    {
        if (!v.hasCargo) return;
        const auto piece = std::find_if(mFurniture.begin(), mFurniture.end(),
                                        [&](const FurniturePiece& p) { return p.origin == v.cargoOrigin; });
        if (piece == mFurniture.end())
        {
            v.hasCargo = false;
            return;
        }
        // Down behind the tailgate; backed up to a doorway, through it to the floor inside.
        const glm::vec2 back = -vehiclerules::forward(v.heading);
        const glm::ivec2 step = std::fabs(back.x) >= std::fabs(back.y) ? glm::ivec2(back.x > 0.0f ? 1 : -1, 0)
                                                                        : glm::ivec2(0, back.y > 0.0f ? 1 : -1);
        const glm::ivec2 first = worldToTile(mTileMap, vehiclerules::rear(v.kind, centre, v.heading));
        const auto taken = [&](const glm::ivec2& tile)
        {
            if (!mTileMap.inBounds(tile.x, tile.y) || mTileMap.isSolid(tile.x, tile.y) || mTileMap.isWater(tile.x, tile.y)) return true;
            if (mTileMap.doorState(tile.x, tile.y) != DoorState::None) return true; // never in the door itself
            if (furnitureAt(tile) != nullptr) return true;
            for (const glm::ivec2& t : occupiedTiles())
            {
                if (t == tile) return true;
            }
            return false;
        };
        // A doorway within two tiles of the tailgate: carried through, onto
        // the floor inside it. Otherwise, the ground behind the tailgate.
        glm::ivec2 target(-1);
        for (int k = 0; k <= 2 && target.x < 0; ++k)
        {
            const glm::ivec2 tile = first + step * k;
            if (mTileMap.doorState(tile.x, tile.y) == DoorState::None) continue;
            if (mTileMap.isSolid(tile.x, tile.y))
            {
                mInteractionMessage = "The door is shut.";
                mInteractionMessageTimer = 2.0f;
                return;
            }
            if (!taken(tile + step)) target = tile + step;
        }
        if (target.x < 0 && !taken(first)) target = first;
        if (target.x < 0)
        {
            mInteractionMessage = "There's no room to set it down there.";
            mInteractionMessageTimer = 2.0f;
            return;
        }
        const std::string what = pieceNameOf(piece->kind);
        moveFurniturePiece(piece->tile, target);
        v.hasCargo = false;
        mNoiseModel.emitNoise(tileCentre(mTileMap, target), NoiseTier::Medium, 0.8f, mPlayerEntity);
        mInteractionMessage = "The " + what + " is down.";
        noteDoorwayBlocked(target);
        refreshVehicleSprite(vehicle);
        std::cout << "[Vehicle] unloaded " << what << " at " << target.x << ',' << target.y
                  << (mTileMap.doorwayBlocked(target.x, target.y) ? " (doorway blocked)" : "") << '\n';
        break;
    }
    default:
        return;
    }
    mInteractionMessageTimer = 2.5f;
}

void Game::noteCarKeys(Entity container)
{
    if (container == kInvalidEntity || mWorld.hasComponent<Sprite>(container) || !mWorld.hasComponent<Transform>(container)) return;
    const Transform& t = mWorld.getComponent<Transform>(container);
    const FurniturePiece* piece = furnitureAt(worldToTile(mTileMap, glm::vec2(t.x + 1.0f, t.y + 1.0f)));
    if (piece == nullptr) return;
    mWorld.forEach<Vehicle>(
        [&](Entity, Vehicle& v)
        {
            if (v.keyKnown || v.keyInside || v.keyTile != piece->origin) return;
            v.keyKnown = true;
            mInteractionMessage = "Car keys. " + vehicleName(v) + " outside.";
            mInteractionMessage[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(mInteractionMessage[0])));
            mInteractionMessage = "Car keys. " + std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(vehicleName(v)[0])))) +
                                  vehicleName(v).substr(1) + " outside.";
            mInteractionMessageTimer = 3.0f;
            std::cout << "[Vehicle] keys found for " << vehiclerules::kindName(v.kind) << '\n';
        });
}
