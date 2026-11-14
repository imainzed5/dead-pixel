// The cemetery, markers and burial, the town aging between lives, and staying
// for good (phase 10). The rules are legacy/Cemetery's, legacy/LegacyRules'
// and legacy/WorldAging's; this is where they meet the world.
#include "core/GameShared.h"

#include "legacy/Cemetery.h"
#include "systems/BuildSystem.h"
#include "world/Doors.h"
#include "world/Windows.h"

using namespace gameshared;

namespace
{
constexpr float kMarkerRelief = 12.0f;
constexpr float kTendRelief = 4.0f;
constexpr float kTendSeconds = 5.0f;
constexpr float kBurySeconds = 30.0f;
constexpr float kStoneReachTiles = 1.3f;

glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}

glm::vec2 tileTopLeft(const TileMap& map, const glm::ivec2& tile)
{
    // An entity stands on a tile with its 32 px square's corner at the tile's.
    return glm::vec2(static_cast<float>(tile.x * map.tileWidth()), static_cast<float>(tile.y * map.tileHeight()));
}
} // namespace

bool Game::stampCemetery(LayoutData& layout)
{
    if (layout.cemetery.placed) return false;
    std::string why;
    if (!cemetery::stamp(layout, &why))
    {
        std::cerr << "[Cemetery] not placed: " << why << '\n';
        return false;
    }
    std::cout << "[Cemetery] plot at " << layout.cemetery.fenceMin.x << "," << layout.cemetery.fenceMin.y << " to "
              << layout.cemetery.fenceMax.x << "," << layout.cemetery.fenceMax.y << '\n';
    return true;
}

bool Game::cemeteryPlaced() const
{
    return mMapGenerator.layoutData().cemetery.placed;
}

glm::ivec2 Game::graveSlotTile(const GraveRecord& grave) const
{
    const LayoutData::CemeterySite& site = mMapGenerator.layoutData().cemetery;
    return legacy::cemeterySlot(site.fenceMin, site.fenceMax, std::max(0, grave.burialOrder) + legacy::kOldStones);
}

bool Game::graveWeathered(const GraveRecord& grave) const
{
    return aging::weathered(grave.runIndex, grave.tendedRun, mSaveManager.runNumber());
}

int Game::nextBurialOrder() const
{
    int next = 0;
    for (const GraveRecord& g : mLegacyGraves) next = std::max(next, g.burialOrder + 1);
    return next;
}

void Game::setupCemetery()
{
    mCemeteryEntities.clear();
    if (!cemeteryPlaced()) return;
    const LayoutData::CemeterySite& site = mMapGenerator.layoutData().cemetery;
    const GLuint sheetTex = mSpriteSheet.textureId();
    const auto prop = [&](const glm::ivec2& tile, const char* sprite, float size, int layer)
    {
        const Entity e = mWorld.createEntity();
        Transform t{};
        const glm::vec2 at = tileTopLeft(mTileMap, tile);
        t.x = at.x;
        t.y = at.y;
        t.prevX = t.x;
        t.prevY = t.y;
        Sprite s{};
        s.textureId = sheetTex;
        s.uvRect = mSpriteSheet.uvRect(sprite);
        s.layer = layer;
        s.drawSize = size;
        mWorld.addComponent<Transform>(e, t);
        mWorld.addComponent<Sprite>(e, s);
        mCemeteryEntities.push_back(e);
        return e;
    };
    // A low fence, a gap on the street side.
    for (const glm::ivec2& tile : cemetery::fenceTiles(site))
    {
        const bool alongX = tile.y == site.fenceMin.y || tile.y == site.fenceMax.y;
        prop(tile, alongX ? "prop_fence_x" : "prop_fence_y", SpriteSizes::kHumanoid, 5);
        mTileMap.setBlocked(tile.x, tile.y, true);
    }
    // The town's own old stones, the names worn off.
    for (int i = 0; i < legacy::kOldStones; ++i)
    {
        const glm::ivec2 tile = legacy::cemeterySlot(site.fenceMin, site.fenceMax, i);
        const Entity stone = prop(tile, "prop_stone_old", SpriteSizes::kHumanoid, 5);
        Interactable inter{};
        inter.type = InteractableType::GraveMarker;
        inter.prompt = "Read";
        inter.message = i == 0 ? "An old stone. The name has worn away." : i == 1 ? "An old stone. Only a year is left on it."
                                                                           : "An old stone, leaning. Moss in the letters.";
        mWorld.addComponent<Interactable>(stone, inter);
    }
    std::cout << "[Cemetery] " << cemetery::fenceTiles(site).size() << " fence tiles, "
              << std::count_if(mLegacyGraves.begin(), mLegacyGraves.end(), [](const GraveRecord& g) { return g.buried; })
              << " stones of the line\n";
}

