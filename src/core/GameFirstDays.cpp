// The first three days of a run: the house the run starts in is home, its
// taps run until day 3, the objective panel shows a plan for each day and
// night, and after dark the dead come to the door, more of them each night.
// The rules live in world/FirstDays; this is where they meet the game.
#include "core/GameShared.h"

#include <random>

#include "world/EntityFactory.h"

using namespace gameshared;

namespace
{
constexpr float kDayCardSeconds = 6.0f;

// Callers come from the far wanderers, or rise out of sight when there are
// too few of them.
constexpr float kCallerMinTiles = 16.0f;
constexpr float kCallerMaxTiles = 45.0f;
constexpr float kCallerSpawnMinTiles = 20.0f;
constexpr float kCallerSpawnMaxTiles = 28.0f;

bool hasTile(const std::vector<std::pair<int, int>>& tiles, int x, int y)
{
    return std::find(tiles.begin(), tiles.end(), std::make_pair(x, y)) != tiles.end();
}

glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}
} // namespace

void Game::setupHome()
{
    const FirstDaysRecord fresh;
    mFirstDays = FirstDaysState{};
    mFirstDays.record = fresh;
    mFirstDays.homeBuilding = mTileMap.getBuildingId(mSpawnData.playerStart.x, mSpawnData.playerStart.y);
    if (mFirstDays.homeBuilding <= 0)
    {
        return;
    }

    // The home's doors: those with the home's floor on one side.
    const glm::ivec2 sides[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const Doors::Door& door : mDoors.all())
    {
        for (const glm::ivec2& side : sides)
        {
            const glm::ivec2 next = door.tile + side;
            if (mTileMap.inBounds(next.x, next.y) && !mTileMap.isSolid(next.x, next.y) &&
                mTileMap.getBuildingId(next.x, next.y) == mFirstDays.homeBuilding)
            {
                mFirstDays.homeDoors.push_back(door.tile);
                break;
            }
        }
    }

    // Its cupboards, fridges and wardrobes.
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity, Interactable& inter, Inventory&, Transform& t)
        {
            if (inter.type != InteractableType::Container) return;
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x, t.y));
            if (mTileMap.hasFurniture(tile.x, tile.y) && mTileMap.getBuildingId(tile.x, tile.y) == mFirstDays.homeBuilding)
            {
                mFirstDays.homeContainers.emplace_back(tile.x, tile.y);
            }
        });
}

void Game::restoreFirstDays(const FirstDaysRecord& record)
{
    mFirstDays.record = record;
    mFirstDays.cardTimer = 0.0f;
}

FirstDaysRecord Game::firstDaysRecord() const
{
    return mFirstDays.record;
}

