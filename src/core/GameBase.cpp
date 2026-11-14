// The home base: furniture pushed about and broken up for planks, the build
// wheel and the stations it makes, traps and lights set down, how safe a
// room is to sleep in, and the house the next life inherits. The rules live
// in world/FurnitureUse and systems/BuildSystem; this is where they meet
// the game.
#include "core/GameShared.h"

#include <random>

#include "world/Storage.h"

using namespace gameshared;

namespace
{
glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

const char* pieceName(FurnitureKind kind)
{
    switch (kind)
    {
    case FurnitureKind::Bed: return "bed";
    case FurnitureKind::Couch: return "couch";
    case FurnitureKind::Table: return "table";
    case FurnitureKind::Bookshelf: return "bookcase";
    case FurnitureKind::Wardrobe: return "wardrobe";
    case FurnitureKind::Dresser: return "dresser";
    case FurnitureKind::Fridge: return "fridge";
    case FurnitureKind::Stove: return "stove";
    case FurnitureKind::Counter: return "counter";
    case FurnitureKind::Sink: return "sink";
    case FurnitureKind::Toilet: return "toilet";
    case FurnitureKind::TvStand: return "TV stand";
    case FurnitureKind::Plant: return "plant";
    case FurnitureKind::StoreShelf: return "shelf";
    case FurnitureKind::Checkout: return "checkout";
    case FurnitureKind::Crates: return "crates";
    case FurnitureKind::MetalRack: return "metal rack";
    case FurnitureKind::Workbench: return "workbench";
    case FurnitureKind::Barrels: return "barrels";
    case FurnitureKind::Cot: return "cot";
    case FurnitureKind::Desk: return "desk";
    case FurnitureKind::FilingCabinet: return "filing cabinet";
    case FurnitureKind::Locker: return "locker";
    case FurnitureKind::PalletStack: return "pallet stack";
    case FurnitureKind::PharmacyShelf: return "shelf";
    case FurnitureKind::ToolRack: return "tool rack";
    case FurnitureKind::DinerTable: return "table";
    case FurnitureKind::FloorLamp: return "lamp";
    case FurnitureKind::Count: break;
    }
    return "piece";
}

// The player's walking speed (InputSystem), world px/s.
constexpr float kWalkSpeed = 180.0f;
// Within this of the player's centre, a piece is in reach.
constexpr float kPieceReachTiles = 1.3f;
// Seconds a weapon takes to mend at a workbench, and how much it mends.
constexpr float kRepairSeconds = 8.0f;
constexpr float kRepairAmount = 0.4f;
// A campfire: hours of burning from two planks, noise every few seconds.
constexpr float kFireHoursPerFeed = 4.0f;
constexpr float kFireNoiseSeconds = 5.0f;
constexpr float kFireSpreadPerHour = 0.05f;
constexpr float kMatchKeepChance = 0.8f;
constexpr float kCollectorMaxWater = 6.0f;
constexpr float kCollectorStaleHours = 72.0f; // phase 6: then the water is dirty
// Spike boards and alarms.
constexpr int kSpikeUses = 3;
constexpr float kSpikeDamage = 35.0f;
constexpr float kSpikeSlowSeconds = 4.0f;
constexpr float kSpikePlayerDamage = 10.0f;
constexpr float kAlarmNoiseSeconds = 3.0f;
constexpr float kCandleFireChance = 0.2f;
// Depression eased by finishing something.
constexpr float kAccomplishment = 6.0f;
// The way a caller picks: within this of where it is.
constexpr float kCallerChoiceTiles = 12.0f;

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
} // namespace

// ---------------------------------------------------------------------------
// Furniture

void Game::refreshFurnitureLayer()
{
    mTileMap.clearFurniture();
    for (const FurniturePiece& piece : mFurniture)
    {
        mTileMap.setFurniture(piece.tile.x, piece.tile.y, true);
    }
    mWorld.forEach<Structure, Transform>(
        [&](Entity, Structure& s, Transform& t)
        {
            if (!structureInfo(s.type).solid || s.hp <= 0.0f) return;
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            mTileMap.setFurniture(tile.x, tile.y, true);
        });
    for (int y = 0; y < mTileMap.height(); ++y)
    {
        for (int x = 0; x < mTileMap.width(); ++x)
        {
            if (mTileMap.doorwayBlocked(x, y)) mTileMap.setDoorwayBlocked(x, y, false);
        }
    }
    for (const FurniturePiece& piece : mFurniture)
    {
        if (furnitureuse::blockedDoor(mTileMap, piece.tile).x >= 0)
        {
            mTileMap.setDoorwayBlocked(piece.tile.x, piece.tile.y, true);
        }
    }
    mFurnitureView.build(mFurniture);
    applyFurnitureAlts();
}

void Game::rememberFurniture()
{
    const furnitureuse::Memory memory = furnitureuse::memoryOf(mFurniture, mRemovedFurniture);
    FurnitureMemoryRecord record;
    for (const furnitureuse::Move& move : memory.moves)
    {
        record.moves.push_back(FurnitureMoveRecord{move.from.x, move.from.y, move.to.x, move.to.y});
    }
    for (const glm::ivec2& origin : memory.removed)
    {
        record.removed.emplace_back(origin.x, origin.y);
    }
    mSaveManager.setFurnitureMemory(std::move(record));
    mSaveManager.saveFurnitureMemory();
}

FurniturePiece* Game::furnitureAt(const glm::ivec2& tile)
{
    const auto piece = std::find_if(mFurniture.begin(), mFurniture.end(),
                                    [&](const FurniturePiece& p) { return p.tile == tile; });
    return piece != mFurniture.end() ? &*piece : nullptr;
}

std::vector<glm::ivec2> Game::occupiedTiles() const
{
    std::vector<glm::ivec2> tiles;
    auto& world = const_cast<World&>(mWorld);
    world.forEach<Health, Transform>(
        [&](Entity, Health& h, Transform& t)
        {
            if (h.current > 0.0f) tiles.push_back(worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f)));
        });
    world.forEach<Corpse, Transform>(
        [&](Entity, Corpse&, Transform& t) { tiles.push_back(worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f))); });
    world.forEach<Structure, Transform>(
        [&](Entity, Structure&, Transform& t) { tiles.push_back(worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f))); });
    return tiles;
}

std::string Game::pushFurniture(const glm::ivec2& tile, const glm::ivec2& dir, bool chargeStamina)
{
    FurniturePiece* piece = furnitureAt(tile);
    if (piece == nullptr) return "There's nothing there to push.";
    const furnitureuse::UseInfo& info = furnitureuse::useInfo(piece->kind);
    if (info.weight == furnitureuse::Weight::Fixed) return "It won't budge.";
    const bool heavy = info.weight == furnitureuse::Weight::Heavy;
    const float cost = heavy ? furnitureuse::kHeavyPushStamina : furnitureuse::kLightPushStamina;
    if (chargeStamina && mPlayerEntity != kInvalidEntity)
    {
        if (mWorld.hasComponent<Inventory>(mPlayerEntity) &&
            InventorySystem::totalWeight(mWorld.getComponent<Inventory>(mPlayerEntity), mItemDatabase) > Inventory::kMaxCarryWeight)
        {
            return "You're carrying too much to push anything.";
        }
        if (mWorld.hasComponent<Stamina>(mPlayerEntity) && mWorld.getComponent<Stamina>(mPlayerEntity).current < cost)
        {
            return "You're too winded to push it.";
        }
    }
    const glm::ivec2 target = furnitureuse::pushTarget(mTileMap, tile, dir, occupiedTiles());
    if (target.x < 0)
    {
        return "It won't go any further that way.";
    }

    // What stands for the piece in the world (its cupboard, its bed) goes with it.
    const float tileW = static_cast<float>(mTileMap.tileWidth());
    const float tileH = static_cast<float>(mTileMap.tileHeight());
    mWorld.forEach<Interactable, Transform>(
        [&](Entity e, Interactable&, Transform& t)
        {
            if (mWorld.hasComponent<Sprite>(e)) return;
            if (worldToTile(mTileMap, glm::vec2(t.x, t.y)) != tile) return;
            t.x = static_cast<float>(target.x) * tileW;
            t.y = static_cast<float>(target.y) * tileH;
            t.prevX = t.x;
            t.prevY = t.y;
        });
    if (const ContainerRecord* record = mSaveManager.containerRecord(tile.x, tile.y))
    {
        ContainerRecord moved = *record;
        mSaveManager.removeContainerRecord(tile.x, tile.y);
        moved.tileX = target.x;
        moved.tileY = target.y;
        mSaveManager.setContainerRecord(moved);
        mSaveManager.saveWorldMemory();
    }
    for (auto* list : {&mFirstDays.homeContainers, &mFirstDays.record.searched, &mFirstDays.record.taken})
    {
        for (auto& pair : *list)
        {
            if (pair == std::make_pair(tile.x, tile.y)) pair = std::make_pair(target.x, target.y);
        }
    }

    piece->tile = target;
    piece->damage = 0.0f;
    if (chargeStamina && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Stamina>(mPlayerEntity))
    {
        Stamina& stamina = mWorld.getComponent<Stamina>(mPlayerEntity);
        stamina.current = std::max(0.0f, stamina.current - cost);
    }
    mNoiseModel.emitNoise(tileCentre(mTileMap, target), heavy ? NoiseTier::Medium : NoiseTier::Soft, 0.8f, mPlayerEntity);
    mAudioManager.playSound(SoundId::FootstepLoud, heavy ? 0.45f : 0.3f);
    refreshFurnitureLayer();
    rememberFurniture();
    if (mTileMap.doorwayBlocked(target.x, target.y))
    {
        mInteractionMessage = std::string("The ") + pieceName(piece->kind) + " stands across the doorway.";
        mInteractionMessageTimer = 2.5f;
        noteAccomplishment();
    }
    return {};
}

