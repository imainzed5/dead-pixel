// Scavenging (phase 6): getting into buildings, searching what is in them,
// water after the taps, eating and drinking, living off the land, things
// dropped on the floor, and what the town has left. The rules live in
// world/BuildingKinds, world/Forage and world/Garden; this is where they
// meet the game.
#include "core/GameShared.h"
#include "world/WeatherRules.h"

#include <iostream>
#include <random>

#include "world/BuildingKinds.h"
#include "world/Forage.h"
#include "world/Garden.h"
#include "world/Power.h"
#include "world/Storage.h"

using namespace gameshared;

namespace
{
glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

std::uint32_t mix(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = a * 2654435761u ^ (b + 0x9E3779B9u + (a << 6) + (a >> 2));
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

float roll01(std::uint32_t seed)
{
    return static_cast<float>(mix(seed, 0x5bd1e995u) & 0xFFFFFFu) / static_cast<float>(0x1000000);
}

std::uint32_t tileHash(const glm::ivec2& tile)
{
    return static_cast<std::uint32_t>(tile.x) * 73856093u ^ static_cast<std::uint32_t>(tile.y) * 19349663u;
}

// Getting in.
constexpr float kForceSeconds = 6.0f;
constexpr float kBreakDoorDamage = 150.0f;
constexpr float kShoulderDamage = 25.0f;
constexpr float kShoulderStamina = 25.0f;
constexpr float kSmashSeconds = 1.5f;
constexpr float kTapeSeconds = 4.0f;
// Alarms: Loud every 2 s for 45 s; the box takes 3 s.
constexpr float kAlarmSeconds = 45.0f;
constexpr float kAlarmNoiseSeconds = 2.0f;
constexpr float kAlarmBoxSeconds = 3.0f;
constexpr float kAlarmWakeTiles = 30.0f;
// A building's kind is seen from within 8 tiles of its street door.
constexpr float kSeenTiles = 8.0f;
// One kitchen counter in 8 holds a house's keys; they fit 4 houses.
constexpr std::uint32_t kKeyRingOneIn = 8;
constexpr std::size_t kKeyRingHouses = 4;
// A collector's water goes bad after 3 days standing.
constexpr float kCollectorStaleHours = 72.0f;
constexpr float kReachTiles = 1.3f;
// Bad mushrooms, and the radio's comfort in shelter (depression an hour).
constexpr float kBadMushroomIllness = 25.0f;
constexpr const char* kPilePrompt = "Search the pile";

bool hasTile(const std::vector<std::pair<int, int>>& list, const glm::ivec2& tile)
{
    return std::any_of(list.begin(), list.end(), [&](const std::pair<int, int>& p) { return p.first == tile.x && p.second == tile.y; });
}

bool addTile(std::vector<std::pair<int, int>>& list, const glm::ivec2& tile)
{
    if (hasTile(list, tile)) return false;
    list.emplace_back(tile.x, tile.y);
    return true;
}

const char* spotName(forage::Spot spot)
{
    switch (spot)
    {
    case forage::Spot::Bush: return "Bush";
    case forage::Spot::TreeBase: return "Tree";
    case forage::Spot::Tuft: return "Tall grass";
    case forage::Spot::Wild: return "Wild ground";
    case forage::Spot::None: break;
    }
    return "";
}

const char* findName(forage::Find find)
{
    switch (find)
    {
    case forage::Find::Berries: return "berries";
    case forage::Find::Mushrooms: return "mushrooms";
    case forage::Find::Herbs: return "herbs";
    case forage::Find::Nothing: break;
    }
    return "";
}

std::string lowerFirst(std::string text)
{
    if (!text.empty()) text[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
    return text;
}

garden::Bed bedOf(const Structure& s)
{
    garden::Bed bed;
    bed.growHours = s.growHours;
    bed.sinceWaterHours = s.sinceWaterHours;
    bed.regrowing = s.regrowing;
    bed.dead = s.dead;
    return bed;
}

void storeBed(Structure& s, const garden::Bed& bed)
{
    s.growHours = bed.growHours;
    s.sinceWaterHours = bed.sinceWaterHours;
    s.regrowing = bed.regrowing;
    s.dead = bed.dead;
}
} // namespace

// ---------------------------------------------------------------------------
// Each run, the checkpoint, world memory

void Game::addAlarmBox(int building)
{
    const glm::ivec2 door = buildingkinds::streetDoor(mMapGenerator.layoutData(), building);
    if (door.x < 0) return;
    for (const auto& [tile, b] : mAlarmBoxes)
    {
        if (b == building) return;
    }
    const glm::ivec2 box = door + glm::ivec2(0, -1);
    mAlarmBoxes.emplace_back(box, building);
    const Entity e = mWorld.createEntity();
    Transform t{};
    t.x = static_cast<float>(box.x * mTileMap.tileWidth());
    t.y = static_cast<float>(box.y * mTileMap.tileHeight());
    t.prevX = t.x;
    t.prevY = t.y;
    Sprite sprite{};
    sprite.textureId = mSpriteSheet.textureId();
    sprite.uvRect = mSpriteSheet.uvRect("prop_alarm_box");
    sprite.layer = 5;
    sprite.drawSize = 80.0f;
    sprite.sink = 16.0f;
    mWorld.addComponent<Transform>(e, t);
    mWorld.addComponent<Sprite>(e, sprite);
}

void Game::setupScavenging()
{
    const LayoutData& layout = mMapGenerator.layoutData();
    const ScavengeMemoryRecord& memory = mSaveManager.scavengeMemory();

    // Locks follow from the layout; world memory keeps those undone.
    mLocks.clear();
    for (const buildingkinds::DoorLock& lock : buildingkinds::lockedDoors(layout, mFirstDays.homeBuilding))
    {
        if (hasTile(memory.unlockedDoors, lock.tile)) continue;
        if (mTileMap.doorState(lock.tile.x, lock.tile.y) == DoorState::Broken) continue;
        const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(lock.building - 1)];
        const bool edge = lock.tile.x == b.x || lock.tile.y == b.y || lock.tile.x == b.x + b.w - 1 || lock.tile.y == b.y + b.h - 1;
        mLocks.push_back(DoorLockState{lock.tile, lock.building, lock.alarm, 0.0f, edge});
    }

    // A sign over each shop's street door.
    std::vector<StructureView::ShopSign> signs;
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        const auto kind = static_cast<BuildingKind>(std::clamp(layout.buildings[i].kind, 0, static_cast<int>(BuildingKind::Count) - 1));
        const char* sprite = buildingkinds::signSprite(buildingkinds::signOf(kind));
        if (sprite == nullptr || sprite[0] == '\0') continue;
        const glm::ivec2 door = buildingkinds::streetDoor(layout, static_cast<int>(i) + 1);
        if (door.x < 0) continue;
        signs.push_back(StructureView::ShopSign{door, mSpriteSheet.uvRect(sprite)});
    }
    mStructureView.setSigns(std::move(signs));

    // An alarm box just inside the street door of each building with an alarm.
    mAlarmBoxes.clear();
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        const int building = static_cast<int>(i) + 1;
        if (building == mFirstDays.homeBuilding || !buildingkinds::buildingHasAlarm(layout.buildings[i])) continue;
        addAlarmBox(building);
    }

    mAlarms.clear();
    mAlarmsSpent.clear();
    mSearchedThisRun.clear();
    mLitLamps.clear();
    mForaged.clear();
    mForageTile = glm::ivec2(-1);
    mPowerForced = -1;
    mDipAnnounced = false;
    mPowerOutAnnounced = false;
    mPowerWentAsleep = false;
    mRadioOn = false;
    mRadioTimer = 0.0f;
    mSeenTimer = 0.0f;
    mLastRainHour = -1000.0;
    mOnBeds.clear();

    // Things left lying indoors by earlier lives.
    restoreFloorPiles(memory.floorItems);
    mPowerWasOn = powerOn();
    countTownStock();
    applyFurnitureAlts();
    int alarms = 0;
    for (const DoorLockState& lock : mLocks) alarms += lock.alarm ? 1 : 0;
    std::cout << "[Scavenge] " << mLocks.size() << " locked doors (" << alarms << " alarmed), "
              << mAlarmBoxes.size() << " alarm boxes; cupboards unopened " << mTownUnopened << " of " << mTownContainers << "\n";
}

