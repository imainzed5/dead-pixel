// The way out (overhaul phase 9): the service gate in the wall round the
// town, the four things it takes to open it, the Sergeant who has the key,
// the last stand and walking out. The rules are world/Gate's; the gate's
// state is world memory (<slot>_gate.json), the stand is this life's.
#include "core/GameShared.h"

#include <iostream>
#include <random>

#include "world/BuildingKinds.h"

using namespace gameshared;

namespace
{
glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}

constexpr float kReachTiles = 1.3f;
constexpr float kSeenBoxTiles = 3.0f;       // close enough to read the lamps
constexpr float kSergeantThinkSeconds = 2.0f;
constexpr float kStandPullSeconds = 5.0f;
constexpr float kStandPullTiles = 60.0f;   // every one of the dead this close comes
constexpr float kWaveNearTiles = 22.0f;    // fresh dead come in from this far out
constexpr float kWaveFarTiles = 30.0f;
constexpr float kOpenGateNearTiles = 20.0f;
constexpr float kManualMechanicsXp = 100.0f;
const glm::vec4 kSergeantTint(0.70f, 0.76f, 0.98f, 1.0f);
} // namespace

bool Game::gatePlaced() const
{
    return mMapGenerator.layoutData().gate.placed;
}

glm::vec2 Game::gateBoxWorld() const
{
    return tileCentre(mTileMap, mMapGenerator.layoutData().gate.box);
}

bool Game::stampGate(LayoutData& layout)
{
    // The cemetery is stamped after the gate, clear of its yard (phase 10);
    // true when either changed the layout.
    if (layout.gate.placed) return stampCemetery(layout);
    const int legacy = mSaveManager.legacyObjectiveDistrict();
    std::string why;
    if (!gate::stamp(layout, legacy, &why))
    {
        std::cerr << "[Gate] not placed: " << why << '\n';
        return stampCemetery(layout);
    }
    std::cout << "[Gate] placed on the " << gate::sideName(layout.gate.side) << " side, district " << layout.gate.district
              << (legacy > 0 ? " (the old objective's)" : "") << ", gate at " << layout.gate.gate[1].x << ","
              << layout.gate.gate[1].y << '\n';
    mSaveManager.clearLegacyObjectiveDistrict();
    stampCemetery(layout);
    return true;
}

void Game::setupGate()
{
    mGate = mSaveManager.gateState();
    mStand = gate::Stand{};
    mStandStage = 0;
    mStandPullTimer = 0.0f;
    mLeads = 0;
    mGateSeen = clues::Seen{};
    mBroadcastTimer = 1.0f;
    mBroadcastLine = 0;
    mBroadcastHeard = false;
    mGateCheckTimer = 0.0f;
    mSergeantLeg = 0;
    mSergeantTimer = 0.0f;
    mNotebookPage = 0;
    mReadables.clear();
    mReadablesRead.clear();
    mSergeantLoop.clear();
    mFuseCabinet = glm::ivec2(-1);
    if (!gatePlaced()) return;

    const LayoutData& layout = mMapGenerator.layoutData();
    const LayoutData::GateSite& site = layout.gate;
    mClueNames = clues::names(mClueContent, mSaveManager.worldSeed(), site.side);

    // Shut until it has opened once; open for ever after.
    for (const glm::ivec2& t : site.gate) mTileMap.setBlocked(t.x, t.y, !mGate.open);
    mTileMap.setBlocked(site.generator.x, site.generator.y, true);

    // The first life of a world on the way out finds a note in its kitchen.
    const bool campaign = mTitleGameModeIndex == 0;
    if (campaign && mSaveManager.runNumber() == 1 && mGate.noteHome == 0 && mFirstDays.homeBuilding > 0)
    {
        mGate.noteHome = mFirstDays.homeBuilding;
        saveGate();
    }
    mReadables = clues::place(mFurniture, layout, mGate.noteHome);
    mFuseCabinet = clues::fuseCabinet(mFurniture, layout);
    if (mFuseCabinet.x < 0)
    {
        // No hardware store in town: the hut's own locker keeps the spare.
        for (const FurniturePiece& p : mFurniture)
        {
            if (p.building == site.hut && furniture::info(p.kind).prompt != nullptr && p.kind != FurnitureKind::Desk)
            {
                mFuseCabinet = p.tile;
                break;
            }
        }
    }

    mSergeantBuilding = gate::sergeantBuilding(layout, mSaveManager.worldSeed());
    mSergeantLoop = gate::sergeantLoop(layout, mSergeantBuilding);

    // Someone of the line got out: the gate, and every building, are on the map.
    if (const Traits* traits = playerTraits(); traits != nullptr && traits->has(BloodlineTrait::Wayfinder))
    {
        mLeads = static_cast<clues::Mask>(mLeads | clues::bit(clues::WayOut) | clues::bit(clues::Where));
        ScavengeMemoryRecord memory = mSaveManager.scavengeMemory();
        bool changed = false;
        for (std::size_t i = 0; i < layout.buildings.size(); ++i)
        {
            const int building = static_cast<int>(i) + 1;
            if (std::find(memory.seenBuildings.begin(), memory.seenBuildings.end(), building) != memory.seenBuildings.end()) continue;
            memory.seenBuildings.push_back(building);
            changed = true;
        }
        if (changed)
        {
            mSaveManager.setScavengeMemory(std::move(memory));
            mSaveManager.saveWorldMemory();
        }
    }

    // An open gate lets the dead in: half as many again near it.
    if (mGate.open)
    {
        const glm::vec2 box = gateBoxWorld();
        const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
        int near = 0;
        mWorld.forEach<ZombieAI, Transform>(
            [&](Entity, ZombieAI&, Transform& t)
            {
                if (glm::length(centreOf(t) - box) < kOpenGateNearTiles * tile) ++near;
            });
        spawnStandDead(gate::openGateExtra(near));
    }

    // Someone before you was writing things down.
    if (mLegacyCardTimer > 0.0f)
    {
        for (std::size_t i = 0; i < mLegacyGraves.size(); ++i)
        {
            const GraveRecord& grave = mLegacyGraves[i];
            const bool carried = std::any_of(grave.items.begin(), grave.items.end(),
                                             [&](const auto& item) { return item.first == mItems.notebook; });
            if (grave.notebook && grave.leads != 0 && carried)
            {
                mLegacyCardLines.emplace_back("Someone before you was writing things down.");
                break;
            }
        }
    }

    std::cout << "[Gate] " << gate::sideName(site.side) << " side; fuse " << (mGate.fuseFitted ? "fitted" : "blown") << ", fuel "
              << mGate.fuel << ", repairs " << mGate.repairs << (mGate.freed ? " (freed)" : "") << ", key "
              << static_cast<int>(mGate.key) << (mGate.open ? ", open" : "") << "; " << mReadables.size()
              << " readables; Sgt. " << mClueNames.sergeant << " walks building " << mSergeantBuilding << " ("
              << mSergeantLoop.size() << " points)\n";
}

