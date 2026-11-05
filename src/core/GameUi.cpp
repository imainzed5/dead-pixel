// HUD snapshot and the menu and overlay panels (inventory, crafting, pause, title...).
#include "core/GameShared.h"

using namespace gameshared;

void Game::buildHudState()
{
    HudState& hud = mHudState;
    hud = HudState{};
    hud.visible = mRunState == RunState::Playing &&
        !mShowCrafting &&
        !mShowWorldMap &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Transform>(mPlayerEntity);
    if (!hud.visible)
    {
        return;
    }

    hud.timeSeconds = mHudClock;

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
        conditions.illness = needs.illness;
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
    const Inventory* inventory = mWorld.hasComponent<Inventory>(mPlayerEntity)
        ? &mWorld.getComponent<Inventory>(mPlayerEntity)
        : nullptr;
    if (inventory)
    {
        conditions.overburdened = InventorySystem::totalWeight(*inventory, mItemDatabase) > Inventory::kMaxCarryWeight;
    }
    hud.conditions = Hud::buildConditions(conditions);
    if (mFlashlightOn)
    {
        hud.conditions.push_back(HudStatus{"Flashlight on", HudSeverity::Info, ""});
    }

    // Clock and weather.
    hud.day = mCurrentDay;
    hud.hours = mGameHours;
    hud.weather = weatherConditionLabel(mWeatherState.condition);
    hud.outdoorTemperature = mWeatherState.temperature;
    hud.characterName = mCurrentCharacterName;

    // Objective: the most urgent thing to do next.
    {
        const bool campaignMode = (mTitleGameModeIndex == 0);
        std::string title = campaignMode ? "Reach the objective" : "Survive";
        std::string detail = campaignMode ? "Objective not yet located" : "Standing still burns food, water and daylight";
        glm::vec4 color(0.92f, 0.80f, 0.34f, 1.0f);

        if (campaignMode && mCampaignObjective.completed)
        {
            title = "Objective secured";
            detail = "Extraction route confirmed. Legacy recorded.";
            color = glm::vec4(0.58f, 0.88f, 0.56f, 1.0f);
        }
        else if (campaignMode && mCampaignObjective.active)
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            const int tx = static_cast<int>(std::floor(pt.x / static_cast<float>(std::max(1, mTileMap.tileWidth()))));
            const int ty = static_cast<int>(std::floor(pt.y / static_cast<float>(std::max(1, mTileMap.tileHeight()))));
            const int distance = std::abs(mCampaignObjective.tileX - tx) + std::abs(mCampaignObjective.tileY - ty);
            detail = std::to_string(distance) + " tiles away";
            if (mCampaignObjective.infectionDeadlineDay > 0)
            {
                const int daysRemaining = std::max(0, mCampaignObjective.infectionDeadlineDay - mCurrentDay);
                detail += ", " + std::to_string(daysRemaining) + " days before the deadline";
                if (daysRemaining <= 5)
                {
                    color = glm::vec4(0.95f, 0.42f, 0.38f, 1.0f);
                }
            }
        }
        else if (campaignMode && mCampaignObjective.infectionDeadlineDay > 0 &&
                 mCurrentDay > mCampaignObjective.infectionDeadlineDay)
        {
            title = "Objective failed";
            detail = "The infection deadline has passed.";
            color = glm::vec4(0.95f, 0.42f, 0.38f, 1.0f);
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
        else if (mRunStats.itemsLooted < 6)
        {
            title = "Search nearby buildings";
            detail = "Look for food, water and medicine before dusk";
        }

        hud.objectiveTitle = title;
        hud.objectiveDetail = detail;
        hud.objectiveColor = color;
    }

    // Hotbar.
    if (inventory)
    {
        hud.activeSlot = inventory->activeSlot;
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
        }
    }

    // Messages.
    if (!mInteractionMessage.empty() && mInteractionMessageTimer > 0.0f)
    {
        hud.message = mInteractionMessage;
        hud.messageAlpha = std::min(1.0f, mInteractionMessageTimer / 0.5f);
    }
    hud.prompt = mProximityPrompt;

    // Vehicle readout: the car you drive, or the nearest one in reach.
    {
        Entity statusVehicle = mActiveVehicleEntity;
        if (statusVehicle == kInvalidEntity)
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            const glm::vec2 playerCenter(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);
            float bestDistSq = kVehicleInteractRangePixels * kVehicleInteractRangePixels;
            mWorld.forEach<Vehicle, Transform>(
                [&](Entity entity, Vehicle&, Transform& vt)
                {
                    const glm::vec2 vc(vt.x + kSpriteSize * 0.5f, vt.y + kSpriteSize * 0.5f);
                    const glm::vec2 d = vc - playerCenter;
                    const float distSq = glm::dot(d, d);
                    if (distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        statusVehicle = entity;
                    }
                });
        }

        if (statusVehicle != kInvalidEntity && mWorld.hasComponent<Vehicle>(statusVehicle))
        {
            const Vehicle& vehicle = mWorld.getComponent<Vehicle>(statusVehicle);
            hud.driving = (statusVehicle == mActiveVehicleEntity);
            hud.vehicleLine = std::string(hud.driving ? "Driving" : "Car")
                + "  " + vehicleConditionLabel(vehicle.condition)
                + "  Fuel " + std::to_string(static_cast<int>(std::round(vehicle.fuel)))
                + "/" + std::to_string(static_cast<int>(std::round(vehicle.maxFuel)));
        }
    }

    // District you are standing in, and a toast when you cross into a new one.
    if (mHudDistrictId > 0)
    {
        const int infection = static_cast<int>(std::round(mDistrictInfectionSystem.getInfection(mHudDistrictId)));
        const bool overwhelmed = mDistrictInfectionSystem.isOverwhelmed(mHudDistrictId);
        hud.districtLine = overwhelmed ? std::string("Overwhelmed") : "Infection " + std::to_string(infection) + "%";
        hud.districtLineDanger = overwhelmed || infection >= 60;
    }
    if (mDistrictToastTimer > 0.0f)
    {
        hud.districtToast = mDistrictToastText;
        hud.districtToastAlpha = std::min(1.0f, mDistrictToastTimer / 0.6f);
        hud.districtDanger = mDistrictToastDanger;
    }
}