ScavengeRunRecord Game::scavengeRunRecord() const
{
    ScavengeRunRecord record;
    record.saved = true;
    record.powerForced = mPowerForced;
    record.dipAnnounced = mDipAnnounced;
    record.outAnnounced = mPowerOutAnnounced;
    for (const glm::ivec2& tile : mSearchedThisRun) record.searched.emplace_back(tile.x, tile.y);
    for (const ForagedSpot& spot : mForaged) record.foraged.push_back({spot.tile.x, spot.tile.y, spot.day});
    record.floorItems = floorPiles(false);
    for (const glm::ivec2& tile : mLitLamps) record.litLamps.emplace_back(tile.x, tile.y);
    record.alarmsSpent = mAlarmsSpent;
    record.radioOn = mRadioOn;
    return record;
}

void Game::restoreScavenging(const ScavengeRunRecord& record)
{
    if (!record.saved) return;
    mPowerForced = record.powerForced;
    mDipAnnounced = record.dipAnnounced;
    mPowerOutAnnounced = record.outAnnounced;
    mSearchedThisRun.clear();
    for (const auto& [x, y] : record.searched) mSearchedThisRun.emplace_back(x, y);
    mForaged.clear();
    for (const auto& f : record.foraged) mForaged.push_back(ForagedSpot{glm::ivec2(f[0], f[1]), f[2]});
    // The checkpoint's piles replace those world memory laid down.
    std::vector<Entity> old;
    mWorld.forEach<Interactable, Inventory>(
        [&](Entity e, Interactable& inter, Inventory&)
        {
            if (inter.prompt == kPilePrompt) old.push_back(e);
        });
    for (Entity e : old) mWorld.destroyEntity(e);
    restoreFloorPiles(record.floorItems);
    mLitLamps.clear();
    for (const auto& [x, y] : record.litLamps) mLitLamps.emplace_back(x, y);
    mAlarmsSpent = record.alarmsSpent;
    mRadioOn = record.radioOn;
    mPowerWasOn = powerOn();
    applyFurnitureAlts();
}

void Game::saveScavengeMemory()
{
    ScavengeMemoryRecord memory = mSaveManager.scavengeMemory();
    memory.floorItems = floorPiles(true);
    mSaveManager.setScavengeMemory(std::move(memory));
    mSaveManager.saveWorldMemory();
}

void Game::migrateScavenging()
{
    const int from = mSaveManager.migratedFromVersion();
    if (from <= 0 || from >= 3) return;
    // Shops were furnished by older rules: what the world remembers of
    // containers that are no longer there goes.
    std::vector<glm::ivec2> containers;
    for (const FurniturePiece& piece : mFurniture)
    {
        if (furniture::info(piece.kind).prompt != nullptr) containers.push_back(piece.tile);
    }
    std::vector<glm::ivec2> stale;
    for (const ContainerRecord& record : mSaveManager.containerRecords())
    {
        const glm::ivec2 tile(record.tileX, record.tileY);
        if (std::find(containers.begin(), containers.end(), tile) == containers.end()) stale.push_back(tile);
    }
    for (const glm::ivec2& tile : stale) mSaveManager.removeContainerRecord(tile.x, tile.y);
    mSaveManager.saveWorldMemory();
    std::cout << "[Save] migrated from version " << from << ": shops refurnished, " << stale.size()
              << " container records dropped\n";
}

// ---------------------------------------------------------------------------
// Locks and alarms

bool Game::doorLocked(const glm::ivec2& tile) const
{
    return std::any_of(mLocks.begin(), mLocks.end(), [&](const DoorLockState& lock) { return lock.tile == tile; });
}