void Game::restoreGate(const RunCheckpoint& cp)
{
    if (!cp.gateSaved) return;
    mLeads = static_cast<clues::Mask>(mLeads | static_cast<clues::Mask>(cp.leads));
    mGateSeen = cp.seen;
    mBroadcastHeard = cp.broadcastHeard;
    mStand.started = cp.standStarted;
    mStand.opened = cp.standOpened;
    mStand.elapsed = cp.standElapsed;
    mStand.waves = cp.standWaves;
    mStand.running = false; // a restart grinds first
    mStandStage = gate::stage(mStand.opened);
}

void Game::gateCheckpoint(RunCheckpoint& cp) const
{
    cp.gateSaved = true;
    cp.leads = mLeads;
    cp.seen = mGateSeen;
    cp.broadcastHeard = mBroadcastHeard;
    cp.standStarted = mStand.started;
    cp.standOpened = mStand.opened;
    cp.standElapsed = mStand.elapsed;
    cp.standWaves = mStand.waves;
}

void Game::saveGate()
{
    mGateDirty = false;
    mSaveManager.setGateState(mGate);
    mSaveManager.saveGateState();
}

void Game::flushGate()
{
    if (mGateDirty) saveGate();
}

void Game::creditGate(const std::string& deed)
{
    gate::credit(mGate, mCurrentCharacterName, deed, mSaveManager.runNumber());
}

int Game::itemsInTown(int itemId) const
{
    if (itemId < 0) return 0;
    int count = 0;
    const_cast<World&>(mWorld).forEach<Inventory>(
        [&](Entity, Inventory& inv) { count += InventorySystem::countItem(inv, itemId); });
    // Packs by graves not laid out this life.
    for (std::size_t i = 0; i < mLegacyGraves.size(); ++i)
    {
        if (i < mLegacyPackSpawned.size() && mLegacyPackSpawned[i]) continue;
        for (const auto& [id, n] : mLegacyGraves[i].items)
        {
            if (id == itemId) count += n;
        }
    }
    return count;
}

bool Game::gateKeyInHand() const
{
    return playerHasItem(mItems.gateKey);
}

void Game::ensureGateKey()
{
    if (!gatePlaced()) return;
    if (mGate.key == gate::KeyPlace::Sergeant)
    {
        bool alive = false;
        mWorld.forEach<ZombieAI, Health>(
            [&](Entity, ZombieAI& ai, Health& h)
            {
                if (ai.gateRole == 1 && h.current > 0.0f) alive = true;
            });
        if (!alive && !mSergeantLoop.empty()) spawnSergeant(mSergeantLoop.front(), nullptr);
        return;
    }
    if (mGate.key == gate::KeyPlace::Loose && itemsInTown(mItems.gateKey) == 0)
    {
        // It lay outdoors, where nothing is kept between lives: it is still there.
        glm::ivec2 tile = mGate.keyTile;
        if (!mTileMap.inBounds(tile.x, tile.y) && !mSergeantLoop.empty()) tile = mSergeantLoop.front();
        if (mTileMap.inBounds(tile.x, tile.y))
        {
            dropOnFloor(tile, mItems.gateKey, 1);
            std::cout << "[Gate] the key lies at " << tile.x << "," << tile.y << '\n';
        }
    }
}