void Game::placeHomeKit()
{
    if (mFirstDays.homeBuilding <= 0)
    {
        return;
    }

    struct Spot
    {
        Entity entity;
        glm::ivec2 tile;
        FurnitureLoot loot;
        int used = 0;
    };
    std::vector<Spot> spots;
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable& inter, Inventory&, Transform& t)
        {
            if (inter.type != InteractableType::Container) return;
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x, t.y));
            if (!hasTile(mFirstDays.homeContainers, tile.x, tile.y)) return;
            // Taken from before a resume: what was in it went with you.
            if (hasTile(mFirstDays.record.taken, tile.x, tile.y)) return;
            // Already remembered this life: the kit is in it, or was moved out.
            const ContainerRecord* remembered = mSaveManager.containerRecord(tile.x, tile.y);
            if (remembered != nullptr && remembered->runIndex == mSaveManager.runNumber()) return;
            const auto piece = std::find_if(mFurniture.begin(), mFurniture.end(),
                                            [&](const FurniturePiece& p) { return p.tile == tile; });
            spots.push_back(Spot{e, tile, piece != mFurniture.end() ? furniture::info(piece->kind).loot : FurnitureLoot::None});
        });
    if (spots.empty())
    {
        std::cout << "[World] home: building " << mFirstDays.homeBuilding << ", nowhere to keep the kit\n";
        return;
    }

    // Each part of the kit goes where it belongs if the house has such a
    // place, and spreads out rather than piling into one cupboard.
    const auto place = [&](std::initializer_list<FurnitureLoot> prefer, std::initializer_list<std::pair<int, int>> items)
    {
        Spot* best = nullptr;
        int bestScore = 0;
        for (Spot& spot : spots)
        {
            int rank = static_cast<int>(prefer.size());
            int i = 0;
            for (FurnitureLoot loot : prefer)
            {
                if (spot.loot == loot)
                {
                    rank = i;
                    break;
                }
                ++i;
            }
            const int score = rank * 10 + spot.used;
            if (best == nullptr || score < bestScore)
            {
                best = &spot;
                bestScore = score;
            }
        }
        Inventory& inv = mWorld.getComponent<Inventory>(best->entity);
        for (const auto& [itemId, count] : items)
        {
            if (count > 0) InventorySystem::addItem(inv, mItemDatabase, itemId, count);
        }
        ++best->used;
    };

    // Easier modes stock it better, the swarms worse.
    int cans = 2;
    int water = 1;
    int empties = 2;
    switch (saveThreatModeFromIndex(mTitleModeIndex))
    {
    case SaveThreatMode::Stalker:
        cans = 3;
        water = 2;
        break;
    case SaveThreatMode::OverwhelmingSwarm:
    case SaveThreatMode::Hybrid:
        cans = 1;
        water = 0;
        empties = 3;
        break;
    default:
        break;
    }

    using L = FurnitureLoot;
    place({L::Storage, L::Workshop, L::Bedroom, L::Living}, {{mItems.hammer, 1}, {mItems.woodPlanks, 4}, {mItems.nails, 12}});
    place({L::Fridge, L::Kitchen}, {{mItems.cannedFood, cans}, {mItems.bread, 1}});
    place({L::Kitchen, L::Fridge}, {{mItems.waterBottle, water}, {mItems.emptyBottle, empties}});
    place({L::Bathroom, L::Bedroom}, {{mItems.bandage, 1}, {mItems.painkillers, 1}});
    // Something to write in (phase 10: the journal is kept while it is carried).
    place({L::Bedroom, L::Living, L::Storage}, {{mItems.notebook, 1}});

    int used = 0;
    for (const Spot& spot : spots) used += spot.used > 0 ? 1 : 0;
    std::cout << "[World] home: building " << mFirstDays.homeBuilding << ", " << mFirstDays.homeDoors.size()
              << " doors, " << mFirstDays.homeContainers.size() << " containers, kit in " << used << "\n";
}

bool Game::playerAtHome() const
{
    if (mFirstDays.homeBuilding <= 0 || mPlayerEntity == kInvalidEntity) return false;
    const glm::ivec2 tile = playerTile();
    return mTileMap.getBuildingId(tile.x, tile.y) == mFirstDays.homeBuilding;
}

void Game::noteContainerSearched(Entity container, bool took)
{
    if (!mWorld.hasComponent<Transform>(container)) return;
    const Transform& t = mWorld.getComponent<Transform>(container);
    const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x, t.y));
    if (!hasTile(mFirstDays.homeContainers, tile.x, tile.y)) return;
    FirstDaysRecord& record = mFirstDays.record;
    if (!hasTile(record.searched, tile.x, tile.y)) record.searched.emplace_back(tile.x, tile.y);
    if (took && !hasTile(record.taken, tile.x, tile.y)) record.taken.emplace_back(tile.x, tile.y);
}