void Game::renderNotebookOverlay()
{
    if (mRunState != RunState::Playing || !mShowNotebook)
    {
        return;
    }

    // The notebook is where the legacy reads as a story: what earlier runs
    // left out there (live distance and direction), then who they were.
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);

    const auto& history = mSaveManager.characterHistory();
    const auto leads = currentLegacyLeads();
    constexpr std::size_t kMaxLeads = 6;
    constexpr std::size_t kMaxHistory = 10;
    constexpr float kBody = 2.0f;
    const float bodyLine = mFont.lineHeight(kBody) + 5.0f;

    // Sized to what it holds: header, two sections, totals.
    const std::size_t leadRows = std::max<std::size_t>(1, std::min(leads.size(), kMaxLeads));
    const std::size_t historyRows = std::max<std::size_t>(1, std::min(history.size(), kMaxHistory));
    const float contentH = 22.0f + bodyLine + 2.0f + 30.0f +
                           18.0f + static_cast<float>(leadRows) * bodyLine + (leads.size() > kMaxLeads ? 16.0f : 0.0f) + 14.0f +
                           18.0f + static_cast<float>(historyRows) * bodyLine + (history.size() > kMaxHistory ? 16.0f : 0.0f) +
                           44.0f;
    const float panelW = std::min(900.0f, w - 40.0f);
    const float panelH = std::min(contentH, h - 40.0f);
    const float panelX = std::floor((w - panelW) * 0.5f);
    const float panelY = std::floor((h - panelH) * 0.5f);

    const glm::vec4 ink(0.90f, 0.88f, 0.80f, 1.0f);
    const glm::vec4 dimInk(0.66f, 0.64f, 0.58f, 1.0f);
    const glm::vec4 gold(0.95f, 0.84f, 0.56f, 1.0f);
    const glm::vec4 risenColor = legacyLeadColor(legacy::LeadKind::Risen);
    const glm::vec4 survivorColor = legacyLeadColor(legacy::LeadKind::Survivor);

    mSpriteBatch.draw(glm::vec2(0.0f), glm::vec2(w, h), uiUv, glm::vec4(0.0f, 0.0f, 0.0f, 0.62f));
    mSpriteBatch.draw(glm::vec2(panelX, panelY), glm::vec2(panelW, panelH), uiUv, glm::vec4(0.06f, 0.055f, 0.05f, 0.97f));
    mSpriteBatch.draw(glm::vec2(panelX, panelY), glm::vec2(panelW, 3.0f), uiUv, gold);

    const float left = panelX + 28.0f;
    const float textW = panelW - 56.0f;
    const auto fit = [&](const std::string& text, float scale)
    {
        const std::size_t maxChars = static_cast<std::size_t>(std::max(8.0f, textW / (6.0f * scale)));
        return truncateHudText(text, maxChars);
    };
    const auto heading = [&](float y, const std::string& text)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), text, gold, 1.0f);
        const float textEnd = left + mFont.measureText(text, 1.0f).x + 10.0f;
        mSpriteBatch.draw(glm::vec2(textEnd, y + 4.0f), glm::vec2(left + textW - textEnd, 1.0f), uiUv,
                          glm::vec4(gold.r, gold.g, gold.b, 0.35f));
    };

    // Title and who you are now.
    const int run = mSaveManager.runNumber();
    float y = panelY + 22.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left, y), "NOTEBOOK", gold, kBody);
    const std::string closeHint = "J to close";
    mFont.drawText(mSpriteBatch, glm::vec2(left + textW - mFont.measureText(closeHint, 1.0f).x, y + 4.0f), closeHint, dimInk, 1.0f);
    y += bodyLine + 2.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left, y),
                   fit(mCurrentCharacterName + ", day " + std::to_string(mCurrentDay) + ". " +
                           legacy::runHeadline(run, static_cast<int>(history.size())) + ".", 1.0f),
                   dimInk, 1.0f);
    y += 30.0f;

    // What is out there.
    heading(y, "WHAT IS OUT THERE");
    y += 18.0f;
    const glm::vec2 here = playerWorldCenter();
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    if (leads.empty())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), fit("Nothing yet. No one came this way before you.", kBody), dimInk, kBody);
        y += bodyLine;
    }
    for (std::size_t i = 0; i < leads.size() && i < kMaxLeads; ++i)
    {
        const legacy::Lead& lead = leads[i];
        mSpriteBatch.draw(glm::vec2(left, y + 5.0f), glm::vec2(8.0f, 8.0f), uiUv, legacyLeadColor(lead.kind));
        mFont.drawText(mSpriteBatch, glm::vec2(left + 18.0f, y), fit(legacy::describeLead(lead, here, tileSize), kBody), ink, kBody);
        y += bodyLine;
    }
    if (leads.size() > kMaxLeads)
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left + 18.0f, y),
                       "and " + std::to_string(leads.size() - kMaxLeads) + " more on the map (M)", dimInk, 1.0f);
        y += 16.0f;
    }
    y += 14.0f;

    // Who came before, newest first.
    heading(y, "WHO CAME BEFORE");
    y += 18.0f;
    const auto& graves = mLegacyGraves.empty() ? mSaveManager.graves() : mLegacyGraves;
    const float statsY = panelY + panelH - 30.0f;
    if (history.empty())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "No one. You are the first.", dimInk, kBody);
    }
    std::size_t shown = 0;
    for (auto it = history.rbegin(); it != history.rend() && shown < kMaxHistory && y + bodyLine < statsY - 6.0f; ++it, ++shown)
    {
        const CharacterHistoryRecord& rec = *it;
        std::string line = "Run " + std::to_string(rec.runIndex) + "  " + rec.name + " - ";
        glm::vec4 color = ink;
        if (rec.retired)
        {
            line += "retired on day " + std::to_string(rec.dayEnded);
            color = survivorColor;
        }
        else
        {
            line += "day " + std::to_string(rec.dayEnded) + ", " + rec.cause;
            for (const GraveRecord& grave : graves)
            {
                if (grave.runIndex == rec.runIndex && grave.name == rec.name)
                {
                    if (grave.risen) { line += " (walks again)"; color = risenColor; }
                    else if (grave.laidToRest) { line += " (at rest)"; }
                    break;
                }
            }
        }
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), fit(line, kBody), color, kBody);
        y += bodyLine;
    }
    if (shown < history.size())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), "and " + std::to_string(history.size() - shown) + " before them", dimInk, 1.0f);
    }

    // Totals across every run in this town.
    int daysTotal = 0;
    int longest = 0;
    std::string longestName;
    for (const CharacterHistoryRecord& rec : history)
    {
        const int days = std::max(1, rec.dayEnded - rec.dayStarted + 1);
        daysTotal += days;
        if (days > longest)
        {
            longest = days;
            longestName = rec.name;
        }
    }
    int laidToRest = 0;
    for (const GraveRecord& grave : graves)
    {
        laidToRest += grave.laidToRest ? 1 : 0;
    }
    std::string stats = "Lives " + std::to_string(history.size()) + "   Days survived " + std::to_string(daysTotal);
    if (longest > 0) stats += "   Longest " + std::to_string(longest) + (longest == 1 ? " day (" : " days (") + longestName + ")";
    if (laidToRest > 0) stats += "   Laid to rest " + std::to_string(laidToRest);
    mSpriteBatch.draw(glm::vec2(left, statsY - 8.0f), glm::vec2(textW, 1.0f), uiUv, glm::vec4(gold.r, gold.g, gold.b, 0.25f));
    mFont.drawText(mSpriteBatch, glm::vec2(left, statsY), fit(stats, 1.0f), dimInk, 1.0f);
}

