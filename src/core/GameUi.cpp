// HUD snapshot and the menu and overlay panels (inventory, crafting, pause, title...).
#include "core/GameShared.h"
#include "world/BuildingKinds.h"
#include "world/WeatherRules.h"
#include "world/CombatRules.h"

using namespace gameshared;

void Game::buildHudState()
{
    HudState& hud = mHudState;
    hud = HudState{};
    hud.visible = mRunState == RunState::Playing &&
        !mShowWorldMap &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Transform>(mPlayerEntity);
    if (!hud.visible)
    {
        return;
    }

    hud.timeSeconds = mHudClock;
    if (mShowCrafting || mShowInventory || mContainerOpen || mTalk.open)
    {
        const bool wide = mShowCrafting || mContainerOpen || (mTalk.open && mTalk.view == TalkState::View::Trade);
        hud.dockedWidth = dockedPanelWidth(wide, mWindowWidth);
    }

    if (mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        hud.health = health.current;
        hud.healthMax = health.maximum;
    }
    if (mWorld.hasComponent<Stamina>(mPlayerEntity))
    {
        const Stamina& stamina = mWorld.getComponent<Stamina>(mPlayerEntity);
        hud.stamina = stamina.current;
        hud.staminaMax = stamina.maximum;
        hud.staminaBaseMax = stamina.baseMaximum > 0.0f ? stamina.baseMaximum : stamina.maximum;
    }
    if (mWorld.hasComponent<Combat>(mPlayerEntity) && playerHeld())
    {
        const Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
        hud.held = true;
        hud.struggleTicksTotal = static_cast<int>(std::round(combatrules::struggleNeeded(std::max(1, combat.holders))));
        hud.struggleTicks = std::clamp(static_cast<int>(combat.struggle), 0, hud.struggleTicksTotal);
    }

    // Conditions.
    HudConditionInputs conditions;
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        conditions.hunger = needs.hunger;
        conditions.thirst = needs.thirst;
        conditions.sleepDebt = needs.sleepDebt;
        conditions.fatigue = needs.fatigue;
        conditions.bodyTemperature = needs.bodyTemperature;
        conditions.wet = needs.wetHours > 0.0f;
        conditions.coughing = needs.smokeSeconds >= weatherrules::kCoughAfterSeconds;
        conditions.illness = needs.illness;
        // What a coffee hides does not show (phase 5).
        conditions.sleepDebt = sleeprules::shownDebt(needs.sleepDebt, needs.coffeeHours);
        conditions.rested = needs.restedHours > 0.0f;
    }
    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        conditions.panic = mental.panic;
        conditions.depression = mental.depression;
        conditions.desensitisation = mental.desensitisation;
        conditions.inCrisis = mental.inCrisis;
    }
    conditions.bleeding = mWorld.hasComponent<Bleeding>(mPlayerEntity);
    conditions.sleeping = mWorld.hasComponent<Sleeping>(mPlayerEntity);
    bool wounded = false;
    if (mWorld.hasComponent<Wounds>(mPlayerEntity))
    {
        const Wounds& wounds = mWorld.getComponent<Wounds>(mPlayerEntity);
        wounded = !wounds.active.empty();
        for (const auto& wound : wounds.active)
        {
            if (wound.type == WoundType::Bite)
            {
                ++conditions.bites;
            }
            else if (wound.type == WoundType::Cut)
            {
                ++conditions.cuts;
            }
            conditions.infected = conditions.infected || wound.infected;
        }
    }
    // Each wound by kind and place (phase 5). Infection shows only when it
    // shows: Medicine 2 sees it early, everyone sees it once it is a fever.
    {
        std::vector<Wound> shown;
        if (mWorld.hasComponent<Wounds>(mPlayerEntity)) shown = mWorld.getComponent<Wounds>(mPlayerEntity).active;
        conditions.woundLabels = woundrules::hudLabels(shown, conditions.bleeding, conditions.illness);
    }
    const Inventory* inventory = mWorld.hasComponent<Inventory>(mPlayerEntity)
        ? &mWorld.getComponent<Inventory>(mPlayerEntity)
        : nullptr;
    if (inventory)
    {
        conditions.overburdened = InventorySystem::totalWeight(*inventory, mItemDatabase) > playerCarryLimit();
    }
    hud.conditions = Hud::buildConditions(conditions);
    if (mFlashlightOn)
    {
        hud.conditions.push_back(HudStatus{"Flashlight on", HudSeverity::Info, ""});
    }

    // Clock and weather.
    hud.day = mCurrentDay;
    hud.hours = mGameHours;
    hud.weather = weatherrules::label(mWeatherState.condition);
    // The weather word gains a second when a change is close (phase 7).
    if (mWeatherState.nextCondition != mWeatherState.condition && mWeatherState.hoursToChange <= weatherrules::kWarningHours)
    {
        hud.weather += std::string(", ") + weatherrules::changeWord(mWeatherState.nextCondition);
    }
    hud.outdoorTemperature = mWeatherState.temperature;
    hud.season = weatherrules::seasonName(mWeatherState.season);
    hud.duskHour = weatherrules::duskHour(mCurrentDay);
    hud.dawnHour = weatherrules::dawnHour(mCurrentDay);
    hud.characterName = mCurrentCharacterName;

    // Objective: the most urgent thing to do next.
    {
        // The way out (phase 9): the next step this character knows of.
        const bool campaignMode = (mTitleGameModeIndex == 0);
        const std::string wayOut = gateObjective();
        std::string title = campaignMode ? "The way out" : "Survive";
        std::string detail = campaignMode ? wayOut : "Standing still burns food, water and daylight";
        glm::vec4 color(0.92f, 0.80f, 0.34f, 1.0f);

        // The first days: the plan for this day or night, which urgent needs
        // still override.
        const firstdays::Plan plan = firstdays::planFor(mCurrentDay, mGameHours, firstDaysFacts());
        if (!plan.title.empty())
        {
            title = plan.title;
            detail = firstdays::clockLine(mGameHours, mCurrentDay);
            color = firstdays::nightIndex(mCurrentDay, mGameHours) > 0 ? glm::vec4(0.70f, 0.66f, 0.95f, 1.0f)
                                                                       : glm::vec4(0.92f, 0.80f, 0.34f, 1.0f);
            for (const firstdays::Step& step : plan.steps)
            {
                hud.objectiveSteps.push_back(HudStep{mInput.text(step.text), step.done});
            }
        }
        // From day 4, one standing aim (phase 13).
        const bool aimShowing = plan.title.empty() && mAimKind != aims::Kind::None && !mAimShown.title.empty();
        if (aimShowing)
        {
            title = mAimShown.title;
            detail = mAimShown.detail.empty() ? firstdays::clockLine(mGameHours, mCurrentDay) : mAimShown.detail;
            color = firstdays::nightIndex(mCurrentDay, mGameHours) > 0 ? glm::vec4(0.70f, 0.66f, 0.95f, 1.0f)
                                                                       : glm::vec4(0.92f, 0.80f, 0.34f, 1.0f);
            for (const aims::Step& step : mAimShown.steps)
            {
                hud.objectiveSteps.push_back(HudStep{step.text, step.done});
            }
            if (mAimDoneTimer > 0.0f) hud.objectiveSteps.push_back(HudStep{"Done.", true, true});
        }
        // A horde comes tonight: the day says so (the Tonight aim says it already).
        if (hordeDay() && mGameHours >= 12.0 && !(aimShowing && mAimKind == aims::Kind::Tonight))
        {
            hud.objectiveSteps.push_back(HudStep{"Tonight: stay quiet.", false});
        }
        // Under the first days' plan or an aim, the way out is a line of its own.
        if (campaignMode && (!plan.title.empty() || (aimShowing && mAimKind != aims::Kind::WayOut)) && !wayOut.empty())
        {
            hud.objectiveSteps.push_back(HudStep{wayOut, false});
        }

        const glm::vec4 medicalColor(0.95f, 0.50f, 0.45f, 1.0f);
        if (hud.health < hud.healthMax * 0.85f)
        {
            title = "Find medical supplies";
            detail = conditions.bleeding ? "Stop the bleeding first" : "Patch up before the next fight";
            color = medicalColor;
        }
        else if (conditions.bleeding || wounded)
        {
            title = "Treat your wounds";
            detail = conditions.bleeding ? "Stop the bleeding before nightfall" : "Untreated wounds turn into illness";
            color = medicalColor;
        }
        else if (conditions.thirst < 55.0f)
        {
            title = "Find water";
            detail = "Thirst kills faster than hunger";
            color = glm::vec4(0.45f, 0.75f, 0.95f, 1.0f);
        }
        else if (conditions.hunger < 55.0f)
        {
            title = "Find food";
            detail = "Search kitchens and stores";
        }
        else if (conditions.sleepDebt > NeedsSystem::kTiredAbove)
        {
            title = "Find somewhere safe to sleep";
            detail = "Indoors, away from the dead";
            color = glm::vec4(0.70f, 0.66f, 0.95f, 1.0f);
        }
        else if (plan.title.empty() && !aimShowing && mRunStats.itemsLooted < 6)
        {
            title = "Search nearby buildings";
            detail = "Look for food, water and medicine before dusk";
        }

        // The last stand comes before everything (phase 9).
        if (mStand.running || (gatePlaced() && mGate.open && campaignMode))
        {
            title = mStand.running ? "Hold the gate" : "The way out";
            detail = wayOut.empty() ? (mGate.open ? "The gate is open. Walk out." : "") : wayOut;
            if (mStand.running && mTitleGameModeIndex != 0)
            {
                const int left = static_cast<int>(std::ceil(std::max(0.0f, gate::kOpenSeconds - mStand.opened) + mStand.grind));
                detail = std::to_string(left) + " s.";
            }
            color = mStand.running ? glm::vec4(0.95f, 0.42f, 0.38f, 1.0f) : glm::vec4(0.58f, 0.88f, 0.56f, 1.0f);
        }

        hud.objectiveTitle = title;
        hud.objectiveDetail = detail;
        hud.objectiveColor = color;
    }

    // Hotbar.
    if (inventory)
    {
        hud.activeSlot = inventory->activeSlot;
        const float warmest = InventorySystem::bestWarmth(*inventory, mItemDatabase);
        bool wornShown = false;
        for (std::size_t i = 0; i < hud.hotbar.size() && i < inventory->slots.size(); ++i)
        {
            const auto& slot = inventory->slots[i];
            if (slot.itemId < 0 || slot.count <= 0)
            {
                continue;
            }
            const ItemDef* item = mItemDatabase.find(slot.itemId);
            if (!item)
            {
                continue;
            }
            HudSlot& out = hud.hotbar[i];
            out.empty = false;
            out.icon = item->sprite;
            out.name = item->name;
            out.count = slot.count;
            if (item->category == ItemCategory::Weapon)
            {
                out.wear = std::clamp(slot.condition, 0.0f, 1.0f);
                out.equipped = static_cast<int>(i) == mEquippedWeaponSlot;
            }
            // A flashlight or radio whose battery is flat (phase 6).
            out.dim = (slot.itemId == mItems.flashlight || item->hasTag("radio")) && slot.condition <= 0.0f;
            // The warmest thing carried counts as worn (phase 7): the first of it.
            if (item->warmth > 0.0f && item->warmth >= warmest && !wornShown)
            {
                out.worn = true;
                wornShown = true;
            }
        }
    }

    // Messages.
    if (!mInteractionMessage.empty() && mInteractionMessageTimer > 0.0f)
    {
        hud.message = mInput.text(mInteractionMessage); // "E" is the key as bound (phase 13)
        hud.messageAlpha = std::min(1.0f, mInteractionMessageTimer / 0.5f);
    }
    hud.prompt = mInput.text(mProximityPrompt);
    // An ear to a closed door (phase 12): the verb teaches itself.
    if (mListening) hud.prompt = hud.prompt.empty() ? std::string("Listening.") : "Listening.   " + hud.prompt;
    if (teachingOn()) hud.teach = mTeachLine;
    hud.heldLine = "HELD - " + bindings::label(mInput.bindings(), Action::Shove);
    for (char& c : hud.heldLine) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    // Inside a vehicle (phase 11): its fuel, condition and a word for the speed.
    if (mActiveVehicleEntity != kInvalidEntity && mWorld.hasComponent<Vehicle>(mActiveVehicleEntity))
    {
        const Vehicle& vehicle = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
        const bool engine = vehiclerules::info(vehicle.kind).engine;
        hud.vehicle.inside = true;
        hud.vehicle.fuel = vehicle.fuel;
        hud.vehicle.fuelMax = engine ? vehicle.maxFuel : 0.0f;
        hud.vehicle.condition = vehicle.condition;
        if (mWireHeld)
        {
            hud.vehicle.word = "Hot-wiring";
            hud.vehicle.wire = mWireTimer / std::max(1.0f, vehiclerules::hotwireSeconds(skillLevel(Skill::Mechanics)));
        }
        else if (engine && !vehicle.engineOn && std::fabs(vehicle.speed) < 8.0f)
        {
            hud.vehicle.word = "Engine off";
        }
        else
        {
            hud.vehicle.word = vehiclerules::speedWord(vehicle.speed);
        }
    }

    // District you are standing in, and a toast when you cross into a new one.
    if (mHudDistrictId > 0)
    {
        // A word for how far it has gone (phase 4), not a number.
        const int level = ruinLevel(mHudDistrictId);
        std::string word = ruinWord(level);
        word[0] = static_cast<char>(word[0] - 'a' + 'A');
        hud.districtLine = word;
        hud.districtLineDanger = level >= 2;
    }
    if (mDistrictToastTimer > 0.0f)
    {
        hud.districtToast = mDistrictToastText;
        hud.districtToastAlpha = std::min(1.0f, mDistrictToastTimer / 0.6f);
        hud.districtDanger = mDistrictToastDanger;
    }

    // The dawn card fades in, holds, and fades out.
    if (mFirstDays.cardTimer > 0.0f)
    {
        // One line from assets/data/day_cards.json (phase 13), or "Day N" alone.
        const firstdays::DayCard card = dawnCard();
        hud.dayCardTitle = card.title;
        hud.dayCardLine = card.line;
        const float shown = 6.0f - mFirstDays.cardTimer;
        hud.dayCardAlpha = std::clamp(std::min(shown / 0.8f, mFirstDays.cardTimer / 1.2f), 0.0f, 1.0f);
    }
}