firstdays::Facts Game::firstDaysFacts()
{
    firstdays::Facts facts;
    for (const auto& [x, y] : mFirstDays.homeContainers)
    {
        ++facts.searchable;
        if (hasTile(mFirstDays.record.searched, x, y)) ++facts.searched;
    }
    // Every way in: doors boarded or blocked, windows boarded or at least
    // curtained (day 1 and the nights); everything boarded or blocked (day 3).
    // A broken door with nothing across it cannot be closed, but still counts.
    for (const WayIn& way : waysIn(mFirstDays.homeBuilding))
    {
        ++facts.ways;
        if (way.boarded || (way.window && way.covered && way.closed)) ++facts.waysClosed;
        if (way.boarded) ++facts.waysBoarded;
    }
    facts.usedTap = mFirstDays.record.usedTap;
    facts.sleptTonight = mFirstDays.record.sleptNight > 0 &&
                         mFirstDays.record.sleptNight == firstdays::nightIndex(mCurrentDay, mGameHours);
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        facts.waterBottles = InventorySystem::countItem(inv, mItems.waterBottle);
        facts.planks = InventorySystem::countItem(inv, mItems.woodPlanks);
        for (const auto& slot : inv.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* item = mItemDatabase.find(slot.itemId);
            if (item != nullptr && item->category == ItemCategory::Food) facts.food += slot.count;
        }
    }
    mWorld.forEach<Structure>(
        [&](Entity, Structure& structure)
        {
            if (structure.type == StructureType::RainCollector) facts.rainCollector = true;
        });
    return facts;
}

bool Game::handleTapInteraction(const ProximityPrompt& entityPrompt)
{
    if (entityPrompt.entity == kInvalidEntity || !mWorld.hasComponent<Transform>(entityPrompt.entity) ||
        !mWorld.hasComponent<Interactable>(entityPrompt.entity) ||
        mWorld.getComponent<Interactable>(entityPrompt.entity).type != InteractableType::Container)
    {
        return false;
    }
    const Transform& t = mWorld.getComponent<Transform>(entityPrompt.entity);
    const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x, t.y));
    const auto piece = std::find_if(mFurniture.begin(), mFurniture.end(),
                                    [&](const FurniturePiece& p) { return p.tile == tile; });
    if (piece == mFurniture.end() || (piece->kind != FurnitureKind::Counter && piece->kind != FurnitureKind::Sink))
    {
        return false;
    }

    const bool running = firstdays::tapsRunning(mCurrentDay, mGameHours);
    if (!running && mFirstDays.tapsDryNoted)
    {
        return false; // nothing more to learn here
    }
    mProximityPrompt = entityPrompt.text + (running ? "   R: Use the tap" : "   R: Try the tap");
    if (!mInput.wasKeyPressed(SDL_SCANCODE_R) || mPlayerEntity == kInvalidEntity)
    {
        return false;
    }

    if (!running)
    {
        mFirstDays.tapsDryNoted = true;
        mInteractionMessage = "The tap coughs and gives nothing. The water's off.";
        mInteractionMessageTimer = 3.5f;
        return true;
    }

    // Drink your fill, and fill every empty bottle you carry.
    int filled = 0;
    if (mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const int empties = InventorySystem::countItem(inv, mItems.emptyBottle);
        if (empties > 0)
        {
            InventorySystem::removeItem(inv, mItems.emptyBottle, empties);
            filled = InventorySystem::addItem(inv, mItemDatabase, mItems.waterBottle, empties).added;
            if (filled < empties)
            {
                InventorySystem::addItem(inv, mItemDatabase, mItems.emptyBottle, empties - filled);
            }
        }
    }
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        needs.thirst = needs.maxThirst;
    }
    mFirstDays.record.usedTap = true;
    mNoiseModel.emitNoise(playerWorldCenter(), NoiseTier::Soft, 1.0f, mPlayerEntity);
    mAudioManager.playSound(SoundId::FoodEat, 0.45f);

    if (filled > 0)
    {
        mInteractionMessage = "You drink your fill and fill " + std::to_string(filled) +
                              (filled == 1 ? " bottle." : " bottles.");
    }
    else
    {
        mInteractionMessage = "You drink your fill. No empty bottles to fill.";
    }
    if (firstdays::pressureLow(mCurrentDay, mGameHours))
    {
        mInteractionMessage += " The pressure's weak.";
    }
    mInteractionMessageTimer = 3.5f;
    return true;
}