void Game::renderRunEndOverlay()
{
    const bool dead = mRunState == RunState::Dead;
    if (!dead && mRunState != RunState::Retired)
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

    const glm::vec4 accent = dead ? glm::vec4(0.86f, 0.26f, 0.22f, 1.0f) : legacyLeadColor(legacy::LeadKind::Survivor);
    const glm::vec4 ink(0.92f, 0.90f, 0.84f, 1.0f);
    const glm::vec4 dimInk(0.64f, 0.62f, 0.58f, 1.0f);
    const glm::vec4 gold(0.95f, 0.84f, 0.56f, 1.0f);
    const auto faded = [&](glm::vec4 c) { c.a *= fade; return c; };

    const glm::vec4 backdrop = dead ? glm::vec4(0.10f, 0.02f, 0.02f, 0.72f) : glm::vec4(0.03f, 0.04f, 0.09f, 0.70f);
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
    lines.push_back({dead ? "YOU DIED" : "RETIRED", accent, 3.0f, 10.0f});
    lines.push_back({mCurrentCharacterName, ink, 2.0f, 6.0f});
    lines.push_back({mDeathCause, dead ? glm::vec4(0.92f, 0.62f, 0.56f, 1.0f) : ink, 2.0f, 8.0f});

    std::string traitLine;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        if (traits.positive != PositiveTrait::None) traitLine += traits.positiveName();
        if (traits.negative != NegativeTrait::None) traitLine += (traitLine.empty() ? "" : " / ") + std::string(traits.negativeName());
        if (traits.bloodline != BloodlineTrait::None) traitLine += (traitLine.empty() ? "Bloodline: " : "   Bloodline: ") + std::string(traits.bloodlineName());
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
    mSpriteBatch.draw(panelPos, panelSize, uiUv, faded(glm::vec4(0.05f, 0.045f, 0.045f, 0.94f)));
    mSpriteBatch.draw(panelPos, glm::vec2(panelSize.x, 3.0f), uiUv, faded(accent));

    const float cx = panelPos.x + panelSize.x * 0.5f;
    float y = panelPos.y + 18.0f;

    // What marks them now: a grave or a cairn.
    glm::vec4 iconUv = mSpriteSheet.uvRect(dead ? "prop_grave" : "prop_cairn");
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

void Game::renderInventoryPanel()
{
    if (!mShowInventory || mShowCrafting || mRunState != RunState::Playing ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }

    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const InventoryLayout layout = computeInventoryLayout(mWindowWidth, mWindowHeight);
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");

    mSpriteBatch.draw(layout.panelPos, layout.panelSize, uiUv, glm::vec4(0.05f, 0.06f, 0.09f, 0.94f));
    mSpriteBatch.draw(layout.panelPos, glm::vec2(layout.panelSize.x, 2.0f), uiUv, glm::vec4(0.72f, 0.62f, 0.40f, 0.8f));

    // Carry weight bar.
    const float weight = InventorySystem::totalWeight(inv, mItemDatabase);
    const float weightRatio = std::clamp(weight / Inventory::kMaxCarryWeight, 0.0f, 1.0f);
    const glm::vec2 barPos(layout.panelPos.x + 20.0f, layout.weightY + 14.0f);
    const float barW = layout.panelSize.x - 40.0f;
    mSpriteBatch.draw(barPos, glm::vec2(barW, 4.0f), uiUv, glm::vec4(0.12f, 0.12f, 0.13f, 1.0f));
    mSpriteBatch.draw(barPos, glm::vec2(std::round(barW * weightRatio), 4.0f), uiUv,
                      weight > Inventory::kMaxCarryWeight ? glm::vec4(0.95f, 0.40f, 0.30f, 1.0f)
                                                          : glm::vec4(0.72f, 0.66f, 0.50f, 1.0f));

    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const glm::vec2 pos = layout.slotPos(i);
        const bool isHotbar = i < 8;
        const bool isCursor = i == mInventoryCursor;
        const bool isActive = i == inv.activeSlot;
        mSpriteBatch.draw(pos, glm::vec2(InventoryLayout::kSlot), uiUv,
                          isHotbar ? glm::vec4(0.10f, 0.10f, 0.08f, 0.95f) : glm::vec4(0.08f, 0.09f, 0.11f, 0.95f));

        glm::vec4 frame = isHotbar ? glm::vec4(0.50f, 0.46f, 0.36f, 0.7f) : glm::vec4(0.36f, 0.38f, 0.42f, 0.6f);
        float thickness = 1.0f;
        if (isActive)
        {
            frame = glm::vec4(0.98f, 0.84f, 0.38f, 0.9f);
            thickness = 2.0f;
        }
        if (isCursor)
        {
            frame = glm::vec4(1.0f, 1.0f, 0.85f, 1.0f);
            thickness = 2.0f;
        }
        mSpriteBatch.draw(pos, glm::vec2(InventoryLayout::kSlot, thickness), uiUv, frame);
        mSpriteBatch.draw(pos + glm::vec2(0.0f, InventoryLayout::kSlot - thickness), glm::vec2(InventoryLayout::kSlot, thickness), uiUv, frame);
        mSpriteBatch.draw(pos, glm::vec2(thickness, InventoryLayout::kSlot), uiUv, frame);
        mSpriteBatch.draw(pos + glm::vec2(InventoryLayout::kSlot - thickness, 0.0f), glm::vec2(thickness, InventoryLayout::kSlot), uiUv, frame);

        const auto& slot = inv.slots[static_cast<std::size_t>(i)];
        if (slot.itemId < 0 || slot.count <= 0)
        {
            continue;
        }
        const ItemDef* item = mItemDatabase.find(slot.itemId);
        if (!item)
        {
            continue;
        }
        const glm::vec4 iconUv = mSpriteSheet.uvRect(item->sprite);
        if (iconUv.z > 0.0f)
        {
            mSpriteBatch.draw(pos + glm::vec2(8.0f, 10.0f), glm::vec2(32.0f), iconUv, glm::vec4(1.0f));
        }
        // Freshness strip along the bottom for food that spoils.
        if (item->spoilHours > 0.0f && slot.condition < 0.999f)
        {
            const float fresh = std::clamp(slot.condition, 0.0f, 1.0f);
            const glm::vec4 color = fresh > 0.5f ? glm::vec4(0.46f, 0.80f, 0.40f, 1.0f)
                : fresh > 0.2f ? glm::vec4(0.92f, 0.74f, 0.28f, 1.0f) : glm::vec4(0.92f, 0.30f, 0.24f, 1.0f);
            mSpriteBatch.draw(pos + glm::vec2(4.0f, InventoryLayout::kSlot - 6.0f),
                              glm::vec2(std::round((InventoryLayout::kSlot - 8.0f) * fresh), 2.0f), uiUv, color);
        }
    }
}

void Game::renderInventoryOverlay()
{
    if (!mShowInventory || mShowCrafting || mRunState != RunState::Playing ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }

    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const InventoryLayout layout = computeInventoryLayout(mWindowWidth, mWindowHeight);
    const float centerX = layout.panelPos.x + layout.panelSize.x * 0.5f;

    mFont.drawText(mSpriteBatch, glm::vec2(layout.panelPos.x + 20.0f, layout.titleY), "INVENTORY",
                   glm::vec4(0.94f, 0.86f, 0.60f, 1.0f), 2.0f);

    const float weight = InventorySystem::totalWeight(inv, mItemDatabase);
    std::ostringstream weightStr;
    weightStr << std::fixed << std::setprecision(1) << weight << " / " << std::setprecision(0) << Inventory::kMaxCarryWeight << " kg";
    mFont.drawText(mSpriteBatch, glm::vec2(layout.panelPos.x + layout.panelSize.x - 20.0f, layout.titleY + 4.0f), weightStr.str(),
                   weight > Inventory::kMaxCarryWeight ? glm::vec4(0.95f, 0.45f, 0.35f, 1.0f) : glm::vec4(0.72f, 0.72f, 0.68f, 0.95f),
                   1.0f, TextAlign::Right);
    mFont.drawText(mSpriteBatch, glm::vec2(layout.panelPos.x + 20.0f, layout.weightY), "HOTBAR 1-8  /  BACKPACK",
                   glm::vec4(0.56f, 0.56f, 0.52f, 0.9f), 1.0f);

    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const glm::vec2 pos = layout.slotPos(i);
        if (i < 8)
        {
            mFont.drawText(mSpriteBatch, pos + glm::vec2(4.0f, 4.0f), std::to_string(i + 1),
                           i == inv.activeSlot ? glm::vec4(0.98f, 0.84f, 0.38f, 1.0f) : glm::vec4(0.66f, 0.64f, 0.56f, 0.9f), 1.0f);
        }
        const auto& slot = inv.slots[static_cast<std::size_t>(i)];
        if (slot.itemId >= 0 && slot.count > 1)
        {
            mFont.drawText(mSpriteBatch, pos + glm::vec2(InventoryLayout::kSlot - 4.0f, InventoryLayout::kSlot - 14.0f),
                           std::to_string(slot.count), glm::vec4(0.94f, 0.93f, 0.86f, 1.0f), 1.0f, TextAlign::Right);
        }
        else if (slot.itemId >= 0 && slot.count > 0)
        {
            const ItemDef* item = mItemDatabase.find(slot.itemId);
            if (item && mSpriteSheet.uvRect(item->sprite).z <= 0.0f)
            {
                mFont.drawText(mSpriteBatch, pos + glm::vec2(InventoryLayout::kSlot * 0.5f, 20.0f), item->name.substr(0, 3),
                               glm::vec4(0.9f, 0.9f, 0.85f, 1.0f), 1.0f, TextAlign::Center);
            }
        }
    }

    // Details for the slot under the cursor.
    const int cursor = std::clamp(mInventoryCursor, 0, Inventory::kMaxSlots - 1);
    const auto& slot = inv.slots[static_cast<std::size_t>(cursor)];
    const ItemDef* item = (slot.itemId >= 0 && slot.count > 0) ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item)
    {
        std::string title = item->name;
        if (slot.count > 1) title += " x" + std::to_string(slot.count);
        mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.detailY), truncateHudText(title, 24),
                       glm::vec4(1.0f, 0.96f, 0.80f, 1.0f), 2.0f, TextAlign::Center);

        std::ostringstream info;
        info << std::fixed << std::setprecision(1) << item->weight * static_cast<float>(slot.count) << " kg";
        if (item->spoilHours > 0.0f)
        {
            info << "  |  " << static_cast<int>(std::round(std::clamp(slot.condition, 0.0f, 1.0f) * 100.0f)) << "% fresh";
        }
        const std::string effects = itemEffectSummary(*item);
        if (!effects.empty())
        {
            info << "  |  " << effects;
        }
        mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.detailY + 24.0f), truncateHudText(info.str(), 52),
                       glm::vec4(0.70f, 0.74f, 0.70f, 0.95f), 1.0f, TextAlign::Center);
    }
    else
    {
        mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.detailY + 4.0f), "Empty slot",
                       glm::vec4(0.50f, 0.50f, 0.48f, 0.9f), 2.0f, TextAlign::Center);
    }

    mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.footerY),
                   "Click/F use  -  Right click swap  -  Tab close",
                   glm::vec4(0.54f, 0.54f, 0.50f, 0.9f), 1.0f, TextAlign::Center);
}