furnitureuse::Tool Game::breakingTool() const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return furnitureuse::Tool::Hands;
    }
    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    if (InventorySystem::countTagged(inv, mItemDatabase, "tool:axe") > 0 ||
        InventorySystem::countTagged(inv, mItemDatabase, "tool:pry") > 0)
    {
        return furnitureuse::Tool::AxeOrCrowbar;
    }
    if (InventorySystem::countTagged(inv, mItemDatabase, "tool:hammer") > 0)
    {
        return furnitureuse::Tool::Hammer;
    }
    return furnitureuse::Tool::Hands;
}

void Game::dropOnFloor(const glm::ivec2& tile, int itemId, int count, float condition, int loaded)
{
    if (itemId < 0 || count <= 0) return;
    // Onto a pile already there, or a new one.
    Entity pile = kInvalidEntity;
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable& inter, Inventory&, Transform& t)
        {
            if (pile != kInvalidEntity || inter.type != InteractableType::Container) return;
            if (!mWorld.hasComponent<Sprite>(e) || mWorld.hasComponent<Structure>(e) || mWorld.hasComponent<Vehicle>(e)) return;
            if (worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f)) == tile) pile = e;
        });
    if (pile == kInvalidEntity)
    {
        pile = mWorld.createEntity();
        Transform t{};
        t.x = static_cast<float>(tile.x * mTileMap.tileWidth());
        t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
        t.prevX = t.x;
        t.prevY = t.y;
        Sprite s{};
        s.textureId = mSpriteSheet.textureId();
        const ItemDef* item = mItemDatabase.find(itemId);
        s.uvRect = mSpriteSheet.uvRect(item != nullptr ? item->sprite : "item_wood_planks");
        s.layer = 6;
        s.drawSize = SpriteSizes::kPickup;
        Interactable inter{};
        inter.type = InteractableType::Container;
        inter.prompt = "Search the pile";
        Inventory inv{};
        inv.maxSlots = 12;
        inv.maxWeight = 60.0f;
        mWorld.addComponent<Transform>(pile, t);
        mWorld.addComponent<Sprite>(pile, s);
        mWorld.addComponent<Interactable>(pile, inter);
        mWorld.addComponent<Inventory>(pile, inv);
    }
    InventorySystem::addItemWithCondition(mWorld.getComponent<Inventory>(pile), mItemDatabase, itemId, count, condition, loaded);
}

void Game::breakFurniture(const glm::ivec2& tile)
{
    FurniturePiece* piece = furnitureAt(tile);
    if (piece == nullptr) return;
    const FurnitureKind kind = piece->kind;
    const bool hasSaw = playerHasItem(mItems.saw);
    furnitureuse::Yield yield = furnitureuse::breakYield(kind, hasSaw);
    if (!furnitureuse::movable(kind) && !(furnitureuse::useInfo(kind).scrapWithCrowbar && breakingTool() == furnitureuse::Tool::AxeOrCrowbar))
    {
        return;
    }

    // Its cupboard and bed go with it.
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
    mSaveManager.saveWorldMemory();
    std::erase(mFirstDays.homeContainers, std::make_pair(tile.x, tile.y));

    mRemovedFurniture.push_back(piece->origin);
    mFurniture.erase(mFurniture.begin() + (piece - mFurniture.data()));
    refreshFurnitureLayer();
    rememberFurniture();

    // Into the pack; what does not fit falls to the floor.
    const auto give = [&](int itemId, int count)
    {
        if (count <= 0) return;
        int added = 0;
        if (mWorld.hasComponent<Inventory>(mPlayerEntity))
        {
            added = InventorySystem::addItem(mWorld.getComponent<Inventory>(mPlayerEntity), mItemDatabase, itemId, count).added;
        }
        if (added < count) dropOnFloor(tile, itemId, count - added);
    };
    // Someone with nothing to fight with keeps a leg of it as a club, in
    // place of one plank. (Each piece is broken once, so once per piece.)
    bool leg = false;
    if (kind == FurnitureKind::Table && yield.planks > 0 && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        bool armed = false;
        for (const InventorySlot& slot : mWorld.getComponent<Inventory>(mPlayerEntity).slots)
        {
            const ItemDef* def = slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
            armed = armed || (def != nullptr && def->category == ItemCategory::Weapon);
        }
        if (!armed)
        {
            leg = true;
            --yield.planks;
            give(mItems.chairLeg, 1);
        }
    }
    // A practised hand gets more out of it (Carpentry 4: a plank more; 2: nails).
    const int carpentry = skillLevel(Skill::Carpentry);
    if (yield.planks > 0) yield.planks += skillrules::extraPlanksFromFurniture(carpentry);
    give(mItems.woodPlanks, yield.planks);
    give(mItems.rags, yield.rags);
    give(mItems.scrapMetal, yield.scrap);
    if (yield.planks > 0 && skillrules::salvagesNails(carpentry)) give(mItems.nails, 1);
    awardXp(Skill::Carpentry, skillrules::kXpBreakFurniture);

    std::string got;
    const auto part = [&](int n, const char* one, const char* many)
    {
        if (n <= 0) return;
        if (!got.empty()) got += " and ";
        got += std::to_string(n) + " " + (n == 1 ? one : many);
    };
    part(yield.planks, "plank", "planks");
    if (leg) part(1, "leg to swing", "legs");
    part(yield.rags, "rag", "rags");
    part(yield.scrap, "piece of scrap", "pieces of scrap");
    mInteractionMessage = std::string("The ") + pieceName(kind) + " comes apart" + (got.empty() ? "." : ": " + got + ".");
    mInteractionMessageTimer = 3.0f;
    mAudioManager.playSound(SoundId::WeaponHit, 0.6f);
}

void Game::furnitureBlow(const glm::ivec2& tile, float damage)
{
    FurniturePiece* piece = furnitureAt(tile);
    if (piece == nullptr) return;
    const float hp = std::max(50.0f, furnitureuse::useInfo(piece->kind).barrierHp);
    const float before = piece->damage;
    piece->damage += damage;
    const glm::vec2 centre = tileCentre(mTileMap, tile);
    const float tiles = glm::length(centre - playerWorldCenter()) / static_cast<float>(std::max(1, mTileMap.tileWidth()));
    // A thump about once a second.
    if (std::floor(before / Doors::kZombieDamagePerSecond) != std::floor(piece->damage / Doors::kZombieDamagePerSecond))
    {
        mNoiseModel.emitNoise(centre, NoiseTier::Medium, 1.0f);
        if (tiles < 18.0f) mAudioManager.playSound(SoundId::WeaponHit, 0.2f + 0.3f * (1.0f - tiles / 18.0f));
        wakeSleeper(centre, 12.0f, 2, "Something is shoving at the doorway!");
    }
    if (piece->damage < hp) return;

    // Broken through: the piece is gone, and what was in it spills out.
    const FurnitureKind kind = piece->kind;
    std::vector<Entity> gone;
    std::vector<std::pair<int, int>> spilled;
    mWorld.forEach<Interactable, Transform>(
        [&](Entity e, Interactable&, Transform& t)
        {
            if (mWorld.hasComponent<Sprite>(e) || worldToTile(mTileMap, glm::vec2(t.x, t.y)) != tile) return;
            if (mWorld.hasComponent<Inventory>(e))
            {
                for (const InventorySlot& slot : mWorld.getComponent<Inventory>(e).slots)
                {
                    if (slot.itemId >= 0 && slot.count > 0) spilled.emplace_back(slot.itemId, slot.count);
                }
            }
            gone.push_back(e);
        });
    for (Entity e : gone)
    {
        if (e == mOpenContainerEntity) closeContainer();
        mWorld.destroyEntity(e);
    }
    mSaveManager.removeContainerRecord(tile.x, tile.y);
    mSaveManager.saveWorldMemory();
    std::erase(mFirstDays.homeContainers, std::make_pair(tile.x, tile.y));
    mRemovedFurniture.push_back(piece->origin);
    mFurniture.erase(mFurniture.begin() + (piece - mFurniture.data()));
    refreshFurnitureLayer();
    rememberFurniture();
    for (const auto& [itemId, count] : spilled) dropOnFloor(tile, itemId, count);
    dropOnFloor(tile, mItems.woodPlanks, 1);

    mNoiseModel.emitNoise(centre, NoiseTier::Loud, 1.5f);
    mParticleSystem.spawnDust(centre, 12);
    wakeSleeper(centre, 12.0f, 3, "Something crashes through the doorway!");
    if (tiles <= 14.0f)
    {
        mInteractionMessage = std::string("The ") + pieceName(kind) + " in the doorway gives way.";
        mInteractionMessageTimer = 3.0f;
    }
    std::cout << "[Base] the " << pieceName(kind) << " at " << tile.x << "," << tile.y << " was broken through\n";
}

// ---------------------------------------------------------------------------
// Stations and things set down

