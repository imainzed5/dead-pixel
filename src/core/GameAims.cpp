// Life after day 3 (phase 13): from day 4 the objective panel shows one
// standing aim, chosen each dawn from what the world holds. The rules live
// in world/Aims; this gathers the facts, keeps the aim and rewards it.
#include "core/GameShared.h"

#include "world/FirstDays.h"
#include "world/Power.h"
#include "world/WeatherRules.h"

using namespace gameshared;

namespace
{
// What counts as the base: the home building, and this far around it.
constexpr float kBaseTiles = 14.0f;
// Close enough to a lead to have been there.
constexpr float kVisitedTiles = 3.0f;
constexpr float kRefreshSeconds = 0.5f;
constexpr float kDoneSeconds = 5.0f;
constexpr int kDemoLastDay = 7;

glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}
} // namespace

aims::Facts Game::aimFacts()
{
    aims::Facts f;
    f.day = mCurrentDay;
    f.season = weatherrules::seasonName(mWeatherState.season);
    f.autumn = mWeatherState.season == Season::Autumn;
    f.boiled = mBoiledSinceAim;
    if (mPlayerEntity == kInvalidEntity) return f;

    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const int home = mFirstDays.homeBuilding;
    const glm::vec2 homeCentre = home > 0 ? (glm::vec2(mSpawnData.playerStart) + glm::vec2(0.5f)) * tile : playerWorldCenter();
    const auto atBase = [&](const glm::vec2& world)
    {
        const glm::ivec2 at = worldToTile(mTileMap, world);
        if (home > 0 && mTileMap.inBounds(at.x, at.y) && mTileMap.getBuildingId(at.x, at.y) == home) return true;
        return glm::length(world - homeCentre) <= kBaseTiles * tile;
    };

    // What is carried and what is kept at home (its cupboards and crates).
    const auto count = [&](const Inventory& inv, bool fridge)
    {
        for (const auto& slot : inv.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* item = mItemDatabase.find(slot.itemId);
            if (item == nullptr) continue;
            if (item->category == ItemCategory::Food)
            {
                f.food += slot.count;
                if (fridge) f.fridgeFood += slot.count;
            }
            if (slot.itemId == mItems.waterBottle) f.cleanBottles += slot.count;
            else if (slot.itemId == mItems.battery) f.batteries += slot.count;
            else if (slot.itemId == mItems.candle) f.candles += slot.count;
            else if (slot.itemId == mItems.woodPlanks) f.planks += slot.count;
            else if (slot.itemId == mItems.winterCoat) f.coat = true;
        }
    };
    if (mWorld.hasComponent<Inventory>(mPlayerEntity)) count(mWorld.getComponent<Inventory>(mPlayerEntity), false);
    mWorld.forEach<Interactable, Inventory, Transform>(
        [&](Entity e, Interactable& inter, Inventory& inv, Transform& t)
        {
            if (e == mPlayerEntity || inter.type != InteractableType::Container || home <= 0) return;
            const glm::ivec2 at = worldToTile(mTileMap, glm::vec2(t.x, t.y));
            if (!mTileMap.inBounds(at.x, at.y) || mTileMap.getBuildingId(at.x, at.y) != home) return;
            const FurniturePiece* piece = nullptr;
            for (const FurniturePiece& p : mFurniture)
            {
                if (p.tile == at)
                {
                    piece = &p;
                    break;
                }
            }
            if (piece == nullptr && !mWorld.hasComponent<Structure>(e)) return;
            count(inv, piece != nullptr && piece->kind == FurnitureKind::Fridge);
        });

    // What is built at the base.
    bool lit = mFlashlightOn;
    mWorld.forEach<Structure, Transform>(
        [&](Entity e, Structure& s, Transform& t)
        {
            const glm::vec2 at = centreOf(t);
            if (!atBase(at)) return;
            switch (s.type)
            {
            case StructureType::RainCollector: f.rainCollector = true; break;
            case StructureType::CanAlarm: f.alarm = true; break;
            case StructureType::GardenBed: f.bed = true; break;
            case StructureType::Snare: f.snare = true; break;
            case StructureType::Workbench: f.workbench = true; break;
            case StructureType::StorageCrate:
                f.crate = true;
                // A crate's store counts, as the home's cupboards do.
                if (home <= 0 && mWorld.hasComponent<Inventory>(e)) count(mWorld.getComponent<Inventory>(e), false);
                break;
            case StructureType::Campfire:
            case StructureType::Light:
                if (s.lit) lit = true;
                break;
            default: break;
            }
        });
    for (const glm::ivec2& lamp : mLitLamps)
    {
        if (home > 0 && mTileMap.inBounds(lamp.x, lamp.y) && mTileMap.getBuildingId(lamp.x, lamp.y) == home && powerOn())
        {
            lit = true;
        }
    }

    // The house.
    int doors = 0;
    if (home > 0)
    {
        for (const WayIn& way : waysIn(home))
        {
            ++f.ways;
            if (way.boarded) ++f.waysClosed;
            if (!way.window) ++doors;
        }
    }

    // Tonight.
    f.hordeTonight = isHordeNight(mCurrentDay);
    f.lightsOut = !lit;
    f.quiet = mLoudSince < 0.0 || absoluteHour() - mLoudSince > 1.0;
    bool vehicle = false;
    mWorld.forEach<Vehicle, Transform>(
        [&](Entity, Vehicle& v, Transform& t)
        {
            if ((v.started || v.keyKnown || v.keyInside) && v.condition > 0.0f && (v.fuel > 0.0f || v.maxFuel <= 0.0f) &&
                atBase(centreOf(t)))
            {
                vehicle = true;
            }
        });
    f.wayOut = doors >= 2 || vehicle;

    // The power.
    f.powerFailed = !powerOn();

    // The way out (P09): once lead 1 is known.
    f.wayOutMode = mTitleGameModeIndex == 0;
    if (f.wayOutMode && mLeads != 0) f.wayOutStep = gateObjective();

    // Someone out there: the nearest lead within reach not yet visited.
    const glm::vec2 me = playerWorldCenter();
    float best = aims::kLeadTiles * tile;
    for (const legacy::Lead& lead : currentLegacyLeads())
    {
        if (lead.kind == legacy::LeadKind::Survivor) continue;
        const glm::vec2 at = lead.world + glm::vec2(kSpriteSize * 0.5f);
        const glm::ivec2 leadTile = worldToTile(mTileMap, at);
        if (std::find(mLeadsVisited.begin(), mLeadsVisited.end(), leadTile) != mLeadsVisited.end()) continue;
        const float distance = glm::length(at - me);
        if (distance <= kVisitedTiles * tile)
        {
            mLeadsVisited.push_back(leadTile);
            continue;
        }
        if (distance > best) continue;
        best = distance;
        switch (lead.kind)
        {
        case legacy::LeadKind::Risen: f.leadName = lead.name; break;
        case legacy::LeadKind::Base: f.leadName = "the house someone held"; break;
        default: f.leadName = lead.name + "'s grave"; break;
        }
        // On the screen, which is isometric: x - y across, (x + y) / 2 down.
        const glm::vec2 d = at - me;
        f.leadWhere = aims::compass(d.x - d.y, (d.x + d.y) * 0.5f) + ", " +
                      std::to_string(static_cast<int>(std::lround(distance / tile))) + " tiles";
    }
    return f;
}