void Game::ensureGateFuse()
{
    if (!gatePlaced() || mGate.fuseFitted || itemsInTown(mItems.gateFuse) > 0 || mFuseCabinet.x < 0) return;
    Entity cabinet = kInvalidEntity;
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable& inter, Inventory&, Transform& t)
        {
            if (cabinet != kInvalidEntity || inter.type != InteractableType::Container || mWorld.hasComponent<Sprite>(e)) return;
            if (worldToTile(mTileMap, glm::vec2(t.x + 1.0f, t.y + 1.0f)) == mFuseCabinet) cabinet = e;
        });
    if (cabinet == kInvalidEntity) return;
    InventorySystem::addItem(mWorld.getComponent<Inventory>(cabinet), mItemDatabase, mItems.gateFuse, 1);
    std::cout << "[Gate] a fuse in the cabinet at " << mFuseCabinet.x << "," << mFuseCabinet.y << '\n';
}

void Game::spawnSergeant(const glm::ivec2& tile, const ZombieRecord* record)
{
    std::mt19937 rng(mSaveManager.currentRunSeed() ^ 0x5E26u);
    const float size = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 topLeft = record != nullptr ? glm::vec2(record->x, record->y)
                                                : (glm::vec2(tile) + glm::vec2(0.5f)) * size - glm::vec2(kSpriteSize * 0.5f);
    EntityFactory::ZombieParams params = zombieParams(ZombieKind::Brute, topLeft, rng);
    params.health = gate::kSergeantHealth;
    const Entity sergeant = EntityFactory::spawnZombie(mWorld, mSpriteSheet, params);
    mWorld.getComponent<ZombieAI>(sergeant).gateRole = 1;
    if (mWorld.hasComponent<Sprite>(sergeant)) mWorld.getComponent<Sprite>(sergeant).color = kSergeantTint;
    if (record != nullptr) return;

    // The dead who walk with him.
    const int escorts = gate::kEscortMin + static_cast<int>(rng() % static_cast<std::uint32_t>(gate::kEscortMax - gate::kEscortMin + 1));
    int placed = 0;
    for (int attempt = 0; placed < escorts && attempt < escorts * 20; ++attempt)
    {
        const glm::ivec2 at = tile + glm::ivec2(static_cast<int>(rng() % 5u) - 2, static_cast<int>(rng() % 5u) - 2);
        if (at == tile || !mTileMap.inBounds(at.x, at.y) || mTileMap.isSolid(at.x, at.y)) continue;
        const glm::vec2 escortTopLeft = (glm::vec2(at) + glm::vec2(0.5f)) * size - glm::vec2(kSpriteSize * 0.5f);
        const Entity escort = EntityFactory::spawnZombie(mWorld, mSpriteSheet, zombieParams(ZombieKind::Shambler, escortTopLeft, rng));
        mWorld.getComponent<ZombieAI>(escort).gateRole = 2;
        ++placed;
    }
    std::cout << "[Gate] Sgt. " << mClueNames.sergeant << " at " << tile.x << "," << tile.y << " with " << placed << " of the dead\n";
}

void Game::updateSergeant(float dtSeconds)
{
    mSergeantTimer -= dtSeconds;
    if (mSergeantTimer > 0.0f || mSergeantLoop.empty()) return;
    mSergeantTimer = kSergeantThinkSeconds;
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    Entity sergeant = kInvalidEntity;
    glm::vec2 at{0.0f};
    mWorld.forEach<ZombieAI, Transform, Health>(
        [&](Entity e, ZombieAI& ai, Transform& t, Health& h)
        {
            if (ai.gateRole != 1 || h.current <= 0.0f) return;
            sergeant = e;
            at = centreOf(t);
        });
    if (sergeant == kInvalidEntity) return;
    ZombieAI& ai = mWorld.getComponent<ZombieAI>(sergeant);
    const bool hunting = ai.state == ZombieState::Chase || ai.state == ZombieState::Attack;
    if (!hunting)
    {
        // Day and night, round the same five points.
        const glm::vec2 goal = tileCentre(mTileMap, mSergeantLoop[mSergeantLeg % mSergeantLoop.size()]);
        if (glm::length(goal - at) < 2.0f * tile)
        {
            mSergeantLeg = (mSergeantLeg + 1) % mSergeantLoop.size();
            ZombieAISystem::sendTo(ai, tileCentre(mTileMap, mSergeantLoop[mSergeantLeg]), true);
        }
        else if (ai.state == ZombieState::Wander || glm::distance(ai.targetWorld, goal) > tile)
        {
            ZombieAISystem::sendTo(ai, goal, true);
        }
    }
    // The dead with him clump on him.
    mWorld.forEach<ZombieAI, Transform, Health>(
        [&](Entity, ZombieAI& escort, Transform& t, Health& h)
        {
            if (escort.gateRole != 2 || h.current <= 0.0f) return;
            if (escort.state == ZombieState::Chase || escort.state == ZombieState::Attack) return;
            if (glm::length(centreOf(t) - at) > 3.0f * tile) ZombieAISystem::sendTo(escort, at, true);
        });
}