void Game::sendNightCallers(int count)
{
    if (count <= 0 || mPlayerEntity == kInvalidEntity) return;
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 player = playerWorldCenter();
    // At home they make for you through the door; out in the open they
    // close in on where you are.
    const glm::vec2 target = player;

    struct Candidate
    {
        Entity entity;
        float distance;
    };
    std::vector<Candidate> candidates;
    mWorld.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI& ai, Transform& t)
        {
            if (ai.nightCaller || ai.state != ZombieState::Wander || mWorld.hasComponent<LegacyLink>(e)) return;
            if (mWorld.hasComponent<Health>(e) && mWorld.getComponent<Health>(e).current <= 0.0f) return;
            const float distance = glm::length(centreOf(t) - player) / tile;
            if (distance >= kCallerMinTiles && distance <= kCallerMaxTiles) candidates.push_back({e, distance});
        });
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });

    int sent = 0;
    for (const Candidate& candidate : candidates)
    {
        if (sent >= count) break;
        ZombieAI& ai = mWorld.getComponent<ZombieAI>(candidate.entity);
        ZombieAISystem::sendTo(ai, target, true);
        ai.nightCaller = true;
        ++sent;
    }

    // Too few about: the rest come out of the dark.
    std::mt19937 rng(mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(mCurrentDay * 7919 + sent * 104729) ^
                     static_cast<std::uint32_t>(mGameHours * 60.0));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float health = 50.0f * toneTuning(saveTonePresetFromIndex(mTitleToneIndex)).zombieHealthScale;
    // Later nights send worse (phase 4): from night 4 one in four that comes
    // out of the dark is a Runner; from night 6 one is a Brute.
    const int night = firstdays::nightIndex(mCurrentDay, mGameHours);
    bool bruteSent = false;
    mWorld.forEach<ZombieAI>([&](Entity, ZombieAI& ai) { if (ai.nightCaller && ai.kind == ZombieKind::Brute) bruteSent = true; });
    for (int attempt = 0; sent < count && attempt < 200; ++attempt)
    {
        const float angle = unit(rng) * 6.2831853f;
        const float distance = (kCallerSpawnMinTiles + unit(rng) * (kCallerSpawnMaxTiles - kCallerSpawnMinTiles)) * tile;
        const glm::vec2 at = player + glm::vec2(std::cos(angle), std::sin(angle)) * distance;
        const glm::ivec2 spawnTile = worldToTile(mTileMap, at);
        if (!mTileMap.inBounds(spawnTile.x, spawnTile.y) || mTileMap.isSolid(spawnTile.x, spawnTile.y) ||
            mTileMap.getBuildingId(spawnTile.x, spawnTile.y) > 0)
        {
            continue;
        }
        ZombieKind kind = ZombieKind::Shambler;
        if (night >= 6 && !bruteSent)
        {
            kind = ZombieKind::Brute;
            bruteSent = true;
        }
        else if (night >= 4 && sent % 4 == 3)
        {
            kind = ZombieKind::Runner;
        }
        const glm::vec2 topLeft((static_cast<float>(spawnTile.x) + 0.5f) * tile - kSpriteSize * 0.5f,
                                (static_cast<float>(spawnTile.y) + 0.5f) * tile - kSpriteSize * 0.5f);
        EntityFactory::ZombieParams params = zombieParams(kind, topLeft, rng);
        if (kind == ZombieKind::Shambler) params.health = health;
        const Entity zombie = EntityFactory::spawnZombie(mWorld, mSpriteSheet, params);
        ZombieAI& ai = mWorld.getComponent<ZombieAI>(zombie);
        ZombieAISystem::sendTo(ai, target, true);
        ai.nightCaller = true;
        ++sent;
    }
    std::cout << "[Night " << firstdays::nightIndex(mCurrentDay, mGameHours) << "] " << sent << " callers set out ("
              << (playerAtHome() ? "home" : "abroad") << ")\n";
}