void Game::chooseAim()
{
    mBoiledSinceAim = 0;
    const aims::Facts facts = aimFacts();
    mAimKind = aims::choose(facts);
    mAimDay = firstdays::planDay(mCurrentDay, mGameHours);
    mAimShown = aims::build(mAimKind, facts);
    mAimDoneTimer = 0.0f;
    std::cout << "[Aims] day " << mCurrentDay << ": " << aims::key(mAimKind) << "\n";
}

aims::Aim Game::currentAim()
{
    return mAimShown;
}

void Game::updateAims(float dtSeconds)
{
    if (mPlayerEntity == kInvalidEntity || mRunState != RunState::Playing) return;
    mAimDoneTimer = std::max(0.0f, mAimDoneTimer - dtSeconds);
    const int planDay = firstdays::planDay(mCurrentDay, mGameHours);
    if (planDay < aims::kFirstAimDay)
    {
        mAimKind = aims::Kind::None;
        mAimShown = aims::Aim{};
        return;
    }
    // A new aim at each dawn (a resumed day-4-or-later life without one gets one now).
    if (mAimKind == aims::Kind::None || mAimDay < planDay)
    {
        chooseAim();
        mAimRefresh = kRefreshSeconds;
    }
    mAimRefresh -= dtSeconds;
    if (mAimRefresh > 0.0f) return;
    mAimRefresh = kRefreshSeconds;
    mAimShown = aims::build(mAimKind, aimFacts());

    // Every step done: "Done.", once per aim per life. The next one waits
    // for the next dawn: the evening is the player's.
    const int bit = 1 << static_cast<int>(mAimKind);
    if (aims::complete(mAimShown) && (mAimsRewarded & bit) == 0)
    {
        mAimsRewarded |= bit;
        mAimDoneTimer = kDoneSeconds;
        if (mWorld.hasComponent<MentalState>(mPlayerEntity))
        {
            MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
            mental.depression = std::max(0.0f, mental.depression - aims::kDoneRelief);
        }
        std::cout << "[Aims] done: " << aims::key(mAimKind) << "\n";
    }
}