// The notebook (J) is drawn in GameJournal.cpp (phase 10).

void Game::renderRunEndOverlay()
{
    const bool dead = mRunState == RunState::Dead;
    const bool out = mRunState == RunState::Out; // got out by the gate (phase 9)
    if (!dead && !out && mRunState != RunState::Retired)
    {
        return;
    }

    // One screen for both endings: who they were, how it ended, and what
    // they leave in the town for whoever comes next.
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const float t = static_cast<float>(mDeathStateTimer);
    const float fade = std::clamp(t / 0.8f, 0.0f, 1.0f);
    const bool ready = mDeathStateTimer >= kDeathRestartDelaySeconds;

    const glm::vec4 accent = dead ? glm::vec4(0.86f, 0.26f, 0.22f, 1.0f)
                           : out  ? kPanelGood
                                  : legacyLeadColor(legacy::LeadKind::Survivor);
    const glm::vec4 ink = kPanelInk;
    const glm::vec4 dimInk = kPanelDim;
    const glm::vec4 gold = kPanelAmber;
    const auto faded = [&](glm::vec4 c) { c.a *= fade; return c; };

    const glm::vec4 backdrop = dead ? glm::vec4(0.10f, 0.02f, 0.02f, 0.72f)
                             : out  ? glm::vec4(0.02f, 0.05f, 0.03f, 0.74f)
                                    : glm::vec4(0.03f, 0.04f, 0.09f, 0.70f);
    mSpriteBatch.draw(glm::vec2(0.0f), glm::vec2(w, h), uiUv, faded(backdrop));

    // Lines, top to bottom.
    struct Line
    {
        std::string text;
        glm::vec4 color;
        float scale;
        float gapAfter;
    };
    std::vector<Line> lines;
    lines.push_back({dead ? "YOU DIED" : out ? "OUT" : mStayedForGood ? "STAYED" : "RETIRED", accent, 3.0f, 10.0f});
    if (!out) lines.push_back({mCurrentCharacterName, ink, 2.0f, 6.0f});
    lines.push_back({mDeathCause, dead ? glm::vec4(0.92f, 0.62f, 0.56f, 1.0f) : ink, 2.0f, 8.0f});
    // Death teaches (phase 13): one plain line under the cause, at most.
    if (dead && !mDeathHint.empty()) lines.push_back({mDeathHint, ink, 1.0f, 8.0f});
    // Out: the whole line's count, then who did what to open it.
    if (out)
    {
        for (std::size_t i = 0; i < mOutLines.size(); ++i)
        {
            lines.push_back({mOutLines[i], i == 0 ? ink : dimInk, i == 0 ? 2.0f : 1.0f, i == 0 ? 8.0f : 4.0f});
        }
        if (!lines.empty()) lines.back().gapAfter = 8.0f;
    }

    std::string traitLine;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        for (PositiveTrait trait : traits.positives) traitLine += (traitLine.empty() ? "" : ", ") + std::string(traitName(trait));
        for (NegativeTrait trait : traits.negatives) traitLine += (traitLine.empty() ? "" : ", ") + std::string(traitName(trait));
        if (!traits.bloodlines.empty()) traitLine += (traitLine.empty() ? "Bloodline: " : "   Bloodline: ") + traits.bloodlineNames();
    }
    if (!traitLine.empty()) lines.push_back({traitLine, dimInk, 1.0f, 4.0f});

    const int metres = static_cast<int>(mRunStats.distanceWalked / std::max(1.0f, static_cast<float>(mTileMap.tileWidth())));
    std::string stats = "Lasted until day " + std::to_string(mCurrentDay) + "   " +
                        std::to_string(mRunStats.kills) + (mRunStats.kills == 1 ? " kill   " : " kills   ") +
                        std::to_string(metres) + " m walked";
    if (mRunStats.itemsLooted > 0) stats += "   " + std::to_string(mRunStats.itemsLooted) + " items found";
    lines.push_back({stats, dimInk, 1.0f, 16.0f});

    if (!mRunEndLegacyLine.empty()) lines.push_back({mRunEndLegacyLine, gold, 2.0f, 18.0f});

    // Panel sized to the text.
    // Props sit at the bottom of their 64 px cell; skip the empty top third.
    constexpr float kIconCrop = 0.34f;
    constexpr float kIcon = 112.0f;
    const float iconH = kIcon * (1.0f - kIconCrop);
    float contentW = 360.0f;
    float contentH = iconH + 6.0f;
    for (const Line& line : lines)
    {
        contentW = std::max(contentW, mFont.measureText(line.text, line.scale).x);
        contentH += mFont.lineHeight(line.scale) + line.gapAfter;
    }
    contentH += mFont.lineHeight(2.0f) + 8.0f; // prompt or progress bar
    const glm::vec2 panelSize(std::min(w - 24.0f, contentW + 64.0f), contentH + 36.0f);
    const glm::vec2 panelPos(std::floor((w - panelSize.x) * 0.5f), std::floor((h - panelSize.y) * 0.5f));
    mSpriteBatch.draw(panelPos, panelSize, uiUv, faded(glm::vec4(kPanelBack.r, kPanelBack.g, kPanelBack.b, 0.94f)));
    mSpriteBatch.draw(panelPos, glm::vec2(panelSize.x, 3.0f), uiUv, faded(accent));

    const float cx = panelPos.x + panelSize.x * 0.5f;
    float y = panelPos.y + 18.0f;

    // What marks them now: a grave or a cairn.
    glm::vec4 iconUv = mSpriteSheet.uvRect(dead ? "prop_grave" : out ? "prop_gate_box" : "prop_cairn");
    iconUv.y += iconUv.w * kIconCrop;
    iconUv.w *= 1.0f - kIconCrop;
    mSpriteBatch.draw(glm::vec2(cx - kIcon * 0.5f, y), glm::vec2(kIcon, iconH), iconUv, faded(glm::vec4(1.0f)));
    y += iconH + 6.0f;

    for (const Line& line : lines)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(cx, y), line.text, faded(line.color), line.scale, TextAlign::Center);
        y += mFont.lineHeight(line.scale) + line.gapAfter;
    }

    if (ready)
    {
        const float pulse = 0.75f + 0.25f * std::sin((t - static_cast<float>(kDeathRestartDelaySeconds)) * 3.0f);
        mFont.drawText(mSpriteBatch, glm::vec2(cx, y), "Press Enter to begin again",
                       glm::vec4(ink.r, ink.g, ink.b, pulse), 2.0f, TextAlign::Center);
    }
    else
    {
        // A slim bar fills while the restart is held back.
        const float barW = 220.0f;
        const float progress = std::clamp(t / static_cast<float>(kDeathRestartDelaySeconds), 0.0f, 1.0f);
        const glm::vec2 barPos(cx - barW * 0.5f, y + 6.0f);
        mSpriteBatch.draw(barPos, glm::vec2(barW, 3.0f), uiUv, faded(glm::vec4(1.0f, 1.0f, 1.0f, 0.12f)));
        mSpriteBatch.draw(barPos, glm::vec2(barW * progress, 3.0f), uiUv, faded(accent));
    }
}