// ---------------------------------------------------------------------------
// Graves, markers, stones

bool Game::markerAvailable(int graveIndex) const
{
    if (graveIndex < 0 || graveIndex >= static_cast<int>(mLegacyGraves.size()) || !cemeteryPlaced()) return false;
    const GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(graveIndex)];
    if (grave.risen || grave.buried || grave.markerTaken) return false;
    if (grave.laidToRest) return true;
    // Their pack taken (or nothing left in it).
    bool packLeft = false;
    auto& world = const_cast<World&>(mWorld);
    world.forEach<LegacyLink, Inventory>(
        [&](Entity, LegacyLink& link, Inventory& inv)
        {
            if (link.kind != LegacyLinkKind::Pack || link.graveIndex != graveIndex) return;
            for (const InventorySlot& slot : inv.slots)
            {
                if (slot.itemId >= 0 && slot.count > 0) packLeft = true;
            }
        });
    const bool spawned = static_cast<std::size_t>(graveIndex) < mLegacyPackSpawned.size() &&
                         mLegacyPackSpawned[static_cast<std::size_t>(graveIndex)];
    if (!spawned && !grave.items.empty()) packLeft = true;
    return !packLeft;
}

bool Game::takeMarker(int graveIndex)
{
    if (!markerAvailable(graveIndex) || mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(graveIndex)];
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const AddItemResult added = InventorySystem::addItemWithCondition(inv, mItemDatabase, mItems.marker, 1, 1.0f, graveIndex + 1);
    if (added.added <= 0)
    {
        mInteractionMessage = "No room in the pack for the marker.";
        mInteractionMessageTimer = 2.5f;
        return false;
    }
    grave.markerTaken = true;
    mInteractionMessage = "You pull up " + grave.name + "'s marker. The cemetery is near the middle of town.";
    mInteractionMessageTimer = 4.0f;
    std::cout << "[Cemetery] took " << grave.name << "'s marker\n";
    return true;
}

bool Game::placeMarker(int slot)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    if (slot < 0 || slot >= Inventory::kMaxSlots) return false;
    InventorySlot& item = inv.slots[static_cast<std::size_t>(slot)];
    if (item.itemId != mItems.marker || item.count <= 0) return false;
    const LayoutData::CemeterySite& site = mMapGenerator.layoutData().cemetery;
    if (!cemetery::inside(site, playerTile()))
    {
        mInteractionMessage = "It belongs in the cemetery, inside the fence.";
        mInteractionMessageTimer = 2.5f;
        return true;
    }
    const int index = item.loaded - 1;
    item = InventorySlot{};
    if (index < 0 || index >= static_cast<int>(mLegacyGraves.size())) return true;
    GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(index)];
    grave.markerTaken = false;
    grave.buried = true;
    grave.burialOrder = nextBurialOrder();
    const glm::vec2 at = tileTopLeft(mTileMap, graveSlotTile(grave));
    grave.x = at.x;
    grave.y = at.y;
    // The grave where they fell goes; the stone stands here.
    std::vector<Entity> old;
    mWorld.forEach<LegacyLink>(
        [&](Entity e, LegacyLink& link)
        {
            if (link.kind == LegacyLinkKind::Grave && link.graveIndex == index) old.push_back(e);
        });
    for (Entity e : old) mWorld.destroyEntity(e);
    spawnGrave(index);
    addDepression(-kMarkerRelief);
    noteJournal(journal::Kind::StoneSet, grave.name);
    syncLegacyToSave();
    mInteractionMessage = grave.name + "'s stone, with the others. It feels like something.";
    mInteractionMessageTimer = 4.0f;
    std::cout << "[Cemetery] set " << grave.name << "'s marker (stone " << grave.burialOrder << ")\n";
    return true;
}