Entity Game::spawnStructure(StructureType type, const glm::ivec2& tile, const StructureRecord* record)
{
    const StructureInfo& info = structureInfo(type);
    const Entity e = mWorld.createEntity();
    Transform t{};
    t.x = static_cast<float>(tile.x * mTileMap.tileWidth());
    t.y = static_cast<float>(tile.y * mTileMap.tileHeight());
    t.prevX = t.x;
    t.prevY = t.y;

    Structure s{};
    s.type = type;
    s.maxHp = info.hp;
    s.hp = info.hp * (record != nullptr ? std::clamp(record->hpRatio, 0.05f, 1.0f) : 1.0f);
    s.decayRatePerHour = info.decayPerHour;
    s.condition = s.hp > s.maxHp * 0.66f ? StructureCondition::Intact
                : s.hp > s.maxHp * 0.33f ? StructureCondition::Damaged
                                         : StructureCondition::Breaking;
    if (type == StructureType::SpikeBoard) s.uses = kSpikeUses;
    if (type == StructureType::Campfire) s.burnHours = kFireHoursPerFeed;
    if (record != nullptr)
    {
        s.water = record->water;
        s.burnHours = record->burnHours;
        s.lit = record->lit;
        s.lantern = record->lantern;
        if (type == StructureType::SpikeBoard) s.uses = std::clamp(record->uses, 0, kSpikeUses);
        s.target = glm::ivec2(record->targetX, record->targetY);
        s.standingHours = record->standingHours;
        s.growHours = record->growHours;
        s.sinceWaterHours = record->sinceWaterHours;
        s.regrowing = record->regrowing;
        s.dead = record->dead;
        s.snareState = record->snareState;
        s.lastDawn = record->lastDawn;
    }

    Sprite sprite{};
    sprite.textureId = mSpriteSheet.textureId();
    std::string spriteName = info.sprite;
    if (type == StructureType::Light && s.lantern) spriteName = "prop_lantern";
    sprite.uvRect = mSpriteSheet.uvRect(spriteName);
    sprite.layer = type == StructureType::GardenBed || type == StructureType::Snare ? 4 : 5;
    sprite.drawSize = 80.0f;
    sprite.sink = 16.0f;

    mWorld.addComponent<Transform>(e, t);
    mWorld.addComponent<Sprite>(e, sprite);

    Interactable inter{};
    inter.prompt = info.name;
    switch (type)
    {
    case StructureType::Bedroll:
        inter.type = InteractableType::SleepSpot;
        inter.sleepHours = 7.0f;
        inter.sleepQuality = 0.75f;
        inter.prompt = "Sleep on the bedroll";
        mWorld.addComponent<Interactable>(e, inter);
        break;
    case StructureType::StorageCrate:
    {
        inter.type = InteractableType::Container;
        inter.prompt = "Search the storage crate";
        mWorld.addComponent<Interactable>(e, inter);
        Inventory inv{};
        inv.maxSlots = 16;
        inv.maxWeight = 60.0f;
        if (record != nullptr)
        {
            for (const auto& [itemId, count] : record->items) InventorySystem::addItem(inv, mItemDatabase, itemId, count);
        }
        mWorld.addComponent<Inventory>(e, inv);
        break;
    }
    case StructureType::CanAlarm:
        break; // worked from its door or window
    default:
        inter.type = InteractableType::Station;
        mWorld.addComponent<Interactable>(e, inter);
        break;
    }
    mWorld.addComponent<Structure>(e, s);
    return e;
}

std::vector<StructureRecord> Game::structureRecords(bool runEnd) const
{
    std::vector<StructureRecord> records;
    auto& world = const_cast<World&>(mWorld);
    world.forEach<Structure, Transform>(
        [&](Entity e, Structure& s, Transform& t)
        {
            if (s.hp <= 0.0f) return;
            StructureRecord rec;
            rec.type = static_cast<int>(s.type);
            rec.x = t.x;
            rec.y = t.y;
            rec.hpRatio = s.maxHp > 0.0f ? s.hp / s.maxHp : 0.0f;
            if (runEnd)
            {
                if (!BuildSystem::carriesOver(s.type, false)) return;
                // One stage worse between lives.
                if (BuildSystem::wearsBetweenLives(s.type))
                {
                    rec.hpRatio -= 1.0f / 3.0f;
                    if (rec.hpRatio <= 0.02f) return;
                }
            }
            rec.condition = rec.hpRatio > 0.66f ? 0 : rec.hpRatio > 0.33f ? 1 : 2;
            rec.water = s.water;
            rec.burnHours = s.burnHours;
            rec.lit = runEnd ? false : s.lit;
            rec.lantern = s.lantern;
            rec.uses = s.uses;
            rec.targetX = s.target.x;
            rec.targetY = s.target.y;
            rec.standingHours = s.standingHours;
            rec.growHours = s.growHours;
            rec.sinceWaterHours = s.sinceWaterHours;
            rec.regrowing = s.regrowing;
            rec.dead = s.dead;
            rec.snareState = s.snareState;
            rec.lastDawn = s.lastDawn;
            // A garden bed comes back as seedlings; a dead one not at all.
            if (runEnd && s.type == StructureType::GardenBed)
            {
                if (s.dead) return;
                rec.growHours = 0.0f;
                rec.sinceWaterHours = 0.0f;
                rec.regrowing = false;
            }
            if (runEnd && s.type == StructureType::Snare) rec.snareState = 0;
            if (s.type == StructureType::StorageCrate && world.hasComponent<Inventory>(e))
            {
                rec.items = InventorySystem::itemsUnloaded(world.getComponent<Inventory>(e), mItemDatabase);
            }
            records.push_back(rec);
        });
    return records;
}

void Game::restoreStructures(const std::vector<StructureRecord>& records)
{
    std::vector<Entity> old;
    mWorld.forEach<Structure>([&](Entity e, Structure&) { old.push_back(e); });
    for (Entity e : old) mWorld.destroyEntity(e);
    for (const StructureRecord& record : records)
    {
        if (record.type < static_cast<int>(StructureType::Campfire) || record.type > static_cast<int>(StructureType::Snare))
        {
            continue;
        }
        const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(record.x + 16.0f, record.y + 16.0f));
        spawnStructure(static_cast<StructureType>(record.type), tile, &record);
    }
    refreshFurnitureLayer();
}

bool Game::canAffordStation(const StationRecipe& recipe, std::string* missing) const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const auto need = [&](const char* key, int count) -> bool
    {
        if (key == nullptr || count <= 0) return true;
        const ItemDef* def = mItemDatabase.findByKey(key);
        if (def != nullptr && InventorySystem::countItem(inv, def->id) >= count) return true;
        if (missing != nullptr && missing->empty()) *missing = "You need " + std::to_string(count) + " " + (def != nullptr ? def->name : key) + ".";
        return false;
    };
    bool ok = need(recipe.materialKey, recipe.materialCount);
    ok = need(recipe.secondKey, recipe.secondCount) && ok;
    if (recipe.needsHammer && InventorySystem::countTagged(inv, mItemDatabase, "tool:hammer") == 0)
    {
        if (missing != nullptr && missing->empty()) *missing = "You need a hammer.";
        ok = false;
    }
    if (recipe.needsSaw && InventorySystem::countTagged(inv, mItemDatabase, "tool:saw") == 0)
    {
        if (missing != nullptr && missing->empty()) *missing = "You need a saw.";
        ok = false;
    }
    return ok;
}

void Game::startBuild(const StationRecipe& recipe, const glm::ivec2& tile)
{
    // Giving up: nothing gets started (phase 5).
    if (mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        !mindrules::canBuild(mindrules::depressionBand(mWorld.getComponent<MentalState>(mPlayerEntity).depression)))
    {
        mInteractionMessage = "What would be the point.";
        mInteractionMessageTimer = 2.5f;
        return;
    }
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const auto take = [&](const char* key, int count)
    {
        if (key == nullptr || count <= 0) return;
        if (const ItemDef* def = mItemDatabase.findByKey(key)) InventorySystem::removeItem(inv, def->id, count);
    };
    take(recipe.materialKey, recipe.materialCount);
    take(recipe.secondKey, recipe.secondCount);
    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::Build;
    mBaseAction.building = recipe.type;
    mBaseAction.tile = tile;
    mBaseAction.total = recipe.seconds * skillrules::buildTimeScale(skillLevel(Skill::Carpentry));
    mBaseAction.label = std::string("Building a ") + recipe.name;
    mBaseAction.startPos = playerWorldCenter();
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
}

void Game::finishBuild()
{
    const StructureType type = mBaseAction.building;
    const glm::ivec2 tile = mBaseAction.tile;
    mBaseAction = BaseAction{};
    const Entity e = spawnStructure(type, tile, nullptr);
    (void)e;
    refreshFurnitureLayer();
    ++mRunStats.structuresBuilt;
    mInteractionMessage = std::string(structureInfo(type).name) + " built.";
    mInteractionMessageTimer = 3.0f;
    mAudioManager.playSound(SoundId::WeaponHit, 0.5f);
    noteAccomplishment();
    std::cout << "[Base] built " << structureInfo(type).name << " at " << tile.x << "," << tile.y << "\n";
}

void Game::cancelBuild(const char* why)
{
    if (mBaseAction.kind != BaseAction::Kind::Build) return;
    const StationRecipe* recipe = BuildSystem::recipeFor(mBaseAction.building);
    if (recipe != nullptr && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const auto refund = [&](const char* key, int count)
        {
            if (key == nullptr || count <= 0) return;
            if (const ItemDef* def = mItemDatabase.findByKey(key)) InventorySystem::addItem(inv, mItemDatabase, def->id, count);
        };
        refund(recipe->materialKey, recipe->materialCount);
        refund(recipe->secondKey, recipe->secondCount);
    }
    mBaseAction = BaseAction{};
    mInteractionMessage = why;
    mInteractionMessageTimer = 2.5f;
}

bool Game::alarmOn(const glm::ivec2& tile) const
{
    bool on = false;
    auto& world = const_cast<World&>(mWorld);
    world.forEach<Structure>(
        [&](Entity, Structure& s)
        {
            if (s.type == StructureType::CanAlarm && s.target == tile) on = true;
        });
    return on;
}