void Game::gateZombieDied(Entity zombie)
{
    if (!mWorld.hasComponent<ZombieAI>(zombie) || !mWorld.hasComponent<Transform>(zombie)) return;
    const ZombieAI& ai = mWorld.getComponent<ZombieAI>(zombie);
    if (ai.gateRole != 1 || mGate.key != gate::KeyPlace::Sergeant) return;
    glm::ivec2 tile = worldToTile(mTileMap, centreOf(mWorld.getComponent<Transform>(zombie)));
    if (!mTileMap.inBounds(tile.x, tile.y)) return;
    dropOnFloor(tile, mItems.gateKey, 1);
    mGate.key = gate::KeyPlace::Loose;
    mGate.keyTile = tile;
    if (ai.lastDamagedBy == mPlayerEntity) creditGate("sergeant");
    // Written with the next checkpoint or at the run's end, not in the tick
    // he falls (a file written in a fighting tick cost 20 to 40 ms).
    mGateDirty = true;
    mInteractionMessage = "Something falls from the Sergeant's belt. A key on a red tag.";
    mInteractionMessageTimer = 4.0f;
    std::cout << "[Gate] the Sergeant is down at " << tile.x << "," << tile.y << "; the key lies there\n";
}

void Game::spawnStandDead(int count)
{
    if (count <= 0 || !gatePlaced()) return;
    int alive = 0;
    mWorld.forEach<ZombieAI>([&](Entity, ZombieAI&) { ++alive; });
    count = std::min(count, std::max(0, gate::kStandDeadCap - alive));
    if (count <= 0) return;
    const glm::vec2 box = gateBoxWorld();
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    std::mt19937 rng(mSaveManager.currentRunSeed() ^ 0x6A7Eu ^ static_cast<std::uint32_t>(mStand.waves * 7919 + count));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    int placed = 0;
    for (int attempt = 0; placed < count && attempt < count * 40; ++attempt)
    {
        const float angle = unit(rng) * 6.2831853f;
        const float distance = (kWaveNearTiles + unit(rng) * (kWaveFarTiles - kWaveNearTiles)) * tile;
        const glm::vec2 at = box + glm::vec2(std::cos(angle), std::sin(angle)) * distance;
        const glm::ivec2 t = worldToTile(mTileMap, at);
        if (!mTileMap.inBounds(t.x, t.y) || mTileMap.isSolid(t.x, t.y) || mTileMap.isIndoors(t.x, t.y)) continue;
        const glm::vec2 topLeft = (glm::vec2(t) + glm::vec2(0.5f)) * tile - glm::vec2(kSpriteSize * 0.5f);
        const Entity zombie = EntityFactory::spawnZombie(mWorld, mSpriteSheet, zombieParams(rollKind(rng, t), topLeft, rng));
        if (mStand.running) ZombieAISystem::sendTo(mWorld.getComponent<ZombieAI>(zombie), box, false);
        ++placed;
    }
    std::cout << "[Gate] " << placed << " of the dead come in near the gate\n";
}

void Game::stopStand(const std::string& why)
{
    if (!mStand.running) return;
    mStand.running = false;
    mInteractionMessage = why;
    mInteractionMessageTimer = 4.0f;
    saveGate();
    std::cout << "[Gate] the motor stops at " << static_cast<int>(mStand.opened) << " s: " << why << '\n';
}

