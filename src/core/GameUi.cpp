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

void Game::renderDeathOverlay()
{
    if (mRunState != RunState::Dead)
    {
        return;
    }

    const glm::vec4 uiUvRect = mSpriteSheet.uvRect("solid_white");

    mSpriteBatch.draw(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight)),
        uiUvRect,
        glm::vec4(0.08f, 0.02f, 0.02f, 0.65f));

    const glm::vec2 panelSize(360.0f, 120.0f);
    const glm::vec2 panelPos(
        (static_cast<float>(mWindowWidth) - panelSize.x) * 0.5f,
        (static_cast<float>(mWindowHeight) - panelSize.y) * 0.5f);
    mSpriteBatch.draw(panelPos, panelSize, uiUvRect, glm::vec4(0.05f, 0.05f, 0.05f, 0.85f));

    const glm::vec4 graveUvRect = mSpriteSheet.uvRect("env_grave");
    mSpriteBatch.draw(panelPos + glm::vec2(20.0f, 28.0f), glm::vec2(64.0f, 64.0f), graveUvRect, glm::vec4(0.85f, 0.88f, 0.95f, 1.0f));

    if (mDeathStateTimer < kDeathRestartDelaySeconds)
    {
        mSpriteBatch.draw(panelPos + glm::vec2(104.0f, 36.0f), glm::vec2(220.0f, 14.0f), uiUvRect, glm::vec4(0.65f, 0.15f, 0.15f, 0.9f));
    }
    else
    {
        mSpriteBatch.draw(panelPos + glm::vec2(104.0f, 36.0f), glm::vec2(220.0f, 14.0f), uiUvRect, glm::vec4(0.25f, 0.55f, 0.25f, 0.9f));
    }
}