void Game::rattleAlarm(const glm::ivec2& tile)
{
    Entity hung = kInvalidEntity;
    mWorld.forEach<Structure>(
        [&](Entity e, Structure& s)
        {
            if (s.type == StructureType::CanAlarm && s.target == tile) hung = e;
        });
    if (hung == kInvalidEntity) return;
    mWorld.destroyEntity(hung);
    const glm::vec2 centre = tileCentre(mTileMap, tile);
    mNoiseModel.emitNoise(centre, NoiseTier::Loud, kAlarmNoiseSeconds);
    mAudioManager.playSound(SoundId::WeaponHit, 0.8f);
    // The cans fall inside.
    glm::ivec2 floor = tile;
    for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
    {
        const glm::ivec2 next = tile + side;
        if (!mTileMap.isSolid(next.x, next.y) && mTileMap.getBuildingId(next.x, next.y) > 0)
        {
            floor = next;
            break;
        }
    }
    dropOnFloor(floor, mItems.tinCan, 2);
    // Anyone asleep in the house hears it.
    const int building = mTileMap.getBuildingId(floor.x, floor.y);
    const glm::ivec2 player = playerTile();
    if (building > 0 && mTileMap.getBuildingId(player.x, player.y) == building)
    {
        wakeSleeper(playerWorldCenter(), 1.0f, 3, "Cans rattle at the way in!");
    }
    else
    {
        wakeSleeper(centre, 12.0f, 3, "Cans rattle somewhere close!");
    }
    if (glm::length(centre - playerWorldCenter()) < 16.0f * static_cast<float>(mTileMap.tileWidth()))
    {
        mInteractionMessage = "The cans rattle!";
        mInteractionMessageTimer = 3.0f;
    }
    std::cout << "[Base] an alarm rattled at " << tile.x << "," << tile.y << "\n";
}

bool Game::placeItem(int slotIndex)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    if (slotIndex < 0 || slotIndex >= Inventory::kMaxSlots) return false;
    const InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item == nullptr || !item->hasTag("placeable")) return false;
    const int itemId = slot.itemId;
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();

    if (itemId == mItems.canAlarm)
    {
        glm::ivec2 target(-1);
        if (const Doors::Door* door = mDoors.nearest(mTileMap, here, tileSize * 1.05f))
        {
            target = door->tile;
        }
        else if (const Windows::Window* window = mWindows.nearest(mTileMap, here, tileSize * 1.05f))
        {
            target = window->spot.tile;
        }
        if (target.x < 0)
        {
            mInteractionMessage = "Hang it at a door or a window.";
        }
        else if (alarmOn(target))
        {
            mInteractionMessage = "Cans already hang there.";
        }
        else
        {
            StructureRecord record;
            record.targetX = target.x;
            record.targetY = target.y;
            spawnStructure(StructureType::CanAlarm, target, &record);
            InventorySystem::removeItem(inv, itemId, 1);
            mInteractionMessage = "You string the cans across it. Anyone coming through will be heard.";
        }
        mInteractionMessageTimer = 2.5f;
        return true;
    }

    // Spike boards, candles and lanterns go on the floor: under the cursor
    // if it is close, else beside you.
    const bool spikes = itemId == mItems.spikeBoard;
    std::vector<glm::ivec2> taken = occupiedTiles();
    const glm::ivec2 me = playerTile();
    std::erase(taken, me);
    const auto fits = [&](const glm::ivec2& tile)
    {
        if (!mTileMap.inBounds(tile.x, tile.y) || mTileMap.isSolid(tile.x, tile.y)) return false;
        if (mTileMap.doorState(tile.x, tile.y) != DoorState::None) return false;
        if (spikes && !mTileMap.isIndoors(tile.x, tile.y)) return false;
        bool structure = false;
        mWorld.forEach<Structure, Transform>(
            [&](Entity, Structure&, Transform& t)
            {
                if (worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f)) == tile) structure = true;
            });
        return !structure && std::find(taken.begin(), taken.end(), tile) == taken.end();
    };
    glm::ivec2 spot(-1);
    const glm::ivec2 cursor = worldToTile(mTileMap, mMouseWorld);
    if (std::abs(cursor.x - me.x) <= 2 && std::abs(cursor.y - me.y) <= 2 && cursor != me && fits(cursor))
    {
        spot = cursor;
    }
    for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(0, 1), glm::ivec2(-1, 0), glm::ivec2(0, -1),
                                   glm::ivec2(1, 1), glm::ivec2(-1, 1), glm::ivec2(1, -1), glm::ivec2(-1, -1)})
    {
        if (spot.x >= 0) break;
        if (fits(me + side)) spot = me + side;
    }
    if (spot.x < 0)
    {
        mInteractionMessage = spikes ? "Set it down on a floor indoors, clear of everything." : "There's no room to set it down.";
        mInteractionMessageTimer = 2.5f;
        return true;
    }
    StructureRecord record;
    if (spikes)
    {
        record.uses = kSpikeUses;
        spawnStructure(StructureType::SpikeBoard, spot, &record);
        mInteractionMessage = "You set the spike board down. Crouch to step over it.";
    }
    else
    {
        record.lantern = itemId == mItems.canLantern;
        record.burnHours = record.lantern ? 8.0f : 6.0f;
        spawnStructure(StructureType::Light, spot, &record);
        mInteractionMessage = std::string("You set the ") + (record.lantern ? "lantern" : "candle") + " down. Matches will light it.";
    }
    InventorySystem::removeItem(inv, itemId, 1);
    mInteractionMessageTimer = 2.5f;
    return true;
}

// ---------------------------------------------------------------------------
// E, R and X at furniture and at what was built