bool Game::handleLockedDoor(const glm::ivec2& tile)
{
    auto lock = std::find_if(mLocks.begin(), mLocks.end(), [&](const DoorLockState& l) { return l.tile == tile; });
    if (lock == mLocks.end() || mPlayerEntity == kInvalidEntity) return false;
    const glm::ivec2 me = playerTile();
    const glm::vec2 centre = tileCentre(mTileMap, tile);
    // From inside, the lock turns.
    if (lock->edge && mTileMap.getBuildingId(me.x, me.y) == lock->building)
    {
        mProximityPrompt = "Locked   E: Unlock it";
        if (!mInput.pressed(Action::Use)) return false;
        unlockDoor(tile, "from inside");
        mDoors.open(mTileMap, tile);
        makeNoise(centre, NoiseTier::Soft, 0.5f, SoundId::DoorOpen, mPlayerEntity);
        mInteractionMessage = "You turn the lock and open it.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    Inventory* inv = mWorld.hasComponent<Inventory>(mPlayerEntity) ? &mWorld.getComponent<Inventory>(mPlayerEntity) : nullptr;
    const bool crowbar = inv != nullptr && InventorySystem::countTagged(*inv, mItemDatabase, "tool:pry") > 0;
    mProximityPrompt = std::string("Locked   ") + (crowbar ? "X: Force it" : "X: Break it down");
    if (mInput.pressed(Action::Use))
    {
        mInteractionMessage = "Locked.";
        mInteractionMessageTimer = 1.5f;
        return true;
    }
    if (!mInput.pressed(Action::Pry)) return false;
    if (crowbar)
    {
        mBaseAction = BaseAction{};
        mBaseAction.kind = BaseAction::Kind::Force;
        mBaseAction.tile = tile;
        mBaseAction.total = kForceSeconds;
        mBaseAction.label = "Forcing the door";
        mBaseAction.startPos = playerWorldCenter();
        mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
        return true;
    }
    // A blow: the weapon in hand, or a shoulder.
    float damage = kShoulderDamage;
    bool shoulder = true;
    if (mWorld.hasComponent<Combat>(mPlayerEntity))
    {
        const WeaponStats& weapon = mWorld.getComponent<Combat>(mPlayerEntity).weapon;
        if (weapon.itemId >= 0 && weapon.category != WeaponCategory::Ranged && weapon.category != WeaponCategory::Unarmed)
        {
            damage = weapon.damage;
            shoulder = false;
        }
    }
    if (shoulder && mWorld.hasComponent<Stamina>(mPlayerEntity))
    {
        Stamina& stamina = mWorld.getComponent<Stamina>(mPlayerEntity);
        if (stamina.current < kShoulderStamina)
        {
            mInteractionMessage = "Too winded to throw yourself at it.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
        stamina.current -= kShoulderStamina;
    }
    lock->damage += damage;
    makeNoise(centre, NoiseTier::Loud, 1.0f, SoundId::DeadDoorBang, mPlayerEntity);
    mParticleSystem.spawnDust(centre, 4);
    mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.15f);
    if (lock->damage >= kBreakDoorDamage)
    {
        const int building = lock->building;
        mLocks.erase(lock);
        mDoors.setState(mTileMap, tile, DoorState::Broken);
        mParticleSystem.spawnDust(centre, 10);
        mInteractionMessage = "The door gives way.";
        mInteractionMessageTimer = 2.5f;
        std::cout << "[Scavenge] door " << tile.x << "," << tile.y << " broken in\n";
        tripAlarm(building, tile);
        return true;
    }
    mInteractionMessage = shoulder ? "You throw your shoulder into it. It holds." : "You hit the door. It holds.";
    mInteractionMessageTimer = 2.0f;
    return true;
}

void Game::unlockDoor(const glm::ivec2& tile, const char* how)
{
    std::erase_if(mLocks, [&](const DoorLockState& lock) { return lock.tile == tile; });
    ScavengeMemoryRecord memory = mSaveManager.scavengeMemory();
    if (addTile(memory.unlockedDoors, tile))
    {
        mSaveManager.setScavengeMemory(std::move(memory));
        mSaveManager.saveWorldMemory();
    }
    std::cout << "[Scavenge] door " << tile.x << "," << tile.y << " unlocked (" << how << ")\n";
}

void Game::tripAlarm(int building, const glm::ivec2& door)
{
    const LayoutData& layout = mMapGenerator.layoutData();
    if (building <= 0 || building > static_cast<int>(layout.buildings.size()) || !powerOn()) return;
    if (building == mFirstDays.homeBuilding) return;
    // A building with an alarm has its box (setupScavenging).
    if (std::none_of(mAlarmBoxes.begin(), mAlarmBoxes.end(), [&](const auto& box) { return box.second == building; })) return;
    if (std::find(mAlarmsSpent.begin(), mAlarmsSpent.end(), building) != mAlarmsSpent.end()) return;
    mAlarmsSpent.push_back(building);
    mAlarms.push_back(AlarmRinging{building, door, kAlarmSeconds, 0.0f});
    mInteractionMessage = "An alarm starts to ring.";
    mInteractionMessageTimer = 3.0f;
    std::cout << "[Scavenge] alarm ringing at building " << building << " (" << door.x << "," << door.y << ")\n";
}

void Game::updateAlarms(float dtSeconds)
{
    if (mAlarms.empty()) return;
    const bool on = powerOn();
    for (AlarmRinging& alarm : mAlarms)
    {
        if (!on)
        {
            alarm.left = 0.0f;
            std::cout << "[Scavenge] alarm at building " << alarm.building << " died with the power\n";
            continue;
        }
        alarm.left -= dtSeconds;
        alarm.noiseTimer -= dtSeconds;
        if (alarm.left > 0.0f && alarm.noiseTimer <= 0.0f)
        {
            alarm.noiseTimer += kAlarmNoiseSeconds;
            const glm::vec2 centre = tileCentre(mTileMap, alarm.door);
            makeNoise(centre, NoiseTier::Loud, 1.0f, SoundId::ShopAlarm);
            wakeSleeper(centre, kAlarmWakeTiles, 3, "An alarm is ringing!");
        }
        if (alarm.left <= 0.0f) std::cout << "[Scavenge] alarm at building " << alarm.building << " ran down\n";
    }
    std::erase_if(mAlarms, [](const AlarmRinging& alarm) { return alarm.left <= 0.0f; });
}

bool Game::handleAlarmBox(const ProximityPrompt&)
{
    if (mAlarms.empty() || mPlayerEntity == kInvalidEntity) return false;
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    for (const auto& [box, building] : mAlarmBoxes)
    {
        const bool ringing = std::any_of(mAlarms.begin(), mAlarms.end(),
                                         [&, b = building](const AlarmRinging& a) { return a.building == b; });
        if (!ringing || glm::length(tileCentre(mTileMap, box) - here) > kReachTiles * tileSize) continue;
        mProximityPrompt = "Alarm box   E: Stop the alarm";
        if (!mInput.pressed(Action::Use)) return true;
        mBaseAction = BaseAction{};
        mBaseAction.kind = BaseAction::Kind::StopAlarm;
        mBaseAction.tile = box;
        mBaseAction.slot = building;
        mBaseAction.total = kAlarmBoxSeconds;
        mBaseAction.label = "Pulling the wires";
        mBaseAction.startPos = here;
        mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Searching

void Game::beginSearch(Entity container)
{
    if (container == kInvalidEntity || !mWorld.hasComponent<Transform>(container)) return;
    const Transform& t = mWorld.getComponent<Transform>(container);
    const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 1.0f, t.y + 1.0f));
    const bool furnitureContainer = !mWorld.hasComponent<Sprite>(container) && !mWorld.hasComponent<Structure>(container) &&
                                    !mWorld.hasComponent<Corpse>(container) && !mWorld.hasComponent<Vehicle>(container);
    const FurniturePiece* piece = furnitureContainer ? furnitureAt(tile) : nullptr;
    if (piece == nullptr || searchedThisRun(tile))
    {
        openContainer(container);
        return;
    }
    float seconds = furniture::searchSeconds(piece->kind) * skillrules::searchTimeScale(skillLevel(Skill::Scavenging));
    // The hardware store's stock-room cabinet (phase 9) is locked like the armoury.
    const bool fuseCabinet = tile == mFuseCabinet;
    const bool armoury = (piece->armoury || fuseCabinet) && !hasTile(mSaveManager.scavengeMemory().searched, tile);
    if (armoury)
    {
        const bool crowbar = mWorld.hasComponent<Inventory>(mPlayerEntity) &&
                             InventorySystem::countTagged(mWorld.getComponent<Inventory>(mPlayerEntity), mItemDatabase, "tool:pry") > 0;
        if (!crowbar)
        {
            mInteractionMessage = "Locked. It would take a crowbar.";
            mInteractionMessageTimer = 2.5f;
            return;
        }
        seconds = furniture::kArmouryForceSeconds;
    }
    // Their people always knew where to look (phase 10).
    if (const Traits* traits = playerTraits(); !armoury && traits != nullptr && traits->has(BloodlineTrait::Scavenger))
    {
        seconds *= legacy::kScavengerSearch;
    }
    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::Search;
    mBaseAction.tile = tile;
    mBaseAction.target = container;
    mBaseAction.total = seconds;
    mBaseAction.window = armoury; // forced open: Loud
    if (!armoury)
    {
        SoundId opening = SoundId::OpenCupboard;
        switch (piece->kind)
        {
        case FurnitureKind::Fridge: opening = SoundId::OpenFridge; break;
        case FurnitureKind::Locker:
        case FurnitureKind::MetalRack:
        case FurnitureKind::ToolRack: opening = SoundId::OpenLocker; break;
        case FurnitureKind::Dresser:
        case FurnitureKind::Desk:
        case FurnitureKind::FilingCabinet:
        case FurnitureKind::Checkout: opening = SoundId::OpenDrawer; break;
        default: break;
        }
        playAt(opening, tileCentre(mTileMap, tile), 0.85f);
    }
    mBaseAction.label = armoury ? (fuseCabinet ? "Forcing the cabinet" : "Forcing the locker")
                                : "Searching the " + lowerFirst(containerName(container));
    mBaseAction.startPos = playerWorldCenter();
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
}

bool Game::searchedThisRun(const glm::ivec2& tile) const
{
    return std::find(mSearchedThisRun.begin(), mSearchedThisRun.end(), tile) != mSearchedThisRun.end();
}

void Game::finishSearch()
{
    const Entity target = mBaseAction.target;
    const glm::ivec2 tile = mBaseAction.tile;
    const bool armoury = mBaseAction.window;
    mBaseAction = BaseAction{};
    if (target == kInvalidEntity || !mWorld.hasComponent<Inventory>(target)) return;
    if (!searchedThisRun(tile)) mSearchedThisRun.push_back(tile);

    ScavengeMemoryRecord memory = mSaveManager.scavengeMemory();
    bool changed = false;
    if (addTile(memory.searched, tile))
    {
        changed = true;
        mFurnitureView.setAlt(tile, true);
        mTownUnopened = std::max(0, mTownUnopened - 1);
        const int district = mTileMap.getDistrictId(tile.x, tile.y);
        if (district >= 0 && district < static_cast<int>(mDistrictStocked.size()))
        {
            mDistrictStocked[static_cast<std::size_t>(district)] = std::max(0, mDistrictStocked[static_cast<std::size_t>(district)] - 1);
        }
    }

    // A house's keys, in one kitchen counter in eight.
    std::string keysLine;
    const FurniturePiece* piece = furnitureAt(tile);
    const LayoutData& layout = mMapGenerator.layoutData();
    const int building = mTileMap.getBuildingId(tile.x, tile.y);
    const bool house = building > 0 && building <= static_cast<int>(layout.buildings.size()) &&
                       buildingkinds::kindOf(layout.buildings[static_cast<std::size_t>(building - 1)]) == BuildingKind::House;
    if (piece != nullptr && piece->kind == FurnitureKind::Counter && house && !hasTile(memory.usedKeyRings, tile) &&
        mix(tileHash(tile), mSaveManager.worldSeed() ^ 0x4B45u) % kKeyRingOneIn == 0)
    {
        addTile(memory.usedKeyRings, tile);
        changed = true;
        std::vector<std::pair<float, int>> houses;
        for (const DoorLockState& lock : mLocks)
        {
            const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(lock.building - 1)];
            if (buildingkinds::kindOf(b) != BuildingKind::House) continue;
            if (std::any_of(houses.begin(), houses.end(), [&](const auto& h) { return h.second == lock.building; })) continue;
            const glm::vec2 c(static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f, static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f);
            houses.emplace_back(glm::length(c - glm::vec2(tile)), lock.building);
        }
        std::sort(houses.begin(), houses.end());
        if (houses.size() > kKeyRingHouses) houses.resize(kKeyRingHouses);
        std::vector<glm::ivec2> doors;
        for (const DoorLockState& lock : mLocks)
        {
            if (std::any_of(houses.begin(), houses.end(), [&](const auto& h) { return h.second == lock.building; })) doors.push_back(lock.tile);
        }
        for (const glm::ivec2& door : doors)
        {
            addTile(memory.unlockedDoors, door);
            std::erase_if(mLocks, [&](const DoorLockState& lock) { return lock.tile == door; });
        }
        if (!houses.empty()) keysLine = "Keys. They fit the houses on this street.";
        std::cout << "[Scavenge] a key ring at " << tile.x << "," << tile.y << " opens " << houses.size() << " houses\n";
    }
    if (changed)
    {
        mSaveManager.setScavengeMemory(std::move(memory));
        mSaveManager.saveWorldMemory();
    }

    openContainer(target);
    if (!keysLine.empty())
    {
        mInteractionMessage = keysLine;
        mInteractionMessageTimer = 3.5f;
    }
    else if (armoury)
    {
        mInteractionMessage = tile == mFuseCabinet ? "The cabinet door bends open." : "The locker door bends open.";
        mInteractionMessageTimer = 2.5f;
    }
    else if (piece != nullptr && piece->kind == FurnitureKind::Fridge)
    {
        // Without the power, what is in a fridge turns.
        const Inventory& inv = mWorld.getComponent<Inventory>(target);
        for (const InventorySlot& slot : inv.slots)
        {
            const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
            if (item != nullptr && item->spoilHours > 0.0f && slot.condition < 0.3f)
            {
                mInteractionMessage = "It has turned.";
                mInteractionMessageTimer = 2.5f;
                break;
            }
        }
    }
}