namespace
{
const char* itemCategoryLabel(ItemCategory category)
{
    switch (category)
    {
    case ItemCategory::Food: return "FOOD";
    case ItemCategory::Drink: return "DRINK";
    case ItemCategory::Medical: return "MEDICAL";
    case ItemCategory::Weapon: return "WEAPON";
    case ItemCategory::Material: return "MATERIAL";
    }
    return "";
}
} // namespace

void Game::renderDockedHeader(const glm::vec2& pos, float width, bool crafting)
{
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    mSpriteBatch.draw(pos, glm::vec2(width, static_cast<float>(mWindowHeight)), white, kPanelBack);

    // Both panels are named; the open one is underlined.
    float x = pos.x + InventoryLayout::kPadding;
    const float y = pos.y + 22.0f;
    const auto tab = [&](const char* label, const char* key, bool open)
    {
        const float w = mFont.measureText(label, 3.0f).x;
        mFont.drawText(mSpriteBatch, glm::vec2(x, y), label, open ? kPanelInk : kPanelDim, 3.0f);
        if (open)
        {
            mSpriteBatch.draw(glm::vec2(x, y + 27.0f), glm::vec2(w, 3.0f), white, kPanelInk);
        }
        else
        {
            mFont.drawText(mSpriteBatch, glm::vec2(x + w + 6.0f, y + 12.0f), key, kPanelDim, 1.0f);
            x += mFont.measureText(key, 1.0f).x + 6.0f;
        }
        x += w + 24.0f;
    };
    // PACK, BODY, MAKE (phase 5): Tab steps through them.
    const int open = crafting ? 2 : mPackTab;
    tab("PACK", "Tab", open == 0);
    tab("BODY", "Tab", open == 1);
    tab("MAKE", "N", open == 2);
    mSpriteBatch.draw(glm::vec2(pos.x + InventoryLayout::kPadding, pos.y + 64.0f),
                      glm::vec2(width - InventoryLayout::kPadding * 2.0f, 1.0f), white, kPanelRule);
}