bool Game::handleBaseInteraction(const ProximityPrompt& entityPrompt)
{
    if (mPlayerEntity == kInvalidEntity || mBaseAction.kind != BaseAction::Kind::None) return false;
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    Inventory* inv = mWorld.hasComponent<Inventory>(mPlayerEntity) ? &mWorld.getComponent<Inventory>(mPlayerEntity) : nullptr;

    // Something built.
    if (entityPrompt.entity != kInvalidEntity && mWorld.hasComponent<Structure>(entityPrompt.entity) && inv != nullptr)
    {
        Structure& s = mWorld.getComponent<Structure>(entityPrompt.entity);
        const StationRecipe* recipe = BuildSystem::recipeFor(s.type);
        const bool worn = recipe != nullptr && s.hp < s.maxHp * 0.95f;
        std::string prompt = structureInfo(s.type).name;
        if (s.type == StructureType::Light) prompt = s.lantern ? "Can lantern" : "Candle";
        const auto hours = [](float h) { return std::to_string(static_cast<int>(std::ceil(std::max(0.0f, h)))) + "h"; };
        switch (s.type)
        {
        case StructureType::Campfire:
            prompt += s.lit ? " (burning, " + hours(s.burnHours) + ")   E: Cook   X: Add 2 planks" : "   E: Light it";
            break;
        case StructureType::Workbench:
            prompt += "   E: Mend the weapon in hand";
            break;
        case StructureType::RainCollector:
            prompt += " (" + std::to_string(static_cast<int>(s.water)) + "/" + std::to_string(static_cast<int>(kCollectorMaxWater)) +
                      (s.standingHours > kCollectorStaleHours ? ", stale" : "") + ")   R: Fill bottles";
            break;
        case StructureType::SpikeBoard:
            prompt += "   E: Pick it up";
            break;
        case StructureType::Light:
            prompt += s.lit ? " (" + hours(s.burnHours) + ")   E: Put it out" : "   E: Light it   X: Pick it up";
            break;
        default:
            prompt = entityPrompt.text;
            break;
        }
        if (worn) prompt += "   R: Repair";
        mProximityPrompt = prompt;

        if (mInput.wasKeyPressed(SDL_SCANCODE_R))
        {
            if (s.type == StructureType::RainCollector)
            {
                const int empties = InventorySystem::countItem(*inv, mItems.emptyBottle);
                const int fill = std::min(empties, static_cast<int>(s.water));
                if (fill <= 0)
                {
                    mInteractionMessage = empties == 0 ? "No empty bottles to fill." : "There's hardly any water in it.";
                }
                else
                {
                    // Water that has stood three days is not clean (phase 6).
                    const bool stale = s.standingHours > kCollectorStaleHours;
                    InventorySystem::removeItem(*inv, mItems.emptyBottle, fill);
                    const int added = InventorySystem::addItem(*inv, mItemDatabase, stale ? mItems.dirtyWater : mItems.waterBottle, fill).added;
                    if (added < fill) InventorySystem::addItem(*inv, mItemDatabase, mItems.emptyBottle, fill - added);
                    s.water -= static_cast<float>(added);
                    mInteractionMessage = "You fill " + std::to_string(added) + (added == 1 ? " bottle" : " bottles") +
                                          (stale ? ". It has stood too long." : " with rainwater.");
                }
                mInteractionMessageTimer = 2.5f;
                return true;
            }
            if (worn)
            {
                const ItemDef* def = mItemDatabase.findByKey(recipe->materialKey);
                if (def == nullptr || InventorySystem::countItem(*inv, def->id) == 0)
                {
                    mInteractionMessage = std::string("You need ") + (def != nullptr ? def->name : "materials") + " to mend it.";
                }
                else
                {
                    InventorySystem::removeItem(*inv, def->id, 1);
                    s.hp = s.maxHp;
                    s.condition = StructureCondition::Intact;
                    mNoiseModel.emitNoise(here, recipe->hammering ? NoiseTier::Loud : NoiseTier::Soft, 0.8f, mPlayerEntity);
                    mInteractionMessage = std::string("You patch up the ") + structureInfo(s.type).name + ".";
                }
                mInteractionMessageTimer = 2.5f;
                return true;
            }
        }
        // A burning fire: X feeds it, E cooks on it (phase 6).
        if (s.type == StructureType::Campfire && s.lit && mInput.wasKeyPressed(SDL_SCANCODE_X))
        {
            if (InventorySystem::countItem(*inv, mItems.woodPlanks) < 2)
            {
                mInteractionMessage = "You need two planks to feed it.";
                mInteractionMessageTimer = 2.5f;
            }
            else
            {
                InventorySystem::removeItem(*inv, mItems.woodPlanks, 2);
                s.burnHours += kFireHoursPerFeed;
                mInteractionMessage = "You feed the fire.";
                mInteractionMessageTimer = 2.0f;
            }
            return true;
        }
        if (s.type == StructureType::Campfire && s.lit && mInput.wasKeyPressed(SDL_SCANCODE_E))
        {
            mShowCrafting = true;
            mShowInventory = false;
            mCraftingCategory = kCraftingFoodTab;
            mCraftingRecipeCursor = 0;
            return true;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_X) && s.type == StructureType::Light && !s.lit)
        {
            if (s.burnHours > 0.5f) InventorySystem::addItem(*inv, mItemDatabase, s.lantern ? mItems.canLantern : mItems.candle, 1);
            mWorld.destroyEntity(entityPrompt.entity);
            mInteractionMessage = "You pick it up.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
        if (!mInput.wasKeyPressed(SDL_SCANCODE_E))
        {
            return s.type != StructureType::StorageCrate && s.type != StructureType::Bedroll;
        }
        std::mt19937 rng(mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(mGameHours * 3600.0));
        const auto strikeMatch = [&]() -> bool
        {
            if (InventorySystem::countItem(*inv, mItems.matches) == 0)
            {
                mInteractionMessage = "You need matches to light it.";
                mInteractionMessageTimer = 2.5f;
                return false;
            }
            if (std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) >= kMatchKeepChance)
            {
                InventorySystem::removeItem(*inv, mItems.matches, 1);
            }
            return true;
        };
        switch (s.type)
        {
        case StructureType::Campfire:
            if (!s.lit)
            {
                if (s.burnHours <= 0.05f)
                {
                    mInteractionMessage = "Nothing left to burn. Add planks first.";
                    mInteractionMessageTimer = 2.5f;
                    if (InventorySystem::countItem(*inv, mItems.woodPlanks) >= 2)
                    {
                        InventorySystem::removeItem(*inv, mItems.woodPlanks, 2);
                        s.burnHours += kFireHoursPerFeed;
                        mInteractionMessage = "You stack two planks in the fire pit.";
                    }
                    return true;
                }
                if (strikeMatch())
                {
                    s.lit = true;
                    s.noiseTimer = 0.0f;
                    mInteractionMessage = "The fire catches.";
                    mInteractionMessageTimer = 2.5f;
                }
            }
            else if (InventorySystem::countItem(*inv, mItems.woodPlanks) < 2)
            {
                mInteractionMessage = "You need two planks to feed it.";
                mInteractionMessageTimer = 2.5f;
            }
            else
            {
                InventorySystem::removeItem(*inv, mItems.woodPlanks, 2);
                s.burnHours += kFireHoursPerFeed;
                mInteractionMessage = "You feed the fire.";
                mInteractionMessageTimer = 2.0f;
            }
            return true;
        case StructureType::Workbench:
        {
            const int slot = mEquippedWeaponSlot;
            const InventorySlot* weapon = slot >= 0 ? &inv->slots[static_cast<std::size_t>(slot)] : nullptr;
            if (weapon == nullptr || weapon->itemId < 0 || weapon->count <= 0)
            {
                mInteractionMessage = "Take a weapon in hand to mend it.";
            }
            else if (weapon->condition >= 0.99f)
            {
                mInteractionMessage = "It doesn't need mending.";
            }
            else
            {
                const ItemDef* tape = mItemDatabase.findByKey("duct_tape");
                if (InventorySystem::countItem(*inv, mItems.scrapMetal) == 0 && InventorySystem::countItem(*inv, mItems.nails) < 2 &&
                    (tape == nullptr || InventorySystem::countItem(*inv, tape->id) == 0))
                {
                    mInteractionMessage = "You need scrap metal, two nails or tape to mend it.";
                }
                else
                {
                    mBaseAction = BaseAction{};
                    mBaseAction.kind = BaseAction::Kind::Repair;
                    mBaseAction.total = kRepairSeconds;
                    mBaseAction.label = "Mending";
                    mBaseAction.startPos = here;
                    mBaseAction.startHp = mWorld.getComponent<Health>(mPlayerEntity).current;
                    return true;
                }
            }
            mInteractionMessageTimer = 2.5f;
            return true;
        }
        case StructureType::SpikeBoard:
            if (s.uses >= kSpikeUses) InventorySystem::addItem(*inv, mItemDatabase, mItems.spikeBoard, 1);
            mWorld.destroyEntity(entityPrompt.entity);
            mInteractionMessage = "You lift the spike board.";
            mInteractionMessageTimer = 2.0f;
            return true;
        case StructureType::Light:
            if (s.lit)
            {
                s.lit = false;
                mInteractionMessage = "You put it out.";
                mInteractionMessageTimer = 2.0f;
            }
            else if (s.burnHours <= 0.05f)
            {
                mInteractionMessage = "It's burnt down.";
                mInteractionMessageTimer = 2.0f;
            }
            else if (strikeMatch())
            {
                s.lit = true;
                mInteractionMessage = s.lantern ? "The lantern glows behind its tin." : "The candle flickers into life.";
                mInteractionMessageTimer = 2.0f;
            }
            return true;
        default:
            return false; // the crate opens, the bedroll is slept on
        }
    }

    // Furniture in reach.
    FurniturePiece* piece = nullptr;
    float best = kPieceReachTiles * tileSize;
    for (FurniturePiece& candidate : mFurniture)
    {
        const float d = glm::length(tileCentre(mTileMap, candidate.tile) - here);
        if (d < best)
        {
            best = d;
            piece = &candidate;
        }
    }
    if (piece == nullptr) return false;
    // Something else to use is right here and nearer.
    if (entityPrompt.entity != kInvalidEntity && mWorld.hasComponent<Transform>(entityPrompt.entity))
    {
        const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(mWorld.getComponent<Transform>(entityPrompt.entity).x,
                                                                 mWorld.getComponent<Transform>(entityPrompt.entity).y));
        if (tile != piece->tile && mWorld.hasComponent<Sprite>(entityPrompt.entity)) return false;
    }
    const furnitureuse::UseInfo& info = furnitureuse::useInfo(piece->kind);
    const bool movable = info.weight != furnitureuse::Weight::Fixed;
    const bool blocking = mTileMap.doorwayBlocked(piece->tile.x, piece->tile.y);
    const glm::ivec2 me = playerTile();
    const glm::ivec2 across = piece->tile + (piece->tile - me);
    const bool canClimb = blocking && info.weight == furnitureuse::Weight::Light &&
                          (std::abs(piece->tile.x - me.x) + std::abs(piece->tile.y - me.y)) == 1 &&
                          mTileMap.inBounds(across.x, across.y) && !mTileMap.isSolid(across.x, across.y);

    std::string prompt = entityPrompt.entity != kInvalidEntity && entityPrompt.text.size() > 0 ? entityPrompt.text + "   " : "";
    if (prompt.empty())
    {
        std::string name = pieceName(piece->kind);
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        prompt = name + "   ";
    }
    if (canClimb) prompt += "E: Climb over   ";
    if (movable) prompt += "Hold E: Push   ";
    if (movable || info.scrapWithCrowbar) prompt += "X: Break up";
    mProximityPrompt = prompt;

    if (canClimb && mInput.wasKeyPressed(SDL_SCANCODE_E))
    {
        mBaseAction = BaseAction{};
        mBaseAction.kind = BaseAction::Kind::Climb;
        mBaseAction.tile = piece->tile;
        mBaseAction.to = across;
        mBaseAction.total = furnitureuse::kClimbOverSeconds;
        mBaseAction.label = "Climbing over";
        mNoiseModel.emitNoise(here, NoiseTier::Soft, 0.6f, mPlayerEntity);
        return true;
    }
    if (!mInput.wasKeyPressed(SDL_SCANCODE_X))
    {
        return false;
    }
    const furnitureuse::Tool tool = breakingTool();
    const bool crowbar = inv != nullptr && InventorySystem::countTagged(*inv, mItemDatabase, "tool:pry") > 0;
    const float seconds = furnitureuse::breakSeconds(piece->kind, tool, crowbar);
    if (seconds < 0.0f)
    {
        mInteractionMessage = info.scrapWithCrowbar ? "You'd need a crowbar to take that apart." : "It won't come apart.";
        mInteractionMessageTimer = 2.5f;
        return true;
    }
    // Nothing gets broken up with things still in it.
    bool full = false;
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable&, Inventory& contents, Transform& t)
        {
            if (mWorld.hasComponent<Sprite>(e) || worldToTile(mTileMap, glm::vec2(t.x, t.y)) != piece->tile) return;
            for (const InventorySlot& slot : contents.slots)
            {
                if (slot.itemId >= 0 && slot.count > 0) full = true;
            }
        });
    if (full)
    {
        mInteractionMessage = "Empty it first.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::Break;
    mBaseAction.tile = piece->tile;
    mBaseAction.total = seconds;
    mBaseAction.label = std::string("Breaking up the ") + pieceName(piece->kind);
    mBaseAction.startPos = here;
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
    return true;
}

// ---------------------------------------------------------------------------
// Held actions, the build wheel, wear and fire, traps