void Game::finishScavengeAction()
{
    const BaseAction action = mBaseAction;
    switch (action.kind)
    {
    case BaseAction::Kind::Search:
        finishSearch();
        return;
    case BaseAction::Kind::Forage:
        finishForage();
        return;
    case BaseAction::Kind::Force:
    {
        mBaseAction = BaseAction{};
        auto lock = std::find_if(mLocks.begin(), mLocks.end(), [&](const DoorLockState& l) { return l.tile == action.tile; });
        const int building = lock != mLocks.end() ? lock->building : mTileMap.getBuildingId(action.tile.x, action.tile.y);
        unlockDoor(action.tile, "forced");
        mDoors.open(mTileMap, action.tile);
        makeNoise(tileCentre(mTileMap, action.tile), NoiseTier::Loud, 1.0f, SoundId::DoorOpen, mPlayerEntity);
        mInteractionMessage = "The lock gives. The door swings in.";
        mInteractionMessageTimer = 2.5f;
        tripAlarm(building, action.tile);
        return;
    }
    case BaseAction::Kind::Smash:
    case BaseAction::Kind::Tape:
    {
        mBaseAction = BaseAction{};
        const bool loud = action.kind == BaseAction::Kind::Smash;
        if (!loud)
        {
            if (const ItemDef* tape = mItemDatabase.findByKey("duct_tape"))
            {
                if (!InventorySystem::removeItem(mWorld.getComponent<Inventory>(mPlayerEntity), tape->id, 1)) return;
            }
        }
        breakWindow(action.tile, loud);
        mInteractionMessage = loud ? "The glass bursts inward." : "You tape the glass and knock it out. Hardly a sound.";
        mInteractionMessageTimer = 2.5f;
        // The glass: from outside, into whichever building it belongs to.
        int building = 0;
        if (const Windows::Window* window = mWindows.find(action.tile))
        {
            building = mTileMap.getBuildingId(window->spot.inside.x, window->spot.inside.y);
        }
        tripAlarm(building, action.tile);
        return;
    }
    case BaseAction::Kind::StopAlarm:
    {
        mBaseAction = BaseAction{};
        const int building = action.slot;
        std::erase_if(mAlarms, [&](const AlarmRinging& a) { return a.building == building; });
        mInteractionMessage = "The alarm stops.";
        mInteractionMessageTimer = 2.5f;
        std::cout << "[Scavenge] alarm at building " << building << " stopped at the box\n";
        return;
    }
    default:
        mBaseAction = BaseAction{};
        return;
    }
}

// ---------------------------------------------------------------------------
// Water, food and drink

