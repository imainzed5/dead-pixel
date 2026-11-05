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

    GraveRecord record;
    record.name = mCurrentCharacterName;
    record.x = mDeathWorldPosition.x;
    record.y = mDeathWorldPosition.y;
    record.day = mCurrentDay;
    record.runIndex = mSaveManager.runNumber();
    record.cause = cause;

    if (mSaveManager.recordDeath(record))
    {
        mInteractionMessage = "Their grave has been marked.";
    }
    else
    {
        mInteractionMessage = "Death recorded, but legacy write failed.";
    }

    {
        CharacterHistoryRecord history;
        history.name = mCurrentCharacterName;
        history.runIndex = record.runIndex;
        history.dayStarted = 1;
        history.dayEnded = mCurrentDay;
        history.retired = false;
        history.cause = cause;
        history.locationTag = formatTileLocation(record.x, record.y, mTileMap);

        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            history.positiveTrait = traits.positiveName();
            history.negativeTrait = traits.negativeName();
            history.bloodlineTrait = traits.bloodlineName();
        }
        history.finalNote = "Died on Day " + std::to_string(mCurrentDay) + ": " + cause;
        mSaveManager.addCharacterHistoryRecord(history);

        WorldMemoryEventRecord event;
        event.type = WorldMemoryEventType::GravePlaced;
        event.tileX = static_cast<int>(std::floor(record.x / static_cast<float>(std::max(1, mTileMap.tileWidth()))));
        event.tileY = static_cast<int>(std::floor(record.y / static_cast<float>(std::max(1, mTileMap.tileHeight()))));
        event.day = mCurrentDay;
        event.runIndex = record.runIndex;
        event.detail = mCurrentCharacterName;
        mSaveManager.addWorldMemoryEvent(event);
    }

    // Save structures for next run
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
                // Save Supply Cache inventory
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

    // Save cumulative stats
    mSaveManager.addRunStats(mRunStats.kills, mCurrentDay);

    // Persist M8 world state (vehicles + active fires + burned tiles).
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
        mSaveManager.setBurnedTiles(mBurnedTiles);
        for (const auto& [tileX, tileY] : mBurnedTiles)
        {
            WorldMemoryEventRecord event;
            event.type = WorldMemoryEventType::BurnedTile;
            event.tileX = tileX;
            event.tileY = tileY;
            event.day = mCurrentDay;
            event.runIndex = mSaveManager.runNumber();
            event.detail = "burn scar";
            mSaveManager.addWorldMemoryEvent(event);
        }
    }

    // Delete mid-run checkpoint (permadeath)
    mSaveManager.deleteCheckpoint();
    mSaveManager.saveLootedPositions();

    // Save NPC survivors for next run
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
        mSaveManager.saveSurvivors(survivorRecords);
    }

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

    RetirementRecord retRecord;
    retRecord.name = mCurrentCharacterName;
    retRecord.x = mDeathWorldPosition.x;
    retRecord.y = mDeathWorldPosition.y;
    retRecord.day = mCurrentDay;
    retRecord.runIndex = mSaveManager.runNumber();
    mSaveManager.recordRetirement(retRecord);

    {
        CharacterHistoryRecord history;
        history.name = mCurrentCharacterName;
        history.runIndex = retRecord.runIndex;
        history.dayStarted = 1;
        history.dayEnded = mCurrentDay;
        history.retired = true;
        history.cause = customCause.empty() ? "Psychological retirement" : customCause;
        history.locationTag = formatTileLocation(retRecord.x, retRecord.y, mTileMap);

        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            history.positiveTrait = traits.positiveName();
            history.negativeTrait = traits.negativeName();
            history.bloodlineTrait = traits.bloodlineName();
        }
        history.finalNote = customCause.empty()
            ? ("They stopped on Day " + std::to_string(mCurrentDay) + ".")
            : (customCause + ".");
        mSaveManager.addCharacterHistoryRecord(history);

        WorldMemoryEventRecord event;
        event.type = WorldMemoryEventType::RetirementPlaced;
        event.tileX = static_cast<int>(std::floor(retRecord.x / static_cast<float>(std::max(1, mTileMap.tileWidth()))));
        event.tileY = static_cast<int>(std::floor(retRecord.y / static_cast<float>(std::max(1, mTileMap.tileHeight()))));
        event.day = mCurrentDay;
        event.runIndex = retRecord.runIndex;
        event.detail = mCurrentCharacterName;
        mSaveManager.addWorldMemoryEvent(event);
    }

    // Save structures for next run
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

    // Save cumulative stats
    mSaveManager.addRunStats(mRunStats.kills, mCurrentDay);

    // Persist M8 world state (vehicles + active fires + burned tiles).
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
        mSaveManager.setBurnedTiles(mBurnedTiles);
        for (const auto& [tileX, tileY] : mBurnedTiles)
        {
            WorldMemoryEventRecord event;
            event.type = WorldMemoryEventType::BurnedTile;
            event.tileX = tileX;
            event.tileY = tileY;
            event.day = mCurrentDay;
            event.runIndex = mSaveManager.runNumber();
            event.detail = "burn scar";
            mSaveManager.addWorldMemoryEvent(event);
        }
    }

    // Delete mid-run checkpoint (retirement ends the run)
    mSaveManager.deleteCheckpoint();
    mSaveManager.saveLootedPositions();

    // Save NPC survivors for next run
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
        mSaveManager.saveSurvivors(survivorRecords);
    }

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