void Game::renderInventoryPanel()
{
    if (!mShowInventory || mShowCrafting || mRunState != RunState::Playing ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }
    if (mPackTab == 1)
    {
        renderBodyTab();
        return;
    }

    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const InventoryLayout layout = computeInventoryLayout(mWindowWidth, mWindowHeight);
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const float left = layout.panelPos.x + InventoryLayout::kPadding;
    const float right = layout.panelPos.x + layout.panelSize.x - InventoryLayout::kPadding;
    const float innerW = right - left;

    const auto rect = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        mSpriteBatch.draw(pos, size, white, color);
    };
    const auto frame = [&](const glm::vec2& pos, float size, float thickness, const glm::vec4& color)
    {
        rect(pos, glm::vec2(size, thickness), color);
        rect(pos + glm::vec2(0.0f, size - thickness), glm::vec2(size, thickness), color);
        rect(pos + glm::vec2(0.0f, thickness), glm::vec2(thickness, size - thickness * 2.0f), color);
        rect(pos + glm::vec2(size - thickness, thickness), glm::vec2(thickness, size - thickness * 2.0f), color);
    };
    const auto text = [&](const glm::vec2& pos, const std::string& s, const glm::vec4& color, float scale,
                          TextAlign align = TextAlign::Left)
    {
        mFont.drawText(mSpriteBatch, pos, s, color, scale, align);
    };
    const auto fit = [&](const std::string& s, float width, float scale)
    {
        return truncateHudText(s, static_cast<std::size_t>(std::max(4.0f, width / (6.0f * scale))));
    };

    renderDockedHeader(layout.panelPos, layout.panelSize.x, false);

    // Carry weight, top right.
    const float weight = InventorySystem::totalWeight(inv, mItemDatabase);
    const float carryLimit = playerCarryLimit();
    const bool over = weight > carryLimit;
    {
        std::ostringstream weightStr;
        weightStr << std::fixed << std::setprecision(1) << weight << " / " << std::setprecision(0)
                  << carryLimit << " kg";
        text(glm::vec2(right, layout.titleY), weightStr.str(), over ? kPanelBad : kPanelInk, 2.0f, TextAlign::Right);
        const float barW = 150.0f;
        rect(glm::vec2(right - barW, layout.titleY + 24.0f), glm::vec2(barW, 3.0f), kPanelTrack);
        rect(glm::vec2(right - barW, layout.titleY + 24.0f),
             glm::vec2(std::round(barW * std::clamp(weight / carryLimit, 0.0f, 1.0f)), 3.0f),
             over ? kPanelBad : kPanelAmber);
    }

    text(glm::vec2(left, layout.carriedLabelY), "CARRIED", kPanelDim, 1.0f);
    text(glm::vec2(left, layout.beltLabelY), "BELT  -  KEYS 1 TO 8", kPanelDim, 1.0f);

    const int cursor = std::clamp(mInventoryCursor, 0, Inventory::kMaxSlots - 1);
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const glm::vec2 pos = layout.slotPos(i);
        const bool isCursor = i == cursor;
        if (isCursor)
        {
            rect(pos, glm::vec2(InventoryLayout::kSlot), glm::vec4(kPanelInk.r, kPanelInk.g, kPanelInk.b, 0.14f));
            frame(pos, InventoryLayout::kSlot, 2.0f, kPanelInk);
        }
        else if (i == inv.activeSlot)
        {
            frame(pos, InventoryLayout::kSlot, 1.0f, kPanelAmber);
        }
        else
        {
            frame(pos, InventoryLayout::kSlot, 1.0f, glm::vec4(kPanelInk.r, kPanelInk.g, kPanelInk.b, 0.16f));
        }
        if (i < InventoryLayout::kBeltSlots)
        {
            text(pos + glm::vec2(5.0f, 5.0f), std::to_string(i + 1), i == inv.activeSlot ? kPanelAmber : kPanelDim, 1.0f);
        }

        const auto& slot = inv.slots[static_cast<std::size_t>(i)];
        const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
        if (item == nullptr)
        {
            continue;
        }
        const glm::vec4 iconUv = mSpriteSheet.uvRect(item->sprite);
        if (iconUv.z > 0.0f)
        {
            mSpriteBatch.draw(pos + glm::vec2(10.0f, 10.0f), glm::vec2(32.0f), iconUv, glm::vec4(1.0f));
        }
        else
        {
            text(pos + glm::vec2(InventoryLayout::kSlot * 0.5f, 22.0f), item->name.substr(0, 3), kPanelInk, 1.0f,
                 TextAlign::Center);
        }
        if (slot.count > 1)
        {
            text(pos + glm::vec2(InventoryLayout::kSlot - 5.0f, InventoryLayout::kSlot - 13.0f),
                 std::to_string(slot.count), kPanelInk, 1.0f, TextAlign::Right);
        }
        // Along the bottom: how fresh food is, how worn a weapon is.
        const bool wears = item->category == ItemCategory::Weapon || item->spoilHours > 0.0f;
        if (wears && slot.condition < 0.999f)
        {
            const float left01 = std::clamp(slot.condition, 0.0f, 1.0f);
            const glm::vec4 color = left01 > 0.66f ? kPanelInk : left01 > 0.33f ? kPanelAmber : kPanelBad;
            rect(pos + glm::vec2(5.0f, InventoryLayout::kSlot - 5.0f),
                 glm::vec2(std::max(1.0f, std::round((InventoryLayout::kSlot - 10.0f) * left01)), 2.0f), color);
        }
        if (i == mEquippedWeaponSlot)
        {
            rect(pos + glm::vec2(InventoryLayout::kSlot - 11.0f, 5.0f), glm::vec2(6.0f), kPanelAmber);
        }
    }

    // The slot under the cursor.
    rect(glm::vec2(left, layout.detailY - 18.0f), glm::vec2(innerW, 1.0f), kPanelRule);
    const auto& slot = inv.slots[static_cast<std::size_t>(cursor)];
    const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item == nullptr)
    {
        text(glm::vec2(left, layout.detailY + 4.0f), "Empty slot", kPanelDim, 2.0f);
    }
    else
    {
        const glm::vec4 iconUv = mSpriteSheet.uvRect(item->sprite);
        if (iconUv.z > 0.0f)
        {
            mSpriteBatch.draw(glm::vec2(left, layout.detailY), glm::vec2(64.0f), iconUv, glm::vec4(1.0f));
        }
        const float textX = left + 80.0f;
        std::string title = packItemName(slot);
        if (slot.count > 1) title += " x" + std::to_string(slot.count);
        text(glm::vec2(textX, layout.detailY + 2.0f), fit(title, right - textX, 3.0f), kPanelInk, 3.0f);

        std::string tag = itemCategoryLabel(item->category);
        if (cursor == mEquippedWeaponSlot) tag += "  -  IN HAND";
        text(glm::vec2(textX, layout.detailY + 32.0f), tag, kPanelAmber, 1.0f);
        {
            std::ostringstream kg;
            kg << std::fixed << std::setprecision(1) << item->weight * static_cast<float>(slot.count) << " kg";
            text(glm::vec2(textX, layout.detailY + 46.0f), kg.str(), kPanelDim, 2.0f);
        }

        float y = layout.detailY + 84.0f;
        const float barX = left + 124.0f;
        const float barW = innerW - 124.0f - 84.0f;
        // A short window (a large interface size, phase 13): what does not
        // fit above the footer is left off rather than drawn over it.
        const auto room = [&]() { return y + 18.0f <= layout.footerY - 8.0f; };
        const auto stat = [&](const std::string& label, float ratio, const std::string& value, const glm::vec4& color)
        {
            if (!room())
            {
                y += 24.0f;
                return;
            }
            text(glm::vec2(left, y), label, kPanelDim, 2.0f);
            rect(glm::vec2(barX, y + 5.0f), glm::vec2(barW, 4.0f), kPanelTrack);
            rect(glm::vec2(barX, y + 5.0f), glm::vec2(std::round(barW * std::clamp(ratio, 0.0f, 1.0f)), 4.0f), color);
            text(glm::vec2(right, y), value, color, 2.0f, TextAlign::Right);
            y += 24.0f;
        };
        const auto metres = [](float pixels)
        {
            std::ostringstream m;
            m << std::fixed << std::setprecision(1) << pixels / 32.0f << " m";
            return m.str();
        };
        const float left01 = std::clamp(slot.condition, 0.0f, 1.0f);
        const glm::vec4 wearColor = left01 > 0.66f ? kPanelInk : left01 > 0.33f ? kPanelAmber : kPanelBad;
        const std::string percent = std::to_string(static_cast<int>(std::round(left01 * 100.0f))) + "%";

        if (item->category == ItemCategory::Weapon)
        {
            const WeaponStats stats = weaponStatsForItem(*item, &mItemDatabase);
            stat("Damage", stats.damage / 70.0f, std::to_string(static_cast<int>(std::round(stats.damage))), kPanelInk);
            stat("Reach", stats.rangePixels / 96.0f, metres(stats.rangePixels), kPanelInk);
            const float speed = combatrules::speedRating(stats);
            stat("Speed", speed, speed > 0.66f ? "quick" : speed > 0.33f ? "steady" : "slow", kPanelInk);
            stat("Condition", left01, percent, wearColor);
            if (stats.magazine > 0 && room())
            {
                text(glm::vec2(left, y), "Loaded " + std::to_string(slot.loaded) + " of " + std::to_string(stats.magazine),
                     kPanelInk, 2.0f);
                y += 24.0f;
            }
            const std::string special = combatrules::specialLine(stats);
            if (!special.empty() && room())
            {
                text(glm::vec2(left, y), fit(special, innerW, 2.0f), kPanelInk, 2.0f);
                y += 24.0f;
            }

            // Against whatever is in hand now.
            if (cursor != mEquippedWeaponSlot && mWorld.hasComponent<Combat>(mPlayerEntity) && y + 64.0f <= layout.footerY - 8.0f)
            {
                const WeaponStats& held = mWorld.getComponent<Combat>(mPlayerEntity).weapon;
                const ItemDef* heldItem = held.itemId >= 0 ? mItemDatabase.find(held.itemId) : nullptr;
                y += 6.0f;
                rect(glm::vec2(left, y), glm::vec2(innerW, 1.0f), kPanelRule);
                y += 12.0f;
                text(glm::vec2(left, y),
                     fit("Against " + (heldItem != nullptr ? "the " + heldItem->name + " in hand" : std::string("bare hands")),
                         innerW, 2.0f),
                     kPanelDim, 2.0f);
                y += 22.0f;
                const int damage = static_cast<int>(std::round(stats.damage - held.damage));
                const float reach = (stats.rangePixels - held.rangePixels) / 32.0f;
                std::ostringstream reachStr;
                reachStr << (reach >= 0.0f ? "+" : "") << std::fixed << std::setprecision(1) << reach << " m reach";
                const std::string damageStr = (damage >= 0 ? "+" : "") + std::to_string(damage) + " damage";
                text(glm::vec2(left, y), damageStr, damage >= 0 ? kPanelGood : kPanelBad, 2.0f);
                text(glm::vec2(left + mFont.measureText(damageStr, 2.0f).x + 24.0f, y), reachStr.str(),
                     reach >= 0.0f ? kPanelGood : kPanelBad, 2.0f);
                y += 24.0f;
            }
        }
        else
        {
            if (item->spoilHours > 0.0f)
            {
                stat("Freshness", left01, percent, wearColor);
            }
            // A flashlight or radio: what is left of its battery (phase 6).
            if (slot.itemId == mItems.flashlight || item->hasTag("radio"))
            {
                stat("Battery", left01, percent, wearColor);
            }
            const std::string effects = itemEffectSummary(*item);
            if (!effects.empty())
            {
                // One effect to a line.
                std::istringstream parts(effects);
                std::string part;
                while (std::getline(parts, part, ',') && room())
                {
                    const std::size_t start = part.find_first_not_of(' ');
                    text(glm::vec2(left, y), fit(part.substr(start == std::string::npos ? 0 : start), innerW, 2.0f),
                         kPanelInk, 2.0f);
                    y += 22.0f;
                }
            }
        }
        // Where it is useful (phase 6).
        if (!item->fits.empty() && room())
        {
            text(glm::vec2(left, y), fit("Fits: " + item->fits, innerW, 2.0f), kPanelDim, 2.0f);
            y += 22.0f;
        }
    }

    text(glm::vec2(left, layout.footerY),
         fit(mInput.text("F use   Enter swap with belt   Backspace drop (Shift: stack)   Tab body   Esc close"), innerW, 2.0f),
         kPanelDim, 2.0f);
}