bool Game::handleWaterSource(const ProximityPrompt&)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const glm::ivec2 me = playerTile();

    const FurniturePiece* toilet = nullptr;
    for (const FurniturePiece& piece : mFurniture)
    {
        if (piece.kind == FurnitureKind::Toilet && glm::length(tileCentre(mTileMap, piece.tile) - here) < kReachTiles * tileSize)
        {
            toilet = &piece;
            break;
        }
    }
    bool pond = false;
    for (int dy = -1; dy <= 1 && !pond && toilet == nullptr; ++dy)
    {
        for (int dx = -1; dx <= 1 && !pond; ++dx)
        {
            const int x = me.x + dx;
            const int y = me.y + dy;
            if (!mTileMap.inBounds(x, y)) continue;
            const int base = ((std::max(1, mTileMap.groundGid(x, y)) - 1) % TileGid::kPerRow) + 1;
            pond = base == TileGid::kWater;
        }
    }
    if (toilet == nullptr && !pond) return false;
    const bool used = toilet != nullptr && hasTile(mSaveManager.scavengeMemory().usedWater, toilet->tile);
    if (toilet != nullptr)
    {
        if (!used) mProximityPrompt += "   R: Fill a bottle from the cistern";
    }
    else
    {
        // Frozen in winter (phase 7): the ice must be broken first.
        const char* fill = pondFrozen() ? "   R: Break the ice, fill bottles (dirty)" : "   R: Fill bottles (dirty)";
        mProximityPrompt = mProximityPrompt.empty() ? std::string("Pond") + fill : mProximityPrompt + fill;
    }
    if (!mInput.pressed(Action::Board)) return false;
    // A frozen pond (phase 7): the ice has to be broken first.
    if (toilet == nullptr && pondFrozen() && !playerHasItem(mItems.axe) && !playerHasItem(mItems.hammer))
    {
        mInteractionMessage = "The pond is frozen. An axe or a hammer would break the ice.";
        mInteractionMessageTimer = 2.5f;
        return true;
    }
    if (used)
    {
        mInteractionMessage = "The cistern is dry.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    const int empties = InventorySystem::countItem(inv, mItems.emptyBottle);
    if (empties == 0)
    {
        mInteractionMessage = "No empty bottles to fill.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    const int fill = toilet != nullptr ? 1 : empties;
    InventorySystem::removeItem(inv, mItems.emptyBottle, fill);
    const int added = InventorySystem::addItem(inv, mItemDatabase, mItems.dirtyWater, fill).added;
    if (added < fill) InventorySystem::addItem(inv, mItemDatabase, mItems.emptyBottle, fill - added);
    if (toilet != nullptr && added > 0)
    {
        ScavengeMemoryRecord memory = mSaveManager.scavengeMemory();
        addTile(memory.usedWater, toilet->tile);
        mSaveManager.setScavengeMemory(std::move(memory));
        mSaveManager.saveWorldMemory();
    }
    makeNoise(here, NoiseTier::Soft, 0.5f, SoundId::BottleFill, mPlayerEntity);
    mInteractionMessage = toilet != nullptr ? "One bottle from the cistern. It is not clean."
                                            : "You fill " + std::to_string(added) + (added == 1 ? " bottle" : " bottles") + ". The water is not clean.";
    mInteractionMessageTimer = 2.5f;
    return true;
}

bool Game::eatOrDrink(int slotIndex)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity) || !mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        return false;
    }
    if (slotIndex < 0 || slotIndex >= Inventory::kMaxSlots) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const InventorySlot slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item == nullptr || (item->category != ItemCategory::Food && item->category != ItemCategory::Drink)) return false;
    if (item->hungerRestore <= 0.0f && item->thirstRestore <= 0.0f)
    {
        mInteractionMessage = "It needs cooking first.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
    const Traits* traits = playerTraits();
    const float weak = traits != nullptr ? traitrules::badFoodIllnessScale(*traits) : 1.0f;
    const float comfort = mWorld.hasComponent<MentalState>(mPlayerEntity)
                              ? mindrules::comfortScale(mindrules::depressionBand(mWorld.getComponent<MentalState>(mPlayerEntity).depression))
                              : 1.0f;
    const bool perishable = item->spoilHours > 0.0f;
    const float value = perishable ? food::valueScale(slot.condition) : 1.0f;
    needs.hunger = std::min(needs.maxHunger, needs.hunger + item->hungerRestore * value);
    needs.thirst = std::min(needs.maxThirst, needs.thirst + item->thirstRestore * value);
    // A drink cools you when you are hot (phase 7).
    if (item->thirstRestore > 0.0f && needs.bodyTemperature > 37.5f)
    {
        needs.bodyTemperature = std::max(37.5f, needs.bodyTemperature - weatherrules::kDrinkCooling);
    }
    const bool drink = item->category == ItemCategory::Drink;
    std::string message = std::string(drink ? "Drank " : "Ate ") + lowerFirst(item->name) + ".";

    // What it does to you.
    const float illness = food::illnessFrom(perishable ? slot.condition : 1.0f, item->rawIllness, weak);
    if (illness > 0.0f)
    {
        needs.illness = std::min(100.0f, needs.illness + illness);
        message = item->rawIllness > 0.0f ? "Ate it raw. Your stomach turns." : "Ate spoiled " + lowerFirst(item->name) + ". You feel sick.";
    }
    if (slot.itemId == mItems.dirtyWater)
    {
        needs.illness = std::min(100.0f, needs.illness + food::kDirtyWaterIllness * weak);
        message = "The water tastes of mud.";
        if (bodyRoll() < food::kBadStomachChance)
        {
            needs.badStomachHours = food::kBadStomachHours;
            message = "The water tastes of mud. Your stomach cramps.";
        }
    }
    if (slot.itemId == mItems.mushrooms && forage::badMushroom(skillLevel(Skill::LivingOffTheLand), bodyRoll()))
    {
        needs.illness = std::min(100.0f, needs.illness + kBadMushroomIllness * weak);
        message = "That one tasted wrong.";
    }

    // Coffee, comfort and calm.
    if (item->sleepDebtMask > 0.0f)
    {
        const double nowHour = absoluteHour();
        if (sleeprules::doubleCoffee(nowHour, needs.lastCoffeeHour))
        {
            addPanic(sleeprules::kCoffeeDoublePanic);
            needs.coffeeJitterHours = sleeprules::kCoffeeJitterHours;
            message = "Another coffee. Your hands start to shake.";
        }
        needs.coffeeHours = item->sleepDebtMask;
        needs.lastCoffeeHour = nowHour;
    }
    if (item->hasTag("sedative")) needs.lastSedativeHour = absoluteHour();
    if (item->panicRelief > 0.0f) addPanic(-item->panicRelief);
    // Hot food (anything off a fire) warms you and lifts the mood.
    bool hot = false;
    for (const CraftingRecipe& recipe : mCraftingSystem.recipes())
    {
        if (recipe.station == CraftStation::Fire && recipe.outputItemId == item->id && recipe.outputItemId != mItems.waterBottle) hot = true;
    }
    float lift = item->depressionRelief;
    if (hot)
    {
        lift = std::max(lift, mindrules::kHotMealRelief);
        needs.hotMealHours = food::kHotMealHours;
    }
    if (lift > 0.0f) addDepression(-lift * comfort);

    InventorySystem::consumeOne(inv, mItemDatabase, slotIndex);
    mInteractionMessage = message;
    mInteractionMessageTimer = 3.0f;
    mAudioManager.play(drink ? SoundId::BodyDrink : SoundId::BodyEat);
    return true;
}

std::string Game::packItemName(const InventorySlot& slot) const
{
    const ItemDef* item = slot.itemId >= 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item == nullptr) return "";
    if ((item->category == ItemCategory::Food || item->category == ItemCategory::Drink) && item->spoilHours > 0.0f)
    {
        return item->name + " (" + food::stateName(food::stateOf(slot.condition)) + ")";
    }
    // A grave's marker names whose it is (phase 10).
    if (slot.itemId == mItems.marker) return markerName(slot);
    return item->name;
}

// ---------------------------------------------------------------------------
// Batteries, the radio, dropping things