bool Game::handleLegacyInteraction(const ProximityPrompt& entityPrompt)
{
    const Entity e = entityPrompt.entity;
    if (e == kInvalidEntity || !mWorld.hasComponent<LegacyLink>(e)) return false;
    const LegacyLink link = mWorld.getComponent<LegacyLink>(e);
    if (link.kind != LegacyLinkKind::Grave || link.graveIndex < 0 || link.graveIndex >= static_cast<int>(mLegacyGraves.size())) return false;
    const int index = link.graveIndex;
    const GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(index)];
    const bool pressed = mInput.wasKeyPressed(SDL_SCANCODE_E);
    const std::string what = grave.buried ? grave.name + "'s stone" : grave.name + "'s grave";
    if (grave.ofBloodline())
    {
        // One of the line: their journal, and what can be done for them.
        mProximityPrompt = what + "   E: Read";
        if (pressed && !(mReader.open && mReader.grave == index)) // E in the open reader is its own
        {
            int history = -1;
            const auto& lives = mSaveManager.characterHistory();
            for (std::size_t i = 0; i < lives.size(); ++i)
            {
                if (lives[i].runIndex == grave.runIndex && lives[i].name == grave.name) history = static_cast<int>(i);
            }
            openReader(index, history, true);
        }
        return true;
    }
    // A companion, a survivor buried by hand: the epitaph, and the marker.
    if (markerAvailable(index))
    {
        mProximityPrompt = what + "   E: Take the marker";
        if (pressed) takeMarker(index);
        return true;
    }
    if (graveWeathered(grave))
    {
        mProximityPrompt = what + "   E: Tend (5 s)";
        if (pressed) beginTend(index);
        return true;
    }
    mProximityPrompt = what + "   E: Read";
    if (pressed)
    {
        mInteractionMessage = "Here lies " + grave.name + ". " + grave.cause + ".";
        mInteractionMessageTimer = 4.0f;
    }
    return true;
}

void Game::beginTend(int graveIndex)
{
    if (graveIndex < 0 || graveIndex >= static_cast<int>(mLegacyGraves.size()) || mPlayerEntity == kInvalidEntity) return;
    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::Tend;
    mBaseAction.slot = graveIndex;
    mBaseAction.total = kTendSeconds;
    mBaseAction.label = "Tending " + mLegacyGraves[static_cast<std::size_t>(graveIndex)].name + "'s grave";
    mBaseAction.startPos = playerWorldCenter();
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
}

bool Game::beginBurial(Entity body)
{
    if (body == kInvalidEntity || !mWorld.hasComponent<SurvivorAI>(body) || mPlayerEntity == kInvalidEntity) return false;
    const SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(body);
    if (ai.alive || ai.buried) return false;
    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::Bury;
    mBaseAction.target = body;
    mBaseAction.total = kBurySeconds;
    mBaseAction.label = "Burying " + ai.name;
    mBaseAction.startPos = playerWorldCenter();
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
    std::cout << "[Cemetery] burying " << ai.name << '\n';
    return true;
}

void Game::finishLegacyAction()
{
    const BaseAction action = mBaseAction;
    mBaseAction = BaseAction{};
    if (action.kind == BaseAction::Kind::Tend)
    {
        if (action.slot < 0 || action.slot >= static_cast<int>(mLegacyGraves.size())) return;
        GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(action.slot)];
        grave.tendedRun = mSaveManager.runNumber();
        addDepression(-kTendRelief);
        syncLegacyToSave();
        mInteractionMessage = "You clear the stone and cut the grass back. " + grave.name + " can be read again.";
        mInteractionMessageTimer = 4.0f;
        std::cout << "[Cemetery] tended " << grave.name << "'s grave\n";
        return;
    }
    if (action.kind != BaseAction::Kind::Bury) return;
    const Entity body = action.target;
    if (body == kInvalidEntity || !mWorld.hasComponent<SurvivorAI>(body) || !mWorld.hasComponent<Transform>(body)) return;
    SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(body);
    if (ai.alive || ai.buried) return;
    const Transform& t = mWorld.getComponent<Transform>(body);
    GraveRecord grave;
    grave.name = ai.name;
    grave.x = t.x;
    grave.y = t.y;
    grave.day = mCurrentDay;
    grave.runIndex = mSaveManager.runNumber();
    grave.cause = "Buried by " + mCurrentCharacterName;
    grave.survivor = true;
    ai.buried = true;
    if (ai.id >= 0 && ai.id < static_cast<int>(mSurvivors.size())) mSurvivors[static_cast<std::size_t>(ai.id)].buried = true;
    mSaveManager.addGrave(grave);
    mLegacyGraves.push_back(grave);
    mLegacyPackSpawned.push_back(false);
    spawnGrave(static_cast<int>(mLegacyGraves.size()) - 1);
    mWorld.destroyEntity(body);
    syncLegacyToSave();
    mInteractionMessage = ai.name + " is in the ground. You say their name once.";
    mInteractionMessageTimer = 4.0f;
    std::cout << "[Cemetery] buried " << grave.name << " where they lay\n";
}