Game::CraftingLayout Game::craftingLayout() const
{
    CraftingLayout layout;
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    layout.size = glm::vec2(dockedPanelWidth(true, mWindowWidth), h);
    layout.pos = glm::vec2(w - layout.size.x, 0.0f);
    const float left = layout.pos.x + InventoryLayout::kPadding;
    const float right = layout.pos.x + layout.size.x - InventoryLayout::kPadding;

    // Category tabs in a row under the header.
    float tabX = left;
    for (int i = 0; i < kCraftingCategories; ++i)
    {
        const float tabW = mFont.measureText(craftingCategoryLabel(i), 2.0f).x;
        layout.tabs[static_cast<std::size_t>(i)] = glm::vec4(tabX - 6.0f, 74.0f, tabW + 12.0f, 28.0f);
        tabX += tabW + 22.0f;
    }

    layout.footerY = h - 40.0f;
    layout.listPos = glm::vec2(left, 116.0f);
    layout.listW = std::min(262.0f, (right - left) * 0.42f);
    layout.rowH = 44.0f;
    layout.maxRows = std::max(1, static_cast<int>((layout.footerY - 8.0f - layout.listPos.y) / layout.rowH));
    layout.detailPos = glm::vec2(layout.listPos.x + layout.listW + 24.0f, 118.0f);
    layout.detailW = right - layout.detailPos.x;
    layout.detailBottom = layout.footerY - 12.0f;
    return layout;
}

int Game::craftingListScroll(int count, int maxRows) const
{
    return std::clamp(mCraftingRecipeCursor - maxRows + 1, 0, std::max(0, count - maxRows));
}