bool Game::useBatteryOrRadio(int slotIndex)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    if (slotIndex < 0 || slotIndex >= Inventory::kMaxSlots) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item == nullptr) return false;
    if (item->id == mItems.battery)
    {
        // Into the emptiest light or radio.
        int best = -1;
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            const InventorySlot& other = inv.slots[static_cast<std::size_t>(i)];
            const ItemDef* def = other.itemId >= 0 && other.count > 0 ? mItemDatabase.find(other.itemId) : nullptr;
            if (def == nullptr || (other.itemId != mItems.flashlight && !def->hasTag("radio")) || other.condition >= 0.999f) continue;
            if (best < 0 || other.condition < inv.slots[static_cast<std::size_t>(best)].condition) best = i;
        }
        if (best < 0)
        {
            mInteractionMessage = "Nothing you carry needs a battery.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
        InventorySlot& target = inv.slots[static_cast<std::size_t>(best)];
        target.condition = 1.0f;
        const bool radio = target.itemId != mItems.flashlight;
        InventorySystem::consumeOne(inv, mItemDatabase, slotIndex);
        mInteractionMessage = radio ? "A fresh battery in the radio." : "A fresh battery in the flashlight.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    if (!item->hasTag("radio")) return false;
    if (!mRadioOn && slot.condition <= 0.001f)
    {
        mInteractionMessage = "The radio is dead. It needs a battery.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    mRadioOn = !mRadioOn;
    mRadioTimer = 0.0f; // the forecast at once
    mBroadcastTimer = 6.0f; // then the looped voice, once the forecast is said (phase 9)
    mBroadcastLine = 0;
    mInteractionMessage = mRadioOn ? "The radio hisses, then a voice." : "You switch the radio off.";
    mInteractionMessageTimer = 2.0f;
    std::cout << "[Power] radio " << (mRadioOn ? "on" : "off") << "\n";
    return true;
}

void Game::dropFromPack(int slotIndex, bool wholeStack)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    if (slotIndex < 0 || slotIndex >= Inventory::kMaxSlots) return;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    if (inv.slots[static_cast<std::size_t>(slotIndex)].itemId < 0 || inv.slots[static_cast<std::size_t>(slotIndex)].count <= 0) return;
    if (slotIndex == mEquippedWeaponSlot) syncEquippedWeapon(); // its wear goes with it
    InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    if (slot.itemId == mItems.marker)
    {
        mInteractionMessage = "The marker stays with you until it is set.";
        mInteractionMessageTimer = 2.0f;
        return;
    }
    const ItemDef* item = mItemDatabase.find(slot.itemId);
    const int count = wholeStack ? slot.count : 1;
    dropOnFloor(playerTile(), slot.itemId, count, slot.condition, slot.loaded);
    slot.count -= count;
    if (slot.count <= 0) slot = InventorySlot{};
    if (slotIndex == mEquippedWeaponSlot) syncEquippedWeapon();
    if (mFlashlightOn && !playerHasItem(mItems.flashlight)) mFlashlightOn = false;
    const std::string name = item != nullptr ? lowerFirst(item->name) : "it";
    mInteractionMessage = count == 1 ? "You put down the " + name + "." : "You put down " + std::to_string(count) + " " + name + ".";
    mInteractionMessageTimer = 2.0f;
}

Entity Game::pileAt(const glm::ivec2& tile) const
{
    Entity pile = kInvalidEntity;
    auto& world = const_cast<World&>(mWorld);
    world.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable& inter, Inventory&, Transform& t)
        {
            if (pile != kInvalidEntity || inter.prompt != kPilePrompt) return;
            if (worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f)) == tile) pile = e;
        });
    return pile;
}

std::vector<FloorItemRecord> Game::floorPiles(bool indoorsOnly) const
{
    std::vector<FloorItemRecord> piles;
    auto& world = const_cast<World&>(mWorld);
    world.forEach<Interactable, Inventory, Transform>(
        [&](Entity, Interactable& inter, Inventory& inv, Transform& t)
        {
            if (inter.prompt != kPilePrompt) return;
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            if (indoorsOnly && !mTileMap.isIndoors(tile.x, tile.y)) return;
            FloorItemRecord pile;
            pile.x = tile.x;
            pile.y = tile.y;
            for (const InventorySlot& slot : inv.slots)
            {
                if (slot.itemId < 0 || slot.count <= 0) continue;
                ContainerItemRecord item;
                item.itemId = slot.itemId;
                item.count = slot.count;
                item.condition = slot.condition;
                item.loaded = slot.loaded;
                pile.items.push_back(item);
            }
            if (!pile.items.empty()) piles.push_back(std::move(pile));
        });
    return piles;
}

void Game::restoreFloorPiles(const std::vector<FloorItemRecord>& piles)
{
    for (const FloorItemRecord& pile : piles)
    {
        const glm::ivec2 tile(pile.x, pile.y);
        if (!mTileMap.inBounds(tile.x, tile.y)) continue;
        for (const ContainerItemRecord& item : pile.items)
        {
            if (mItemDatabase.find(item.itemId) == nullptr) continue;
            dropOnFloor(tile, item.itemId, item.count, item.condition, item.loaded);
        }
    }
}

// ---------------------------------------------------------------------------
// Living off the land

bool Game::handleForage(const ProximityPrompt& entityPrompt)
{
    if (mForageSpots.empty() || mPlayerEntity == kInvalidEntity) return false;
    if (entityPrompt.entity != kInvalidEntity) return false; // something else to use is here
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const std::pair<glm::ivec2, forage::Spot>* spot = nullptr;
    float best = kReachTiles * tileSize;
    for (const auto& candidate : mForageSpots)
    {
        const float d = glm::length(tileCentre(mTileMap, candidate.first) - here);
        if (d < best)
        {
            best = d;
            spot = &candidate;
        }
    }
    if (spot == nullptr) return false;
    const auto picked = std::find_if(mForaged.begin(), mForaged.end(), [&](const ForagedSpot& f) { return f.tile == spot->first; });
    const bool bare = picked != mForaged.end() && !forage::regrown(picked->day, mCurrentDay);
    mProximityPrompt = std::string(spotName(spot->second)) + (bare ? " (picked over)" : "   E: Search it");
    if (!mInput.pressed(Action::Use)) return false;
    if (bare)
    {
        mInteractionMessage = "Nothing left here. Give it a few days.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::Forage;
    mBaseAction.tile = spot->first;
    mBaseAction.total = forage::kSearchSeconds;
    mBaseAction.label = "Searching the " + lowerFirst(spotName(spot->second));
    mBaseAction.startPos = here;
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
    mForageTile = spot->first;
    return true;
}

void Game::finishForage()
{
    const glm::ivec2 tile = mBaseAction.tile;
    mBaseAction = BaseAction{};
    mForageTile = glm::ivec2(-1);
    const auto spot = std::find_if(mForageSpots.begin(), mForageSpots.end(), [&](const auto& s) { return s.first == tile; });
    if (spot == mForageSpots.end() || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    const bool rained = absoluteHour() - mLastRainHour <= 48.0;
    const forage::Result result = forage::search(spot->second, tile, mCurrentDay, mWeatherState.season, rained,
                                                 skillLevel(Skill::LivingOffTheLand), mSaveManager.currentRunSeed());
    auto picked = std::find_if(mForaged.begin(), mForaged.end(), [&](const ForagedSpot& f) { return f.tile == tile; });
    if (picked != mForaged.end()) picked->day = mCurrentDay;
    else mForaged.push_back(ForagedSpot{tile, mCurrentDay});
    if (result.find == forage::Find::Nothing || result.count <= 0)
    {
        mInteractionMessage = "Nothing worth taking.";
        mInteractionMessageTimer = 2.0f;
        return;
    }
    const int itemId = result.find == forage::Find::Berries ? mItems.berries
                     : result.find == forage::Find::Mushrooms ? mItems.mushrooms
                                                              : mItems.herbs;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const int added = InventorySystem::addItem(inv, mItemDatabase, itemId, result.count).added;
    if (added < result.count) dropOnFloor(playerTile(), itemId, result.count - added);
    awardXp(Skill::LivingOffTheLand, skillrules::kXpForage);
    std::string what = findName(result.find);
    mInteractionMessage = "You find " + std::string(result.count == 1 ? "a few " : "") + what + " (" + std::to_string(result.count) + ").";
    mInteractionMessageTimer = 2.5f;
    std::cout << "[Forage] " << what << " x" << result.count << " at " << tile.x << "," << tile.y << "\n";
}

bool Game::placeSnare(int slotIndex)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    if (slotIndex < 0 || slotIndex >= Inventory::kMaxSlots) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    if (slot.itemId != mItems.snare || slot.count <= 0) return false;
    const glm::ivec2 tile = playerTile();
    std::string why;
    if (mTileMap.isIndoors(tile.x, tile.y) || mTileMap.getSurfaceType(tile.x, tile.y) != SurfaceType::Grass)
    {
        why = "It wants grass, out in the open.";
    }
    else
    {
        const int r = forage::kSnareMinTilesFromBuildings;
        for (int dy = -r; dy <= r && why.empty(); ++dy)
        {
            for (int dx = -r; dx <= r && why.empty(); ++dx)
            {
                if (dx * dx + dy * dy > r * r || !mTileMap.inBounds(tile.x + dx, tile.y + dy)) continue;
                if (mTileMap.getBuildingId(tile.x + dx, tile.y + dy) > 0) why = "Too near the houses. Nothing comes this close.";
            }
        }
        // Something built already stands here (you stand on it, which is fine).
        bool taken = false;
        mWorld.forEach<Structure, Transform>(
            [&](Entity, Structure&, Transform& t)
            {
                if (worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f)) == tile) taken = true;
            });
        if (why.empty() && taken) why = "Something is already here.";
    }
    if (!why.empty())
    {
        mInteractionMessage = why;
        mInteractionMessageTimer = 2.5f;
        return true;
    }
    const Entity e = spawnStructure(StructureType::Snare, tile, nullptr);
    mWorld.getComponent<Structure>(e).lastDawn = mCurrentDay;
    InventorySystem::consumeOne(inv, mItemDatabase, slotIndex);
    mInteractionMessage = "You set the snare. Look at it after dawn.";
    mInteractionMessageTimer = 2.5f;
    std::cout << "[Forage] snare set at " << tile.x << "," << tile.y << "\n";
    return true;
}