void Game::reconcileMarkers()
{
    // The pack is the truth: a marker it holds is out of its grave; any other
    // grave keeps its own. A marker for a grave already set is dropped.
    std::vector<bool> carried(mLegacyGraves.size(), false);
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        for (InventorySlot& slot : mWorld.getComponent<Inventory>(mPlayerEntity).slots)
        {
            if (slot.itemId != mItems.marker || slot.count <= 0) continue;
            const int index = slot.loaded - 1;
            if (index < 0 || index >= static_cast<int>(mLegacyGraves.size()) || mLegacyGraves[static_cast<std::size_t>(index)].buried ||
                carried[static_cast<std::size_t>(index)])
            {
                slot = InventorySlot{};
                continue;
            }
            carried[static_cast<std::size_t>(index)] = true;
        }
    }
    for (std::size_t i = 0; i < mLegacyGraves.size(); ++i) mLegacyGraves[i].markerTaken = carried[i];
}

void Game::releaseMarkers()
{
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        for (InventorySlot& slot : mWorld.getComponent<Inventory>(mPlayerEntity).slots)
        {
            if (slot.itemId == mItems.marker) slot = InventorySlot{};
        }
    }
    for (GraveRecord& grave : mLegacyGraves) grave.markerTaken = false;
}

void Game::spawnBones()
{
    const SpriteRegion* bones = mSpriteSheet.find("prop_bones");
    if (bones == nullptr) return;
    for (const auto& [x, y] : mSaveManager.aging().bones)
    {
        if (!mTileMap.inBounds(x, y) || mTileMap.isSolid(x, y)) continue;
        const Entity e = mWorld.createEntity();
        Transform t{};
        const glm::vec2 at = tileTopLeft(mTileMap, glm::ivec2(x, y));
        t.x = at.x;
        t.y = at.y;
        t.prevX = t.x;
        t.prevY = t.y;
        Sprite s{};
        s.textureId = mSpriteSheet.textureId();
        s.uvRect = mSpriteSheet.uvRect("prop_bones");
        s.layer = 2;
        s.drawSize = SpriteSizes::kDefault;
        s.flipX = ((x * 7 + y * 13) & 1) != 0;
        mWorld.addComponent<Transform>(e, t);
        mWorld.addComponent<Sprite>(e, s);
    }
}

// ---------------------------------------------------------------------------
// The town ages

