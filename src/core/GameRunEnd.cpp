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
    releaseMarkers(); // a marker carried goes back to its grave (phase 10)
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
        record.items = InventorySystem::itemsUnloaded(mWorld.getComponent<Inventory>(mPlayerEntity), mItemDatabase);
        // A notebook in the pack keeps what they knew of the way out (phase 9).
        if (InventorySystem::countItem(mWorld.getComponent<Inventory>(mPlayerEntity), mItems.notebook) > 0 &&
            (mLeads != 0 || mGateSeen.valid))
        {
            record.notebook = true;
            record.leads = mLeads;
            record.seen = mGateSeen;
        }
    }
    // Who they were, for the journals of phase 10: their traits and their
    // two best skills.
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        for (PositiveTrait trait : traits.positives) record.positiveTraits.emplace_back(traitrules::key(trait));
        for (NegativeTrait trait : traits.negatives) record.negativeTraits.emplace_back(traitrules::key(trait));
    }
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Skills>(mPlayerEntity))
    {
        const Skills& skills = mWorld.getComponent<Skills>(mPlayerEntity);
        std::vector<std::pair<int, int>> ranked;
        for (std::size_t i = 0; i < kSkillCount; ++i)
        {
            if (skills.level[i] > 0) ranked.emplace_back(static_cast<int>(i), skills.level[i]);
        }
        std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (ranked.size() > 2) ranked.resize(2);
        record.skills = ranked;
    }
    // A turning always rises (Hopeless).
    const bool infectedBite = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Wounds>(mPlayerEntity) &&
                              (hasInfectedWound(mWorld.getComponent<Wounds>(mPlayerEntity)) || cause.rfind("Turned", 0) == 0);
    record.risen = legacy::shouldRise(cause, infectedBite);
    mRunEndLegacyLine = record.risen          ? "They will not stay down. Whoever comes next will meet them."
                      : !record.items.empty() ? "Their pack will wait by the grave for whoever comes next."
                                              : "A grave will mark where they fell.";

    // Earlier graves first (packs taken, risen put down), then this one.
    syncLegacyToSave();
    mInteractionMessage = mSaveManager.recordDeath(record)
        ? "Their grave has been marked."
        : "Death recorded, but legacy write failed.";

    CharacterHistoryRecord history = runEndHistory(false, runIndex);
    history.cause = cause;
    history.finalNote = "Died on Day " + std::to_string(mCurrentDay) + ": " + cause;
    // The last entry: hunger, thirst and a fever end slowly; the rest stop the writing.
    {
        std::string lower = cause;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const bool slow = legacy::hungerDeath(lower) || lower.find("infection") != std::string::npos || lower.find("turned") != std::string::npos;
        history.journal = finishJournal(slow ? journal::Kind::EndSlow : journal::Kind::EndSudden);
    }
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
    mRunEndLegacyLine = mStayedForGood ? "They will keep the house. Whoever comes next can knock."
                                       : "They will live on in town, keeping to themselves.";
    releaseMarkers();
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
        : (customCause.back() == '.' ? customCause : customCause + ".");
    history.chosen = mStayedForGood;
    // A retirement always leaves its last words: the life in one sentence.
    {
        journal::LifeStats stats;
        stats.day = mCurrentDay;
        stats.built = mRunStats.structuresBuilt;
        stats.kills = mRunStats.kills;
        stats.met = static_cast<int>(std::count_if(mSurvivors.begin(), mSurvivors.end(), [&](const SurvivorRecord& r)
                                                   { return r.met && !r.retired && r.metRun == runIndex; }));
        history.journal = finishJournal(mStayedForGood ? journal::Kind::EndStayed : journal::Kind::EndRetired, journal::lifeSummary(stats));
    }
    mSaveManager.addCharacterHistoryRecord(history);
    addRunEndMemoryEvent(WorldMemoryEventType::RetirementPlaced, runIndex);

    // They stay on in town as a survivor (phase 8): the base, or the house
    // they stopped in, is home; their pack is their stock; they greet the
    // bloodline at trust 50 and will teach what they knew.
    const glm::ivec2 retireTile = worldToTile(mTileMap, mDeathWorldPosition + glm::vec2(kSpriteSize * 0.5f));
    SurvivorRecord retiree;
    retiree.name = mCurrentCharacterName;
    retiree.alive = true;
    retiree.spawnDay = mCurrentDay;
    retiree.x = mDeathWorldPosition.x;
    retiree.y = mDeathWorldPosition.y;
    retiree.aggression = 0.1f;
    retiree.cooperation = 0.7f;
    retiree.competence = 0.8f;
    retiree.homeDistrictId = mTileMap.getDistrictId(retireTile.x, retireTile.y);
    retiree.role = static_cast<int>(survivorrules::Role::Retired);
    retiree.retired = true;
    retiree.outfit = static_cast<int>(mSurvivors.size() % 3);
    retiree.trust = survivorrules::kBloodlineTrust;
    retiree.met = true;
    retiree.gateLeads = mLeads; // what they knew of the way out, to tell (phase 9)
    {
        int home = mFirstDays.homeBuilding;
        if (home <= 0 && mSaveManager.baseRecord().building > 0) home = mSaveManager.baseRecord().building;
        const int here = mTileMap.getBuildingId(retireTile.x, retireTile.y);
        if (here > 0 && mTileMap.isIndoors(retireTile.x, retireTile.y)) home = home > 0 ? home : here;
        retiree.homeBuilding = std::max(0, home);
        const glm::ivec2 tile = home > 0 ? homeTileOf(home) : retireTile;
        retiree.homeX = tile.x;
        retiree.homeY = tile.y;
        retiree.homeKnown = home > 0;
    }
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        for (const InventorySlot& slot : mWorld.getComponent<Inventory>(mPlayerEntity).slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            retiree.stock.push_back(ContainerItemRecord{slot.itemId, slot.count, slot.condition, slot.loaded});
        }
    }

    persistWorldForNextRun(runIndex, &retiree);

    // Balance instrumentation
    std::cout << "[Run End] Retirement - " << mCurrentCharacterName
              << " | Day " << mCurrentDay
              << " | Kills: " << mRunStats.kills
              << " | Items: " << mRunStats.itemsLooted
              << " | Structures: " << mRunStats.structuresBuilt
              << " | Distance: " << static_cast<int>(mRunStats.distanceWalked) << "px"
              << '\n';

    mInteractionMessage = mStayedForGood ? mCurrentCharacterName + " stayed." : "They couldn't go on.";
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
        for (PositiveTrait trait : traits.positives) history.positiveTraits.emplace_back(traitName(trait));
        for (NegativeTrait trait : traits.negatives) history.negativeTraits.emplace_back(traitName(trait));
        history.bloodlineTrait = traits.bloodlineNames();
    }
    // What makes the bloodline's traits (phase 10).
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        history.depression = mWorld.getComponent<MentalState>(mPlayerEntity).depression;
    }
    history.tilesBurned = static_cast<int>(mBurnedTiles.size() - std::min(mBurnedTilesAtRunStart, mBurnedTiles.size()));
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
    // The days this character lived pass for everyone else (phase 8): their
    // stock falls, they empty cupboards near home, some of them die.
    survivorsBetweenLives(std::max(1, mCurrentDay), retiree != nullptr);
    // The one who retired is among them now (a kept house: phase 10's aging).
    if (retiree != nullptr)
    {
        SurvivorRecord record = *retiree;
        record.id = static_cast<int>(mSurvivors.size());
        mSurvivors.push_back(record);
    }

    // What was built and set down lasts, a stage worse; windows stay as they
    // were left; and the house this life held is remembered.
    mSaveManager.saveStructures(structureRecords(true));
    mSaveManager.setWindows(windowRecords());
    recordBase(mRunState == RunState::Dead);
    saveDistrictMemory(); // infection, this life's kills, the rules learned
    saveScavengeMemory(); // things left lying indoors
    persistGate();        // where the gate key lies (phase 9)

    mSaveManager.addRunStats(mRunStats.kills, mCurrentDay);

    persistVehicles(true); // a life that used one leaves it a stage worse (GameVehicles.cpp)

    {
        std::vector<FireTileRecord> fireRecords;
        for (const FireSystem::Burning& b : mFireSystem.all())
        {
            FireTileRecord record;
            record.tileX = b.tile.x;
            record.tileY = b.tile.y;
            record.intensity = b.heat;
            record.lifeRemaining = b.fuel;
            record.byPlayer = false; // the next life did not light it
            fireRecords.push_back(record);
        }
        mSaveManager.setFireTileRecords(std::move(fireRecords));
    }

    // Doors the dead broke down stay broken for whoever comes next.
    {
        std::vector<std::pair<int,int>> broken;
        for (const glm::ivec2& tile : mDoors.tilesIn(mTileMap, DoorState::Broken))
        {
            broken.emplace_back(tile.x, tile.y);
        }
        mSaveManager.setBrokenDoors(std::move(broken));
        // So do the boards someone nailed up.
        mSaveManager.setBoardedDoors(boardedDoorRecords());
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

    // The town ages: built things, boards, crates, gardens, the dead in the
    // street, the districts (phase 10).
    if (livingWorld()) ageWorldForNextLife(std::max(1, mCurrentDay), runIndex);

    mSaveManager.deleteCheckpoint();
    refreshContainerRecords();
    mSaveManager.saveWorldMemory();

    // The town's people as they are now (already advanced, the one who
    // retired among them).
    mSaveManager.saveSurvivors(mSurvivors);

    // Fresh Start: the next life finds the town as it was made (phase 10).
    if (!livingWorld())
    {
        mSaveManager.forgetWorld();
        std::cout << "[World] fresh start: nothing carried over\n";
    }
}