bool Game::handleFieldStructure(const ProximityPrompt& entityPrompt)
{
    const Entity e = entityPrompt.entity;
    if (e == kInvalidEntity || !mWorld.hasComponent<Structure>(e) || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    Structure& s = mWorld.getComponent<Structure>(e);
    if (s.type != StructureType::GardenBed && s.type != StructureType::Snare) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const glm::ivec2 tile = mWorld.hasComponent<Transform>(e)
                                ? worldToTile(mTileMap, glm::vec2(mWorld.getComponent<Transform>(e).x + 16.0f, mWorld.getComponent<Transform>(e).y + 16.0f))
                                : playerTile();

    if (s.type == StructureType::Snare)
    {
        mProximityPrompt = s.snareState == 1 ? "Snare (something in it)   E: Take it and reset   X: Pick it up"
                         : s.snareState == 2 ? "Snare (sprung)   E: Reset it   X: Pick it up"
                                             : "Snare (set)   X: Pick it up";
        if (mInput.pressed(Action::Pry))
        {
            InventorySystem::addItem(inv, mItemDatabase, mItems.snare, 1);
            mWorld.destroyEntity(e);
            mInteractionMessage = "You take up the snare.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
        if (!mInput.pressed(Action::Use)) return true;
        if (s.snareState == 1)
        {
            const int added = InventorySystem::addItem(inv, mItemDatabase, mItems.rawMeat, 1).added;
            if (added == 0) dropOnFloor(playerTile(), mItems.rawMeat, 1);
            awardXp(Skill::LivingOffTheLand, skillrules::kXpSnareCatch);
            mInteractionMessage = "A rabbit. You take it and set the snare again.";
        }
        else
        {
            mInteractionMessage = s.snareState == 2 ? "You set it again." : "Nothing yet.";
        }
        s.snareState = 0;
        mInteractionMessageTimer = 2.5f;
        return true;
    }

    // A garden bed.
    garden::Bed bed = bedOf(s);
    static const char* kStageNames[garden::kStages] = {"seedlings", "growing", "nearly ready", "ripe"};
    std::string prompt = "Garden bed (";
    prompt += bed.dead ? "dead" : kStageNames[std::clamp(garden::stage(bed), 0, garden::kStages - 1)];
    if (!bed.dead && garden::thirsty(bed)) prompt += ", dry";
    prompt += ")";
    if (bed.dead) prompt += "   E: Clear it";
    else if (garden::ripe(bed)) prompt += "   E: Harvest";
    if (!bed.dead) prompt += "   R: Water it";
    mProximityPrompt = prompt;

    if (mInput.pressed(Action::Board) && !bed.dead)
    {
        if (InventorySystem::countItem(inv, mItems.waterBottle) == 0)
        {
            mInteractionMessage = "You need a bottle of water.";
        }
        else
        {
            for (int i = 0; i < Inventory::kMaxSlots; ++i)
            {
                if (inv.slots[static_cast<std::size_t>(i)].itemId == mItems.waterBottle && inv.slots[static_cast<std::size_t>(i)].count > 0)
                {
                    InventorySystem::consumeOne(inv, mItemDatabase, i);
                    break;
                }
            }
            garden::water(bed);
            storeBed(s, bed);
            mInteractionMessage = "You water the bed.";
        }
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    if (!mInput.pressed(Action::Use)) return true;
    if (bed.dead)
    {
        mWorld.destroyEntity(e);
        refreshFurnitureLayer();
        mInteractionMessage = "You pull up the dead plants.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    if (!garden::ripe(bed))
    {
        mInteractionMessage = "Not ready yet.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    const int yield = garden::harvest(bed);
    storeBed(s, bed);
    const int added = InventorySystem::addItem(inv, mItemDatabase, mItems.vegetables, yield).added;
    if (added < yield) dropOnFloor(playerTile(), mItems.vegetables, yield - added);
    awardXp(Skill::LivingOffTheLand, skillrules::kXpHarvest);
    noteAccomplishment();
    mInteractionMessage = "You pull " + std::to_string(yield) + " vegetables. It will grow again.";
    mInteractionMessageTimer = 3.0f;
    std::cout << "[Garden] harvested " << yield << " at " << tile.x << "," << tile.y << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// Each tick

void Game::updateScavenging(float dtSeconds)
{
    if (mPlayerEntity == kInvalidEntity) return;
    const bool raining = weatherrules::rainy(mWeatherState.condition);
    if (raining) mLastRainHour = absoluteHour();
    mSeenTimer -= dtSeconds;
    if (mSeenTimer <= 0.0f)
    {
        mSeenTimer = 1.0f;
        markSeenBuildings();
    }

    const float hours = static_cast<float>(dtSeconds * hoursPerSecond());
    const int living = skillLevel(Skill::LivingOffTheLand);
    const std::uint32_t seed = mSaveManager.currentRunSeed();

    // The dead where they stand: through a snare it breaks, through a bed
    // they may trample it.
    std::vector<std::pair<Entity, glm::ivec2>> dead;
    mWorld.forEach<ZombieAI, Transform, Health>(
        [&](Entity z, ZombieAI&, Transform& t, Health& h)
        {
            if (h.current > 0.0f) dead.emplace_back(z, worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f)));
        });

    std::vector<Entity> gone;
    std::vector<std::pair<Entity, glm::ivec2>> onBeds;
    mWorld.forEach<Structure, Transform>(
        [&](Entity e, Structure& s, Transform& t)
        {
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            const bool outdoors = !mTileMap.isIndoors(tile.x, tile.y);
            switch (s.type)
            {
            case StructureType::RainCollector:
                // Fresh rain keeps it clean; water left standing goes bad.
                if (raining && outdoors) s.standingHours = 0.0f;
                else if (s.water > 0.01f) s.standingHours += hours;
                else s.standingHours = 0.0f;
                break;
            case StructureType::GardenBed:
            {
                garden::Bed bed = bedOf(s);
                // Nothing grows in winter (phase 7).
                if (weatherrules::gardensGrow(mCurrentDay)) garden::tick(bed, hours, raining && outdoors, living);
                for (const auto& [z, at] : dead)
                {
                    if (at != tile) continue;
                    onBeds.emplace_back(z, tile);
                    const bool already = std::any_of(mOnBeds.begin(), mOnBeds.end(),
                                                     [&, zz = z](const auto& p) { return p.first == zz && p.second == tile; });
                    if (!already) garden::trample(bed, roll01(mix(static_cast<std::uint32_t>(z), tileHash(tile) ^ static_cast<std::uint32_t>(absoluteHour() * 60.0))));
                }
                storeBed(s, bed);
                break;
            }
            case StructureType::Snare:
                if (s.lastDawn < mCurrentDay && mGameHours >= 6.0)
                {
                    s.lastDawn = mCurrentDay;
                    if (s.snareState == 0)
                    {
                        const forage::SnareDawn dawn = forage::snareAtDawn(tile, mCurrentDay, living, seed);
                        s.snareState = dawn == forage::SnareDawn::Caught ? 1 : dawn == forage::SnareDawn::Sprung ? 2 : 0;
                        if (s.snareState != 0) std::cout << "[Forage] snare at " << tile.x << "," << tile.y << (s.snareState == 1 ? " caught something\n" : " sprung empty\n");
                    }
                }
                for (const auto& [z, at] : dead)
                {
                    if (at == tile)
                    {
                        gone.push_back(e);
                        break;
                    }
                }
                break;
            default:
                break;
            }
        });
    mOnBeds = std::move(onBeds);
    for (Entity e : gone)
    {
        if (mWorld.hasComponent<Structure>(e)) mWorld.destroyEntity(e);
    }

    // How beds and snares look.
    mWorld.forEach<Structure, Sprite>(
        [&](Entity, Structure& s, Sprite& sprite)
        {
            if (s.type == StructureType::GardenBed)
            {
                const garden::Bed bed = bedOf(s);
                static const char* kStages[garden::kStages] = {"prop_garden_0", "prop_garden_1", "prop_garden_2", "prop_garden_3"};
                sprite.uvRect = mSpriteSheet.uvRect(bed.dead ? "prop_garden_dead" : kStages[std::clamp(garden::stage(bed), 0, garden::kStages - 1)]);
            }
            else if (s.type == StructureType::Snare)
            {
                sprite.uvRect = mSpriteSheet.uvRect(s.snareState == 1 ? "prop_snare_caught" : s.snareState == 2 ? "prop_snare_sprung" : "prop_snare");
            }
        });
}

// ---------------------------------------------------------------------------
// What the town has left

void Game::countTownStock()
{
    const ScavengeMemoryRecord& memory = mSaveManager.scavengeMemory();
    mDistrictContainers.clear();
    mDistrictStocked.clear();
    mTownContainers = 0;
    mTownUnopened = 0;
    for (const FurniturePiece& piece : mFurniture)
    {
        const FurnitureInfo& info = furniture::info(piece.kind);
        if (info.prompt == nullptr || piece.kind == FurnitureKind::Bed || piece.kind == FurnitureKind::Cot) continue;
        const int district = mTileMap.getDistrictId(piece.tile.x, piece.tile.y);
        const bool unopened = !hasTile(memory.searched, piece.tile);
        ++mTownContainers;
        if (unopened) ++mTownUnopened;
        if (district < 0) continue;
        if (district >= static_cast<int>(mDistrictContainers.size()))
        {
            mDistrictContainers.resize(static_cast<std::size_t>(district) + 1, 0);
            mDistrictStocked.resize(static_cast<std::size_t>(district) + 1, 0);
        }
        ++mDistrictContainers[static_cast<std::size_t>(district)];
        if (unopened) ++mDistrictStocked[static_cast<std::size_t>(district)];
    }
}

float Game::districtStock(int district) const
{
    if (district < 0 || district >= static_cast<int>(mDistrictContainers.size())) return -1.0f;
    const int total = mDistrictContainers[static_cast<std::size_t>(district)];
    if (total <= 0) return -1.0f;
    return static_cast<float>(mDistrictStocked[static_cast<std::size_t>(district)]) / static_cast<float>(total);
}

void Game::markSeenBuildings()
{
    const LayoutData& layout = mMapGenerator.layoutData();
    const glm::vec2 me = glm::vec2(playerTile());
    ScavengeMemoryRecord memory = mSaveManager.scavengeMemory();
    bool changed = false;
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        const int building = static_cast<int>(i) + 1;
        const BuildingKind kind = buildingkinds::kindOf(layout.buildings[i]);
        if (kind == BuildingKind::House) continue;
        if (std::find(memory.seenBuildings.begin(), memory.seenBuildings.end(), building) != memory.seenBuildings.end()) continue;
        const glm::ivec2 door = buildingkinds::streetDoor(layout, building);
        if (door.x < 0) continue;
        if (glm::length(glm::vec2(door) - me) > kSeenTiles) continue;
        memory.seenBuildings.push_back(building);
        changed = true;
        std::cout << "[Scavenge] seen: " << buildingkinds::name(kind) << " (building " << building << ")\n";
    }
    if (changed)
    {
        mSaveManager.setScavengeMemory(std::move(memory));
        mSaveManager.saveWorldMemory();
    }
}

void Game::applyFurnitureAlts()
{
    const ScavengeMemoryRecord& memory = mSaveManager.scavengeMemory();
    std::vector<glm::ivec2> tiles;
    for (const auto& [x, y] : memory.searched) tiles.emplace_back(x, y);
    if (powerOn())
    {
        for (const glm::ivec2& lamp : mLitLamps) tiles.push_back(lamp);
    }
    mFurnitureView.setAlts(std::move(tiles));
}

void Game::renderScavengeMarks()
{
    // Scavenging 2: a small mark over containers near you not yet searched,
    // in this life or an earlier one. It never says what is inside.
    if (mRunState != RunState::Playing || mPlayerEntity == kInvalidEntity) return;
    if (!skillrules::seesWorthSearching(skillLevel(Skill::Scavenging))) return;
    const ScavengeMemoryRecord& memory = mSaveManager.scavengeMemory();
    const glm::ivec2 me = playerTile();
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const float zoom = uiZoom();
    const float tw = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float th = static_cast<float>(std::max(1, mTileMap.tileHeight()));
    const float reach = skillrules::kWorthSearchingTiles;
    const float pulse = 0.65f + 0.25f * std::sin(mHudClock * 3.0f);
    for (const FurniturePiece& piece : mFurniture)
    {
        const FurnitureInfo& info = furniture::info(piece.kind);
        if (info.prompt == nullptr || piece.kind == FurnitureKind::Bed || piece.kind == FurnitureKind::Cot) continue;
        const glm::vec2 d(static_cast<float>(piece.tile.x - me.x), static_cast<float>(piece.tile.y - me.y));
        if (glm::length(d) > reach) continue;
        if (hasTile(memory.searched, piece.tile) || searchedThisRun(piece.tile)) continue;
        const glm::vec2 world((static_cast<float>(piece.tile.x) + 0.5f) * tw, (static_cast<float>(piece.tile.y) + 0.5f) * th);
        const glm::vec2 at = glm::round(uiAt(world) - glm::vec2(0.0f, 44.0f * zoom));
        const float px = std::max(1.0f, std::round(zoom));
        // A small diamond: three rows, pale, with a dark rim.
        const glm::vec4 rim(0.08f, 0.07f, 0.06f, 0.75f * pulse);
        const glm::vec4 fill(0.96f, 0.90f, 0.62f, pulse);
        mSpriteBatch.draw(at + glm::vec2(-2.0f, -1.0f) * px, glm::vec2(5.0f, 3.0f) * px, white, rim);
        mSpriteBatch.draw(at + glm::vec2(-1.0f, -2.0f) * px, glm::vec2(3.0f, 5.0f) * px, white, rim);
        mSpriteBatch.draw(at + glm::vec2(-1.0f, -1.0f) * px, glm::vec2(3.0f, 3.0f) * px, white, fill);
    }
}