void Game::renderCraftingOverlay()
{
    if (!mShowCrafting || mRunState != RunState::Playing ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }

    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const auto visible = buildVisibleRecipeList(mCraftingSystem, mSaveManager, inv, mSessionDiscoveredRecipes, mCraftingCategory);
    const auto& recipes = mCraftingSystem.recipes();
    const CraftingLayout layout = craftingLayout();

    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const glm::vec4 ink = kPanelInk;
    const glm::vec4 dimInk = kPanelDim;
    const glm::vec4 gold = kPanelAmber;
    const glm::vec4 good = kPanelGood;
    const glm::vec4 bad = kPanelBad;
    const float bodyLine = mFont.lineHeight(2.0f) + 6.0f;

    const auto rect = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        mSpriteBatch.draw(pos, size, white, color);
    };
    const auto text = [&](const glm::vec2& pos, const std::string& s, const glm::vec4& color, float scale)
    {
        mFont.drawText(mSpriteBatch, pos, s, color, scale);
    };
    const auto textRight = [&](float right, float y, const std::string& s, const glm::vec4& color, float scale)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(right - mFont.measureText(s, scale).x, y), s, color, scale);
    };
    const auto fit = [&](const std::string& s, float width, float scale)
    {
        return truncateHudText(s, static_cast<std::size_t>(std::max(4.0f, width / (6.0f * scale))));
    };
    const auto wrap = [&](const std::string& s, float width, float scale)
    {
        const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, width / (6.0f * scale)));
        std::vector<std::string> lines;
        std::string line;
        std::istringstream words(s);
        std::string word;
        while (words >> word)
        {
            if (!line.empty() && line.size() + 1 + word.size() > maxChars)
            {
                lines.push_back(line);
                line.clear();
            }
            line += (line.empty() ? "" : " ") + word;
        }
        if (!line.empty()) lines.push_back(line);
        return lines;
    };
    const auto itemIcon = [&](const glm::vec2& pos, float size, int itemId, float alpha)
    {
        const ItemDef* def = mItemDatabase.find(itemId);
        if (def != nullptr && !def->sprite.empty())
        {
            mSpriteBatch.draw(pos, glm::vec2(size), mSpriteSheet.uvRect(def->sprite), glm::vec4(1.0f, 1.0f, 1.0f, alpha));
        }
    };
    const auto unknownIcon = [&](const glm::vec2& pos, float size)
    {
        const float scale = size >= 48.0f ? 4.0f : 2.0f;
        const glm::vec2 q = mFont.measureText("?", scale);
        text(glm::floor(pos + (glm::vec2(size) - q) * 0.5f), "?", dimInk, scale);
    };
    const auto heading = [&](float x, float y, float width, const std::string& s)
    {
        text(glm::vec2(x, y), s, dimInk, 1.0f);
        const float end = x + mFont.measureText(s, 1.0f).x + 10.0f;
        rect(glm::vec2(end, y + 4.0f), glm::vec2(x + width - end, 1.0f), kPanelRule);
    };
    const auto isDiscovered = [&](const CraftingRecipe& recipe)
    {
        return recipe.startsKnown ||
            mSaveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB) ||
            std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(),
                      CraftingSystem::normalizePair(recipe.inputA, recipe.inputB)) != mSessionDiscoveredRecipes.end();
    };
    const auto itemName = [&](int itemId)
    {
        const ItemDef* def = mItemDatabase.find(itemId);
        return def != nullptr ? def->name : std::string("something");
    };
    // For an undiscovered recipe: the half you hold, and a clue to the other.
    const auto heldHalf = [&](const CraftingRecipe& recipe, std::string& clue)
    {
        if (InventorySystem::countItem(inv, recipe.inputA) > 0)
        {
            clue = recipe.hintB;
            return recipe.inputA;
        }
        clue = recipe.hintA;
        return recipe.inputB;
    };
    const bool hasNotebook = InventorySystem::countItem(inv, mItems.notebook) > 0;
    const glm::ivec2 standingOn = playerTile();
    const auto stationOk = [&](const CraftingRecipe& recipe)
    {
        return craftStationNearby(recipe.station, mFurniture, mWorld, mTileMap, standingOn, powerOn(),
                                  !mFireSystem.burningNear(standingOn, 3).empty());
    };

    // Backing and header; the world stays in view to the left.
    renderDockedHeader(layout.pos, layout.size.x, true);
    for (int i = 0; i < kCraftingCategories; ++i)
    {
        const glm::vec4 tab = layout.tabs[static_cast<std::size_t>(i)];
        const bool active = i == mCraftingCategory;
        text(glm::vec2(tab.x + 6.0f, tab.y + 4.0f), craftingCategoryLabel(i), active ? gold : dimInk, 2.0f);
        if (active)
        {
            rect(glm::vec2(tab.x + 6.0f, tab.y + 22.0f), glm::vec2(tab.z - 12.0f, 2.0f), gold);
        }
    }

    // Footer: the keys.
    {
        const float footW = layout.size.x - InventoryLayout::kPadding * 2.0f;
        const std::string keys = "W/S choose  A/D kind  Enter make  +/- amount  X stop  N close";
        const float scale = mFont.measureText(keys, 2.0f).x <= footW ? 2.0f : 1.0f;
        text(glm::vec2(layout.pos.x + InventoryLayout::kPadding, layout.footerY + (scale > 1.0f ? 4.0f : 8.0f)), keys,
             dimInk, scale);
    }

    const float dx = layout.detailPos.x;
    const float dy = layout.detailPos.y;
    const float dw = layout.detailW;
    // A name beside the big icon: large if it fits, otherwise the body size.
    const auto titleScale = [&](const std::string& s)
    {
        return mFont.measureText(s, 3.0f).x <= dw - 80.0f ? 3.0f : 2.0f;
    };

    // Recipe list.
    const int count = static_cast<int>(visible.size());
    const int first = craftingListScroll(count, layout.maxRows);
    if (visible.empty())
    {
        for (const std::string& line : wrap("Nothing to make yet.", layout.listW - 16.0f, 2.0f))
        {
            text(layout.listPos + glm::vec2(8.0f, 10.0f), line, dimInk, 2.0f);
        }
    }
    for (int row = 0; row < layout.maxRows && first + row < count; ++row)
    {
        const int listIndex = first + row;
        const CraftingRecipe& recipe = recipes[visible[static_cast<std::size_t>(listIndex)]];
        const bool selected = listIndex == mCraftingRecipeCursor;
        const bool known = isDiscovered(recipe);
        const bool craftable = known && hasCraftIngredients(inv, recipe, 1) && hasCraftTools(inv, mItemDatabase, recipe) &&
                               stationOk(recipe);
        const glm::vec2 rowPos = layout.listPos + glm::vec2(0.0f, static_cast<float>(row) * layout.rowH);

        if (selected)
        {
            rect(rowPos, glm::vec2(layout.listW, layout.rowH - 2.0f), ink);
        }

        const glm::vec2 iconPos = rowPos + glm::vec2(8.0f, 5.0f);
        std::string label;
        if (known)
        {
            itemIcon(iconPos, 32.0f, recipe.outputItemId, 1.0f);
            label = recipe.label;
        }
        else
        {
            unknownIcon(iconPos, 32.0f);
            std::string clue;
            label = itemName(heldHalf(recipe, clue)) + " + ?";
        }
        // Name, and under it where the recipe stands.
        const glm::vec4 nameColor = selected ? kPanelOnInk : (known ? ink : dimInk);
        const glm::vec4 stateColor = selected ? kPanelOnInk : (craftable ? good : (known ? gold : dimInk));
        text(rowPos + glm::vec2(50.0f, 6.0f), fit(label, layout.listW - 50.0f - 8.0f, 2.0f), nameColor, 2.0f);
        text(rowPos + glm::vec2(50.0f, 26.0f), craftable ? "Ready" : (known ? "Missing something" : "A hunch"),
             stateColor, 1.0f);
    }
    if (first > 0)
    {
        textRight(layout.listPos.x + layout.listW, layout.listPos.y - 12.0f, "more above", dimInk, 1.0f);
    }
    if (first + layout.maxRows < count)
    {
        textRight(layout.listPos.x + layout.listW, layout.listPos.y + static_cast<float>(layout.maxRows) * layout.rowH,
                  "more below", dimInk, 1.0f);
    }

    // Recent message (crafting results, discoveries) above the action area.
    const auto drawMessage = [&](float bottom)
    {
        if (mInteractionMessage.empty() || mInteractionMessageTimer <= 0.0f) return;
        const auto lines = wrap(mInteractionMessage, dw, 2.0f);
        float y = bottom - static_cast<float>(lines.size()) * bodyLine;
        for (const std::string& line : lines)
        {
            text(glm::vec2(dx, y), line, gold, 2.0f);
            y += bodyLine;
        }
    };

    if (visible.empty())
    {
        float y = dy;
        for (const std::string& line : wrap("Pick things up as you go. When you hold one half of something useful, it shows here.", dw, 2.0f))
        {
            text(glm::vec2(dx, y), line, dimInk, 2.0f);
            y += bodyLine;
        }
        drawMessage(layout.detailBottom);
        return;
    }

    const int cursor = std::clamp(mCraftingRecipeCursor, 0, count - 1);
    const std::size_t recipeIndex = visible[static_cast<std::size_t>(cursor)];
    const CraftingRecipe& recipe = recipes[recipeIndex];

    if (!isDiscovered(recipe))
    {
        // Half a recipe: what you hold and a clue to the rest.
        unknownIcon(glm::vec2(dx, dy), 64.0f);
        text(glm::vec2(dx + 80.0f, dy + 6.0f), "Unknown recipe", ink, titleScale("Unknown recipe"));
        std::string clue;
        const std::string held = itemName(heldHalf(recipe, clue));
        text(glm::vec2(dx + 80.0f, dy + 40.0f), fit("Something goes with your " + held + ".", dw - 80.0f, 2.0f), dimInk, 2.0f);

        float y = dy + 88.0f;
        std::string body = "The " + held + " might go with " + (clue.empty() ? std::string("something else") : clue) + ".";
        for (const std::string& line : wrap(body, dw, 2.0f))
        {
            text(glm::vec2(dx, y), line, ink, 2.0f);
            y += bodyLine;
        }
        for (const std::string& line : wrap("Carry both together and you will work it out.", dw, 2.0f))
        {
            text(glm::vec2(dx, y), line, ink, 2.0f);
            y += bodyLine;
        }
        y += 12.0f;
        for (const std::string& line : wrap(hasNotebook ? "Your notebook will keep what you work out for whoever comes next."
                                                        : "Without a notebook, what you work out dies with you.",
                                            dw, 2.0f))
        {
            text(glm::vec2(dx, y), line, hasNotebook ? good : dimInk, 2.0f);
            y += bodyLine;
        }
        drawMessage(layout.detailBottom);
        return;
    }

    // A known recipe: what it makes and what it takes.
    itemIcon(glm::vec2(dx, dy), 64.0f, recipe.outputItemId, 1.0f);
    text(glm::vec2(dx + 80.0f, dy + (titleScale(recipe.label) > 2.0f ? 6.0f : 12.0f)),
         fit(recipe.label, dw - 80.0f, titleScale(recipe.label)), ink, titleScale(recipe.label));
    {
        std::string sub = std::string(recipe.category == "MED" ? "Medical" : "Tools") + "  -  " +
                          std::to_string(recipe.craftTimeSeconds) + " s";
        if (recipe.outputCount > 1) sub += "  -  makes " + std::to_string(recipe.outputCount);
        text(glm::vec2(dx + 80.0f, dy + 40.0f), sub, dimInk, 2.0f);
    }

    float y = dy + 82.0f;
    for (const std::string& line : wrap(recipe.notes, dw, 2.0f))
    {
        text(glm::vec2(dx, y), line, ink, 2.0f);
        y += bodyLine;
    }
    y += 10.0f;
    heading(dx, y, dw, "NEEDS");
    y += 18.0f;

    std::string missing;
    const auto needRow = [&](int itemId, const std::string& right, bool ok)
    {
        itemIcon(glm::vec2(dx, y), 32.0f, itemId, ok ? 1.0f : 0.45f);
        text(glm::vec2(dx + 44.0f, y + 8.0f), fit(itemName(itemId), dw - 44.0f - 120.0f, 2.0f), ok ? ink : dimInk, 2.0f);
        textRight(dx + dw, y + 8.0f, right, ok ? good : bad, 2.0f);
        if (!ok && missing.empty()) missing = itemName(itemId);
        y += 38.0f;
    };
    for (int itemId : {recipe.inputA, recipe.inputB})
    {
        if (itemId < 0) continue;
        const int have = InventorySystem::countItem(inv, itemId);
        const int need = craftInputNeed(recipe, itemId);
        needRow(itemId, std::to_string(have) + " / " + std::to_string(need), have >= need);
        if (recipe.inputA == recipe.inputB) break;
    }
    for (const CraftTool& tool : recipe.tools)
    {
        const float condition = craftToolCondition(inv, mItemDatabase, tool);
        const bool ok = condition >= 0.0f;
        // A tag met by what is carried shows that item (a Cooking Pot, not
        // the first thing in the book with the tag).
        int shown = tool.iconItemId;
        if (ok && !tool.tag.empty())
        {
            for (const InventorySlot& slot : inv.slots)
            {
                const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
                if (item != nullptr && item->hasTag(tool.tag))
                {
                    shown = item->id;
                    break;
                }
            }
        }
        needRow(shown, ok ? "tool, " + std::to_string(static_cast<int>(std::round(std::clamp(condition, 0.0f, 1.0f) * 100.0f))) + "%"
                                    : "tool, missing",
                ok);
    }
    if (recipe.station != CraftStation::None)
    {
        const bool nearWorkbench = stationOk(recipe);
        rect(glm::vec2(dx + 6.0f, y + 12.0f), glm::vec2(20.0f, 4.0f), nearWorkbench ? ink : dimInk);
        rect(glm::vec2(dx + 8.0f, y + 16.0f), glm::vec2(3.0f, 10.0f), nearWorkbench ? ink : dimInk);
        rect(glm::vec2(dx + 21.0f, y + 16.0f), glm::vec2(3.0f, 10.0f), nearWorkbench ? ink : dimInk);
        text(glm::vec2(dx + 44.0f, y + 8.0f), craftStationName(recipe.station), nearWorkbench ? ink : dimInk, 2.0f);
        textRight(dx + dw, y + 8.0f, nearWorkbench ? "nearby" : "too far", nearWorkbench ? good : bad, 2.0f);
        if (!nearWorkbench && missing.empty())
            missing = recipe.station == CraftStation::Workbench ? "a workbench nearby" : "a fire nearby";
        y += 38.0f;
    }

    // Action: progress while making it, otherwise what Enter will do.
    const float actionY = layout.detailBottom - 40.0f;
    const bool makingThis = !mCraftQueue.empty() && mCraftQueue.front().recipeIndex == static_cast<int>(recipeIndex);
    if (makingThis)
    {
        const float duration = std::max(0.25f, static_cast<float>(recipe.craftTimeSeconds));
        const float progress = std::clamp(mActiveCraftProgressSeconds / duration, 0.0f, 1.0f);
        const int left = static_cast<int>(std::ceil(duration - mActiveCraftProgressSeconds));
        std::string status = "Making " + recipe.label + "...  " + std::to_string(std::max(0, left)) + " s";
        if (mCraftQueue.front().remaining > 1) status += "  (" + std::to_string(mCraftQueue.front().remaining) + " to go)";
        text(glm::vec2(dx, actionY), fit(status, dw, 2.0f), ink, 2.0f);
        rect(glm::vec2(dx, actionY + 28.0f), glm::vec2(dw, 4.0f), kPanelTrack);
        rect(glm::vec2(dx, actionY + 28.0f), glm::vec2(std::floor(dw * progress), 4.0f), gold);
    }
    else if (missing.empty())
    {
        // Amount, then the craft button.
        const std::string amount = "- " + std::to_string(mCraftingQuantity) + " +";
        const float amountW = mFont.measureText(amount, 2.0f).x + 24.0f;
        rect(glm::vec2(dx, actionY), glm::vec2(amountW, 1.0f), kPanelRule);
        rect(glm::vec2(dx, actionY + 35.0f), glm::vec2(amountW, 1.0f), kPanelRule);
        rect(glm::vec2(dx, actionY), glm::vec2(1.0f, 36.0f), kPanelRule);
        rect(glm::vec2(dx + amountW - 1.0f, actionY), glm::vec2(1.0f, 36.0f), kPanelRule);
        text(glm::vec2(dx + 12.0f, actionY + 11.0f), amount, ink, 2.0f);
        const float buttonX = dx + amountW + 8.0f;
        rect(glm::vec2(buttonX, actionY), glm::vec2(dx + dw - buttonX, 36.0f), ink);
        const std::string label = "Enter  Make " + std::to_string(mCraftingQuantity);
        text(glm::vec2(std::floor(buttonX + (dx + dw - buttonX - mFont.measureText(label, 2.0f).x) * 0.5f), actionY + 11.0f),
             label, kPanelOnInk, 2.0f);
    }
    else
    {
        text(glm::vec2(dx, actionY + 9.0f), fit("Missing: " + missing, dw, 2.0f), bad, 2.0f);
    }
    if (!mCraftQueue.empty() && !makingThis)
    {
        const int other = mCraftQueue.front().recipeIndex;
        if (other >= 0 && other < static_cast<int>(recipes.size()))
        {
            text(glm::vec2(dx, actionY - 22.0f), fit("Making " + recipes[static_cast<std::size_t>(other)].label + "...", dw, 2.0f), dimInk, 2.0f);
        }
    }
    drawMessage(actionY - (mCraftQueue.empty() || makingThis ? 10.0f : 32.0f));
}