void Game::updateBaseActions(float dtSeconds)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity)) return;
    const bool alive = mWorld.hasComponent<Health>(mPlayerEntity) && mWorld.getComponent<Health>(mPlayerEntity).current > 0.0f;
    const bool asleep = mWorld.hasComponent<Sleeping>(mPlayerEntity);
    const bool driving = mActiveVehicleEntity != kInvalidEntity;
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    if (!alive || asleep || driving)
    {
        if (mBaseAction.kind == BaseAction::Kind::Build) cancelBuild("Building interrupted.");
        mBaseAction = BaseAction{};
        mBuildWheelOpen = false;
        return;
    }
    const glm::vec2 here = playerWorldCenter();
    const bool walking = mInput.isKeyDown(SDL_SCANCODE_W) || mInput.isKeyDown(SDL_SCANCODE_A) ||
                         mInput.isKeyDown(SDL_SCANCODE_S) || mInput.isKeyDown(SDL_SCANCODE_D);
    const float hp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
    Velocity* velocity = mWorld.hasComponent<Velocity>(mPlayerEntity) ? &mWorld.getComponent<Velocity>(mPlayerEntity) : nullptr;
    const auto holdStill = [&]()
    {
        if (velocity != nullptr)
        {
            velocity->dx = 0.0f;
            velocity->dy = 0.0f;
        }
    };

    // The build wheel: B opens it at the cursor; a click picks a station.
    const bool panels = mShowCrafting || mContainerOpen || mShowInventory || mShowWorldMap || mShowNotebook;
    if (mInput.wasKeyPressed(SDL_SCANCODE_B) && !panels)
    {
        if (mBuildWheelOpen || mBuildChoice >= 0)
        {
            mBuildWheelOpen = false;
            mBuildChoice = -1;
        }
        else
        {
            mBuildWheelOpen = true;
            mBuildWheelCentre = glm::vec2(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY()));
        }
    }
    if (mBuildWheelOpen)
    {
        if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
        {
            // The entry nearest the cursor, if it is near one.
            const glm::vec2 mouse = glm::vec2(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY()));
            int pick = -1;
            float bestDist = 80.0f;
            for (int i = 0; i < BuildSystem::kRecipeCount; ++i)
            {
                const glm::vec2 at = buildWheelEntry(i);
                const float d = glm::length(mouse - at);
                if (d < bestDist)
                {
                    bestDist = d;
                    pick = i;
                }
            }
            mBuildWheelOpen = false;
            mBuildChoice = pick;
        }
        else if (mInput.wasMouseButtonPressed(SDL_BUTTON_RIGHT))
        {
            mBuildWheelOpen = false;
        }
    }

    // A station chosen: the ghost follows the cursor; a click starts building.
    mBuildGhostValid = false;
    if (mBuildChoice >= 0 && !mBuildWheelOpen && mBaseAction.kind == BaseAction::Kind::None)
    {
        const StationRecipe& recipe = BuildSystem::kRecipes[static_cast<std::size_t>(mBuildChoice)];
        mBuildGhostTile = worldToTile(mTileMap, mMouseWorld);
        const glm::ivec2 me = playerTile();
        const float reach = glm::length(glm::vec2(mBuildGhostTile - me));
        mBuildGhostValid = reach <= BuildSystem::kReachTiles + 0.5f && mBuildGhostTile != me &&
                           BuildSystem::placeable(mTileMap, mBuildGhostTile, recipe.type, occupiedTiles());
        if (mInput.wasMouseButtonPressed(SDL_BUTTON_RIGHT))
        {
            mBuildChoice = -1;
        }
        else if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
        {
            std::string missing;
            if (!mBuildGhostValid)
            {
                mInteractionMessage = reach > BuildSystem::kReachTiles + 0.5f ? "Too far away." : "It can't stand there.";
                mInteractionMessageTimer = 2.0f;
            }
            else if (!canAffordStation(recipe, &missing))
            {
                mInteractionMessage = missing;
                mInteractionMessageTimer = 2.5f;
            }
            else
            {
                startBuild(recipe, mBuildGhostTile);
                mBuildChoice = -1;
            }
        }
    }

    // Grabbing a piece: E held a moment beside something movable.
    if (mInput.isKeyDown(SDL_SCANCODE_E) && (mBaseAction.kind == BaseAction::Kind::None || mBaseAction.kind == BaseAction::Kind::Grab))
    {
        mBaseAction.holdTimer += dtSeconds;
        if (mBaseAction.kind == BaseAction::Kind::None && mBaseAction.holdTimer >= furnitureuse::kHoldToGrabSeconds)
        {
            const FurniturePiece* piece = nullptr;
            float best = kPieceReachTiles * tileSize;
            for (const FurniturePiece& candidate : mFurniture)
            {
                const float d = glm::length(tileCentre(mTileMap, candidate.tile) - here);
                if (d < best && furnitureuse::movable(candidate.kind))
                {
                    best = d;
                    piece = &candidate;
                }
            }
            if (piece != nullptr)
            {
                // The tap opened its cupboard; holding on means pushing.
                if (mContainerOpen) closeContainer();
                const float hold = mBaseAction.holdTimer;
                mBaseAction = BaseAction{};
                mBaseAction.kind = BaseAction::Kind::Grab;
                mBaseAction.tile = piece->tile;
                mBaseAction.holdTimer = hold;
                mBaseAction.label = std::string("Holding the ") + pieceName(piece->kind);
            }
        }
    }
    else if (mBaseAction.kind == BaseAction::Kind::Grab || mBaseAction.kind == BaseAction::Kind::None)
    {
        mBaseAction = BaseAction{};
    }

    switch (mBaseAction.kind)
    {
    case BaseAction::Kind::None:
        return;
    case BaseAction::Kind::Grab:
    {
        FurniturePiece* piece = furnitureAt(mBaseAction.tile);
        if (piece == nullptr || glm::length(tileCentre(mTileMap, mBaseAction.tile) - here) > 1.8f * tileSize)
        {
            mBaseAction = BaseAction{};
            return;
        }
        mProximityPrompt = mBaseAction.label + "   walk into it to push   let go of E to stop";
        if (velocity == nullptr || (velocity->dx == 0.0f && velocity->dy == 0.0f)) return;
        // Walking into it: it slides away from you, along the grid.
        const glm::vec2 toPiece = tileCentre(mTileMap, piece->tile) - here;
        const glm::vec2 moving(velocity->dx, velocity->dy);
        if (glm::dot(glm::normalize(moving), glm::normalize(toPiece)) < 0.35f) return;
        const glm::ivec2 dir = std::abs(toPiece.x) >= std::abs(toPiece.y) ? glm::ivec2(toPiece.x > 0.0f ? 1 : -1, 0)
                                                                          : glm::ivec2(0, toPiece.y > 0.0f ? 1 : -1);
        if (glm::length(toPiece) > 1.25f * tileSize) return; // not up against it yet
        holdStill();
        const bool heavy = furnitureuse::useInfo(piece->kind).weight == furnitureuse::Weight::Heavy;
        const float speed = kWalkSpeed * (heavy ? furnitureuse::kHeavyPushSpeed : furnitureuse::kLightPushSpeed);
        mBaseAction.pushProgress += dtSeconds * speed;
        if (mBaseAction.pushProgress < tileSize) return;
        mBaseAction.pushProgress = 0.0f;
        const std::string why = pushFurniture(piece->tile, dir, true);
        if (!why.empty())
        {
            mInteractionMessage = why;
            mInteractionMessageTimer = 2.0f;
            return;
        }
        mBaseAction.tile += dir;
        return;
    }
    default:
        break;
    }

    // Held work: stand still and see it through.
    if (mBaseAction.kind != BaseAction::Kind::Climb && (walking || hp < mBaseAction.startHp - 0.5f))
    {
        if (mBaseAction.kind == BaseAction::Kind::Build)
        {
            cancelBuild(hp < mBaseAction.startHp - 0.5f ? "Hit! The build is abandoned." : "You stop building.");
        }
        else
        {
            mInteractionMessage = "You stop.";
            mInteractionMessageTimer = 1.5f;
            mBaseAction = BaseAction{};
        }
        return;
    }
    holdStill();
    // Shaking hands make held work slower (phase 5).
    mBaseAction.timer += mBaseAction.kind == BaseAction::Kind::Climb ? dtSeconds : dtSeconds / heldActionScale();
    mBaseAction.noiseTimer += dtSeconds;
    if (mBaseAction.noiseTimer >= 1.0f)
    {
        mBaseAction.noiseTimer -= 1.0f;
        NoiseTier tier = NoiseTier::Soft;
        if (mBaseAction.kind == BaseAction::Kind::Break || mBaseAction.kind == BaseAction::Kind::Board) tier = NoiseTier::Loud;
        // Phase 6: glass breaking, a door or the armoury forced (a crowbar
        // on a door: every 2 s).
        if (mBaseAction.kind == BaseAction::Kind::Smash ||
            (mBaseAction.kind == BaseAction::Kind::Search && mBaseAction.window))
        {
            tier = NoiseTier::Loud;
        }
        if (mBaseAction.kind == BaseAction::Kind::Force)
        {
            tier = static_cast<int>(mBaseAction.timer) % 2 == 1 ? NoiseTier::Loud : NoiseTier::Soft;
        }
        if (mBaseAction.kind == BaseAction::Kind::Build)
        {
            const StationRecipe* recipe = BuildSystem::recipeFor(mBaseAction.building);
            if (recipe != nullptr && recipe->hammering) tier = NoiseTier::Loud;
        }
        const bool quiet = mBaseAction.kind == BaseAction::Kind::Climb || mBaseAction.kind == BaseAction::Kind::Treat ||
                           mBaseAction.kind == BaseAction::Kind::Read;
        if (!quiet)
        {
            mNoiseModel.emitNoise(here, tier, 1.0f, mPlayerEntity);
            if (tier == NoiseTier::Loud) mAudioManager.playSound(SoundId::WeaponHit, 0.35f);
        }
    }
    if (mBaseAction.timer < mBaseAction.total) return;

    switch (mBaseAction.kind)
    {
    case BaseAction::Kind::Break:
    {
        const glm::ivec2 tile = mBaseAction.tile;
        mBaseAction = BaseAction{};
        breakFurniture(tile);
        break;
    }
    case BaseAction::Kind::Climb:
    {
        Transform& t = mWorld.getComponent<Transform>(mPlayerEntity);
        const glm::vec2 landing = tileCentre(mTileMap, mBaseAction.to);
        t.x = landing.x - kSpriteSize * 0.5f;
        t.y = landing.y - kSpriteSize * 0.5f;
        t.prevX = t.x;
        t.prevY = t.y;
        mBaseAction = BaseAction{};
        break;
    }
    case BaseAction::Kind::Build:
        finishBuild();
        awardXp(Skill::Carpentry, skillrules::kXpBuild);
        break;
    case BaseAction::Kind::Board:
        finishBoarding();
        break;
    case BaseAction::Kind::Treat:
        finishTreatment();
        break;
    case BaseAction::Kind::Read:
        finishReading();
        break;
    case BaseAction::Kind::Search:
    case BaseAction::Kind::Force:
    case BaseAction::Kind::Smash:
    case BaseAction::Kind::Tape:
    case BaseAction::Kind::StopAlarm:
    case BaseAction::Kind::Forage:
        finishScavengeAction();
        break;
    case BaseAction::Kind::Repair:
    {
        mBaseAction = BaseAction{};
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const ItemDef* tape = mItemDatabase.findByKey("duct_tape");
        if (InventorySystem::countItem(inv, mItems.scrapMetal) > 0) InventorySystem::removeItem(inv, mItems.scrapMetal, 1);
        else if (InventorySystem::countItem(inv, mItems.nails) >= 2) InventorySystem::removeItem(inv, mItems.nails, 2);
        else if (tape != nullptr && InventorySystem::countItem(inv, tape->id) > 0) InventorySystem::removeItem(inv, tape->id, 1);
        else break;
        const int slot = mEquippedWeaponSlot;
        if (slot >= 0 && slot < Inventory::kMaxSlots)
        {
            InventorySlot& weapon = inv.slots[static_cast<std::size_t>(slot)];
            weapon.condition = std::min(1.0f, weapon.condition + kRepairAmount);
            equipWeaponFromSlot(slot);
        }
        awardXp(Skill::Carpentry, skillrules::kXpRepair);
        mInteractionMessage = "You straighten and bind it. Good for a while yet.";
        mInteractionMessageTimer = 2.5f;
        break;
    }
    default:
        mBaseAction = BaseAction{};
        break;
    }
}