void Game::updateStand(float dtSeconds)
{
    if (!mStand.running) return;
    const LayoutData::GateSite& site = mMapGenerator.layoutData().gate;
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 box = gateBoxWorld();
    const glm::vec2 here = playerWorldCenter();
    if (glm::length(here - box) > gate::kStandRadiusTiles * tile)
    {
        stopStand("Behind you the motor coughs and dies.");
        return;
    }

    const int cans = gate::run(mStand, mGate, dtSeconds);
    if (cans > 0)
    {
        saveGate();
        std::cout << "[Gate] a can burned; fuel " << mGate.fuel << '\n';
    }
    noteGateSeen();

    // The loudest thing in town.
    mStand.noiseTimer -= dtSeconds;
    if (mStand.noiseTimer <= 0.0f)
    {
        mStand.noiseTimer += gate::kNoiseSeconds;
        const glm::vec2 generator = tileCentre(mTileMap, site.generator);
        // The engine itself is the generator layer (GameSound.cpp); this is
        // the noise the dead hear, and the gate's grind as it labours.
        makeNoise(generator, NoiseTier::Explosive, 1.0f, SoundId::GateGrind);
        mShakeTrauma = std::max(mShakeTrauma, 0.08f);
    }
    // Every one of the dead within 60 tiles comes.
    mStandPullTimer -= dtSeconds;
    if (mStandPullTimer <= 0.0f)
    {
        mStandPullTimer = kStandPullSeconds;
        int pulled = 0;
        mWorld.forEach<ZombieAI, Transform>(
            [&](Entity, ZombieAI& ai, Transform& t)
            {
                if (ai.dormant || ai.state == ZombieState::Chase || ai.state == ZombieState::Attack) return;
                if (glm::length(centreOf(t) - box) > kStandPullTiles * tile) return;
                if (ai.state == ZombieState::Investigate && glm::distance(ai.targetWorld, box) < 3.0f * tile) return;
                ZombieAISystem::sendTo(ai, box, false);
                ++pulled;
            });
        if (pulled > 0) std::cout << "[Gate] " << pulled << " of the dead turn toward the noise\n";
    }
    // Fresh dead at the district's edge: 3, then 4, then 5...
    const int before = mStand.waves;
    const int due = gate::wavesDue(mStand);
    for (int w = before; w < before + due; ++w)
    {
        int alive = 0;
        mWorld.forEach<ZombieAI, Health>([&](Entity, ZombieAI&, Health& h) { if (h.current > 0.0f) ++alive; });
        std::cout << "[Gate] wave " << (w + 1) << " at " << static_cast<int>(mStand.elapsed) << " s: " << gate::waveSize(w)
                  << " more; " << alive << " of the dead in town\n";
        spawnStandDead(gate::waveSize(w));
    }

    const int stage = gate::stage(mStand.opened);
    if (stage != mStandStage)
    {
        mStandStage = stage;
        if (!site.gate.empty()) playAt(SoundId::GateGrind, tileCentre(mTileMap, site.gate.front()));
        if (stage < 3)
        {
            mInteractionMessage = stage == 1 ? "The gate grinds open a third." : "Two thirds open.";
            mInteractionMessageTimer = 3.0f;
        }
        std::cout << "[Gate] open " << stage << " of 3 at " << static_cast<int>(mStand.elapsed) << " s\n";
    }
    if (mGate.open)
    {
        // The key stays in the box.
        mGate.key = gate::KeyPlace::Turned;
        InventorySystem::removeItem(mWorld.getComponent<Inventory>(mPlayerEntity), mItems.gateKey, 1);
        for (const glm::ivec2& t : site.gate) mTileMap.setBlocked(t.x, t.y, false);
        creditGate("held");
        saveGate();
        noteGateSeen();
        if (!site.gate.empty()) playAt(SoundId::GateOpen, tileCentre(mTileMap, site.gate.front()));
        mInteractionMessage = "The gate stands open. Past it, the road.";
        mInteractionMessageTimer = 6.0f;
        std::cout << "[Gate] the gate is open\n";
        return;
    }
    if (!mStand.running)
    {
        saveGate();
        mInteractionMessage = "The generator runs dry and stops.";
        mInteractionMessageTimer = 4.0f;
        std::cout << "[Gate] dry at " << static_cast<int>(mStand.opened) << " s\n";
    }
}

bool Game::updateGate(float dtSeconds)
{
    if (!gatePlaced() || mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity)) return false;
    const LayoutData::GateSite& site = mMapGenerator.layoutData().gate;
    updateSergeant(dtSeconds);
    updateStand(dtSeconds);

    // Through the open gate: out.
    if (mGate.open)
    {
        const glm::ivec2 me = playerTile();
        if (std::find(site.gate.begin(), site.gate.end(), me) != site.gate.end())
        {
            beginEscape();
            return true;
        }
    }

    mGateCheckTimer -= dtSeconds;
    if (mGateCheckTimer > 0.0f) return false;
    mGateCheckTimer = 0.5f;
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float distance = glm::length(playerWorldCenter() - gateBoxWorld()) / tile;
    if (distance <= gate::kSeenTiles && !clues::knows(mLeads, clues::Where))
    {
        learnLeads(clues::bit(clues::Where), "seen");
        mInteractionMessage = "A steel gate in the wall, a hut beside it. The service gate.";
        mInteractionMessageTimer = 4.0f;
    }
    if (distance <= kSeenBoxTiles) noteGateSeen();
    return false;
}