void Game::updateFirstDays(float dtSeconds)
{
    if (mPlayerEntity == kInvalidEntity) return;

    // A caller that reaches the house picks the weakest way in near it:
    // the fewest boards, glass before boards, nothing across it; ties to the
    // nearest.
    if (mFirstDays.homeBuilding > 0 && playerAtHome())
    {
        std::vector<WayIn> ways;
        bool waysKnown = false;
        const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
        mWorld.forEach<ZombieAI, Transform>(
            [&](Entity, ZombieAI& ai, Transform& t)
            {
                if (!ai.nightCaller || ai.entryChosen || ai.state == ZombieState::Chase || ai.state == ZombieState::Attack) return;
                const glm::vec2 here = centreOf(t);
                if (!waysKnown)
                {
                    ways = waysIn(mFirstDays.homeBuilding);
                    waysKnown = true;
                }
                const WayIn* best = nullptr;
                float bestDistance = 0.0f;
                for (const WayIn& way : ways)
                {
                    const glm::vec2 at = (glm::vec2(way.tile) + glm::vec2(0.5f)) * tile;
                    const float distance = glm::length(at - here) / tile;
                    if (distance > 12.0f) continue;
                    if (best == nullptr || way.strength < best->strength - 0.5f ||
                        (std::abs(way.strength - best->strength) <= 0.5f && distance < bestDistance))
                    {
                        best = &way;
                        bestDistance = distance;
                    }
                }
                if (best == nullptr) return;
                ai.entryChosen = true;
                ZombieAISystem::sendTo(ai, (glm::vec2(best->tile) + glm::vec2(0.5f)) * tile, true);
            });
    }

    FirstDaysRecord& record = mFirstDays.record;
    const bool sleeping = mWorld.hasComponent<Sleeping>(mPlayerEntity);
    const int night = firstdays::nightIndex(mCurrentDay, mGameHours);

    // A night slept at home.
    if (sleeping && night > 0 && playerAtHome())
    {
        record.sleptNight = night;
    }

    // The night's callers, group by group. A horde night has none: the
    // crowd is the night's danger, and quiet is the answer to it.
    if (night > 0 && !isHordeNight(night))
    {
        if (record.night != night)
        {
            record.night = night;
            record.groupsSent = 0;
        }
        // Night 1 is the lesson (boards hold, and they give up), the same in
        // every mode; from night 2 the mode and tone set how many come.
        const float scale = night == 1 ? 1.0f
            : std::clamp(toneTuning(saveTonePresetFromIndex(mTitleToneIndex)).zombieCountScale *
                             threatDirectorTuning(modeRules()).migrationScale,
                         0.75f, 1.5f);
        const int callers = std::max(1, static_cast<int>(std::lround(firstdays::nightCallers(night) * scale)));
        const std::vector<firstdays::CallerGroup> schedule = firstdays::callerSchedule(callers);
        const double dusk = firstdays::duskHour(mGameHours >= 12.0 ? mCurrentDay : mCurrentDay - 1);
        const double sinceDusk = mGameHours >= dusk ? mGameHours - dusk : mGameHours + 24.0 - dusk;
        while (record.groupsSent < static_cast<int>(schedule.size()) &&
               sinceDusk >= schedule[static_cast<std::size_t>(record.groupsSent)].hoursAfterDusk)
        {
            sendNightCallers(schedule[static_cast<std::size_t>(record.groupsSent)].count);
            ++record.groupsSent;
        }
    }
    else if (record.night > 0)
    {
        // First light: the callers who never got in drift away.
        record.night = 0;
        record.groupsSent = 0;
        mWorld.forEach<ZombieAI>(
            [&](Entity, ZombieAI& ai)
            {
                if (!ai.nightCaller) return;
                ai.nightCaller = false;
                if (ai.state == ZombieState::Investigate)
                {
                    ai.state = ZombieState::Wander;
                    ai.migrating = false;
                    ai.path.clear();
                    ai.stateTimer = 0.0f;
                }
            });
    }

    // The water goes off.
    if (!firstdays::tapsRunning(mCurrentDay, mGameHours) && !mFirstDays.waterOffAnnounced)
    {
        mFirstDays.waterOffAnnounced = true;
        if (!sleeping && mCurrentDay == firstdays::kTapsDieDay)
        {
            mInteractionMessage = "Somewhere in the walls a pipe shudders. The water's off.";
            mInteractionMessageTimer = 4.0f;
        }
    }

    // A card at dawn, once you are up.
    if (!sleeping && mCurrentDay > record.cardDay && night == 0)
    {
        record.cardDay = mCurrentDay;
        mFirstDays.cardTimer = kDayCardSeconds;
    }
    mFirstDays.cardTimer = std::max(0.0f, mFirstDays.cardTimer - dtSeconds);
}