void Game::updateStructures(float dtSeconds)
{
    const float hours = static_cast<float>(dtSeconds * kGameHoursPerRealSecond);
    const WeatherCondition weather = mWeatherState.condition;
    const bool raining = weather == WeatherCondition::Rain || weather == WeatherCondition::HeavyRain ||
                         weather == WeatherCondition::Thunderstorm;
    const float rainRate = weather == WeatherCondition::Rain ? 1.0f : raining ? 2.0f : 0.0f;
    std::vector<Entity> gone;
    std::vector<glm::ivec2> ignite;
    bool solidGone = false;
    mWorld.forEach<Structure, Transform>(
        [&](Entity e, Structure& s, Transform& t)
        {
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            const bool outdoors = !mTileMap.isIndoors(tile.x, tile.y);
            s.hp -= BuildSystem::decayPerHour(s.type, outdoors, raining) * hours;
            s.condition = s.hp > s.maxHp * 0.66f ? StructureCondition::Intact
                        : s.hp > s.maxHp * 0.33f ? StructureCondition::Damaged
                                                 : StructureCondition::Breaking;
            if (s.hp <= 0.0f)
            {
                gone.push_back(e);
                solidGone = solidGone || structureInfo(s.type).solid;
                return;
            }
            if (s.type == StructureType::RainCollector && rainRate > 0.0f && outdoors)
            {
                s.water = std::min(kCollectorMaxWater, s.water + rainRate * hours);
            }
            if ((s.type == StructureType::Campfire || s.type == StructureType::Light) && s.lit)
            {
                s.burnHours -= hours;
                if (s.burnHours <= 0.0f)
                {
                    s.burnHours = 0.0f;
                    s.lit = false;
                }
            }
            if (s.type != StructureType::Campfire || !s.lit) return;
            if (outdoors && raining)
            {
                s.lit = false;
                if (glm::length(tileCentre(mTileMap, tile) - playerWorldCenter()) < 10.0f * 32.0f)
                {
                    mInteractionMessage = "The rain puts the fire out.";
                    mInteractionMessageTimer = 3.0f;
                }
                return;
            }
            s.noiseTimer -= dtSeconds;
            if (s.noiseTimer <= 0.0f)
            {
                s.noiseTimer = kFireNoiseSeconds;
                mNoiseModel.emitNoise(tileCentre(mTileMap, tile), NoiseTier::Medium, 1.0f);
            }
            // Indoors on wood or carpet, it may catch the floor.
            if (!outdoors)
            {
                const int base = ((std::max(1, mTileMap.groundGid(tile.x, tile.y)) - 1) % TileGid::kPerRow) + 1;
                const bool flammable = mTileMap.getSurfaceType(tile.x, tile.y) == SurfaceType::Carpet || base == TileGid::kWood;
                const std::uint32_t seed = mix(static_cast<std::uint32_t>(tile.x * 92821 + tile.y),
                                               static_cast<std::uint32_t>(mGameHours * 600.0) + static_cast<std::uint32_t>(mCurrentDay) * 14400u);
                if (flammable && roll01(seed) < kFireSpreadPerHour * hours) ignite.push_back(tile);
            }
        });
    for (Entity e : gone) mWorld.destroyEntity(e);
    if (solidGone) refreshFurnitureLayer();
    for (const glm::ivec2& tile : ignite)
    {
        FireTileRecord record;
        record.tileX = tile.x;
        record.tileY = tile.y;
        record.intensity = 1.0f;
        record.lifeRemaining = 24.0f;
        spawnFireTile(record, 0.0f);
        std::cout << "[Base] the campfire caught the floor at " << tile.x << "," << tile.y << "\n";
    }

    // Lit things look lit.
    mWorld.forEach<Structure, Sprite>(
        [&](Entity, Structure& s, Sprite& sprite)
        {
            if (s.type == StructureType::Campfire)
            {
                sprite.uvRect = mSpriteSheet.uvRect(s.lit ? "prop_campfire_lit" : "prop_campfire");
            }
            else if (s.type == StructureType::Light)
            {
                sprite.uvRect = mSpriteSheet.uvRect(s.lantern ? (s.lit ? "prop_lantern_lit" : "prop_lantern")
                                                              : (s.lit ? "prop_candle_lit" : "prop_candle"));
            }
            sprite.color = s.condition == StructureCondition::Breaking ? glm::vec4(0.78f, 0.74f, 0.70f, 1.0f)
                         : s.condition == StructureCondition::Damaged ? glm::vec4(0.9f, 0.88f, 0.84f, 1.0f)
                                                                      : glm::vec4(1.0f);
        });
}

void Game::updateTraps(float dtSeconds)
{
    // The dead limp off a spike board at half speed.
    mWorld.forEach<ZombieAI, Velocity>(
        [&](Entity, ZombieAI& ai, Velocity& v)
        {
            if (ai.slowTimer <= 0.0f) return;
            ai.slowTimer = std::max(0.0f, ai.slowTimer - dtSeconds);
            v.dx *= 0.5f;
            v.dy *= 0.5f;
        });

    struct Trap
    {
        Entity entity;
        glm::ivec2 tile;
        StructureType type;
    };
    std::vector<Trap> traps;
    mWorld.forEach<Structure, Transform>(
        [&](Entity e, Structure& s, Transform& t)
        {
            if (s.type == StructureType::SpikeBoard || (s.type == StructureType::Light && s.lit))
            {
                traps.push_back(Trap{e, worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f)), s.type});
            }
        });
    if (traps.empty()) return;

    std::vector<Entity> gone;
    for (const Trap& trap : traps)
    {
        Structure& s = mWorld.getComponent<Structure>(trap.entity);
        mWorld.forEach<ZombieAI, Transform, Health>(
            [&](Entity z, ZombieAI& ai, Transform& t, Health& h)
            {
                if (h.current <= 0.0f || std::find(gone.begin(), gone.end(), trap.entity) != gone.end()) return;
                if (worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f)) != trap.tile) return;
                if (trap.type == StructureType::SpikeBoard)
                {
                    if (ai.slowTimer > 0.0f || s.uses <= 0) return;
                    h.current = std::max(0.0f, h.current - kSpikeDamage);
                    ai.slowTimer = kSpikeSlowSeconds;
                    ai.lastDamagedBy = mPlayerEntity;
                    --s.uses;
                    mNoiseModel.emitNoise(tileCentre(mTileMap, trap.tile), NoiseTier::Soft, 0.8f);
                    mParticleSystem.spawnBloodSpray(tileCentre(mTileMap, trap.tile), glm::vec2(0.0f, -1.0f), 6);
                    if (s.uses <= 0) gone.push_back(trap.entity);
                    std::cout << "[Base] a spike board caught one (" << s.uses << " left)\n";
                    (void)z;
                }
                else
                {
                    // A candle knocked over.
                    gone.push_back(trap.entity);
                    const int base = ((std::max(1, mTileMap.groundGid(trap.tile.x, trap.tile.y)) - 1) % TileGid::kPerRow) + 1;
                    const bool flammable = mTileMap.getSurfaceType(trap.tile.x, trap.tile.y) == SurfaceType::Carpet ||
                                           base == TileGid::kWood;
                    const std::uint32_t seed = mix(static_cast<std::uint32_t>(trap.tile.x * 31 + trap.tile.y * 17),
                                                   static_cast<std::uint32_t>(mGameHours * 60.0));
                    if (flammable && roll01(seed) < kCandleFireChance)
                    {
                        FireTileRecord record;
                        record.tileX = trap.tile.x;
                        record.tileY = trap.tile.y;
                        record.intensity = 1.0f;
                        record.lifeRemaining = 24.0f;
                        spawnFireTile(record, 0.0f);
                    }
                }
            });
    }

    // You, stepping on your own spikes without care.
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const glm::ivec2 me = playerTile();
        const bool crouched = mInput.isKeyDown(SDL_SCANCODE_C) || mInput.isKeyDown(SDL_SCANCODE_LCTRL);
        static glm::ivec2 lastHurtTile(-1);
        bool onSpikes = false;
        for (const Trap& trap : traps)
        {
            if (trap.type != StructureType::SpikeBoard || trap.tile != me) continue;
            onSpikes = true;
            if (crouched || lastHurtTile == me) continue;
            lastHurtTile = me;
            Health& h = mWorld.getComponent<Health>(mPlayerEntity);
            h.current = std::max(0.0f, h.current - kSpikePlayerDamage);
            if (mWorld.hasComponent<Wounds>(mPlayerEntity))
            {
                mWorld.getComponent<Wounds>(mPlayerEntity).active.push_back(
                    woundrules::make(WoundType::Cut, woundrules::legPart(bodyRoll()), WoundDepth::Shallow));
            }
            mInteractionMessage = "You tread on your own spikes.";
            mInteractionMessageTimer = 2.5f;
        }
        if (!onSpikes) lastHurtTile = glm::ivec2(-1);
    }
    for (Entity e : gone)
    {
        if (mWorld.hasComponent<Structure>(e)) mWorld.destroyEntity(e);
    }
}