int Game::firstDayWithoutPower() const
{
    // The first dawn after the power fails.
    const double fail = power::failHour(mSaveManager.currentRunSeed());
    const int day = static_cast<int>(std::floor(fail / 24.0));
    const double hour = fail - static_cast<double>(day) * 24.0;
    return hour < firstdays::dawnHour(day) ? day : day + 1;
}

firstdays::DayCard Game::dawnCard()
{
    firstdays::DayCard card;
    card.title = "Day " + std::to_string(mCurrentDay);
    // The demo's week ends here (phase 13).
    if (mDemo && mCurrentDay >= kDemoLastDay)
    {
        card.line = aims::fill(mDayCards.demoEnd, "name", mCurrentCharacterName);
        return card;
    }
    aims::CardFacts facts;
    facts.day = mCurrentDay;
    facts.hordeTonight = isHordeNight(mCurrentDay);
    facts.seasonFirstDay = weatherrules::seasonDay(mCurrentDay) == 0;
    facts.season = weatherrules::seasonName(weatherrules::seasonOf(mCurrentDay));
    facts.firstDayWithoutPower = mPowerForced < 0 && mCurrentDay == firstDayWithoutPower();
    facts.frost = weatherrules::frostMorning(mCurrentDay);
    card.line = aims::cardLine(mDayCards, facts);
    return card;
}

void Game::updateDemo()
{
    // A demo life ends at dawn on day 7, once its card has been read.
    if (!mDemo || mDemoEnded || mRunState != RunState::Playing || mPlayerEntity == kInvalidEntity) return;
    if (mCurrentDay < kDemoLastDay || mFirstDays.record.cardDay < kDemoLastDay || mFirstDays.cardTimer > 0.0f) return;
    mDemoEnded = true;
    std::cout << "[Demo] the week is over\n";
    beginRetirementState("The demo's week ended on Day " + std::to_string(mCurrentDay));
    mRunEndLegacyLine = aims::fill(mDayCards.demoEnd, "name", mCurrentCharacterName);
}

void Game::updateAutosave(float dtSeconds)
{
    // Checkpoints at dawn and on coming home (sleep and quitting to the
    // title save as before). The harness keeps them off unless asked.
    if (!mAutosaveAllowed || mRunState != RunState::Playing || mPlayerEntity == kInvalidEntity) return;
    mHomeSaveCooldown = std::max(0.0f, mHomeSaveCooldown - dtSeconds);
    if (mDawnSavedDay == 0)
    {
        // The first look this life: where things stand is not news.
        mDawnSavedDay = mCurrentDay;
        mWasAtHome = playerAtHome();
    }
    const bool sleeping = mWorld.hasComponent<Sleeping>(mPlayerEntity);
    if (!sleeping && mCurrentDay > mDawnSavedDay && mFirstDays.record.cardDay >= mCurrentDay)
    {
        mDawnSavedDay = mCurrentDay;
        saveCurrentRunCheckpoint("Saved at dawn.");
        mInteractionMessage.clear(); // quietly
        mInteractionMessageTimer = 0.0f;
    }
    const bool home = playerAtHome();
    if (home && !mWasAtHome && mHomeSaveCooldown <= 0.0f && mActiveVehicleEntity == kInvalidEntity)
    {
        mHomeSaveCooldown = 120.0f;
        saveCurrentRunCheckpoint("Saved.");
    }
    mWasAtHome = home;
}