void Game::renderCraftingOverlay()
{
    if (!mShowCrafting || mRunState != RunState::Playing ||
        mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }

    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const auto visible = buildVisibleRecipeList(
        mCraftingSystem,
        mSaveManager,
        inv,
        mSessionDiscoveredRecipes,
        mCraftingCategory);

    const auto& recipes = mCraftingSystem.recipes();

    // Sample a single texel from the center of solid_white to avoid atlas edge bleeding
    // when drawing large stretched UI quads.
    const glm::vec4 uiUvRegion = mSpriteSheet.uvRect("solid_white");
    const glm::vec4 uiUv(
        uiUvRegion.x + uiUvRegion.z * 0.5f,
        uiUvRegion.y + uiUvRegion.w * 0.5f,
        0.0f,
        0.0f);

    const float pad = 0.0f;
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);

    const float listW = std::max(260.0f, w * 0.17f);
    const float invW = std::max(260.0f, w * 0.19f);
    const float detailX = listW + pad;
    const float detailW = w - listW - invW - pad * 2.0f;
    const float invX = w - invW;

    auto drawRect = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        if (!mCraftingRenderTextPass)
        {
            mSpriteBatch.draw(pos, size, uiUv, color);
        }
    };

    auto drawText = [&](const glm::vec2& pos, const std::string& text, const glm::vec4& color, float scale, TextAlign align = TextAlign::Left)
    {
        if (mCraftingRenderTextPass)
        {
            const glm::vec4 shadowColor(0.0f, 0.0f, 0.0f, std::min(1.0f, color.a * 0.9f));
            mFont.drawText(mSpriteBatch, pos + glm::vec2(1.0f, 1.0f), text, shadowColor, scale, align);
            mFont.drawText(mSpriteBatch, pos, text, color, scale, align);
        }
    };

    auto isDiscovered = [&](const CraftingRecipe& recipe) -> bool
    {
        return recipe.startsKnown ||
            mSaveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB) ||
            std::find(
                mSessionDiscoveredRecipes.begin(),
                mSessionDiscoveredRecipes.end(),
                CraftingSystem::normalizePair(recipe.inputA, recipe.inputB)) != mSessionDiscoveredRecipes.end();
    };

    // Dim whole scene and column backgrounds.
    drawRect(glm::vec2(0.0f, 0.0f), glm::vec2(w, h), glm::vec4(0.0f, 0.0f, 0.0f, 0.82f));
    drawRect(glm::vec2(0.0f, 0.0f), glm::vec2(listW, h), glm::vec4(0.035f, 0.042f, 0.05f, 1.0f));
    drawRect(glm::vec2(detailX, 0.0f), glm::vec2(detailW, h), glm::vec4(0.045f, 0.05f, 0.058f, 1.0f));
    drawRect(glm::vec2(invX, 0.0f), glm::vec2(invW, h), glm::vec4(0.052f, 0.057f, 0.064f, 1.0f));
    drawRect(glm::vec2(0.0f, 0.0f), glm::vec2(w, 40.0f), glm::vec4(0.01f, 0.01f, 0.015f, 0.75f));
    drawRect(glm::vec2(listW - 1.0f, 0.0f), glm::vec2(1.0f, h), glm::vec4(0.16f, 0.14f, 0.10f, 0.95f));
    drawRect(glm::vec2(invX, 0.0f), glm::vec2(1.0f, h), glm::vec4(0.16f, 0.14f, 0.10f, 0.95f));

    drawText(glm::vec2(12.0f, 12.0f), "CRAFTING", glm::vec4(0.92f, 0.84f, 0.58f, 1.0f), 1.95f);
    drawText(glm::vec2(invX + 12.0f, 12.0f), "INVENTORY", glm::vec4(0.84f, 0.86f, 0.88f, 1.0f), 1.95f);

    // Search strip (visual only for now).
    drawRect(glm::vec2(0.0f, 44.0f), glm::vec2(listW, 32.0f), glm::vec4(0.02f, 0.02f, 0.024f, 0.96f));
    drawText(glm::vec2(14.0f, 54.0f), "> search recipes...", glm::vec4(0.52f, 0.55f, 0.56f, 0.95f), 1.05f);

    // Category tabs.
    const float tabY = 78.0f;
    const float tabW = listW / 3.0f;
    const float tabH = 46.0f;
    const float tabTextScale = 1.28f;
    for (int i = 0; i < 3; ++i)
    {
        const float x0 = std::floor(tabW * static_cast<float>(i));
        const float x1 = (i == 2) ? listW : std::floor(tabW * static_cast<float>(i + 1));
        const float tabWidth = std::max(1.0f, x1 - x0);
        const bool active = (i == mCraftingCategory);
        drawRect(
            glm::vec2(x0, tabY),
            glm::vec2(tabWidth, tabH),
            active ? glm::vec4(0.13f, 0.11f, 0.08f, 0.95f) : glm::vec4(0.05f, 0.05f, 0.05f, 0.95f));
        drawText(
            glm::vec2(x0 + tabWidth * 0.5f, tabY + 15.0f),
            craftingCategoryLabel(i),
            active ? glm::vec4(0.90f, 0.78f, 0.45f, 0.95f) : glm::vec4(0.40f, 0.40f, 0.36f, 0.9f),
            tabTextScale,
            TextAlign::Center);
    }

    // Recipe list (name + craftability dot).
    float rowY = tabY + tabH + 8.0f;
    const float rowH = 58.0f;

    for (std::size_t listIndex = 0; listIndex < visible.size(); ++listIndex)
    {
        const std::size_t recipeIndex = visible[listIndex];
        const CraftingRecipe& recipe = recipes[recipeIndex];
        const bool selected = static_cast<int>(listIndex) == mCraftingRecipeCursor;
        const bool discovered = isDiscovered(recipe);
        const bool craftable =
            discovered &&
            hasCraftIngredients(inv, recipe, 1) &&
            hasCraftTools(inv, recipe) &&
            (!recipe.requiresWorkbench || hasNearbyWorkbench(mWorld, mPlayerEntity));

        drawRect(
            glm::vec2(0.0f, rowY),
            glm::vec2(listW, rowH),
            selected ? glm::vec4(0.165f, 0.12f, 0.07f, 0.98f) : glm::vec4(0.028f, 0.03f, 0.034f, 0.95f));

        drawRect(
            glm::vec2(0.0f, rowY),
            glm::vec2(3.0f, rowH),
            selected ? glm::vec4(0.90f, 0.72f, 0.30f, 0.95f) : glm::vec4(0.16f, 0.18f, 0.20f, 0.80f));

        drawText(
            glm::vec2(14.0f, rowY + 18.0f),
            discovered ? recipe.label : "???",
            discovered ? glm::vec4(0.94f, 0.88f, 0.74f, 1.0f) : glm::vec4(0.68f, 0.70f, 0.72f, 0.92f),
            1.75f);

        drawRect(
            glm::vec2(listW - 14.0f, rowY + 24.0f),
            glm::vec2(6.0f, 6.0f),
            craftable ? glm::vec4(0.25f, 0.75f, 0.30f, 1.0f) : glm::vec4(0.55f, 0.20f, 0.20f, 1.0f));

        rowY += rowH;
        if (rowY > h - 90.0f)
        {
            break;
        }
    }

    if (visible.empty())
    {
        drawText(
            glm::vec2(14.0f, 140.0f),
            "No known or nearby clues in this category.",
            glm::vec4(0.74f, 0.76f, 0.78f, 0.95f),
            1.15f);
    }

    if (mCraftingNotebookView)
    {
        drawRect(glm::vec2(detailX + 14.0f, 10.0f), glm::vec2(detailW - 28.0f, 46.0f), glm::vec4(0.065f, 0.07f, 0.08f, 0.98f));
        drawText(glm::vec2(detailX + 18.0f, 16.0f), "NOTEBOOK LOG", glm::vec4(0.86f, 0.74f, 0.44f, 0.95f), 2.0f);
        drawText(glm::vec2(detailX + 18.0f, 52.0f), "Discovered Recipes", glm::vec4(0.72f, 0.72f, 0.66f, 0.9f), 1.2f);

        float logY = 82.0f;
        const auto& discoveredPairs = mSaveManager.discoveredRecipes();
        if (discoveredPairs.empty())
        {
            drawText(glm::vec2(detailX + 18.0f, logY), "No preserved entries yet.", glm::vec4(0.55f, 0.55f, 0.52f, 0.9f), 1.2f);
        }
        else
        {
            for (const auto& pair : discoveredPairs)
            {
                const CraftingRecipe* recipe = mCraftingSystem.findRecipe(pair.first, pair.second);
                const std::string title = (recipe != nullptr) ? recipe->label : "Unknown Entry";
                drawText(glm::vec2(detailX + 18.0f, logY), "- " + title, glm::vec4(0.84f, 0.78f, 0.62f, 0.95f), 1.2f);
                logY += 18.0f;
                if (logY > h - 70.0f)
                {
                    break;
                }
            }
        }
    }
    else if (!visible.empty())
    {
        const int selectedRecipeListIndex = std::clamp(mCraftingRecipeCursor, 0, static_cast<int>(visible.size()) - 1);
        const std::size_t selectedRecipeIndex = visible[static_cast<std::size_t>(selectedRecipeListIndex)];
        const CraftingRecipe& selectedRecipe = recipes[selectedRecipeIndex];
        const bool discovered = isDiscovered(selectedRecipe);

        const ItemDef* aDef = mItemDatabase.find(selectedRecipe.inputA);
        const ItemDef* bDef = mItemDatabase.find(selectedRecipe.inputB);
        const ItemDef* outDef = mItemDatabase.find(selectedRecipe.outputItemId);

        drawRect(glm::vec2(detailX + 14.0f, 10.0f), glm::vec2(detailW - 28.0f, 46.0f), glm::vec4(0.065f, 0.07f, 0.08f, 0.98f));

        drawText(
            glm::vec2(detailX + 14.0f, 16.0f),
            discovered ? selectedRecipe.label : "???",
            glm::vec4(0.95f, 0.88f, 0.62f, 1.0f),
            2.7f);

        drawText(
            glm::vec2(detailX + 14.0f, 50.0f),
            discovered ? selectedRecipe.category + "  -  CRAFT ENTRY" : "UNKNOWN ENTRY",
            glm::vec4(0.72f, 0.76f, 0.78f, 0.96f),
            1.15f);

        if (selectedRecipe.requiresWorkbench)
        {
            const bool nearWorkbench = hasNearbyWorkbench(mWorld, mPlayerEntity);
            drawRect(
                glm::vec2(detailX + 14.0f, 76.0f),
                glm::vec2(detailW - 28.0f, 24.0f),
                nearWorkbench ? glm::vec4(0.10f, 0.22f, 0.12f, 0.95f) : glm::vec4(0.26f, 0.09f, 0.09f, 0.95f));
            drawText(
                glm::vec2(detailX + 20.0f, 83.0f),
                nearWorkbench ? "WORKBENCH IN RANGE" : "WORKBENCH REQUIRED (MOVE CLOSER)",
                nearWorkbench ? glm::vec4(0.45f, 0.86f, 0.50f, 0.95f) : glm::vec4(0.95f, 0.45f, 0.45f, 0.95f),
                1.1f);
        }

        const float ingredientTop = selectedRecipe.requiresWorkbench ? 116.0f : 104.0f;
        drawText(glm::vec2(detailX + 14.0f, ingredientTop), "INGREDIENTS", glm::vec4(0.82f, 0.84f, 0.86f, 0.98f), 1.34f);

        auto drawIngredientCard = [&](float x, const ItemDef* def, int needed)
        {
            const int have = (def != nullptr) ? InventorySystem::countItem(inv, def->id) : 0;
            const bool ok = discovered && have >= needed;
            drawRect(
                glm::vec2(x, ingredientTop + 22.0f),
                glm::vec2((detailW - 42.0f) * 0.5f, 60.0f),
                ok ? glm::vec4(0.07f, 0.18f, 0.08f, 0.92f) : glm::vec4(0.20f, 0.07f, 0.07f, 0.92f));

            drawText(
                glm::vec2(x + 10.0f, ingredientTop + 36.0f),
                (!discovered || def == nullptr) ? "???" : def->name,
                glm::vec4(0.86f, 0.80f, 0.68f, 0.95f),
                1.4f);

            std::ostringstream qty;
            qty << (discovered ? have : 0) << "/" << needed;
            drawText(
                glm::vec2(x + 10.0f, ingredientTop + 54.0f),
                qty.str(),
                ok ? glm::vec4(0.35f, 0.88f, 0.40f, 0.95f) : glm::vec4(0.92f, 0.45f, 0.45f, 0.95f),
                1.2f);
        };

        drawIngredientCard(detailX + 14.0f, aDef, 1);
        drawIngredientCard(detailX + 22.0f + (detailW - 42.0f) * 0.5f, bDef, 1);

        const float toolsY = ingredientTop + 96.0f;
        drawText(glm::vec2(detailX + 14.0f, toolsY), "REQUIRED TOOLS", glm::vec4(0.82f, 0.84f, 0.86f, 0.98f), 1.34f);

        if (selectedRecipe.requiredTools.empty())
        {
            drawRect(glm::vec2(detailX + 14.0f, toolsY + 22.0f), glm::vec2(140.0f, 30.0f), glm::vec4(0.10f, 0.20f, 0.07f, 0.92f));
            drawText(glm::vec2(detailX + 24.0f, toolsY + 31.0f), "NONE REQUIRED", glm::vec4(0.36f, 0.74f, 0.32f, 0.95f), 1.2f);
        }
        else
        {
            float toolX = detailX + 14.0f;
            for (int toolId : selectedRecipe.requiredTools)
            {
                const ItemDef* toolDef = mItemDatabase.find(toolId);
                const bool ok = toolDef != nullptr && InventorySystem::countItem(inv, toolId) > 0;

                float bestCondition = 0.0f;
                for (const auto& slot : inv.slots)
                {
                    if (slot.itemId == toolId && slot.count > 0)
                    {
                        bestCondition = std::max(bestCondition, slot.condition);
                    }
                }
                const int conditionPct = static_cast<int>(std::round(std::clamp(bestCondition, 0.0f, 1.0f) * 100.0f));

                drawRect(
                    glm::vec2(toolX, toolsY + 22.0f),
                    glm::vec2(170.0f, 30.0f),
                    ok ? glm::vec4(0.09f, 0.18f, 0.10f, 0.92f) : glm::vec4(0.20f, 0.07f, 0.07f, 0.92f));

                std::string toolLabel = toolDef ? toolDef->name : "Unknown tool";
                if (ok)
                {
                    toolLabel += " [" + std::to_string(conditionPct) + "%]";
                }

                drawText(
                    glm::vec2(toolX + 8.0f, toolsY + 31.0f),
                    toolLabel,
                    ok ? glm::vec4(0.40f, 0.86f, 0.48f, 0.95f) : glm::vec4(0.94f, 0.46f, 0.46f, 0.95f),
                    1.1f);
                toolX += 176.0f;
            }
        }

        const float outY = toolsY + 70.0f;
        drawText(glm::vec2(detailX + 14.0f, outY), "OUTPUT", glm::vec4(0.82f, 0.84f, 0.86f, 0.98f), 1.34f);
        drawRect(glm::vec2(detailX + 14.0f, outY + 20.0f), glm::vec2(detailW - 28.0f, 82.0f), glm::vec4(0.08f, 0.08f, 0.08f, 0.92f));
        drawText(
            glm::vec2(detailX + 24.0f, outY + 36.0f),
            discovered ? (outDef ? outDef->name : selectedRecipe.label) : "Unknown result",
            glm::vec4(0.90f, 0.82f, 0.65f, 0.95f),
            2.0f);
        if (discovered && !selectedRecipe.notes.empty())
        {
            drawText(glm::vec2(detailX + 24.0f, outY + 58.0f), selectedRecipe.notes, glm::vec4(0.50f, 0.50f, 0.48f, 0.90f), 1.1f);
        }

        const float timeY = outY + 120.0f;
        drawText(glm::vec2(detailX + 14.0f, timeY), "Craft time", glm::vec4(0.78f, 0.82f, 0.84f, 0.98f), 1.24f);
        drawRect(glm::vec2(detailX + 14.0f, timeY + 18.0f), glm::vec2(detailW - 28.0f, 10.0f), glm::vec4(0.12f, 0.13f, 0.14f, 0.95f));

        const float craftDuration = std::max(0.25f, static_cast<float>(selectedRecipe.craftTimeSeconds));
        float progressRatio = 0.0f;
        if (!mCraftQueue.empty() && mCraftQueue.front().recipeIndex == static_cast<int>(selectedRecipeIndex))
        {
            progressRatio = std::clamp(mActiveCraftProgressSeconds / craftDuration, 0.0f, 1.0f);
        }
        drawRect(
            glm::vec2(detailX + 14.0f, timeY + 18.0f),
            glm::vec2((detailW - 28.0f) * progressRatio, 10.0f),
            glm::vec4(0.60f, 0.50f, 0.25f, 0.95f));

        std::ostringstream timeText;
        timeText << std::fixed << std::setprecision(1) << craftDuration << "s";
        drawText(
            glm::vec2(detailX + detailW - 96.0f, timeY + 12.0f),
            timeText.str(),
            glm::vec4(0.42f, 0.42f, 0.40f, 0.9f),
            1.1f);

        int queuedTotal = 0;
        for (const CraftQueueEntry& entry : mCraftQueue)
        {
            queuedTotal += std::max(0, entry.remaining);
        }

        if (!mCraftQueue.empty())
        {
            const int activeRecipeIndex = mCraftQueue.front().recipeIndex;
            const int activeRemaining = mCraftQueue.front().remaining;
            const std::string activeName =
                (activeRecipeIndex >= 0 && activeRecipeIndex < static_cast<int>(recipes.size()))
                    ? recipes[static_cast<std::size_t>(activeRecipeIndex)].label
                    : "Unknown";
            drawText(
                glm::vec2(detailX + 14.0f, timeY + 38.0f),
                "Queue: " + activeName + " x" + std::to_string(std::max(0, activeRemaining)) +
                    " (total " + std::to_string(queuedTotal) + ")",
                glm::vec4(0.78f, 0.80f, 0.74f, 0.98f),
                1.15f);
        }
        else
        {
            drawText(glm::vec2(detailX + 14.0f, timeY + 38.0f), "Queue: idle", glm::vec4(0.70f, 0.72f, 0.73f, 0.96f), 1.15f);
        }

        const float footerY = h - 64.0f;
        drawRect(glm::vec2(detailX + 14.0f, footerY), glm::vec2(126.0f, 34.0f), glm::vec4(0.08f, 0.08f, 0.08f, 0.95f));
        drawText(glm::vec2(detailX + 22.0f, footerY + 9.0f), "-", glm::vec4(0.75f, 0.70f, 0.58f, 0.95f), 2.0f);
        drawText(glm::vec2(detailX + 64.0f, footerY + 9.0f), std::to_string(mCraftingQuantity), glm::vec4(0.88f, 0.82f, 0.66f, 0.95f), 1.8f);
        drawText(glm::vec2(detailX + 108.0f, footerY + 9.0f), "+", glm::vec4(0.75f, 0.70f, 0.58f, 0.95f), 2.0f);

        bool ready = discovered && hasCraftIngredients(inv, selectedRecipe, 1) && hasCraftTools(inv, selectedRecipe);
        if (selectedRecipe.requiresWorkbench)
        {
            ready = ready && hasNearbyWorkbench(mWorld, mPlayerEntity);
        }

        drawRect(
            glm::vec2(detailX + 150.0f, footerY),
            glm::vec2(detailW - 164.0f, 34.0f),
            ready ? glm::vec4(0.16f, 0.18f, 0.10f, 0.95f) : glm::vec4(0.12f, 0.12f, 0.12f, 0.95f));
        drawText(
            glm::vec2(detailX + detailW * 0.5f, footerY + 9.0f),
            ready ? "QUEUE CRAFT" : "MISSING ITEMS",
            ready ? glm::vec4(0.60f, 0.62f, 0.30f, 0.95f) : glm::vec4(0.45f, 0.45f, 0.42f, 0.95f),
            1.8f,
            TextAlign::Center);
    }

    float invY = 44.0f;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const auto& slot = inv.slots[static_cast<std::size_t>(i)];
        if (slot.itemId < 0 || slot.count <= 0)
        {
            continue;
        }

        const ItemDef* item = mItemDatabase.find(slot.itemId);
        std::string line = item ? item->name : ("Item " + std::to_string(slot.itemId));
        line += " x" + std::to_string(slot.count);

        drawRect(glm::vec2(invX + 10.0f, invY), glm::vec2(invW - 20.0f, 34.0f), glm::vec4(0.11f, 0.12f, 0.13f, 0.96f));
        drawText(glm::vec2(invX + 18.0f, invY + 10.0f), line, glm::vec4(0.92f, 0.94f, 0.96f, 1.0f), 1.58f);

        invY += 40.0f;
        if (invY > h - 60.0f)
        {
            break;
        }
    }

    drawRect(glm::vec2(0.0f, h - 24.0f), glm::vec2(w, 24.0f), glm::vec4(0.015f, 0.018f, 0.022f, 0.98f));
    drawText(
        glm::vec2(12.0f, h - 18.0f),
        "N: Open/Close  Mouse: Hover/LMB Queue/RMB Append  W/S: Select  A/D: Category  +/-: Quantity  Enter: Queue  Shift+Enter: Append  X: Cancel  J: Notebook",
        glm::vec4(0.78f, 0.82f, 0.84f, 0.98f),
        1.06f);
}

