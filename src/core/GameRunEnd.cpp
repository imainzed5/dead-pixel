// End of a run: death and retirement, and what the world remembers of them.
#include "core/GameShared.h"

using namespace gameshared;

void Game::beginDeathState(const std::string& cause)
{
    if (mRunState == RunState::Dead)
    {
        return;
    }

    mRunState = RunState::Dead;
    mDeathStateTimer = 0.0;
    mDeathCause = cause;
    mAudioManager.playSound(SoundId::Death, 0.8f);
    stopPlayerAtRunEnd();
    // Recording the grave or retirement advances the save's run number, so
    // everything about this run uses the index it had while it was played.
    const int runIndex = mSaveManager.runNumber();

    GraveRecord record;
    record.name = mCurrentCharacterName;
    record.x = mDeathWorldPosition.x;
    record.y = mDeathWorldPosition.y;
    record.day = mCurrentDay;
    record.runIndex = runIndex;
    record.cause = cause;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        for (const auto& slot : mWorld.getComponent<Inventory>(mPlayerEntity).slots)
        {
            if (slot.itemId >= 0 && slot.count > 0)
            {
                record.items.emplace_back(slot.itemId, slot.count);
            }
        }
    }
    const bool infectedBite = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Wounds>(mPlayerEntity) &&
                              hasInfectedWound(mWorld.getComponent<Wounds>(mPlayerEntity));
    record.risen = legacy::shouldRise(cause, infectedBite);

    // Earlier graves first (packs taken, risen put down), then this one.
    syncLegacyToSave();
    mInteractionMessage = mSaveManager.recordDeath(record)
        ? "Their grave has been marked."
        : "Death recorded, but legacy write failed.";

    CharacterHistoryRecord history = runEndHistory(false, runIndex);
    history.cause = cause;
    history.finalNote = "Died on Day " + std::to_string(mCurrentDay) + ": " + cause;
    mSaveManager.addCharacterHistoryRecord(history);
    addRunEndMemoryEvent(WorldMemoryEventType::GravePlaced, runIndex);

    persistWorldForNextRun(runIndex);

    // Balance instrumentation
    std::cout << "[Run End] Death - " << mCurrentCharacterName
              << " | Day " << mCurrentDay << " | Cause: " << cause
              << " | Kills: " << mRunStats.kills
              << " | Items: " << mRunStats.itemsLooted
              << " | Structures: " << mRunStats.structuresBuilt
              << " | Distance: " << static_cast<int>(mRunStats.distanceWalked) << "px"
              << '\n';

    mInteractionMessageTimer = 5.0f;
}

void Game::beginRetirementState(const std::string& customCause)
{
    if (mRunState == RunState::Retired || mRunState == RunState::Dead)
    {
        return;
    }

    mRunState = RunState::Retired;
    mDeathStateTimer = 0.0;
    mDeathCause = customCause.empty()
        ? ("Gave up on Day " + std::to_string(mCurrentDay))
        : customCause;
    stopPlayerAtRunEnd();
    // Recording the grave or retirement advances the save's run number, so
    // everything about this run uses the index it had while it was played.
    const int runIndex = mSaveManager.runNumber();

    RetirementRecord record;
    record.name = mCurrentCharacterName;
    record.x = mDeathWorldPosition.x;
    record.y = mDeathWorldPosition.y;
    record.day = mCurrentDay;
    record.runIndex = runIndex;
    syncLegacyToSave();
    mSaveManager.recordRetirement(record);

    CharacterHistoryRecord history = runEndHistory(true, runIndex);
    history.cause = customCause.empty() ? "Psychological retirement" : customCause;
    history.finalNote = customCause.empty()
        ? ("They stopped on Day " + std::to_string(mCurrentDay) + ".")
        : (customCause + ".");
    mSaveManager.addCharacterHistoryRecord(history);
    addRunEndMemoryEvent(WorldMemoryEventType::RetirementPlaced, runIndex);

    // They stay on in town as a survivor, keeping to themselves.
    const glm::ivec2 retireTile = worldToTile(mTileMap, mDeathWorldPosition);
    SurvivorRecord retiree;
    retiree.name = mCurrentCharacterName;
    retiree.alive = true;
    retiree.spawnDay = mCurrentDay;
    retiree.x = mDeathWorldPosition.x;
    retiree.y = mDeathWorldPosition.y;
    retiree.aggression = 0.1f;
    retiree.cooperation = 0.85f;
    retiree.competence = std::clamp(0.3f + static_cast<float>(mCurrentDay) * 0.05f, 0.3f, 0.9f);
    retiree.homeDistrictId = mTileMap.getDistrictId(retireTile.x, retireTile.y);

    persistWorldForNextRun(runIndex, &retiree);

    // Balance instrumentation
    std::cout << "[Run End] Retirement - " << mCurrentCharacterName
              << " | Day " << mCurrentDay
              << " | Kills: " << mRunStats.kills
              << " | Items: " << mRunStats.itemsLooted
              << " | Structures: " << mRunStats.structuresBuilt
              << " | Distance: " << static_cast<int>(mRunStats.distanceWalked) << "px"
              << '\n';

    mInteractionMessage = "They couldn't go on.";
    mInteractionMessageTimer = 5.0f;
}

void Game::stopPlayerAtRunEnd()
{
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& transform = mWorld.getComponent<Transform>(mPlayerEntity);
        mDeathWorldPosition = glm::vec2(transform.x, transform.y);
    }

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
        velocity.dx = 0.0f;
        velocity.dy = 0.0f;
    }
}