glm::vec2 Game::baseActionAnchor() const
{
    const glm::vec2 here = playerWorldCenter();
    return uiAt(here) - glm::vec2(0.0f, 78.0f * uiZoom());
}

void Game::renderBaseActionBar()
{
    if (mRunState != RunState::Playing || mBaseAction.kind == BaseAction::Kind::None ||
        mBaseAction.kind == BaseAction::Kind::Grab || mBaseAction.total <= 0.0f)
    {
        return;
    }
    // A thin line over the player's head that fills as the work goes on.
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const glm::vec2 anchor = baseActionAnchor();
    constexpr float kWidth = 72.0f;
    const float done = std::clamp(mBaseAction.timer / mBaseAction.total, 0.0f, 1.0f);
    mSpriteBatch.draw(anchor + glm::vec2(-kWidth * 0.5f, 0.0f), glm::vec2(kWidth, 3.0f), uiUv, kPanelTrack);
    mSpriteBatch.draw(anchor + glm::vec2(-kWidth * 0.5f, 0.0f), glm::vec2(kWidth * done, 3.0f), uiUv, kPanelInk);
}

glm::vec2 Game::buildWheelEntry(int index) const
{
    // An oval, wider than tall, so the names side by side at the bottom
    // keep apart.
    const float angle = -1.5707963f + 6.2831853f * static_cast<float>(index) / static_cast<float>(BuildSystem::kRecipeCount);
    return mBuildWheelCentre + glm::vec2(std::cos(angle) * 190.0f, std::sin(angle) * 110.0f);
}