bool Game::handleGateInteraction(const ProximityPrompt&)
{
    if (!gatePlaced() || mPlayerEntity == kInvalidEntity) return false;
    const LayoutData::GateSite& site = mMapGenerator.layoutData().gate;
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const bool pressed = mInput.pressed(Action::Use);
    const auto beginHeld = [&](BaseAction::Kind kind, const glm::ivec2& tile, float seconds, const char* label)
    {
        mBaseAction = BaseAction{};
        mBaseAction.kind = kind;
        mBaseAction.tile = tile;
        mBaseAction.total = seconds;
        mBaseAction.label = label;
        mBaseAction.startPos = here;
        mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
    };

    // The control box.
    if (glm::length(tileCentre(mTileMap, site.box) - here) <= kReachTiles * tileSize)
    {
        const bool keyInHand = gateKeyInHand();
        const int need = gateFuelNeeded();
        std::string prompt = "Gate box   ";
        if (mGate.open) prompt += "The gate stands open.";
        else if (mStand.running) prompt += mStand.grind > 0.0f ? "The motor grinds." : "The motor is running.";
        else if (!mGate.fuseFitted && playerHasItem(mItems.gateFuse)) prompt += "E: Fit the fuse";
        else if (gate::ready(mGate, need) && keyInHand) prompt += "E: Start it";
        else prompt += gate::missingText(mGate, keyInHand, need);
        mProximityPrompt = prompt;
        if (!pressed) return true;
        learnLeads(static_cast<clues::Mask>(clues::bit(clues::Where) | clues::teaches(clues::Source::ControlBox)), "the box");
        noteGateSeen();
        if (mGate.open || mStand.running) return true;
        if (!mGate.fuseFitted && playerHasItem(mItems.gateFuse))
        {
            beginHeld(BaseAction::Kind::FitFuse, site.box, gate::kFitFuseSeconds, "Fitting the fuse");
            return true;
        }
        if (gate::ready(mGate, need) && keyInHand)
        {
            gate::start(mStand);
            mStandStage = gate::stage(mStand.opened);
            creditGate("started");
            if (mStand.grind <= 0.0f)
            {
                // The first start: the gate district's dead set the crowd.
                const int infection = static_cast<int>(mDistrictInfectionSystem.getInfection(site.district));
                spawnStandDead(gate::baseCrowd(infection));
            }
            saveGate();
            playAt(SoundId::CarCrank, tileCentre(mTileMap, site.generator));
            mInteractionMessage = mStand.grind > 0.0f ? "The generator catches. The gate grinds and will not move yet."
                                                      : "The generator catches. The gate begins to grind open.";
            mInteractionMessageTimer = 4.0f;
            std::cout << "[Gate] started (opened " << static_cast<int>(mStand.opened) << " s, fuel " << mGate.fuel << ")\n";
            return true;
        }
        mInteractionMessage = gate::missingText(mGate, keyInHand, need);
        mInteractionMessageTimer = 3.0f;
        return true;
    }

    // The generator.
    if (glm::length(tileCentre(mTileMap, site.generator) - here) <= (kReachTiles + 0.3f) * tileSize)
    {
        const bool toolbox = playerHasItem(mItems.toolbox);
        const bool knowsHow = clues::knows(mLeads, clues::Fixable) || skillLevel(Skill::Mechanics) >= gate::kMechanicsToFree;
        std::string prompt = "Generator   ";
        const bool canPour = mGate.fuel < gate::kFuelNeeded && playerHasItem(mItems.fuelCan);
        const bool canFree = !mGate.freed && knowsHow && toolbox;
        const std::string fuel = std::to_string(mGate.fuel) + " of " + std::to_string(gate::kFuelNeeded);
        if (mStand.running) prompt += "Running. Fuel " + fuel + ".";
        else if (canFree) prompt += "E: Work it loose (" + std::to_string(mGate.repairs) + " of " + std::to_string(gate::kRepairSessions) + ")";
        else if (canPour) prompt += "E: Pour fuel (" + fuel + ")";
        else if (!mGate.freed && !knowsHow) prompt += "Seized solid. You do not know how to free it. Fuel " + fuel + ".";
        else if (!mGate.freed) prompt += "Seized solid. It would take tools. Fuel " + fuel + ".";
        else prompt += "Fuel " + fuel + ".";
        mProximityPrompt = prompt;
        if (!pressed) return true;
        noteGateSeen();
        if (mStand.running) return true;
        if (canFree)
        {
            beginHeld(BaseAction::Kind::FreeGenerator, site.generator, gate::kRepairSessionSeconds, "Working the flywheel loose");
        }
        else if (canPour)
        {
            beginHeld(BaseAction::Kind::PourFuel, site.generator, gate::kPourSeconds, "Pouring fuel");
        }
        return true;
    }

    // Who left by it.
    if (!mGate.leavers.empty() && mGate.open)
    {
        const glm::ivec2 marker = gate::insideGate(mMapGenerator.layoutData());
        if (glm::length(tileCentre(mTileMap, marker) - here) <= kReachTiles * tileSize)
        {
            const gate::Leaver& last = mGate.leavers.back();
            std::string text = last.name + " left by this gate on day " + std::to_string(last.day) + ".";
            if (mGate.leavers.size() > 1) text += " And " + std::to_string(mGate.leavers.size() - 1) + " before.";
            mProximityPrompt = "Marker   " + text;
            return pressed;
        }
    }
    return false;
}