// ---------------------------------------------------------------------------
// Ways in, sleep and safety, what the next life inherits

std::vector<Game::WayIn> Game::waysIn(int building) const
{
    std::vector<WayIn> ways;
    if (building <= 0) return ways;
    const glm::ivec2 sides[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const Doors::Door& door : mDoors.all())
    {
        bool ours = false;
        bool toOutside = false;
        glm::ivec2 floor(-1);
        for (const glm::ivec2& side : sides)
        {
            const glm::ivec2 next = door.tile + side;
            if (mTileMap.inBounds(next.x, next.y) && !mTileMap.isIndoors(next.x, next.y) &&
                !mTileMap.blocksSight(next.x, next.y))
            {
                toOutside = true; // a way in, not a door between two rooms
            }
            if (mTileMap.inBounds(next.x, next.y) && mTileMap.getBuildingId(next.x, next.y) == building &&
                mTileMap.isIndoors(next.x, next.y) && mTileMap.doorState(next.x, next.y) == DoorState::None &&
                !mTileMap.blocksSight(next.x, next.y))
            {
                ours = true;
                if (mTileMap.doorwayBlocked(next.x, next.y)) floor = next;
            }
        }
        if (!ours || !toOutside) continue;
        WayIn way;
        way.tile = door.tile;
        const DoorState state = mTileMap.doorState(door.tile.x, door.tile.y);
        const bool blocked = floor.x >= 0;
        float blockHp = 0.0f;
        if (blocked)
        {
            for (const FurniturePiece& piece : mFurniture)
            {
                if (piece.tile == floor) blockHp = std::max(0.0f, furnitureuse::useInfo(piece.kind).barrierHp - piece.damage);
            }
        }
        way.boarded = door.boardHp > 0.0f || blocked;
        way.closed = state == DoorState::Closed || blocked;
        way.open = !way.closed;
        way.strength = (state == DoorState::Closed ? door.hp : 0.0f) + door.boardHp + blockHp;
        ways.push_back(way);
    }
    for (const Windows::Window& window : mWindows.all())
    {
        if (window.spot.building != building) continue;
        WayIn way;
        way.tile = window.spot.tile;
        way.window = true;
        const bool broken = mTileMap.windowState(way.tile.x, way.tile.y) == WindowState::Broken;
        way.boarded = window.boardHp > 0.0f;
        way.covered = mTileMap.windowCovered(way.tile.x, way.tile.y);
        way.open = broken && !way.boarded;
        way.closed = !way.open;
        way.strength = window.boardHp + (broken ? 0.0f : Windows::kGlassHp);
        ways.push_back(way);
    }
    return ways;
}

int Game::sleepSafety(int building, std::string* openWay) const
{
    if (building <= 0) return 0;
    const std::vector<WayIn> ways = waysIn(building);
    if (ways.empty()) return 2;
    glm::vec2 centre(0.0f);
    for (const WayIn& way : ways) centre += glm::vec2(way.tile);
    centre /= static_cast<float>(ways.size());
    bool allBoarded = true;
    for (const WayIn& way : ways)
    {
        if (way.open)
        {
            if (openWay != nullptr)
            {
                const glm::vec2 from = (centre + glm::vec2(0.5f)) * 32.0f;
                const glm::vec2 to = (glm::vec2(way.tile) + glm::vec2(0.5f)) * 32.0f;
                const std::string dir = legacy::screenDirection(from, to);
                static const std::pair<const char*, const char*> kNames[] = {
                    {"N", "north"}, {"NE", "north-east"}, {"E", "east"}, {"SE", "south-east"},
                    {"S", "south"}, {"SW", "south-west"}, {"W", "west"}, {"NW", "north-west"}};
                std::string name = dir;
                for (const auto& [shortName, longName] : kNames)
                {
                    if (dir == shortName) name = longName;
                }
                *openWay = "the " + name + (way.window ? " window is open" : " door is open");
            }
            return 1;
        }
        allBoarded = allBoarded && way.boarded;
    }
    return allBoarded ? 3 : 2;
}

void Game::wakeSleeper(const glm::vec2& where, float rangeTiles, int wakeAtOrBelow, const std::string& message)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Sleeping>(mPlayerEntity)) return;
    const Sleeping& sleeping = mWorld.getComponent<Sleeping>(mPlayerEntity);
    if (sleeping.safety > wakeAtOrBelow) return;
    const float tiles = glm::length(where - playerWorldCenter()) / static_cast<float>(std::max(1, mTileMap.tileWidth()));
    if (tiles > rangeTiles) return;
    mWorld.removeComponent<Sleeping>(mPlayerEntity);
    mInteractionMessage = message;
    mInteractionMessageTimer = 3.5f;
}

void Game::noteAccomplishment()
{
    if (mAccomplishedDay == mCurrentDay || mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        return;
    }
    // A station finished counts at once; the house only when every way in is shut.
    bool earned = mBaseAction.kind == BaseAction::Kind::None && mRunStats.structuresBuilt > 0;
    if (!earned && mFirstDays.homeBuilding > 0)
    {
        const std::vector<WayIn> ways = waysIn(mFirstDays.homeBuilding);
        earned = !ways.empty() && std::all_of(ways.begin(), ways.end(), [](const WayIn& w) { return w.boarded; });
    }
    if (!earned) return;
    mAccomplishedDay = mCurrentDay;
    MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
    mental.depression = std::max(0.0f, mental.depression - kAccomplishment);
    mInteractionMessage += mInteractionMessage.empty() ? "That's something done." : " That's something done.";
}

void Game::recordBase(bool died)
{
    const int home = mFirstDays.homeBuilding;
    if (home <= 0) return;
    int closures = 0;
    for (const WayIn& way : waysIn(home)) closures += way.boarded ? 1 : 0;
    bool station = false;
    mWorld.forEach<Structure, Transform>(
        [&](Entity, Structure& s, Transform& t)
        {
            if (s.type == StructureType::CanAlarm || s.type == StructureType::SpikeBoard || s.type == StructureType::Light) return;
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            if (mTileMap.getBuildingId(tile.x, tile.y) == home) station = true;
        });
    if (closures >= 3 || station)
    {
        // The middle of the house: the mean of its floor near where the run began.
        glm::vec2 sum(0.0f);
        int count = 0;
        for (int y = 0; y < mTileMap.height(); ++y)
        {
            for (int x = 0; x < mTileMap.width(); ++x)
            {
                if (mTileMap.getBuildingId(x, y) == home && mTileMap.isIndoors(x, y) && !mTileMap.blocksSight(x, y))
                {
                    sum += glm::vec2(static_cast<float>(x), static_cast<float>(y));
                    ++count;
                }
            }
        }
        BaseRecord base;
        base.building = home;
        const glm::vec2 centre = count > 0 ? sum / static_cast<float>(count) : glm::vec2(mSpawnData.playerStart);
        base.x = (centre.x + 0.5f) * static_cast<float>(mTileMap.tileWidth());
        base.y = (centre.y + 0.5f) * static_cast<float>(mTileMap.tileHeight());
        base.runIndex = mSaveManager.runNumber();
        mSaveManager.setBaseRecord(base);
        std::cout << "[Base] house " << home << " held (" << closures << " closed" << (station ? ", a station" : "") << ")\n";
    }
    LastHomeRecord last;
    last.building = home;
    const glm::ivec2 at = worldToTile(mTileMap, mDeathWorldPosition + glm::vec2(16.0f));
    last.diedAway = died && mTileMap.getBuildingId(at.x, at.y) != home;
    mSaveManager.setLastHome(last);
}