CharacterHistoryRecord Game::runEndHistory(bool retired, int runIndex) const
{
    CharacterHistoryRecord history;
    history.name = mCurrentCharacterName;
    history.runIndex = runIndex;
    history.dayStarted = 1;
    history.dayEnded = mCurrentDay;
    history.retired = retired;
    history.locationTag = formatTileLocation(mDeathWorldPosition.x, mDeathWorldPosition.y, mTileMap);

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        history.positiveTrait = traits.positiveName();
        history.negativeTrait = traits.negativeName();
        history.bloodlineTrait = traits.bloodlineName();
    }
    return history;
}

void Game::addRunEndMemoryEvent(WorldMemoryEventType type, int runIndex)
{
    const glm::ivec2 tile = worldToTile(mTileMap, mDeathWorldPosition);
    WorldMemoryEventRecord event;
    event.type = type;
    event.tileX = tile.x;
    event.tileY = tile.y;
    event.day = mCurrentDay;
    event.runIndex = runIndex;
    event.detail = mCurrentCharacterName;
    mSaveManager.addWorldMemoryEvent(event);
}

// Everything the next run inherits: structures and caches, cars, fires and
// burn scars, survivors, looted spots and cumulative stats. The mid-run
// checkpoint is deleted (death is permanent; retirement ends the run).
void Game::persistWorldForNextRun(int runIndex, const SurvivorRecord* retiree)
{
    {
        std::vector<StructureRecord> structRecords;
        mWorld.forEach<Structure, Transform>(
            [&](Entity e, Structure& s, Transform& t)
            {
                StructureRecord rec;
                rec.type = static_cast<int>(s.type);
                rec.x = t.x;
                rec.y = t.y;
                rec.condition = static_cast<int>(s.condition);
                rec.hpRatio = s.maxHp > 0.0f ? s.hp / s.maxHp : 0.0f;
                if (s.type == StructureType::SupplyCache && mWorld.hasComponent<Inventory>(e))
                {
                    const Inventory& inv = mWorld.getComponent<Inventory>(e);
                    for (const auto& slot : inv.slots)
                    {
                        if (slot.itemId >= 0 && slot.count > 0)
                        {
                            rec.items.emplace_back(slot.itemId, slot.count);
                        }
                    }
                }
                structRecords.push_back(rec);
            });
        mSaveManager.saveStructures(structRecords);
    }

    mSaveManager.addRunStats(mRunStats.kills, mCurrentDay);

    {
        std::vector<VehicleRecord> vehicleRecords;
        mWorld.forEach<Vehicle, Transform>(
            [&](Entity e, Vehicle& vehicle, Transform& transform)
            {
                VehicleRecord record;
                record.x = transform.x;
                record.y = transform.y;
                record.condition = vehicle.condition;
                record.fuel = vehicle.fuel;
                record.maxFuel = vehicle.maxFuel;
                record.engineOn = false;

                if (mWorld.hasComponent<Inventory>(e))
                {
                    const Inventory& trunk = mWorld.getComponent<Inventory>(e);
                    for (const auto& slot : trunk.slots)
                    {
                        if (slot.itemId >= 0 && slot.count > 0)
                        {
                            record.items.emplace_back(slot.itemId, slot.count);
                        }
                    }
                }

                vehicleRecords.push_back(record);
            });
        mSaveManager.setVehicleRecords(std::move(vehicleRecords));
    }

    {
        std::vector<FireTileRecord> fireRecords;
        mWorld.forEach<Fire, Transform>(
            [&](Entity, Fire& fire, Transform& transform)
            {
                const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(transform.x, transform.y));
                FireTileRecord record;
                record.tileX = tile.x;
                record.tileY = tile.y;
                record.intensity = fire.intensity;
                record.lifeRemaining = fire.lifeRemaining;
                fireRecords.push_back(record);
            });
        mSaveManager.setFireTileRecords(std::move(fireRecords));
    }

    // Burn scars: earlier runs' scars are already in world memory, so only
    // tiles that burned during this run become new events.
    mSaveManager.setBurnedTiles(mBurnedTiles);
    for (std::size_t i = std::min(mBurnedTilesAtRunStart, mBurnedTiles.size()); i < mBurnedTiles.size(); ++i)
    {
        WorldMemoryEventRecord event;
        event.type = WorldMemoryEventType::BurnedTile;
        event.tileX = mBurnedTiles[i].first;
        event.tileY = mBurnedTiles[i].second;
        event.day = mCurrentDay;
        event.runIndex = runIndex;
        event.detail = "burn scar";
        mSaveManager.addWorldMemoryEvent(event);
    }

    mSaveManager.deleteCheckpoint();
    mSaveManager.saveLootedPositions();

    {
        std::vector<SurvivorRecord> survivorRecords;
        mWorld.forEach<SurvivorAI, Transform, Health>(
            [&](Entity, SurvivorAI& ai, Transform& t, Health& h)
            {
                SurvivorRecord sr;
                sr.name = ai.name;
                sr.alive = ai.alive && h.current > 0.0f;
                sr.spawnDay = ai.spawnDay;
                sr.deathDay = sr.alive ? 0 : mCurrentDay;
                sr.x = t.x;
                sr.y = t.y;
                sr.hunger = ai.hunger;
                sr.thirst = ai.thirst;
                sr.aggression = ai.personality.aggression;
                sr.cooperation = ai.personality.cooperation;
                sr.competence = ai.personality.competence;
                sr.homeDistrictId = ai.homeDistrictId;
                survivorRecords.push_back(sr);
            });
        if (retiree != nullptr)
        {
            survivorRecords.push_back(*retiree);
        }
        mSaveManager.saveSurvivors(survivorRecords);
    }
}