void Game::renderContainerOverlay()
{
    if (!mContainerOpen || mOpenContainerEntity == kInvalidEntity || mRunState != RunState::Playing)
    {
        return;
    }

    if (!mWorld.hasComponent<Inventory>(mOpenContainerEntity))
    {
        return;
    }

    const Inventory& containerInv = mWorld.getComponent<Inventory>(mOpenContainerEntity);
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    constexpr float kItemScale = 2.0f;
    const float itemLine = mFont.lineHeight(kItemScale) + 4.0f;

    std::string header = "Container";
    if (mWorld.hasComponent<Interactable>(mOpenContainerEntity))
    {
        const Interactable& inter = mWorld.getComponent<Interactable>(mOpenContainerEntity);
        if (!inter.prompt.empty()) header = inter.prompt;
    }

    std::vector<std::string> lines;
    for (const auto& slot : containerInv.slots)
    {
        if (slot.itemId < 0 || slot.count <= 0) continue;
        const ItemDef* item = mItemDatabase.find(slot.itemId);
        std::string line = item ? item->name : ("Item #" + std::to_string(slot.itemId));
        if (slot.count > 1) line += " x" + std::to_string(slot.count);
        lines.push_back(std::move(line));
    }
    const std::string hint = lines.empty() ? "Esc close" : "W/S select   E take   Esc close";

    // Wide enough for the header and every line, never off screen.
    float contentW = std::max(mFont.measureText(header, kItemScale).x, mFont.measureText(hint, 1.0f).x);
    for (const std::string& line : lines)
    {
        contentW = std::max(contentW, mFont.measureText(line, kItemScale).x + 8.0f);
    }
    const float panelW = std::min(static_cast<float>(mWindowWidth) - 24.0f, std::max(300.0f, contentW + 32.0f));
    const float panelX = std::floor((static_cast<float>(mWindowWidth) - panelW) * 0.5f);
    const float panelY = 110.0f;
    const float listY = panelY + 40.0f;
    const float panelH = 40.0f + static_cast<float>(std::max<std::size_t>(1, lines.size())) * itemLine + 26.0f;

    mSpriteBatch.draw(glm::vec2(panelX, panelY), glm::vec2(panelW, panelH), uiUv, glm::vec4(0.06f, 0.055f, 0.05f, 0.94f));
    mSpriteBatch.draw(glm::vec2(panelX, panelY), glm::vec2(panelW, 2.0f), uiUv, glm::vec4(0.95f, 0.84f, 0.56f, 1.0f));
    mFont.drawText(mSpriteBatch, glm::vec2(panelX + 16.0f, panelY + 12.0f), header, glm::vec4(0.95f, 0.84f, 0.56f, 1.0f), kItemScale);

    float y = listY;
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const bool selected = static_cast<int>(i) == mContainerCursor;
        if (selected)
        {
            mSpriteBatch.draw(glm::vec2(panelX + 8.0f, y - 2.0f), glm::vec2(panelW - 16.0f, itemLine), uiUv,
                              glm::vec4(0.95f, 0.84f, 0.56f, 0.16f));
            mSpriteBatch.draw(glm::vec2(panelX + 8.0f, y - 2.0f), glm::vec2(2.0f, itemLine), uiUv,
                              glm::vec4(0.95f, 0.84f, 0.56f, 0.9f));
        }
        const glm::vec4 color = selected ? glm::vec4(1.0f, 0.97f, 0.86f, 1.0f) : glm::vec4(0.76f, 0.75f, 0.70f, 1.0f);
        mFont.drawText(mSpriteBatch, glm::vec2(panelX + 20.0f, y), lines[i], color, kItemScale);
        y += itemLine;
    }
    if (lines.empty())
    {
        mFont.drawText(mSpriteBatch, glm::vec2(panelX + 20.0f, y), "Empty", glm::vec4(0.56f, 0.55f, 0.52f, 1.0f), kItemScale);
    }

    mFont.drawText(mSpriteBatch, glm::vec2(panelX + 16.0f, panelY + panelH - 18.0f), hint,
                   glm::vec4(0.66f, 0.64f, 0.58f, 1.0f), 1.0f);
}