void Game::finishGateAction()
{
    const BaseAction action = mBaseAction;
    mBaseAction = BaseAction{};
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    switch (action.kind)
    {
    case BaseAction::Kind::ReadClue:
        mBaseAction = action;
        finishReadClue();
        return;
    case BaseAction::Kind::FitFuse:
        if (mGate.fuseFitted || !InventorySystem::removeItem(inv, mItems.gateFuse, 1)) return;
        mGate.fuseFitted = true;
        creditGate("fuse");
        mInteractionMessage = "The fuse seats with a click. One lamp comes on.";
        std::cout << "[Gate] fuse fitted\n";
        break;
    case BaseAction::Kind::PourFuel:
        if (mGate.fuel >= gate::kFuelNeeded || !InventorySystem::removeItem(inv, mItems.fuelCan, 1)) return;
        ++mGate.fuel;
        creditGate("can");
        awardXp(Skill::Mechanics, skillrules::kXpRefuel);
        mInteractionMessage = "Fuel " + std::to_string(mGate.fuel) + " of " + std::to_string(gate::kFuelNeeded) + ".";
        std::cout << "[Gate] fuel " << mGate.fuel << '\n';
        break;
    case BaseAction::Kind::FreeGenerator:
        if (mGate.freed) return;
        mGate.repairs = std::min(gate::kRepairSessions, mGate.repairs + 1);
        creditGate("repair");
        awardXp(Skill::Mechanics, skillrules::kXpVehicleRepair);
        if (skillLevel(Skill::Mechanics) >= gate::kMechanicsToFree || mGate.repairs >= gate::kRepairSessions)
        {
            mGate.freed = true;
            creditGate("freed");
            mInteractionMessage = "The flywheel turns. The generator is free.";
        }
        else
        {
            mInteractionMessage = "It gives a little. " + std::to_string(mGate.repairs) + " of " +
                                  std::to_string(gate::kRepairSessions) + ".";
        }
        std::cout << "[Gate] generator work " << mGate.repairs << (mGate.freed ? " (freed)" : "") << '\n';
        break;
    default:
        return;
    }
    mInteractionMessageTimer = 3.0f;
    saveGate();
    noteGateSeen();
}

void Game::beginEscape()
{
    if (mRunState != RunState::Playing) return;
    mRunState = RunState::Out;
    mDeathStateTimer = 0.0;
    stopPlayerAtRunEnd();
    mBaseAction = BaseAction{};
    releaseMarkers();
    const int runIndex = mSaveManager.runNumber();
    mDeathCause = mCurrentCharacterName + " got out.";
    mRunEndLegacyLine = "The road goes on.";

    creditGate("left");
    mGate.leavers.push_back(gate::Leaver{mCurrentCharacterName, mCurrentDay, runIndex});

    // A companion at the gate goes too: gone, not dead.
    mOutCompanion.clear();
    const Entity companion = companionEntity();
    if (companion != kInvalidEntity && mWorld.hasComponent<Transform>(companion))
    {
        const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
        const SurvivorAI& ai = mWorld.getComponent<SurvivorAI>(companion);
        if (glm::length(centreOf(mWorld.getComponent<Transform>(companion)) - playerWorldCenter()) < 8.0f * tile &&
            ai.id >= 0 && ai.id < static_cast<int>(mSurvivors.size()))
        {
            syncSurvivorRecords();
            SurvivorRecord& record = mSurvivors[static_cast<std::size_t>(ai.id)];
            record.left = true;
            record.alive = false;
            record.buried = true;
            record.companion = false;
            record.deathDay = mCurrentDay;
            mOutCompanion = record.name;
            mWorld.destroyEntity(companion);
        }
    }

    syncLegacyToSave();
    CharacterHistoryRecord history = runEndHistory(false, runIndex);
    history.escaped = true;
    history.cause = "Got out by the gate";
    history.finalNote = "Got out by the gate on Day " + std::to_string(mCurrentDay) + ".";
    history.journal = finishJournal(journal::Kind::EndEscaped);
    mSaveManager.recordEscape();
    mSaveManager.addCharacterHistoryRecord(history);
    addRunEndMemoryEvent(WorldMemoryEventType::Other, runIndex);
    persistWorldForNextRun(runIndex);
    saveGate();

    const int lives = static_cast<int>(mSaveManager.characterHistory().size());
    const int days = mSaveManager.totalDaysSurvived();
    mOutLines.clear();
    mOutLines.push_back(std::to_string(lives) + (lives == 1 ? " life. " : " lives. ") + std::to_string(days) +
                        (days == 1 ? " day." : " days."));
    for (const std::string& line : gate::contributorLines(mGate)) mOutLines.push_back(line);
    if (!mOutCompanion.empty()) mOutLines.push_back(mOutCompanion + " went with them.");

    std::cout << "[Run End] Out - " << mCurrentCharacterName << " | Day " << mCurrentDay << " | " << lives << " lives, "
              << days << " days" << (mOutCompanion.empty() ? "" : " | with " + mOutCompanion) << '\n';
    for (const std::string& line : mOutLines) std::cout << "[Gate] " << line << '\n';
    mInteractionMessage.clear();
    mInteractionMessageTimer = 0.0f;
}

void Game::persistGate()
{
    if (!gatePlaced()) return;
    if (mGate.key == gate::KeyPlace::Loose)
    {
        // Outdoors nothing is kept: remember where it lies.
        for (const FloorItemRecord& pile : floorPiles(false))
        {
            for (const ContainerItemRecord& item : pile.items)
            {
                if (item.itemId == mItems.gateKey) mGate.keyTile = glm::ivec2(pile.x, pile.y);
            }
        }
    }
    if (mStand.running) mStand.running = false;
    mGate.burned = 0.0f;
    saveGate();
}