void Game::renderBuildWheel()
{
    if (mShowCrafting || mRunState != RunState::Playing)
    {
        return;
    }
    const auto put = [&](const glm::vec2& at, const std::string& text, const glm::vec4& color, float scale, bool centred)
    {
        const float width = mFont.measureText(text, scale).x;
        const glm::vec2 pos = centred ? at - glm::vec2(width * 0.5f, 0.0f) : at;
        const float offset = scale >= 2.0f ? 2.0f : 1.0f;
        mFont.drawText(mSpriteBatch, pos + glm::vec2(offset), text, glm::vec4(0.0f, 0.0f, 0.0f, 0.85f * color.a), scale);
        mFont.drawText(mSpriteBatch, pos, text, color, scale);
    };

    // The held action's name over the line.
    if (mBaseAction.kind != BaseAction::Kind::None && mBaseAction.kind != BaseAction::Kind::Grab && !mBaseAction.label.empty())
    {
        put(baseActionAnchor() - glm::vec2(0.0f, 14.0f), mBaseAction.label, kPanelInk, 1.0f, true);
    }

    if (mBuildChoice >= 0 && !mBuildWheelOpen)
    {
        const StationRecipe& recipe = BuildSystem::kRecipes[static_cast<std::size_t>(mBuildChoice)];
        const glm::vec2 mouse = uiMouse();
        put(mouse + glm::vec2(18.0f, 14.0f), recipe.name, mBuildGhostValid ? kPanelInk : kPanelAmber, 1.0f, false);
        put(mouse + glm::vec2(18.0f, 28.0f), "Click: build here   Right click: put away", kPanelDim, 1.0f, false);
    }
    if (!mBuildWheelOpen)
    {
        return;
    }

    // Plain words around the point where B was pressed: the one under the
    // cursor in ink if it can be built, amber if something is missing, with
    // what it costs beneath it.
    const glm::vec2 mouse = uiMouse();
    int hover = -1;
    float best = 80.0f;
    for (int i = 0; i < BuildSystem::kRecipeCount; ++i)
    {
        const glm::vec2 at = buildWheelEntry(i);
        const float d = glm::length(mouse - at);
        if (d < best)
        {
            best = d;
            hover = i;
        }
    }
    put(mBuildWheelCentre - glm::vec2(0.0f, 6.0f), "BUILD", kPanelDim, 1.0f, true);
    for (int i = 0; i < BuildSystem::kRecipeCount; ++i)
    {
        const StationRecipe& recipe = BuildSystem::kRecipes[static_cast<std::size_t>(i)];
        const glm::vec2 at = buildWheelEntry(i);
        const bool affordable = canAffordStation(recipe, nullptr);
        const glm::vec4 nameColor = i == hover ? (affordable ? kPanelInk : kPanelAmber) : kPanelDim;
        put(at - glm::vec2(0.0f, 10.0f), recipe.name, nameColor, 2.0f, true);

        std::string cost;
        const auto part = [&](const char* key, int count)
        {
            if (key == nullptr || count <= 0) return;
            const ItemDef* def = mItemDatabase.findByKey(key);
            if (!cost.empty()) cost += ", ";
            cost += std::to_string(count) + " " + (def != nullptr ? def->name : key);
        };
        part(recipe.materialKey, recipe.materialCount);
        part(recipe.secondKey, recipe.secondCount);
        if (recipe.needsHammer) cost += ", hammer";
        if (recipe.needsSaw) cost += ", saw";
        put(at + glm::vec2(0.0f, 10.0f), cost, i == hover ? kPanelDim : glm::vec4(kPanelDim.r, kPanelDim.g, kPanelDim.b, 0.7f),
            1.0f, true);
    }
}

void Game::renderPauseOverlay()
{
    if (mRunState != RunState::Paused)
    {
        return;
    }

    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");

    // Dim background
    mSpriteBatch.draw(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight)),
        uiUv,
        glm::vec4(0.0f, 0.0f, 0.0f, 0.65f));

    const PauseLayout layout = computePauseLayout(mWindowWidth, mWindowHeight, static_cast<int>(pauseOptions().size()));
    mSpriteBatch.draw(layout.panelPos, layout.panelSize, uiUv, kPanelBack);
    mSpriteBatch.draw(
        glm::vec2(layout.panelPos.x + 20.0f, layout.optionsY - 10.0f),
        glm::vec2(layout.panelSize.x - 40.0f, 1.0f),
        uiUv,
        kPanelRule);

    // The chosen option is inverted
    mSpriteBatch.draw(
        glm::vec2(layout.panelPos.x + 60.0f, layout.optionsY + static_cast<float>(mPauseSelection) * PauseLayout::kOptionH),
        glm::vec2(layout.panelSize.x - 120.0f, PauseLayout::kOptionH - 2.0f),
        uiUv,
        kPanelInk);
}

// The title is drawn in GameTitle.cpp (phase 13).

// The Controls panel (F2) is drawn in GameSettings.cpp (phase 13).