void Game::renderBuildModeOverlay()
{
    if (mShowCrafting || !mBuildSystem.isBuildMode() || mRunState != RunState::Playing)
    {
        return;
    }

    constexpr float kSmallScale = 1.5f;
    const float panelX = 12.0f;
    float y = static_cast<float>(mWindowHeight) - 120.0f;

    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), "BUILD MODE", glm::vec4(0.9f, 0.8f, 0.3f, 1.0f), 2.0f);
    y += 22.0f;

    for (int i = 0; i < BuildSystem::kRecipeCount; ++i)
    {
        const auto& recipe = BuildSystem::kRecipes[i];
        const bool selected = (i == mBuildSystem.selectedRecipe());

        // Check if player has enough materials
        bool canAfford = false;
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
        {
            const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
            canAfford = InventorySystem::countItem(inv, recipe.materialId) >= recipe.materialCost;
        }

        std::string line = std::to_string(i + 1) + ": " + std::string(recipe.name) + " (" + std::to_string(recipe.materialCost) + " mat)";
        glm::vec4 color;
        if (selected)
        {
            color = canAfford ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.5f, 0.4f, 1.0f);
        }
        else
        {
            color = canAfford ? glm::vec4(0.4f, 0.75f, 0.45f, 0.85f) : glm::vec4(0.5f, 0.5f, 0.5f, 0.6f);
        }
        mFont.drawText(mSpriteBatch, glm::vec2(panelX, y), line, color, kSmallScale);
        y += 16.0f;
    }

    mFont.drawText(mSpriteBatch, glm::vec2(panelX, y + 2.0f), "Click: Place  |  B/ESC: Cancel", glm::vec4(0.5f, 0.5f, 0.45f, 0.8f), 1.0f);
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

    const PauseLayout layout = computePauseLayout(mWindowWidth, mWindowHeight);
    mSpriteBatch.draw(layout.panelPos, layout.panelSize, uiUv, glm::vec4(0.06f, 0.07f, 0.10f, 0.94f));
    mSpriteBatch.draw(layout.panelPos, glm::vec2(layout.panelSize.x, 2.0f), uiUv, glm::vec4(0.72f, 0.62f, 0.40f, 0.8f));
    mSpriteBatch.draw(
        glm::vec2(layout.panelPos.x + 20.0f, layout.optionsY - 10.0f),
        glm::vec2(layout.panelSize.x - 40.0f, 1.0f),
        uiUv,
        glm::vec4(0.72f, 0.62f, 0.40f, 0.35f));

    // Highlight selected option
    mSpriteBatch.draw(
        glm::vec2(layout.panelPos.x + 60.0f, layout.optionsY + static_cast<float>(mPauseSelection) * PauseLayout::kOptionH),
        glm::vec2(layout.panelSize.x - 120.0f, PauseLayout::kOptionH - 2.0f),
        uiUv,
        glm::vec4(0.25f, 0.4f, 0.55f, 0.5f));
}