void Game::renderRetirementOverlay()
{
    if (mRunState != RunState::Retired)
    {
        return;
    }

    const glm::vec4 uiUvRect = mSpriteSheet.uvRect("solid_white");

    mSpriteBatch.draw(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight)),
        uiUvRect,
        glm::vec4(0.05f, 0.05f, 0.12f, 0.60f));

    const glm::vec2 panelSize(360.0f, 120.0f);
    const glm::vec2 panelPos(
        (static_cast<float>(mWindowWidth) - panelSize.x) * 0.5f,
        (static_cast<float>(mWindowHeight) - panelSize.y) * 0.5f);
    mSpriteBatch.draw(panelPos, panelSize, uiUvRect, glm::vec4(0.06f, 0.06f, 0.10f, 0.85f));

    if (mDeathStateTimer >= kDeathRestartDelaySeconds)
    {
        mSpriteBatch.draw(panelPos + glm::vec2(20.0f, 90.0f), glm::vec2(320.0f, 14.0f), uiUvRect, glm::vec4(0.25f, 0.45f, 0.55f, 0.9f));
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

    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const float pulse = 0.5f + 0.5f * std::sin(mTitlePulseTimer * 1.2f);
    const TitlePanelLayout layout = computeTitlePanelLayout(mWindowWidth, mWindowHeight);

    // Global dim to push the title UI to the foreground.
    mSpriteBatch.draw(
        glm::vec2(0.0f, 0.0f),
        glm::vec2(w, h),
        uiUv,
        glm::vec4(0.02f, 0.03f, 0.04f, 0.78f));

    // Main briefing panel.
    mSpriteBatch.draw(
        glm::vec2(layout.panelX, layout.panelY),
        glm::vec2(layout.panelW, layout.panelH),
        uiUv,
        glm::vec4(0.07f, 0.08f, 0.09f, 0.95f));

    // Outer border.
    mSpriteBatch.draw(
        glm::vec2(layout.panelX - 1.0f, layout.panelY - 1.0f),
        glm::vec2(layout.panelW + 2.0f, 1.0f),
        uiUv,
        glm::vec4(0.75f, 0.40f, 0.30f, 0.60f));
    mSpriteBatch.draw(
        glm::vec2(layout.panelX - 1.0f, layout.panelY + layout.panelH),
        glm::vec2(layout.panelW + 2.0f, 1.0f),
        uiUv,
        glm::vec4(0.75f, 0.40f, 0.30f, 0.35f));
    mSpriteBatch.draw(
        glm::vec2(layout.panelX - 1.0f, layout.panelY),
        glm::vec2(1.0f, layout.panelH),
        uiUv,
        glm::vec4(0.75f, 0.40f, 0.30f, 0.35f));
    mSpriteBatch.draw(
        glm::vec2(layout.panelX + layout.panelW, layout.panelY),
        glm::vec2(1.0f, layout.panelH),
        uiUv,
        glm::vec4(0.75f, 0.40f, 0.30f, 0.60f));

    // Header stripe.
    mSpriteBatch.draw(
        glm::vec2(layout.panelX, layout.panelY),
        glm::vec2(layout.panelW, 56.0f),
        uiUv,
        glm::vec4(0.20f, 0.08f, 0.06f, 0.94f));

    // Left interaction column.
    mSpriteBatch.draw(
        glm::vec2(layout.leftColX, layout.leftColY),
        glm::vec2(layout.leftColW, layout.leftColH),
        uiUv,
        glm::vec4(0.11f, 0.12f, 0.13f, 0.88f));

    // Right status column.
    mSpriteBatch.draw(
        glm::vec2(layout.rightColX, layout.rightColY),
        glm::vec2(layout.rightColW, layout.rightColH),
        uiUv,
        glm::vec4(0.10f, 0.11f, 0.12f, 0.90f));

    // Strong center divider between action and intel.
    mSpriteBatch.draw(
        glm::vec2(layout.rightColX - 6.0f, layout.leftColY),
        glm::vec2(2.0f, layout.leftColH),
        uiUv,
        glm::vec4(0.30f, 0.20f, 0.17f, 0.55f));

    // Right column section cards for readability.
    mSpriteBatch.draw(
        glm::vec2(layout.rightCardTopX, layout.rightCardTopY),
        glm::vec2(layout.rightCardTopW, layout.rightCardTopH),
        uiUv,
        glm::vec4(0.12f, 0.14f, 0.16f, 0.72f));
    mSpriteBatch.draw(
        glm::vec2(layout.rightCardMidX, layout.rightCardMidY),
        glm::vec2(layout.rightCardMidW, layout.rightCardMidH),
        uiUv,
        glm::vec4(0.12f, 0.14f, 0.16f, 0.68f));
    mSpriteBatch.draw(
        glm::vec2(layout.rightCardBottomX, layout.rightCardBottomY),
        glm::vec2(layout.rightCardBottomW, layout.rightCardBottomH),
        uiUv,
        glm::vec4(0.12f, 0.14f, 0.16f, 0.64f));

    // Menu row strips to make scanning options easier.
    for (int i = 0; i < static_cast<int>(kTitleMenuItems.size()); ++i)
    {
        const float rowY = layout.menuRowY + static_cast<float>(i) * layout.menuRowStep;
        glm::vec4 rowColor = (i % 2 == 0)
            ? glm::vec4(0.15f, 0.16f, 0.18f, 0.48f)
            : glm::vec4(0.12f, 0.13f, 0.15f, 0.40f);

        if (i == 0 && !mTitleHasCheckpoint)
        {
            rowColor = glm::vec4(0.20f, 0.12f, 0.12f, 0.44f);
        }
        if (i == mTitleMenuSelection)
        {
            rowColor = glm::vec4(0.40f, 0.26f, 0.16f, 0.70f);
        }

        mSpriteBatch.draw(
            glm::vec2(layout.menuRowX, rowY),
            glm::vec2(layout.menuRowW, layout.menuRowH),
            uiUv,
            rowColor);
    }

    // Accent pulse strip.
    mSpriteBatch.draw(
        glm::vec2(layout.panelX + layout.panelW - 10.0f, layout.leftColY),
        glm::vec2(4.0f, layout.leftColH),
        uiUv,
        glm::vec4(0.85f, 0.25f + 0.25f * pulse, 0.18f, 0.65f));

    // Light scan lines for a briefing-terminal feel.
    for (int i = 0; i < 14; ++i)
    {
        const float lineY = layout.panelY + 74.0f + static_cast<float>(i) * ((layout.panelH - 100.0f) / 13.0f);
        mSpriteBatch.draw(
            glm::vec2(layout.panelX + 16.0f, lineY),
            glm::vec2(layout.panelW - 32.0f, 1.0f),
            uiUv,
            glm::vec4(0.35f, 0.35f, 0.35f, 0.07f));
    }

    if (mShowTitleCredits)
    {
        mSpriteBatch.draw(
            glm::vec2(layout.leftColX, layout.leftColY),
            glm::vec2(layout.panelW - 24.0f, layout.leftColH),
            uiUv,
            glm::vec4(0.03f, 0.04f, 0.05f, 0.84f));
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