void Game::ageWorldForNextLife(int daysLived, int runIndex)
{
    aging::WorldState world;
    for (const StructureRecord& r : mSaveManager.structures())
    {
        aging::Built b;
        b.record = r;
        const auto type = static_cast<StructureType>(r.type);
        const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(r.x + 16.0f, r.y + 16.0f));
        b.building = mTileMap.inBounds(tile.x, tile.y) ? mTileMap.getBuildingId(tile.x, tile.y) : 0;
        b.wears = BuildSystem::wearsBetweenLives(type);
        b.crate = type == StructureType::StorageCrate;
        b.garden = type == StructureType::GardenBed;
        world.built.push_back(std::move(b));
    }
    world.doors = mSaveManager.boardedDoors();
    world.windows = mSaveManager.windows();
    world.doorBoardHp = Doors::boardHpEach();
    world.windowBoardHp = Windows::kBoardHp;
    for (const SurvivorRecord& r : mSurvivors)
    {
        if (r.alive && !r.left && r.homeBuilding > 0) world.keptBuildings.push_back(r.homeBuilding);
    }
    for (const ItemDef& item : mItemDatabase.allItems())
    {
        if (item.category == ItemCategory::Food || item.category == ItemCategory::Drink) world.foodItems.push_back(item.id);
    }
    DistrictMemoryRecord memory = mSaveManager.districtMemory();
    world.infection = memory.infection;
    world.lifeStart = memory.lifeStart;
    world.bones = mSaveManager.aging().bones;
    mWorld.forEach<Corpse, Transform>(
        [&](Entity, Corpse&, Transform& t)
        {
            const glm::ivec2 tile = worldToTile(mTileMap, centreOf(t));
            world.bodies.emplace_back(tile.x, tile.y);
        });

    const std::uint32_t endedSeed = mSaveManager.worldSeed() ^ static_cast<std::uint32_t>(runIndex);
    const aging::Report report = aging::ageWorld(world, daysLived, endedSeed);

    std::vector<StructureRecord> structures;
    for (const aging::Built& b : world.built) structures.push_back(b.record);
    mSaveManager.saveStructures(structures);
    mSaveManager.setBoardedDoors(std::move(world.doors));
    mSaveManager.setWindows(std::move(world.windows));
    // The districts as the next life finds them. The seed is the ended life's,
    // so the next one takes its first look (and thins where this one killed).
    memory.infection = world.infection;
    memory.appliedRunSeed = endedSeed;
    mSaveManager.setDistrictMemory(std::move(memory));
    mSaveManager.saveDistrictMemory();
    AgingRecord kept;
    kept.bones = world.bones;
    mSaveManager.setAging(std::move(kept));
    mSaveManager.saveAging();

    // A survivor found the crate: what was in it is in their stock now.
    for (std::size_t i = 0; i < report.strippedAt.size(); ++i)
    {
        const glm::vec2 crate(static_cast<float>(report.strippedAt[i].first) + 16.0f, static_cast<float>(report.strippedAt[i].second) + 16.0f);
        SurvivorRecord* nearest = nullptr;
        float best = 1e9f;
        for (SurvivorRecord& r : mSurvivors)
        {
            if (!r.alive || r.left || r.retired) continue;
            const float d = glm::length(glm::vec2(r.x, r.y) - crate);
            if (d < best)
            {
                best = d;
                nearest = &r;
            }
        }
        if (nearest == nullptr) break;
        for (const ContainerItemRecord& item : report.strippedItems) nearest->stock.push_back(item);
        std::cout << "[Aging] " << nearest->name << " found a crate and took what was in it\n";
        break; // one survivor's luck a life
    }
    std::cout << "[Aging] " << daysLived << " days: " << report.worn << " worn, " << report.lost << " gone, " << report.boardsGone
              << " boards gone, " << report.foodLost << " food lost, " << report.stripped << " crates stripped, "
              << world.bones.size() << " bones\n";
}

// ---------------------------------------------------------------------------
// Staying for good

std::string Game::stayForGoodBlocker() const
{
    legacy::StayCheck check;
    check.day = mCurrentDay;
    if (mPlayerEntity == kInvalidEntity) return "Not now.";
    const glm::ivec2 here = playerTile();
    const int building = mTileMap.inBounds(here.x, here.y) ? mTileMap.getBuildingId(here.x, here.y) : 0;
    if (building > 0 && mTileMap.isIndoors(here.x, here.y))
    {
        int closures = 0;
        for (const WayIn& way : waysIn(building)) closures += way.boarded ? 1 : 0;
        const bool bed = std::any_of(mFurniture.begin(), mFurniture.end(),
                                     [&](const FurniturePiece& p) { return p.building == building && p.kind == FurnitureKind::Bed; });
        check.atBase = closures >= 3 && bed;
    }
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        check.hunger = needs.hunger;
        check.thirst = needs.thirst;
    }
    if (mWorld.hasComponent<Health>(mPlayerEntity)) check.health = mWorld.getComponent<Health>(mPlayerEntity).current;
    check.infected = mWorld.hasComponent<Wounds>(mPlayerEntity) && hasInfectedWound(mWorld.getComponent<Wounds>(mPlayerEntity));
    return legacy::stayBlocker(check);
}

void Game::stayForGood()
{
    if (!stayForGoodBlocker().empty()) return;
    mStayedForGood = true;
    mStayConfirm = 0;
    mRunState = RunState::Playing; // from the pause menu
    beginRetirementState(mCurrentCharacterName + " stayed.");
}