void Game::renderTitleScreen()
{
    if (mRunState != RunState::TitleScreen)
    {
        return;
    }

    // The town itself is the backdrop (the title camera drifts over it);
    // the menu sits on the darker left, the town's memory on the right.
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const glm::vec4 ink(0.92f, 0.90f, 0.84f, 1.0f);
    const glm::vec4 dimInk(0.62f, 0.61f, 0.57f, 1.0f);
    const glm::vec4 gold(0.95f, 0.84f, 0.56f, 1.0f);
    const glm::vec4 red(0.90f, 0.30f, 0.24f, 1.0f);

    constexpr int kStrips = 48;
    for (int i = 0; i < kStrips; ++i)
    {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kStrips);
        const float fadeT = std::clamp((t - 0.30f) / 0.70f, 0.0f, 1.0f);
        const float alpha = 0.92f - 0.50f * fadeT * fadeT * (3.0f - 2.0f * fadeT);
        // Strips meet exactly; overlapping edges would show as dark lines.
        const float x0 = std::floor(w * static_cast<float>(i) / kStrips);
        const float x1 = std::floor(w * static_cast<float>(i + 1) / kStrips);
        mSpriteBatch.draw(glm::vec2(x0, 0.0f), glm::vec2(x1 - x0, h), uiUv, glm::vec4(0.02f, 0.025f, 0.03f, alpha));
    }

    const auto wrap = [](const std::string& text, std::size_t maxChars)
    {
        std::vector<std::string> lines;
        std::string line;
        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t end = text.find(' ', start);
            if (end == std::string::npos) end = text.size();
            const std::string word = text.substr(start, end - start);
            if (!line.empty() && line.size() + 1 + word.size() > maxChars)
            {
                lines.push_back(line);
                line.clear();
            }
            line += (line.empty() ? "" : " ") + word;
            start = end + 1;
        }
        if (!line.empty()) lines.push_back(line);
        return lines;
    };

    const float left = std::max(32.0f, std::floor(w * 0.06f));
    float y = std::max(28.0f, std::floor(h * 0.08f));

    // Title.
    const float titleScale = (w >= 1100.0f && h >= 680.0f) ? 6.0f : 4.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left, y), "DEAD PIXEL", ink, titleScale);
    y += mFont.lineHeight(titleScale) + 2.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left + 2.0f, y), "SURVIVAL", red, titleScale * 0.5f);
    y += mFont.lineHeight(titleScale * 0.5f) + 10.0f;
    mFont.drawText(mSpriteBatch, glm::vec2(left + 2.0f, y), "The town remembers every life you lose in it.", dimInk, 1.0f);
    y += 34.0f;

    // Menu.
    const float rowH = 28.0f;
    const float valueX = left + 220.0f;
    const float rowW = std::min(560.0f, w - left * 2.0f);
    const auto valueOf = [&](int item) -> std::string
    {
        switch (item)
        {
        case 3: return kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)];
        case 4: return kTitleGameModeNames[static_cast<std::size_t>(mTitleGameModeIndex)];
        case 5: return kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)];
        case 6: return kTitleToneNames[static_cast<std::size_t>(mTitleToneIndex)];
        default: return {};
        }
    };
    for (int i = 0; i < static_cast<int>(kTitleMenuItems.size()); ++i)
    {
        const float rowY = y + static_cast<float>(i) * rowH;
        const bool selected = i == mTitleMenuSelection;
        const bool disabled = i == 0 && !mTitleHasCheckpoint;
        if (selected)
        {
            mSpriteBatch.draw(glm::vec2(left - 14.0f, rowY - 5.0f), glm::vec2(rowW, rowH - 2.0f), uiUv,
                              glm::vec4(gold.r, gold.g, gold.b, 0.13f));
            mSpriteBatch.draw(glm::vec2(left - 14.0f, rowY - 5.0f), glm::vec2(3.0f, rowH - 2.0f), uiUv, gold);
        }
        glm::vec4 color = selected ? glm::vec4(1.0f, 0.97f, 0.86f, 1.0f) : glm::vec4(0.78f, 0.77f, 0.72f, 1.0f);
        if (disabled) color = glm::vec4(0.42f, 0.41f, 0.39f, 1.0f);
        mFont.drawText(mSpriteBatch, glm::vec2(left, rowY), kTitleMenuItems[static_cast<std::size_t>(i)], color, 2.0f);

        const std::string value = valueOf(i);
        if (!value.empty())
        {
            mFont.drawText(mSpriteBatch, glm::vec2(valueX, rowY), selected ? "< " + value + " >" : value,
                           selected ? gold : dimInk, 2.0f);
        }
    }
    y += static_cast<float>(kTitleMenuItems.size()) * rowH + 12.0f;

    // What the selected item does.
    const auto& history = mSaveManager.characterHistory();
    std::string about;
    glm::vec4 aboutColor = dimInk;
    switch (mTitleMenuSelection)
    {
    case 0:
        about = mTitleHasCheckpoint
            ? "Pick up where " + mTitleCheckpointName + " left off, day " + std::to_string(mTitleCheckpointDay) + "."
            : "No run in progress in this slot.";
        break;
    case 1:
        about = legacy::runHeadline(mSaveManager.runNumber(), static_cast<int>(history.size())) +
                ". Someone new wakes up in town.";
        break;
    case 2:
        about = mTitleConfirmNewWorld ? "Press Enter again. This town and everything it remembers will be gone."
                                      : "A different town. This one, and everything it remembers, is erased.";
        if (mTitleConfirmNewWorld) aboutColor = red;
        break;
    case 3: about = "Each slot is its own town with its own history."; break;
    case 4: about = kTitleGameModeDescriptions[static_cast<std::size_t>(mTitleGameModeIndex)]; break;
    case 5: about = kTitleModeDescriptions[static_cast<std::size_t>(mTitleModeIndex)]; break;
    case 6: about = kTitleToneDescriptions[static_cast<std::size_t>(mTitleToneIndex)]; break;
    case 7: about = "Every key on one page."; break;
    case 8: about = "Who made this."; break;
    case 9: about = "The town will still be here."; break;
    default: break;
    }
    const std::size_t aboutChars = static_cast<std::size_t>(std::max(16.0f, (rowW - 16.0f) / 12.0f));
    for (const std::string& line : wrap(about, aboutChars))
    {
        mFont.drawText(mSpriteBatch, glm::vec2(left, y), line, aboutColor, 2.0f);
        y += mFont.lineHeight(2.0f) + 4.0f;
    }

    // The town remembers.
    {
        int risenWalking = 0;
        int packsWaiting = 0;
        for (const GraveRecord& grave : mSaveManager.graves())
        {
            if (grave.risen && !grave.laidToRest) ++risenWalking;
            else if (!grave.items.empty()) ++packsWaiting;
        }
        const int graves = static_cast<int>(mSaveManager.graves().size());
        const int survivors = static_cast<int>(mSaveManager.retirements().size());
        const auto plural = [](int n, const char* one, const char* many)
        {
            return std::to_string(n) + " " + (n == 1 ? one : many);
        };

        std::vector<std::pair<std::string, glm::vec4>> lines;
        if (history.empty())
        {
            lines.emplace_back("Nothing yet.", ink);
            lines.emplace_back("Every life you lose", dimInk);
            lines.emplace_back("here stays here.", dimInk);
        }
        else
        {
            lines.emplace_back(plural(static_cast<int>(history.size()), "life", "lives") + ", " +
                                   plural(mSaveManager.totalDaysSurvived(), "day", "days"), ink);
            if (graves > 0) lines.emplace_back(plural(graves, "grave", "graves"), legacyLeadColor(legacy::LeadKind::Grave));
            if (risenWalking > 0) lines.emplace_back(plural(risenWalking, "walks again", "walk again"), legacyLeadColor(legacy::LeadKind::Risen));
            if (packsWaiting > 0) lines.emplace_back(plural(packsWaiting, "pack waiting", "packs waiting"), legacyLeadColor(legacy::LeadKind::Pack));
            if (survivors > 0) lines.emplace_back(plural(survivors, "survivor lives on", "survivors live on"), legacyLeadColor(legacy::LeadKind::Survivor));
        }
        std::string lastLine;
        if (!history.empty())
        {
            const CharacterHistoryRecord& last = history.back();
            lastLine = "Last: " + last.name + (last.retired ? ", retired" : ", " + last.cause);
        }

        const float cardW = 360.0f;
        const float lineH = mFont.lineHeight(2.0f) + 6.0f;
        const float cardH = 40.0f + static_cast<float>(lines.size()) * lineH + (lastLine.empty() ? 0.0f : 22.0f);
        const float cardX = w - left - cardW;
        const float cardY = h - 56.0f - cardH;
        if (cardX > left + rowW + 24.0f || cardY > y + 12.0f)
        {
            mSpriteBatch.draw(glm::vec2(cardX, cardY), glm::vec2(cardW, cardH), uiUv, glm::vec4(0.04f, 0.04f, 0.045f, 0.86f));
            mSpriteBatch.draw(glm::vec2(cardX, cardY), glm::vec2(cardW, 2.0f), uiUv, gold);
            mFont.drawText(mSpriteBatch, glm::vec2(cardX + 18.0f, cardY + 14.0f), "THE TOWN REMEMBERS", gold, 1.0f);
            float ly = cardY + 34.0f;
            for (const auto& [text, color] : lines)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(cardX + 18.0f, ly), truncateHudText(text, 27), color, 2.0f);
                ly += lineH;
            }
            if (!lastLine.empty())
            {
                mFont.drawText(mSpriteBatch, glm::vec2(cardX + 18.0f, ly + 2.0f), truncateHudText(lastLine, 52), dimInk, 1.0f);
            }
        }
    }

    // Footer.
    mFont.drawText(mSpriteBatch, glm::vec2(left, h - 30.0f), "W/S move    A/D change    Enter select", dimInk, 1.0f);
    const std::string seed = std::string(kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)]) +
                             " slot  -  town " + std::to_string(mSaveManager.worldSeed());
    mFont.drawText(mSpriteBatch, glm::vec2(w - left - mFont.measureText(seed, 1.0f).x, h - 30.0f), seed,
                   glm::vec4(dimInk.r, dimInk.g, dimInk.b, 0.7f), 1.0f);

    if (!mTitleStatusMessage.empty() && mTitleStatusTimer > 0.0f)
    {
        const float alpha = std::clamp(mTitleStatusTimer / 0.4f, 0.0f, 1.0f);
        const glm::vec2 size = mFont.measureText(mTitleStatusMessage, 2.0f);
        const glm::vec2 pos(std::floor((w - size.x) * 0.5f), h - 92.0f);
        mSpriteBatch.draw(pos - glm::vec2(14.0f, 8.0f), size + glm::vec2(28.0f, 16.0f), uiUv, glm::vec4(0.04f, 0.04f, 0.045f, 0.9f * alpha));
        mFont.drawText(mSpriteBatch, pos, mTitleStatusMessage, glm::vec4(gold.r, gold.g, gold.b, alpha), 2.0f);
    }

    if (mShowTitleCredits)
    {
        const glm::vec2 size(std::min(560.0f, w - 40.0f), 220.0f);
        const glm::vec2 pos(std::floor((w - size.x) * 0.5f), std::floor((h - size.y) * 0.5f));
        mSpriteBatch.draw(pos, size, uiUv, glm::vec4(0.05f, 0.045f, 0.045f, 0.97f));
        mSpriteBatch.draw(pos, glm::vec2(size.x, 3.0f), uiUv, gold);
        const float cx = pos.x + size.x * 0.5f;
        mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + 24.0f), "CREDITS", gold, 3.0f, TextAlign::Center);
        mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + 74.0f), "Dead Pixel Survival", ink, 2.0f, TextAlign::Center);
        mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + 102.0f), "Design and code: L. Bartolome", dimInk, 2.0f, TextAlign::Center);
        mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + 128.0f), "C++20, SDL2, OpenGL", dimInk, 2.0f, TextAlign::Center);
        mFont.drawText(mSpriteBatch, glm::vec2(cx, pos.y + size.y - 30.0f), "Enter to close", dimInk, 1.0f, TextAlign::Center);
    }
}