void Game::collectGateSprites()
{
    if (!gatePlaced()) return;
    const LayoutData& layout = mMapGenerator.layoutData();
    const LayoutData::GateSite& site = layout.gate;
    const float zoom = mCamera.zoom();
    const glm::vec2 viewport = mCamera.viewportSize();
    const float tileW = 64.0f * zoom;
    const float tileH = 32.0f * zoom;
    const float wallH = StructureView::kWallHeight * zoom;
    const auto onScreen = [&](const glm::vec2& ground)
    {
        return ground.x > -96.0f && ground.y > -96.0f && ground.x < viewport.x + 96.0f && ground.y < viewport.y + 160.0f;
    };
    const auto prop = [&](const char* name, const glm::ivec2& tile, float bias, const glm::vec4& color)
    {
        const glm::vec2 ground = mCamera.worldToScreen(tileCentre(mTileMap, tile));
        if (!onScreen(ground)) return;
        const float size = 80.0f * zoom;
        mStaticSprites.push_back(StaticSprite{glm::vec2(std::round(ground.x - 40.0f * zoom), std::round(ground.y - 64.0f * zoom)),
                                              glm::vec2(size), mSpriteSheet.uvRect(name), color,
                                              static_cast<float>(tile.x + tile.y) * 32.0f + bias});
    };

    // The gate: one panel a tile, a third of it sliding away each minute.
    const int stage = mGate.open ? 3 : gate::stage(mStand.opened);
    const char* panel = site.side <= 1 ? "gate_panel_y" : "gate_panel_x";
    for (std::size_t i = 0; i < site.gate.size(); ++i)
    {
        if (static_cast<int>(i) < stage) continue;
        const glm::ivec2& t = site.gate[i];
        const glm::vec2 ground = mCamera.worldToScreen(tileCentre(mTileMap, t));
        if (!onScreen(ground)) continue;
        const float left = std::round(ground.x - tileW * 0.5f);
        const float top = std::round(ground.y - tileH * 0.5f);
        mStaticSprites.push_back(StaticSprite{glm::vec2(left, top - wallH), glm::vec2(tileW, tileH + wallH), mSpriteSheet.uvRect(panel),
                                              glm::vec4(1.0f), static_cast<float>(t.x + t.y) * 32.0f});
    }

    // The generator shakes while it runs.
    glm::vec4 genColor(1.0f);
    if (mStand.running && static_cast<int>(mHudClock * 20.0f) % 2 == 0) genColor = glm::vec4(1.08f, 1.08f, 1.08f, 1.0f);
    prop("prop_generator", site.generator, 1.0f, genColor);
    prop("prop_gate_box", site.box, 1.0f, glm::vec4(1.0f));

    // Four lamps on the box: fuse, fuel, generator, key (dark, amber, lit).
    {
        const glm::vec2 ground = mCamera.worldToScreen(tileCentre(mTileMap, site.box));
        if (onScreen(ground))
        {
            const glm::vec4 dark(0.20f, 0.20f, 0.22f, 1.0f);
            const glm::vec4 amber(1.0f, 0.70f, 0.20f, 1.0f);
            const glm::vec4 lit(0.45f, 1.0f, 0.45f, 1.0f);
            const bool keyHere = gateKeyInHand() && glm::length(playerWorldCenter() - gateBoxWorld()) < 4.0f * static_cast<float>(mTileMap.tileWidth());
            const glm::vec4 lamps[4] = {
                mGate.fuseFitted ? lit : dark,
                mGate.fuel >= gate::kFuelNeeded ? lit : (mGate.fuel > 0 ? amber : dark),
                mGate.freed ? lit : (mGate.repairs > 0 ? amber : dark),
                mGate.key == gate::KeyPlace::Turned || keyHere ? lit : (mGate.key == gate::KeyPlace::Loose ? amber : dark)};
            const int dx[4] = {-7, -3, 1, 5};
            const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
            const float depth = static_cast<float>(site.box.x + site.box.y) * 32.0f + 1.5f;
            for (int i = 0; i < 4; ++i)
            {
                mStaticSprites.push_back(StaticSprite{glm::vec2(std::round(ground.x + static_cast<float>(dx[i] - 1) * zoom),
                                                                std::round(ground.y - 44.0f * zoom)),
                                                      glm::vec2(3.0f * zoom), white, lamps[i], depth});
            }
        }
    }

    // Who left by it.
    if (!mGate.leavers.empty() && mGate.open)
    {
        prop("prop_cairn", gate::insideGate(layout), 1.0f, glm::vec4(1.0f));
    }

    // The readables, where the room can be seen into.
    for (const clues::Readable& r : mReadables)
    {
        if (r.building > 0 && mStructureView.roofAlpha(r.building) > 0.5f) continue;
        const char* name = "prop_read_book";
        if (r.source == clues::Source::FridgeNote || r.source == clues::Source::GarageNote) name = "prop_read_note";
        else if (r.source == clues::Source::WallMap || r.source == clues::Source::DutyBoard) name = "prop_read_board";
        prop(name, r.tile, 2.0f, glm::vec4(1.0f));
    }
}