void Game::renderControlsPanel()
{
    if (!mShowControls)
    {
        return;
    }

    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const ControlsLayout layout = computeControlsLayout(mWindowWidth, mWindowHeight);
    mSpriteBatch.draw(layout.panelPos, layout.panelSize, uiUv, glm::vec4(0.05f, 0.06f, 0.09f, 0.94f));
    mSpriteBatch.draw(layout.panelPos, glm::vec2(layout.panelSize.x, 2.0f), uiUv, glm::vec4(0.72f, 0.62f, 0.40f, 0.8f));
    mSpriteBatch.draw(
        glm::vec2(layout.panelPos.x + layout.panelSize.x * 0.5f, layout.rowsY),
        glm::vec2(1.0f, ControlsLayout::kRowH * static_cast<float>(ControlsLayout::kRowsPerColumn) - 4.0f),
        uiUv,
        glm::vec4(0.72f, 0.62f, 0.40f, 0.3f));
}

void Game::renderControlsOverlay()
{
    if (!mShowControls)
    {
        return;
    }

    const ControlsLayout layout = computeControlsLayout(mWindowWidth, mWindowHeight);
    const glm::vec4 headerColor(0.94f, 0.86f, 0.60f, 1.0f);
    const glm::vec4 keyColor(0.98f, 0.84f, 0.38f, 1.0f);
    const glm::vec4 textColor(0.80f, 0.80f, 0.76f, 0.95f);

    mFont.drawText(mSpriteBatch, glm::vec2(layout.panelPos.x + layout.panelSize.x * 0.5f, layout.panelPos.y + 18.0f),
                   "CONTROLS", headerColor, 3.0f, TextAlign::Center);

    for (std::size_t i = 0; i < kControlBindings.size(); ++i)
    {
        const std::size_t column = i / ControlsLayout::kRowsPerColumn;
        const std::size_t row = i % ControlsLayout::kRowsPerColumn;
        const float x = layout.panelPos.x + 24.0f + static_cast<float>(column) * (layout.columnWidth + 8.0f);
        const float y = layout.rowsY + static_cast<float>(row) * ControlsLayout::kRowH;
        mFont.drawText(mSpriteBatch, glm::vec2(x, y), kControlBindings[i].key, keyColor, 2.0f);
        mFont.drawText(mSpriteBatch, glm::vec2(x + ControlsLayout::kKeyColumn, y), kControlBindings[i].action,
                       textColor, 2.0f);
    }
}
