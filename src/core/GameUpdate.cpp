// Per-tick simulation: input, systems, combat, sleep, animation and particles.
#include "core/GameShared.h"

using namespace gameshared;

void Game::update(float dtSeconds)
{
    handleEscapeKey();

    mMouseWorld =screenToWorld(glm::vec2(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY())));

    mInteractionMessageTimer = std::max(0.0f, mInteractionMessageTimer - dtSeconds);
    if (mInteractionMessageTimer <= 0.0f)
    {
        mInteractionMessage.clear();
    }
    updateDistrictToast(dtSeconds);
    mStructureView.update(dtSeconds, playerTile());

    if (mInput.wasKeyPressed(SDL_SCANCODE_F1))
    {
        mShowNoiseDebug = !mShowNoiseDebug;
    }

    if (mInput.wasKeyPressed(SDL_SCANCODE_F2))
    {
        mShowControls = !mShowControls;
    }

    if (mRunState == RunState::Playing)
    {
        if (mInput.wasKeyPressed(SDL_SCANCODE_M))
        {
            mShowWorldMap = !mShowWorldMap;
            if (mShowWorldMap)
            {
                mShowInventory = false;
                mShowCrafting = false;
                mContainerOpen = false;
            }
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_J))
        {
            mShowNotebook = !mShowNotebook;
            if (mShowNotebook)
            {
                mShowInventory = false;
                mShowCrafting = false;
                mContainerOpen = false;
            }
        }

        const bool minimapZoomOut =
            mInput.wasKeyPressed(SDL_SCANCODE_MINUS) ||
            mInput.wasKeyPressed(SDL_SCANCODE_KP_MINUS);
        const bool minimapZoomIn =
            mInput.wasKeyPressed(SDL_SCANCODE_EQUALS) ||
            mInput.wasKeyPressed(SDL_SCANCODE_KP_PLUS);

        if (!mShowCrafting && !mShowWorldMap && !mShowNotebook && (minimapZoomOut || minimapZoomIn))
        {
            const float zoomFactor = minimapZoomIn ? (1.0f + kMinimapZoomStep) : (1.0f - kMinimapZoomStep * 0.75f);
            mMinimapZoom = std::clamp(mMinimapZoom * zoomFactor, kMinimapZoomMin, kMinimapZoomMax);
        }
    }

    if (mInput.wasKeyPressed(SDL_SCANCODE_F3))
    {
        mShowDistrictDebug = !mShowDistrictDebug;
    }

    // Title screen hub: briefing menu, mode, and save-slot selection.
    if (mRunState == RunState::TitleScreen)
    {
        mTitlePulseTimer += dtSeconds;
        mTitleConfirmNewWorldTimer = std::max(0.0f, mTitleConfirmNewWorldTimer - dtSeconds);
        if (mTitleConfirmNewWorldTimer <= 0.0f)
        {
            mTitleConfirmNewWorld = false;
        }

        mTitleStatusTimer = std::max(0.0f, mTitleStatusTimer - dtSeconds);
        if (mTitleStatusTimer <= 0.0f)
        {
            mTitleStatusMessage.clear();
        }

        // The checkpoint lives on disk: re-read it after input (slot changes,
        // deletes) or once a second, not every tick.
        mTitleCheckpointRefreshTimer -= dtSeconds;
        if (mTitleCheckpointRefreshTimer <= 0.0f || mInput.anyKeyPressed())
        {
            refreshTitleCheckpoint();
        }

        const int menuCount = static_cast<int>(kTitleMenuItems.size());
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            const int previousSelection = mTitleMenuSelection;
            mTitleMenuSelection = wrapIndex(mTitleMenuSelection - 1, menuCount);
            if (mTitleMenuSelection != previousSelection)
            {
                mTitleConfirmNewWorld = false;
            }
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            const int previousSelection = mTitleMenuSelection;
            mTitleMenuSelection = wrapIndex(mTitleMenuSelection + 1, menuCount);
            if (mTitleMenuSelection != previousSelection)
            {
                mTitleConfirmNewWorld = false;
            }
        }

        if (mTitleMenuSelection != 2)
        {
            mTitleConfirmNewWorld = false;
        }

        auto adjustMenuValue = [&](int delta)
        {
            if (mTitleMenuSelection == 3) // Save Slot
            {
                const int oldIndex = mTitleSaveSlotIndex;
                const int newIndex = wrapIndex(mTitleSaveSlotIndex + delta, static_cast<int>(kTitleSaveSlots.size()));
                if (newIndex == oldIndex)
                {
                    return;
                }

                if (loadSlotAndWorld(kTitleSaveSlots[static_cast<std::size_t>(newIndex)]))
                {
                    mTitleSaveSlotIndex = newIndex;
                    mTitleStatusMessage = std::string("Active slot: ") + kTitleSaveLabels[static_cast<std::size_t>(newIndex)];
                    mTitleStatusTimer = 2.0f;
                }
                else
                {
                    mTitleStatusMessage = "Unable to load slot data.";
                    mTitleStatusTimer = 2.5f;
                }
                return;
            }

            if (mTitleMenuSelection == 4) // Game Mode
            {
                mTitleGameModeIndex = wrapIndex(mTitleGameModeIndex + delta, static_cast<int>(kTitleGameModeNames.size()));
                mTitleStatusMessage = std::string("Game Mode: ") + kTitleGameModeNames[static_cast<std::size_t>(mTitleGameModeIndex)];
                mTitleStatusTimer = 2.0f;
                syncPresentationSettingsToSave();
                return;
            }

            if (mTitleMenuSelection == 5) // Threat
            {
                mTitleModeIndex = wrapIndex(mTitleModeIndex + delta, static_cast<int>(kTitleModeNames.size()));
                mTitleStatusMessage = std::string("Threat: ") + kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)];
                mTitleStatusTimer = 2.0f;
                syncPresentationSettingsToSave();
                return;
            }

            if (mTitleMenuSelection == 6) // Tone
            {
                mTitleToneIndex = wrapIndex(mTitleToneIndex + delta, static_cast<int>(kTitleToneNames.size()));
                mTitleStatusMessage = std::string("Tone: ") + kTitleToneNames[static_cast<std::size_t>(mTitleToneIndex)];
                mTitleStatusTimer = 2.0f;
                syncPresentationSettingsToSave();
            }
        };

        if (mInput.wasKeyPressed(SDL_SCANCODE_LEFT) || mInput.wasKeyPressed(SDL_SCANCODE_A))
        {
            adjustMenuValue(-1);
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_RIGHT) || mInput.wasKeyPressed(SDL_SCANCODE_D))
        {
            adjustMenuValue(1);
        }

        const bool activate =
            mInput.wasKeyPressed(SDL_SCANCODE_RETURN) ||
            mInput.wasKeyPressed(SDL_SCANCODE_SPACE);

        if (activate)
        {
            switch (mTitleMenuSelection)
            {
            case 0: // Continue Run
                if (mTitleHasCheckpoint)
                {
                    mIgnoreCheckpointRestoreOnce = false;
                    startNewRun();
                }
                else
                {
                    mTitleStatusMessage = "No checkpoint found for this slot.";
                    mTitleStatusTimer = 2.5f;
                }
                break;

            case 1: // Start New Run
                mSaveManager.deleteCheckpoint();
                mIgnoreCheckpointRestoreOnce = true;
                syncPresentationSettingsToSave();
                startNewRun();
                break;

            case 2: // Start New World
            {
                if (!mTitleConfirmNewWorld)
                {
                    mTitleConfirmNewWorld = true;
                    mTitleConfirmNewWorldTimer = 4.0f;
                    mTitleStatusMessage = "Press Enter again to confirm: this resets world and history for this slot.";
                    mTitleStatusTimer = 3.5f;
                }
                else
                {
                    mTitleConfirmNewWorld = false;
                    mTitleConfirmNewWorldTimer = 0.0f;

                    if (!mSaveManager.resetCurrentSlotWorld(true))
                    {
                        mTitleStatusMessage = "Failed to reset world for this slot.";
                        mTitleStatusTimer = 3.0f;
                        break;
                    }

                    if (!loadSlotAndWorld(kTitleSaveSlots[static_cast<std::size_t>(mTitleSaveSlotIndex)]))
                    {
                        mTitleStatusMessage = "World reset complete, but reload failed.";
                        mTitleStatusTimer = 3.0f;
                        break;
                    }

                    mIgnoreCheckpointRestoreOnce = true;
                    syncPresentationSettingsToSave();
                    startNewRun();
                }
                break;
            }

            case 3: // Save Slot
                adjustMenuValue(1);
                break;

            case 4: // Game Mode
                adjustMenuValue(1);
                break;

            case 5: // Threat
                adjustMenuValue(1);
                break;

            case 6: // Tone
                adjustMenuValue(1);
                break;

            case 7: // Options
                mShowControls = !mShowControls;
                if (mShowControls)
                {
                    mShowTitleCredits = false;
                }
                break;

            case 8: // Credits
                mShowTitleCredits = !mShowTitleCredits;
                if (mShowTitleCredits)
                {
                    mShowControls = false;
                }
                break;

            case 9: // Quit
                mIsRunning = false;
                break;

            default:
                break;
            }
        }
        return;
    }

    // Pause menu input
    if (mRunState == RunState::Paused)
    {
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            mPauseSelection = (mPauseSelection + 4) % 5;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            mPauseSelection = (mPauseSelection + 1) % 5;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_RETURN) || mInput.wasKeyPressed(SDL_SCANCODE_SPACE))
        {
            if (mPauseSelection == 0) // Resume
            {
                mRunState = RunState::Playing;
            }
            else if (mPauseSelection == 1) // Save
            {
                saveCurrentRunCheckpoint("Run checkpoint saved.");
            }
            else if (mPauseSelection == 2) // Back to Menu
            {
                returnToTitleScreen();
            }
            else if (mPauseSelection == 3) // Restart
            {
                startNewRun();
            }
            else // Quit
            {
                mIsRunning = false;
            }
        }
        return;
    }

    if (mRunState == RunState::Dead || mRunState == RunState::Retired)
    {
        mDeathStateTimer += static_cast<double>(dtSeconds);

        const bool restartRequested =
            mInput.wasKeyPressed(SDL_SCANCODE_RETURN) ||
            mInput.wasKeyPressed(SDL_SCANCODE_SPACE) ||
            mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT);

        if (restartRequested && mDeathStateTimer >= kDeathRestartDelaySeconds)
        {
            startNewRun();
        }

        return;
    }

    bool playerAlive = false;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        playerAlive = health.current > 0.0f;
    }

    bool drivingVehicle =
        mActiveVehicleEntity != kInvalidEntity &&
        mWorld.hasComponent<Vehicle>(mActiveVehicleEntity) &&
        mWorld.hasComponent<Transform>(mActiveVehicleEntity);
    if (!drivingVehicle)
    {
        mActiveVehicleEntity = kInvalidEntity;
    }

    bool useInventoryCursorItemRequested = false;

    if (playerAlive && !mShowCrafting)
    {
        mInputSystem.update(mWorld, mInput, mMouseWorld);
    }
    else if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
        velocity.dx = 0.0f;
        velocity.dy = 0.0f;
    }

    if (playerAlive &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Transform>(mPlayerEntity) &&
        mWorld.hasComponent<Inventory>(mPlayerEntity) &&
        mInput.wasKeyPressed(SDL_SCANCODE_V) &&
        !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        Inventory& playerInv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const Transform& playerTransform = mWorld.getComponent<Transform>(mPlayerEntity);
        const glm::vec2 playerCenter(playerTransform.x + kSpriteSize * 0.5f, playerTransform.y + kSpriteSize * 0.5f);

        if (drivingVehicle)
        {
            Vehicle& vehicle = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
            vehicle.playerInside = false;
            vehicle.engineOn = false;
            mActiveVehicleEntity = kInvalidEntity;
            drivingVehicle = false;
            mInteractionMessage = "Exited sedan.";
            mInteractionMessageTimer = 1.7f;
        }
        else
        {
            Entity nearestVehicle = kInvalidEntity;
            float bestDistSq = kVehicleInteractRangePixels * kVehicleInteractRangePixels;
            mWorld.forEach<Vehicle, Transform>(
                [&](Entity entity, Vehicle&, Transform& vt)
                {
                    const glm::vec2 vehicleCenter(vt.x + kSpriteSize * 0.5f, vt.y + kSpriteSize * 0.5f);
                    const glm::vec2 d = vehicleCenter - playerCenter;
                    const float distSq = glm::dot(d, d);
                    if (distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        nearestVehicle = entity;
                    }
                });

            if (nearestVehicle != kInvalidEntity)
            {
                Vehicle& vehicle = mWorld.getComponent<Vehicle>(nearestVehicle);

                const bool refuelRequested =
                    mInput.isKeyDown(SDL_SCANCODE_LSHIFT) ||
                    mInput.isKeyDown(SDL_SCANCODE_RSHIFT);

                if (refuelRequested)
                {
                    if (vehicle.fuel >= vehicle.maxFuel - 0.01f)
                    {
                        mInteractionMessage = "Sedan tank is already full.";
                        mInteractionMessageTimer = 2.0f;
                    }
                    else if (InventorySystem::removeItem(playerInv, 12, 1))
                    {
                        vehicle.fuel = std::min(vehicle.maxFuel, vehicle.fuel + kVehicleRefuelAmount);
                        mInteractionMessage = "Refueled sedan.";
                        mInteractionMessageTimer = 2.0f;
                    }
                    else
                    {
                        mInteractionMessage = "Need a fuel can to refuel.";
                        mInteractionMessageTimer = 2.0f;
                    }
                }
                else if (vehicle.condition < 10.0f)
                {
                    mInteractionMessage = "Sedan is broken.";
                    mInteractionMessageTimer = 2.0f;
                }
                else if (vehicle.fuel <= 0.01f)
                {
                    mInteractionMessage = "Sedan is out of fuel. Shift+V to refuel.";
                    mInteractionMessageTimer = 2.4f;
                }
                else
                {
                    vehicle.playerInside = true;
                    vehicle.engineOn = true;
                    mActiveVehicleEntity = nearestVehicle;
                    drivingVehicle = true;
                    mShowCrafting = false;
                    mContainerOpen = false;
                    mOpenContainerEntity = kInvalidEntity;
                    mInteractionMessage = "Driving sedan.";
                    mInteractionMessageTimer = 1.8f;
                }
            }
        }
    }

    if (mInput.wasKeyPressed(SDL_SCANCODE_K) && !mDebugEntities.empty())
    {
        const Entity entity = mDebugEntities.back();
        mDebugEntities.pop_back();
        mWorld.destroyEntity(entity);
    }

    advanceGameClock(static_cast<double>(dtSeconds) * kGameHoursPerRealSecond, mGameHours, mCurrentDay);

    const NeedsEnvironment needsEnvironment = buildNeedsEnvironment();
    mNeedsSystem.update(mWorld, dtSeconds, needsEnvironment);
    mBleedingSystem.update(mWorld, dtSeconds);
    mWoundSystem.update(mWorld, mPlayerEntity, dtSeconds, mGameHours);

    // Inventory food spoilage
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        const float gameHoursThisTick = static_cast<float>(dtSeconds * kGameHoursPerRealSecond);
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        InventorySystem::degradeFood(inv, mItemDatabase, gameHoursThisTick);
    }

    // Mental state system
    mMentalStateSystem.update(mWorld, mNoiseModel, mPlayerEntity, dtSeconds, mGameHours, mCurrentDay, &mTileMap);

    // Weather system
    mWeatherSystem.update(mWeatherState, dtSeconds, mGameHours, mCurrentDay);

    // Flashlight toggle (Q). Needs a flashlight in the pack.
    if (playerAlive && !mShowCrafting && mInput.wasKeyPressed(SDL_SCANCODE_Q))
    {
        if (playerHasItem("Flashlight"))
        {
            mFlashlightOn = !mFlashlightOn;
            mInteractionMessage = mFlashlightOn ? "Flashlight on. It will be seen." : "Flashlight off.";
        }
        else
        {
            mInteractionMessage = "You have no flashlight.";
        }
        mInteractionMessageTimer = 2.0f;
    }
    if (mFlashlightOn && !playerHasItem("Flashlight"))
    {
        mFlashlightOn = false;
    }

    // Fire interaction key (L): active-slot Molotov ignites, active-slot water suppresses.
    if (playerAlive &&
        !mShowCrafting &&
        !drivingVehicle &&
        !mContainerOpen &&
        mInput.wasKeyPressed(SDL_SCANCODE_L) &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Inventory>(mPlayerEntity) &&
        mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const int activeSlot = std::clamp(inv.activeSlot, 0, Inventory::kMaxSlots - 1);
        const InventorySlot& slot = inv.slots[static_cast<std::size_t>(activeSlot)];

        if (slot.itemId == 26 && slot.count > 0)
        {
            const glm::ivec2 targetTile = worldToTile(mTileMap, mMouseWorld);
            if (targetTile.x >= 0 && targetTile.x < mTileMap.width() &&
                targetTile.y >= 0 && targetTile.y < mTileMap.height() &&
                !mTileMap.isSolid(targetTile.x, targetTile.y))
            {
                bool foundExisting = false;
                mWorld.forEach<Fire, Transform>(
                    [&](Entity, Fire& fire, Transform& ft)
                    {
                        const glm::ivec2 fireTile = worldToTile(mTileMap, glm::vec2(ft.x, ft.y));
                        if (fireTile.x == targetTile.x && fireTile.y == targetTile.y)
                        {
                            fire.intensity = std::min(2.0f, fire.intensity + 0.55f);
                            fire.lifeRemaining = std::max(fire.lifeRemaining, 24.0f);
                            foundExisting = true;
                        }
                    });

                if (!foundExisting)
                {
                    Entity fireEntity = mWorld.createEntity();
                    Transform ft{};
                    ft.x = static_cast<float>(targetTile.x * mTileMap.tileWidth());
                    ft.y = static_cast<float>(targetTile.y * mTileMap.tileHeight());

                    Sprite fs{};
                    fs.textureId = mSpriteSheet.textureId();
                    fs.uvRect = mSpriteSheet.uvRect("env_campfire_lit");
                    fs.color = glm::vec4(1.0f, 0.62f, 0.20f, 0.84f);
                    fs.layer = 6;

                    Fire fire{};
                    fire.intensity = 1.05f;
                    fire.lifeRemaining = 24.0f;
                    fire.spreadTimer = 0.0f;
                    fire.noiseTimer = 0.0f;

                    mWorld.addComponent<Transform>(fireEntity, ft);
                    mWorld.addComponent<Sprite>(fireEntity, fs);
                    mWorld.addComponent<Fire>(fireEntity, fire);
                }

                InventorySystem::removeItem(inv, 26, 1);
                const glm::vec2 noisePos(
                    (static_cast<float>(targetTile.x) + 0.5f) * static_cast<float>(mTileMap.tileWidth()),
                    (static_cast<float>(targetTile.y) + 0.5f) * static_cast<float>(mTileMap.tileHeight()));
                mNoiseModel.emitNoise(noisePos, NoiseTier::Explosive, 0.8f, mPlayerEntity);
                mInteractionMessage = "Molotov ignited.";
                mInteractionMessageTimer = 2.0f;
            }
            else
            {
                mInteractionMessage = "Cannot ignite that tile.";
                mInteractionMessageTimer = 1.8f;
            }
        }
        else if (slot.itemId == 3 && slot.count > 0)
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            const glm::vec2 playerCenter(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);

            Entity nearestFire = kInvalidEntity;
            float bestDistSq = 96.0f * 96.0f;
            mWorld.forEach<Fire, Transform>(
                [&](Entity fireEntity, Fire&, Transform& ft)
                {
                    const glm::vec2 fireCenter(ft.x + kSpriteSize * 0.5f, ft.y + kSpriteSize * 0.5f);
                    const glm::vec2 d = fireCenter - playerCenter;
                    const float distSq = glm::dot(d, d);
                    if (distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        nearestFire = fireEntity;
                    }
                });

            if (nearestFire != kInvalidEntity)
            {
                Fire& fire = mWorld.getComponent<Fire>(nearestFire);
                fire.intensity = std::max(0.0f, fire.intensity - 0.9f);
                fire.lifeRemaining = std::max(0.0f, fire.lifeRemaining - 14.0f);
                InventorySystem::removeItem(inv, 3, 1);
                mInteractionMessage = "Doused nearby fire.";
                mInteractionMessageTimer = 1.8f;
            }
            else
            {
                mInteractionMessage = "No nearby fire to suppress.";
                mInteractionMessageTimer = 1.8f;
            }
        }
    }

    // Fire simulation update (M8 narrow slice).
    {
        auto structureFlammabilityBonus = [&](int tileX, int tileY)
        {
            float bonus = 0.0f;
            mWorld.forEach<Structure, Transform>(
                [&](Entity, Structure& structure, Transform& st)
                {
                    const glm::ivec2 structTile = worldToTile(mTileMap, glm::vec2(st.x, st.y));
                    if (structTile.x != tileX || structTile.y != tileY)
                    {
                        return;
                    }

                    switch (structure.type)
                    {
                    case StructureType::Barricade:
                    case StructureType::NoiseTrap:
                    case StructureType::SleepingArea:
                    case StructureType::SupplyCache:
                        bonus = std::max(bonus, 0.35f);
                        break;
                    case StructureType::Campfire:
                        bonus = std::max(bonus, 0.20f);
                        break;
                    case StructureType::RainCollector:
                        bonus = std::max(bonus, 0.02f);
                        break;
                    }
                });
            return bonus;
        };

        auto tileFlammability = [&](int tileX, int tileY)
        {
            if (tileX < 0 || tileX >= mTileMap.width() || tileY < 0 || tileY >= mTileMap.height())
            {
                return 0.0f;
            }
            if (mTileMap.isSolid(tileX, tileY))
            {
                return 0.0f;
            }

            float value = fireBaseFlammability(mTileMap.getSurfaceType(tileX, tileY));
            value += structureFlammabilityBonus(tileX, tileY);
            if (hasTilePair(mBurnedTiles, tileX, tileY))
            {
                value *= 0.35f;
            }
            return std::clamp(value, 0.0f, 1.0f);
        };

        auto tileAlreadyBurning = [&](int tileX, int tileY)
        {
            bool burning = false;
            mWorld.forEach<Fire, Transform>(
                [&](Entity, Fire&, Transform& ft)
                {
                    if (burning) return;
                    const glm::ivec2 ftile = worldToTile(mTileMap, glm::vec2(ft.x, ft.y));
                    burning = (ftile.x == tileX && ftile.y == tileY);
                });
            return burning;
        };

        std::vector<Entity> extinguish;
        std::vector<glm::ivec2> newIgnitions;

        const float rainSuppression = std::clamp(1.0f - mWeatherState.rainIntensity * 0.82f, 0.10f, 1.0f);
        const float windBoost = 1.0f + mWeatherState.windStrength * 0.32f;

        mWorld.forEach<Fire, Transform, Sprite>(
            [&](Entity fireEntity, Fire& fire, Transform& ft, Sprite& fs)
            {
                const glm::ivec2 fireTile = worldToTile(mTileMap, glm::vec2(ft.x, ft.y));
                if (fireTile.x < 0 || fireTile.x >= mTileMap.width() || fireTile.y < 0 || fireTile.y >= mTileMap.height())
                {
                    extinguish.push_back(fireEntity);
                    return;
                }

                fire.noiseTimer += dtSeconds;
                if (fire.noiseTimer >= 1.35f)
                {
                    fire.noiseTimer = 0.0f;
                    const glm::vec2 noisePos(
                        (static_cast<float>(fireTile.x) + 0.5f) * static_cast<float>(mTileMap.tileWidth()),
                        (static_cast<float>(fireTile.y) + 0.5f) * static_cast<float>(mTileMap.tileHeight()));
                    const NoiseTier tier = fire.intensity >= 1.2f ? NoiseTier::Loud : NoiseTier::Medium;
                    mNoiseModel.emitNoise(noisePos, tier, 0.85f, fireEntity);
                }

                fire.lifeRemaining -= dtSeconds * (1.0f + mWeatherState.rainIntensity * 1.5f);
                fire.intensity = std::clamp(
                    fire.intensity + dtSeconds * (0.05f - mWeatherState.rainIntensity * 0.28f),
                    0.0f,
                    2.0f);

                // Direct fire damage on entities occupying the burning tile.
                mWorld.forEach<Transform, Health>(
                    [&](Entity target, Transform& tt, Health& health)
                    {
                        if (health.current <= 0.0f) return;
                        const glm::ivec2 targetTile = worldToTile(mTileMap, glm::vec2(tt.x, tt.y));
                        if (targetTile.x != fireTile.x || targetTile.y != fireTile.y)
                        {
                            return;
                        }

                        const float damage = (target == mPlayerEntity) ? 14.0f : 18.0f;
                        health.current = std::max(0.0f, health.current - damage * fire.intensity * dtSeconds);
                        if (mWorld.hasComponent<ZombieAI>(target))
                        {
                            mWorld.getComponent<ZombieAI>(target).lastDamagedBy = fireEntity;
                        }
                    });

                // Structures on burning tiles take accelerated damage.
                mWorld.forEach<Structure, Transform>(
                    [&](Entity, Structure& structure, Transform& st)
                    {
                        const glm::ivec2 structureTile = worldToTile(mTileMap, glm::vec2(st.x, st.y));
                        if (structureTile.x != fireTile.x || structureTile.y != fireTile.y)
                        {
                            return;
                        }

                        float multiplier = 1.0f;
                        if (structure.type == StructureType::RainCollector)
                        {
                            multiplier = 0.25f;
                        }
                        else if (structure.type == StructureType::Campfire)
                        {
                            multiplier = 0.55f;
                        }
                        structure.hp = std::max(0.0f, structure.hp - dtSeconds * fire.intensity * 16.0f * multiplier);
                    });

                fire.spreadTimer += dtSeconds;
                const float spreadInterval = std::max(0.55f, 1.5f - fire.intensity * 0.45f);
                if (fire.spreadTimer >= spreadInterval)
                {
                    fire.spreadTimer = 0.0f;
                    static constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& dir : kDirs)
                    {
                        const int nx = fireTile.x + dir[0];
                        const int ny = fireTile.y + dir[1];
                        if (nx < 0 || nx >= mTileMap.width() || ny < 0 || ny >= mTileMap.height())
                        {
                            continue;
                        }
                        if (tileAlreadyBurning(nx, ny))
                        {
                            continue;
                        }

                        const float flammability = tileFlammability(nx, ny);
                        if (flammability <= 0.02f)
                        {
                            continue;
                        }

                        const float chance = std::clamp(
                            flammability * (0.22f + fire.intensity * 0.32f) * rainSuppression * windBoost,
                            0.0f,
                            0.95f);
                        const int salt = static_cast<int>(std::floor(mGameHours * 120.0)) + mCurrentDay * 17;
                        if (deterministic01(nx, ny, salt) < chance)
                        {
                            newIgnitions.emplace_back(nx, ny);
                        }
                    }
                }

                fs.color = glm::vec4(
                    1.0f,
                    std::clamp(0.40f + fire.intensity * 0.30f, 0.0f, 1.0f),
                    std::clamp(0.12f + fire.intensity * 0.14f, 0.0f, 1.0f),
                    std::clamp(0.30f + fire.intensity * 0.34f, 0.0f, 1.0f));

                if (fire.lifeRemaining <= 0.0f || fire.intensity <= 0.05f)
                {
                    addUniqueTilePair(mBurnedTiles, fireTile.x, fireTile.y);
                    extinguish.push_back(fireEntity);
                }
            });

        for (Entity fireEntity : extinguish)
        {
            mWorld.destroyEntity(fireEntity);
        }

        for (const glm::ivec2& tile : newIgnitions)
        {
            if (tileAlreadyBurning(tile.x, tile.y))
            {
                continue;
            }

            Entity fireEntity = mWorld.createEntity();
            Transform ft{};
            ft.x = static_cast<float>(tile.x * mTileMap.tileWidth());
            ft.y = static_cast<float>(tile.y * mTileMap.tileHeight());

            Sprite fs{};
            fs.textureId = mSpriteSheet.textureId();
            fs.uvRect = mSpriteSheet.uvRect("env_campfire_lit");
            fs.color = glm::vec4(1.0f, 0.52f, 0.18f, 0.72f);
            fs.layer = 6;

            Fire fire{};
            fire.intensity = 0.55f;
            fire.lifeRemaining = 16.0f;
            fire.spreadTimer = 0.0f;
            fire.noiseTimer = 0.0f;

            mWorld.addComponent<Transform>(fireEntity, ft);
            mWorld.addComponent<Sprite>(fireEntity, fs);
            mWorld.addComponent<Fire>(fireEntity, fire);
        }
    }

    // Survivor AI system
    mSurvivorAISystem.update(mWorld, mTileMap, mNoiseModel, mPlayerEntity, dtSeconds, mGameHours, mCurrentDay);

    // Survivor interaction system tick
    mSurvivorInteractionSystem.update(mWorld, mPlayerEntity, dtSeconds);

    // District infection daily update
    mDistrictInfectionSystem.dailyUpdate(mWorld, mMapGenerator.layoutData(), mCurrentDay);

    // Build system
    if (!drivingVehicle)
    {
        int structCountBefore = 0;
        mWorld.forEach<Structure>([&](Entity, Structure&) { ++structCountBefore; });
        const std::string buildMsg = mBuildSystem.update(mWorld, mInput, mTileMap, mNoiseModel, mItemDatabase, mSpriteSheet, mPlayerEntity, mMouseWorld, dtSeconds, mGameHours);
        int structCountAfter = 0;
        mWorld.forEach<Structure>([&](Entity, Structure&) { ++structCountAfter; });
        if (structCountAfter > structCountBefore)
        {
            mRunStats.structuresBuilt += (structCountAfter - structCountBefore);
        }
        if (!buildMsg.empty())
        {
            mInteractionMessage = buildMsg;
            mInteractionMessageTimer = 3.0f;
        }
    }

    // Crafting panel toggle (N) and panel controls.
    if (mInput.wasKeyPressed(SDL_SCANCODE_N) && mRunState == RunState::Playing && !drivingVehicle)
    {
        mShowCrafting = !mShowCrafting;
        if (mShowCrafting)
        {
            mShowInventory = false;
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
            mCraftingRecipeCursor = 0;
            mCraftingQuantity = 1;
            mCraftingNotebookView = false;

            if (mWorld.hasComponent<Velocity>(mPlayerEntity))
            {
                Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
                velocity.dx = 0.0f;
                velocity.dy = 0.0f;
            }
        }
        else if (!mCraftQueue.empty())
        {
            mCraftQueue.clear();
            mActiveCraftProgressSeconds = 0.0f;
            mInteractionMessage = "Crafting interrupted.";
            mInteractionMessageTimer = 2.0f;
        }
    }

    if (mShowCrafting && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);

        // Discovery trigger: holding both ingredients together reveals recipe entry.
        const ItemDef* notebookItem = mItemDatabase.findByName("Notebook");
        const bool hasNotebook = notebookItem != nullptr && InventorySystem::countItem(inv, notebookItem->id) > 0;
        for (const CraftingRecipe& recipe : mCraftingSystem.recipes())
        {
            const bool alreadyKnown =
                recipe.startsKnown ||
                mSaveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB) ||
                std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(), CraftingSystem::normalizePair(recipe.inputA, recipe.inputB)) != mSessionDiscoveredRecipes.end();
            if (alreadyKnown)
            {
                continue;
            }

            if (InventorySystem::countItem(inv, recipe.inputA) > 0 && InventorySystem::countItem(inv, recipe.inputB) > 0)
            {
                const auto normalized = CraftingSystem::normalizePair(recipe.inputA, recipe.inputB);
                if (std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(), normalized) == mSessionDiscoveredRecipes.end())
                {
                    mSessionDiscoveredRecipes.push_back(normalized);
                }
                if (hasNotebook)
                {
                    if (mSaveManager.addDiscoveredRecipe(recipe.inputA, recipe.inputB))
                    {
                        mSaveManager.saveDiscoveredRecipes();
                        mInteractionMessage = "Notebook updated: " + recipe.label;
                    }
                }
                else if (mInteractionMessage.empty())
                {
                    mInteractionMessage = "Discovered " + recipe.label + ". Carry a notebook to preserve it.";
                }
                mInteractionMessageTimer = std::max(mInteractionMessageTimer, 2.5f);
            }
        }

        const auto visible = buildVisibleRecipeList(
            mCraftingSystem,
            mSaveManager,
            inv,
            mSessionDiscoveredRecipes,
            mCraftingCategory);

        if (visible.empty())
        {
            mCraftingRecipeCursor = 0;
        }
        else
        {
            mCraftingRecipeCursor = std::clamp(mCraftingRecipeCursor, 0, static_cast<int>(visible.size()) - 1);
        }

        bool craftActivateRequested =
            mInput.wasKeyPressed(SDL_SCANCODE_RETURN) ||
            mInput.wasKeyPressed(SDL_SCANCODE_SPACE);
        bool craftQueueAppendRequested = false;

        if (!mCraftingNotebookView)
        {
            const float w = static_cast<float>(mWindowWidth);
            const float listW = std::max(260.0f, w * 0.17f);
            const float tabY = 78.0f;
            const float tabH = 46.0f;
            const float tabW = listW / 3.0f;

            const float mouseX = static_cast<float>(mInput.mouseX());
            const float mouseY = static_cast<float>(mInput.mouseY());

            const bool overTabStrip =
                mouseX >= 0.0f && mouseX <= listW &&
                mouseY >= tabY && mouseY <= tabY + tabH;

            if (overTabStrip && mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
            {
                const int clickedCategory = std::clamp(
                    static_cast<int>((mouseX / std::max(1.0f, tabW))),
                    0,
                    2);
                if (clickedCategory != mCraftingCategory)
                {
                    mCraftingCategory = clickedCategory;
                    mCraftingRecipeCursor = 0;
                }
            }

            if (!visible.empty())
            {
                const float rowY0 = tabY + tabH + 8.0f;
                const float rowH = 58.0f;
                if (mouseX >= 0.0f && mouseX <= listW && mouseY >= rowY0)
                {
                    const int hovered = static_cast<int>((mouseY - rowY0) / rowH);
                    if (hovered >= 0 && hovered < static_cast<int>(visible.size()))
                    {
                        mCraftingRecipeCursor = hovered;

                        if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
                        {
                            craftActivateRequested = true;
                        }

                        if (mInput.wasMouseButtonPressed(SDL_BUTTON_RIGHT))
                        {
                            craftActivateRequested = true;
                            craftQueueAppendRequested = true;
                        }
                    }
                }
            }
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            if (!visible.empty())
            {
                mCraftingRecipeCursor = (mCraftingRecipeCursor + static_cast<int>(visible.size()) - 1) % static_cast<int>(visible.size());
            }
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            if (!visible.empty())
            {
                mCraftingRecipeCursor = (mCraftingRecipeCursor + 1) % static_cast<int>(visible.size());
            }
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_A) || mInput.wasKeyPressed(SDL_SCANCODE_LEFT))
        {
            mCraftingCategory = (mCraftingCategory + 2) % 3;
            mCraftingRecipeCursor = 0;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_D) || mInput.wasKeyPressed(SDL_SCANCODE_RIGHT))
        {
            mCraftingCategory = (mCraftingCategory + 1) % 3;
            mCraftingRecipeCursor = 0;
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_MINUS) || mInput.wasKeyPressed(SDL_SCANCODE_KP_MINUS))
        {
            mCraftingQuantity = std::max(1, mCraftingQuantity - 1);
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_EQUALS) || mInput.wasKeyPressed(SDL_SCANCODE_KP_PLUS))
        {
            mCraftingQuantity = std::min(10, mCraftingQuantity + 1);
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_J))
        {
            mCraftingNotebookView = !mCraftingNotebookView;
        }

        if (mInput.wasKeyPressed(SDL_SCANCODE_X) || mInput.wasKeyPressed(SDL_SCANCODE_BACKSPACE))
        {
            if (!mCraftQueue.empty())
            {
                mCraftQueue.clear();
                mActiveCraftProgressSeconds = 0.0f;
                mInteractionMessage = "Crafting cancelled.";
                mInteractionMessageTimer = 2.0f;
            }
        }

        if (!visible.empty() && craftActivateRequested)
        {
            const int selectedRecipeIndex = static_cast<int>(visible[static_cast<std::size_t>(mCraftingRecipeCursor)]);
            const CraftingRecipe& recipe = mCraftingSystem.recipes()[static_cast<std::size_t>(selectedRecipeIndex)];

            const bool discovered =
                recipe.startsKnown ||
                mSaveManager.isRecipeDiscovered(recipe.inputA, recipe.inputB) ||
                std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(), CraftingSystem::normalizePair(recipe.inputA, recipe.inputB)) != mSessionDiscoveredRecipes.end();

            if (!discovered)
            {
                mInteractionMessage = "Recipe is still undiscovered.";
                mInteractionMessageTimer = 2.0f;
            }
            else if (recipe.requiresWorkbench && !hasNearbyWorkbench(mWorld, mPlayerEntity))
            {
                mInteractionMessage = "Need nearby workbench surface.";
                mInteractionMessageTimer = 2.5f;
            }
            else
            {
                const bool queueAppend =
                    craftQueueAppendRequested ||
                    mInput.isKeyDown(SDL_SCANCODE_LSHIFT) ||
                    mInput.isKeyDown(SDL_SCANCODE_RSHIFT);

                if (!hasCraftTools(inv, recipe))
                {
                    mInteractionMessage = "Missing required tools.";
                    mInteractionMessageTimer = 2.0f;
                }
                else if (!hasCraftIngredients(inv, recipe, 1))
                {
                    mInteractionMessage = "Missing ingredients.";
                    mInteractionMessageTimer = 2.0f;
                }
                else
                {
                    if (!queueAppend)
                    {
                        mCraftQueue.clear();
                        mActiveCraftProgressSeconds = 0.0f;
                    }

                    if (!mCraftQueue.empty() && mCraftQueue.back().recipeIndex == selectedRecipeIndex)
                    {
                        mCraftQueue.back().remaining += mCraftingQuantity;
                    }
                    else
                    {
                        mCraftQueue.push_back({selectedRecipeIndex, mCraftingQuantity});
                    }

                    mInteractionMessage = queueAppend ? "Added recipe to queue." : "Crafting queued.";
                    mInteractionMessageTimer = 1.8f;
                }
            }
        }

        // Timed crafting progression with interrupt checks.
        if (!mCraftQueue.empty())
        {
            CraftQueueEntry& active = mCraftQueue.front();
            if (active.recipeIndex < 0 || active.recipeIndex >= static_cast<int>(mCraftingSystem.recipes().size()) || active.remaining <= 0)
            {
                mCraftQueue.pop_front();
                mActiveCraftProgressSeconds = 0.0f;
            }
            else
            {
                const CraftingRecipe& recipe = mCraftingSystem.recipes()[static_cast<std::size_t>(active.recipeIndex)];

                bool interrupted = false;
                std::string interruptReason;

                if (mWorld.hasComponent<Velocity>(mPlayerEntity))
                {
                    const Velocity& v = mWorld.getComponent<Velocity>(mPlayerEntity);
                    const float speedSq = v.dx * v.dx + v.dy * v.dy;
                    if (speedSq > 4.0f)
                    {
                        interrupted = true;
                        interruptReason = "Crafting interrupted by movement.";
                    }
                }

                if (!interrupted && recipe.requiresWorkbench && !hasNearbyWorkbench(mWorld, mPlayerEntity))
                {
                    interrupted = true;
                    interruptReason = "Crafting interrupted: left workbench range.";
                }

                if (!interrupted && !hasCraftTools(inv, recipe))
                {
                    interrupted = true;
                    interruptReason = "Crafting interrupted: required tool unavailable.";
                }

                if (!interrupted && !hasCraftIngredients(inv, recipe, 1))
                {
                    interrupted = true;
                    interruptReason = "Crafting interrupted: missing ingredients.";
                }

                if (interrupted)
                {
                    mCraftQueue.clear();
                    mActiveCraftProgressSeconds = 0.0f;
                    mInteractionMessage = interruptReason;
                    mInteractionMessageTimer = 2.0f;
                }
                else
                {
                    const float craftDuration = std::max(0.25f, static_cast<float>(recipe.craftTimeSeconds));
                    mActiveCraftProgressSeconds += dtSeconds;

                    if (mActiveCraftProgressSeconds >= craftDuration)
                    {
                        mActiveCraftProgressSeconds = 0.0f;

                        Inventory preview = inv;
                        if (!InventorySystem::removeItem(preview, recipe.inputA, 1) ||
                            !InventorySystem::removeItem(preview, recipe.inputB, 1))
                        {
                            mCraftQueue.clear();
                            mInteractionMessage = "Crafting failed: ingredients changed.";
                            mInteractionMessageTimer = 2.0f;
                        }
                        else
                        {
                            const AddItemResult addResult =
                                InventorySystem::addItem(preview, mItemDatabase, recipe.outputItemId, recipe.outputCount);
                            if (addResult.added < recipe.outputCount)
                            {
                                mCraftQueue.clear();
                                mInteractionMessage = "Crafting failed: inventory is full.";
                                mInteractionMessageTimer = 2.0f;
                            }
                            else
                            {
                                inv = preview;

                                const auto normalized = CraftingSystem::normalizePair(recipe.inputA, recipe.inputB);
                                if (std::find(mSessionDiscoveredRecipes.begin(), mSessionDiscoveredRecipes.end(), normalized) == mSessionDiscoveredRecipes.end())
                                {
                                    mSessionDiscoveredRecipes.push_back(normalized);
                                }

                                const ItemDef* notebook = mItemDatabase.findByName("Notebook");
                                const bool hasNotebook = notebook != nullptr && InventorySystem::countItem(inv, notebook->id) > 0;
                                if (hasNotebook)
                                {
                                    if (mSaveManager.addDiscoveredRecipe(recipe.inputA, recipe.inputB))
                                    {
                                        mSaveManager.saveDiscoveredRecipes();
                                    }
                                }

                                --active.remaining;
                                const ItemDef* outDef = mItemDatabase.find(recipe.outputItemId);
                                mInteractionMessage = "Crafted " + std::string(outDef ? outDef->name : recipe.label) + ".";
                                if (!hasNotebook)
                                {
                                    mInteractionMessage += " Carry a notebook to preserve discoveries.";
                                }
                                mInteractionMessageTimer = 1.8f;
                                mAudioManager.playSound(SoundId::WeaponHit, 0.35f);

                                if (active.remaining <= 0)
                                {
                                    mCraftQueue.pop_front();
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Inventory toggle (Tab)
    if (mInput.wasKeyPressed(SDL_SCANCODE_TAB))
    {
        if (drivingVehicle)
        {
            mInteractionMessage = "Stop the vehicle before opening inventory.";
            mInteractionMessageTimer = 1.6f;
        }
        else if (mShowCrafting)
        {
            // Keep inventory disabled while crafting panel is open.
        }
        else
        {
        if (mContainerOpen)
        {
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
        }
        else
        {
            mShowInventory = !mShowInventory;
            if (mShowInventory && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
                mInventoryCursor = inv.activeSlot;
            }
        }
        }
    }

    // Container view input
    if (mContainerOpen && mOpenContainerEntity != kInvalidEntity &&
        mWorld.hasComponent<Inventory>(mOpenContainerEntity) &&
        mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& containerInv = mWorld.getComponent<Inventory>(mOpenContainerEntity);
        Inventory& playerInv = mWorld.getComponent<Inventory>(mPlayerEntity);

        // Count filled slots for cursor bounds
        int filledSlots = 0;
        for (const auto& slot : containerInv.slots)
        {
            if (slot.itemId >= 0 && slot.count > 0) ++filledSlots;
        }

        if (filledSlots == 0)
        {
            mContainerOpen = false;
            mOpenContainerEntity = kInvalidEntity;
            mInteractionMessage = "Container empty.";
            mInteractionMessageTimer = 2.0f;
        }
        else
        {
            if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
            {
                mContainerCursor = (mContainerCursor + filledSlots - 1) % filledSlots;
            }
            if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
            {
                mContainerCursor = (mContainerCursor + 1) % filledSlots;
            }
            mContainerCursor = std::clamp(mContainerCursor, 0, filledSlots - 1);

            // E or Enter: take selected item
            if (mInput.wasKeyPressed(SDL_SCANCODE_E) || mInput.wasKeyPressed(SDL_SCANCODE_RETURN))
            {
                // Find the Nth filled slot
                int idx = 0;
                for (auto& slot : containerInv.slots)
                {
                    if (slot.itemId < 0 || slot.count <= 0) continue;
                    if (idx == mContainerCursor)
                    {
                        auto result = InventorySystem::addItem(playerInv, mItemDatabase, slot.itemId, 1);
                        if (result.added > 0)
                        {
                            const ItemDef* item = mItemDatabase.find(slot.itemId);
                            std::string name = item ? item->name : "item";
                            slot.count -= 1;
                            if (slot.count <= 0)
                            {
                                slot.itemId = -1;
                                slot.count = 0;
                                slot.condition = 1.0f;
                            }
                            mInteractionMessage = "Took " + name + ".";
                            mInteractionMessageTimer = 2.0f;
                            mAudioManager.playSound(SoundId::WeaponHit, 0.3f);
                            ++mRunStats.itemsLooted;

                            // Track looted position for world memory
                            if (mWorld.hasComponent<Transform>(mOpenContainerEntity))
                            {
                                const auto& ct = mWorld.getComponent<Transform>(mOpenContainerEntity);
                                const int tx = static_cast<int>(ct.x) / mTileMap.tileWidth();
                                const int ty = static_cast<int>(ct.y) / mTileMap.tileHeight();
                                mSaveManager.addLootedPosition(tx, ty);
                            }
                        }
                        else
                        {
                            mInteractionMessage = "Inventory full.";
                            mInteractionMessageTimer = 2.0f;
                        }
                        break;
                    }
                    ++idx;
                }
            }

            // Escape: close container
            if (mInput.wasKeyPressed(SDL_SCANCODE_ESCAPE))
            {
                mContainerOpen = false;
                mOpenContainerEntity = kInvalidEntity;
            }
        }
    }

    // Inventory cursor navigation (when inventory is open)
    if (mShowInventory && !mShowCrafting && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);

        const InventoryLayout layout = computeInventoryLayout(mWindowWidth, mWindowHeight);
        const int hoveredSlot = layout.slotAt(glm::vec2(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY())));

        if (hoveredSlot >= 0)
        {
            mInventoryCursor = hoveredSlot;

            if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT))
            {
                const auto& hovered = inv.slots[static_cast<std::size_t>(mInventoryCursor)];
                if (hovered.itemId >= 0 && hovered.count > 0)
                {
                    useInventoryCursorItemRequested = true;
                }
            }

            if (mInput.wasMouseButtonPressed(SDL_BUTTON_RIGHT) && mInventoryCursor != inv.activeSlot)
            {
                std::swap(inv.slots[static_cast<std::size_t>(mInventoryCursor)],
                          inv.slots[static_cast<std::size_t>(inv.activeSlot)]);
                mInteractionMessage = "Swapped items.";
                mInteractionMessageTimer = 2.0f;
            }
        }

        // Grid navigation: W/S or Up/Down move a row, Left/Right move a slot.
        constexpr int kRowStep = InventoryLayout::kColumns;
        if (mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W))
        {
            mInventoryCursor = (mInventoryCursor + Inventory::kMaxSlots - kRowStep) % Inventory::kMaxSlots;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S))
        {
            mInventoryCursor = (mInventoryCursor + kRowStep) % Inventory::kMaxSlots;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_LEFT))
        {
            mInventoryCursor = (mInventoryCursor + Inventory::kMaxSlots - 1) % Inventory::kMaxSlots;
        }
        if (mInput.wasKeyPressed(SDL_SCANCODE_RIGHT))
        {
            mInventoryCursor = (mInventoryCursor + 1) % Inventory::kMaxSlots;
        }

        // Enter key: swap cursor slot with active hotbar slot
        if (mInput.wasKeyPressed(SDL_SCANCODE_RETURN) && mInventoryCursor != inv.activeSlot)
        {
            std::swap(inv.slots[static_cast<std::size_t>(mInventoryCursor)],
                      inv.slots[static_cast<std::size_t>(inv.activeSlot)]);
            mInteractionMessage = "Swapped items.";
            mInteractionMessageTimer = 2.0f;
        }
    }

    // Hotbar selection (1-8 keys) when not in build mode
    if (!mShowCrafting && !mBuildSystem.isBuildMode() && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        for (int key = 0; key < 8; ++key)
        {
            if (mInput.wasKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_1 + key)))
            {
                inv.activeSlot = key;
            }
        }

        // Use item (F key or inventory click): use cursor slot when inventory is open, otherwise active hotbar slot
        if (mInput.wasKeyPressed(SDL_SCANCODE_F) || useInventoryCursorItemRequested)
        {
            const int useSlot = mShowInventory ? mInventoryCursor : inv.activeSlot;
            const auto& slot = inv.slots[static_cast<std::size_t>(useSlot)];
            if (slot.itemId >= 0 && slot.count > 0)
            {
                const ItemDef* item = mItemDatabase.find(slot.itemId);
                if (item)
                {
                    if (item->category == ItemCategory::Food && mWorld.hasComponent<Needs>(mPlayerEntity))
                    {
                        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                        const float restore = item->hungerRestore * slot.condition;
                        needs.hunger = std::min(needs.maxHunger, needs.hunger + restore);

                        // Spoiled food causes illness
                        if (slot.condition < 0.3f)
                        {
                            needs.illness = std::min(100.0f, needs.illness + (1.0f - slot.condition) * 20.0f);
                            mInteractionMessage = "Ate spoiled " + item->name + "... feeling sick.";
                        }
                        else
                        {
                            mInteractionMessage = "Ate " + item->name + ".";
                        }

                        InventorySystem::removeItem(inv, slot.itemId, 1);
                        mInteractionMessageTimer = 3.0f;
                        mAudioManager.playSound(SoundId::FoodEat, 0.5f);
                    }
                    else if (item->category == ItemCategory::Drink && mWorld.hasComponent<Needs>(mPlayerEntity))
                    {
                        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                        needs.thirst = std::min(needs.maxThirst, needs.thirst + item->thirstRestore);
                        InventorySystem::removeItem(inv, slot.itemId, 1);
                        mInteractionMessage = "Drank " + item->name + ".";
                        mInteractionMessageTimer = 3.0f;
                        mAudioManager.playSound(SoundId::FoodEat, 0.4f);
                    }
                    else if (item->category == ItemCategory::Medical)
                    {
                        bool used = false;

                        if (item->curesInfection && mWorld.hasComponent<Wounds>(mPlayerEntity))
                        {
                            Wounds& wounds = mWorld.getComponent<Wounds>(mPlayerEntity);
                            for (Wound& wound : wounds.active)
                            {
                                if (!wound.infected)
                                {
                                    continue;
                                }

                                wound.infected = false;
                                wound.infectionTimer = 0.0f;
                                used = true;
                            }
                        }

                        if (item->illnessRelief > 0.0f && mWorld.hasComponent<Needs>(mPlayerEntity))
                        {
                            Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                            const float previousIllness = needs.illness;
                            needs.illness = std::max(0.0f, needs.illness - item->illnessRelief);
                            used = used || needs.illness < previousIllness;
                        }

                        if (item->panicRelief > 0.0f && mWorld.hasComponent<MentalState>(mPlayerEntity))
                        {
                            MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                            const float previousPanic = mental.panic;
                            mental.panic = std::max(0.0f, mental.panic - item->panicRelief);
                            used = used || mental.panic < previousPanic;
                        }

                        if (item->stopsBleeding && mWorld.hasComponent<Bleeding>(mPlayerEntity))
                        {
                            mWorld.removeComponent<Bleeding>(mPlayerEntity);
                            used = true;
                        }

                        if (item->woundTreatment > 0 && mWorld.hasComponent<Wounds>(mPlayerEntity))
                        {
                            Wounds& wounds = mWorld.getComponent<Wounds>(mPlayerEntity);
                            const int treated = treatWorstWounds(wounds, item->woundTreatment);
                            if (treated > 0)
                            {
                                used = true;
                                if (mWorld.hasComponent<Health>(mPlayerEntity))
                                {
                                    Health& health = mWorld.getComponent<Health>(mPlayerEntity);
                                    syncHealthWithWounds(health, wounds);
                                }
                            }
                        }

                        if (item->healthRestore > 0.0f && mWorld.hasComponent<Health>(mPlayerEntity))
                        {
                            Health& health = mWorld.getComponent<Health>(mPlayerEntity);
                            const float healedAmount = std::min(item->healthRestore, health.maximum - health.current);
                            if (healedAmount > 0.0f)
                            {
                                health.current += healedAmount;
                                used = true;
                            }
                        }

                        if (used)
                        {
                            InventorySystem::removeItem(inv, slot.itemId, 1);
                            mInteractionMessage = "Used " + item->name + ".";
                            mInteractionMessageTimer = 3.0f;
                        }
                        else
                        {
                            mInteractionMessage = "You don't need that right now.";
                            mInteractionMessageTimer = 2.0f;
                        }
                    }
                    else if (item->category == ItemCategory::Weapon && mWorld.hasComponent<Combat>(mPlayerEntity))
                    {
                        Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
                        combat.weapon = makeWeaponStats(static_cast<WeaponCategory>(item->weaponCategory));
                        mInteractionMessage = "Equipped " + item->name + ".";
                        mInteractionMessageTimer = 3.0f;
                    }
                }
            }
        }

    }

    // Panic drift: add random velocity jitter proportional to panic level
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        if (mental.panic >= 25.0f)
        {
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            // Scale drift strength: 0 at 25 panic, full at 100
            const float t = (mental.panic - 25.0f) / 75.0f; // 0..1
            const float driftStrength = t * 80.0f; // up to 80 px/s drift
            vel.dx += std::cos(mental.panicDriftAngle) * driftStrength;
            vel.dy += std::sin(mental.panicDriftAngle) * driftStrength;
        }
    }

    // Depression effects: slow movement
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        if (mental.depression > 20.0f)
        {
            const float slowFactor = 1.0f - mental.depression * 0.003f;
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            vel.dx *= slowFactor;
            vel.dy *= slowFactor;
        }
    }

    // Depression crisis penalties: heavy movement slowdown while in crisis state.
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        if (mental.inCrisis)
        {
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            vel.dx *= 0.6f;
            vel.dy *= 0.6f;
        }
    }

    // Encumbrance slowdown
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Inventory>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const float weight = InventorySystem::totalWeight(inv, mItemDatabase);
        if (weight > Inventory::kMaxCarryWeight)
        {
            const float overRatio = std::min((weight - Inventory::kMaxCarryWeight) / 10.0f, 0.6f);
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            vel.dx *= (1.0f - overRatio);
            vel.dy *= (1.0f - overRatio);
        }
    }

    // Vehicle driving overrides on-foot speed profile.
    if (drivingVehicle &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Velocity>(mPlayerEntity) &&
        mWorld.hasComponent<Vehicle>(mActiveVehicleEntity))
    {
        Vehicle& vehicle = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);

        const float driveSpeed = vehicleSpeedFromCondition(vehicle.condition);
        if (driveSpeed <= 0.0f || vehicle.fuel <= 0.01f)
        {
            vehicle.engineOn = false;
            vehicle.playerInside = false;
            mActiveVehicleEntity = kInvalidEntity;
            drivingVehicle = false;
            vel.dx = 0.0f;
            vel.dy = 0.0f;
            mInteractionMessage = "Sedan stalled.";
            mInteractionMessageTimer = 1.8f;
        }
        else
        {
            const float speedSq = vel.dx * vel.dx + vel.dy * vel.dy;
            if (speedSq > 0.001f)
            {
                const float invLen = 1.0f / std::sqrt(speedSq);
                vel.dx = vel.dx * invLen * driveSpeed;
                vel.dy = vel.dy * invLen * driveSpeed;
            }
            else
            {
                vel.dx = 0.0f;
                vel.dy = 0.0f;
            }

            if (mWorld.hasComponent<Facing>(mPlayerEntity))
            {
                Facing& facing = mWorld.getComponent<Facing>(mPlayerEntity);
                facing.running = false;
                facing.crouching = false;
            }
        }
    }

    // Retirement: depression crisis window expired
    if (mMentalStateSystem.shouldForceRetire())
    {
        mMentalStateSystem.clearForceRetire();
        beginRetirementState();
        return;
    }

    // Sleep time-skip: if player is sleeping, fast-forward time and recover needs
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        Sleeping& sleeping = mWorld.getComponent<Sleeping>(mPlayerEntity);
        constexpr float kSleepTimeMultiplier = 8.0f;
        constexpr float kSleepWakeRange = 160.0f;
        constexpr float kSleepHealPerHour = 3.0f;
        const float extraDtSeconds = dtSeconds * (kSleepTimeMultiplier - 1.0f);
        const float gameHoursThisTick = static_cast<float>(dtSeconds * kGameHoursPerRealSecond * kSleepTimeMultiplier);
        sleeping.durationRemaining -= gameHoursThisTick;
        advanceGameClock(gameHoursThisTick, mGameHours, mCurrentDay);

        if (extraDtSeconds > 0.0f)
        {
            mNeedsSystem.update(mWorld, extraDtSeconds, buildNeedsEnvironment());
            mBleedingSystem.update(mWorld, extraDtSeconds);
            mWoundSystem.update(mWorld, mPlayerEntity, extraDtSeconds, mGameHours);

            if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
                InventorySystem::degradeFood(inv, mItemDatabase, extraDtSeconds * static_cast<float>(kGameHoursPerRealSecond));
            }
        }

        if (mWorld.hasComponent<Needs>(mPlayerEntity))
        {
            Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
            needs.sleepDebt = std::max(0.0f, needs.sleepDebt - sleeping.debtRecoveryRate * sleeping.quality * gameHoursThisTick);
            needs.fatigue = std::max(0.0f, needs.fatigue - sleeping.fatigueRecoveryRate * sleeping.quality * gameHoursThisTick);

            const bool canRecoverHealth =
                needs.hunger > 45.0f &&
                needs.thirst > 45.0f &&
                needs.illness < 25.0f &&
                !mWorld.hasComponent<Bleeding>(mPlayerEntity) &&
                (!mWorld.hasComponent<Wounds>(mPlayerEntity) || !hasInfectedWound(mWorld.getComponent<Wounds>(mPlayerEntity)));

            if (canRecoverHealth && mWorld.hasComponent<Health>(mPlayerEntity))
            {
                Health& health = mWorld.getComponent<Health>(mPlayerEntity);
                health.current = std::min(health.maximum, health.current + kSleepHealPerHour * sleeping.quality * gameHoursThisTick);
            }
        }

        // Check for noise interruption (zombies nearby while sleeping)
        bool interrupted = false;
        float wakeRange = kSleepWakeRange;
        if (mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            if (traits.positive == PositiveTrait::LightSleeper) wakeRange *= 1.5f;
        }
        if (mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            const glm::vec2 pc(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);
            mWorld.forEach<ZombieAI, Transform>(
                [&](Entity, ZombieAI&, Transform& zt)
                {
                    const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                    if (glm::distance(pc, zc) < wakeRange)
                    {
                        interrupted = true;
                    }
                });
        }

        if (sleeping.durationRemaining <= 0.0f || interrupted)
        {
            if (interrupted)
            {
                mInteractionMessage = "Woken up by nearby threat!";
            }
            else
            {
                mInteractionMessage = "Slept, but supplies are running down.";

                if (mWorld.hasComponent<MentalState>(mPlayerEntity))
                {
                    MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                    // A full night's sleep is the main way to shake off a bad day.
                    const float depressionRelief = sleeping.quality >= 0.85f
                        ? 8.0f
                        : std::max(2.0f, sleeping.quality * 6.0f);
                    mental.depression = std::max(0.0f, mental.depression - depressionRelief);
                }

                // Checkpoint after a full sleep cycle
                if (mPlayerEntity != kInvalidEntity)
                {
                    RunCheckpoint cp;
                    cp.runSeed = mSaveManager.currentRunSeed();
                    cp.characterName = mCurrentCharacterName;
                    cp.currentDay = mCurrentDay;
                    cp.gameHours = mGameHours;
                    if (mWorld.hasComponent<Transform>(mPlayerEntity))
                    {
                        const auto& t = mWorld.getComponent<Transform>(mPlayerEntity);
                        cp.playerX = t.x; cp.playerY = t.y;
                    }
                    if (mWorld.hasComponent<Health>(mPlayerEntity))
                    {
                        const auto& h = mWorld.getComponent<Health>(mPlayerEntity);
                        cp.healthCurrent = h.current; cp.healthMaximum = h.maximum;
                    }
                    if (mWorld.hasComponent<Needs>(mPlayerEntity))
                        cp.needs = mWorld.getComponent<Needs>(mPlayerEntity);
                    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
                        cp.mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                    if (mWorld.hasComponent<Wounds>(mPlayerEntity))
                        cp.wounds = mWorld.getComponent<Wounds>(mPlayerEntity).active;
                    cp.bleeding = mWorld.hasComponent<Bleeding>(mPlayerEntity);
                    if (mWorld.hasComponent<Inventory>(mPlayerEntity))
                        cp.inventory = mWorld.getComponent<Inventory>(mPlayerEntity);
                    cp.kills = mRunStats.kills;
                    cp.itemsLooted = mRunStats.itemsLooted;
                    cp.structuresBuilt = mRunStats.structuresBuilt;
                    cp.distanceWalked = mRunStats.distanceWalked;
                    mSaveManager.saveCheckpoint(cp);
                    mSaveManager.saveLootedPositions();
                }
            }
            mInteractionMessageTimer = 3.0f;
            mWorld.removeComponent<Sleeping>(mPlayerEntity);
        }
    }

    // Sleep debt collapse: force sleep when exhaustion maxes out
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Needs>(mPlayerEntity) &&
        !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        if (needs.sleepDebt >= needs.maxSleepDebt)
        {
            Sleeping collapse{};
            collapse.durationRemaining = 3.0f;
            collapse.quality = 0.4f;
            mWorld.addComponent<Sleeping>(mPlayerEntity, collapse);
            mInteractionMessage = "Collapsed from exhaustion!";
            mInteractionMessageTimer = 4.0f;
        }
    }

    playerAlive = false;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        playerAlive = health.current > 0.0f;
    }

    if (!playerAlive)
    {
        std::string cause = "Killed by zombies";

        bool diedInFire = false;
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            const glm::ivec2 playerTile = worldToTile(mTileMap, glm::vec2(pt.x, pt.y));
            mWorld.forEach<Fire, Transform>(
                [&](Entity, Fire&, Transform& ft)
                {
                    if (diedInFire) return;
                    const glm::ivec2 fireTile = worldToTile(mTileMap, glm::vec2(ft.x, ft.y));
                    diedInFire = (fireTile.x == playerTile.x && fireTile.y == playerTile.y);
                });
        }

        if (diedInFire)
        {
            cause = "Burned alive";
        }

        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity))
        {
            const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
            if (needs.thirst <= 0.0f)
            {
                cause = "Dehydrated";
            }
            else if (needs.hunger <= 0.0f)
            {
                cause = "Starved";
            }
        }
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Bleeding>(mPlayerEntity))
        {
            cause = "Bled out";
        }

        beginDeathState(cause + " on Day " + std::to_string(mCurrentDay));
        return;
    }

    mProximityPrompt.clear();

    if (playerAlive && !mShowCrafting && !drivingVehicle && !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        // Update proximity prompt for HUD display
        const ProximityPrompt prompt = mInteractionSystem.getProximityPrompt(mWorld, mPlayerEntity);
        mProximityPrompt = prompt.text;

        // Intercept container interactions to open container view instead of auto-loot
        bool interceptedContainer = false;
        if (mInput.wasKeyPressed(SDL_SCANCODE_E) && prompt.entity != kInvalidEntity &&
            mWorld.hasComponent<Interactable>(prompt.entity))
        {
            const Interactable& inter = mWorld.getComponent<Interactable>(prompt.entity);
            if (inter.type == InteractableType::Container && mWorld.hasComponent<Inventory>(prompt.entity))
            {
                mContainerOpen = true;
                mOpenContainerEntity = prompt.entity;
                mContainerCursor = 0;
                mProximityPrompt.clear();
                interceptedContainer = true;
            }
        }

        if (!interceptedContainer)
        {
            const std::string interactionMessage = mInteractionSystem.update(mWorld, mInput, mPlayerEntity, &mItemDatabase);
            if (!interactionMessage.empty())
            {
                mInteractionMessage = interactionMessage;
                mInteractionMessageTimer = 4.0f;

                if (interactionMessage.find("Ate") != std::string::npos ||
                    interactionMessage.find("Drank") != std::string::npos)
                {
                    mAudioManager.playSound(SoundId::FoodEat, 0.5f);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("Picked up") != std::string::npos)
                {
                    mAudioManager.playSound(SoundId::WeaponHit, 0.4f);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("bandage") != std::string::npos)
                {
                    mAudioManager.playSound(SoundId::FoodEat, 0.4f);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("Looted") != std::string::npos)
                {
                    mAudioManager.playSound(SoundId::WeaponHit, 0.3f);
                    auto pos = interactionMessage.find("Looted ");
                    if (pos != std::string::npos)
                    {
                        int n = std::atoi(interactionMessage.c_str() + pos + 7);
                        mRunStats.itemsLooted += std::max(1, n);
                    }
                }
            }
        }
    }

    // --- NPC Survivor Interaction Keybinds ---
    if (playerAlive && !mShowCrafting && !drivingVehicle && !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        // O key: observe nearest survivor
        if (mInput.wasKeyPressed(SDL_SCANCODE_O))
        {
            auto result = mSurvivorInteractionSystem.tryObserve(mWorld, mPlayerEntity);
            if (!result.message.empty())
            {
                mInteractionMessage = result.message;
                mInteractionMessageTimer = 4.0f;
            }
        }

        // G key: approach nearest survivor
        if (mInput.wasKeyPressed(SDL_SCANCODE_G))
        {
            auto result = mSurvivorInteractionSystem.tryApproach(mWorld, mPlayerEntity);
            if (!result.message.empty())
            {
                mInteractionMessage = result.message;
                mInteractionMessageTimer = 4.0f;
            }
        }

        // T key: trade with approached survivor (offers active hotbar slot)
        if (mInput.wasKeyPressed(SDL_SCANCODE_T) &&
            mSurvivorInteractionSystem.currentMode() == SurvivorInteractionMode::Approaching)
        {
            int slot = 0;
            if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                slot = mWorld.getComponent<Inventory>(mPlayerEntity).activeSlot;
            }
            auto result = mSurvivorInteractionSystem.tryTrade(mWorld, mPlayerEntity, mItemDatabase, slot);
            if (!result.message.empty())
            {
                mInteractionMessage = result.message;
                mInteractionMessageTimer = 4.0f;
            }

            if (result.tradeAccepted && mWorld.hasComponent<MentalState>(mPlayerEntity))
            {
                MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                mental.depression = std::max(0.0f, mental.depression - result.depressionRelief);
                mental.panic = std::max(0.0f, mental.panic - result.panicRelief);
            }
        }
    }

    // Track pre-combat state for audio triggers
    const float prevPlayerHp = (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
        ? mWorld.getComponent<Health>(mPlayerEntity).current
        : 0.0f;
    const AttackPhase prevPhase = (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Combat>(mPlayerEntity))
        ? mWorld.getComponent<Combat>(mPlayerEntity).phase
        : AttackPhase::Idle;

    if (!mShowCrafting && !drivingVehicle)
    {
        mCombatSystem.update(mWorld, mInput, mTileMap, mNoiseModel, dtSeconds);
    }
    else if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Combat>(mPlayerEntity))
    {
        Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
        combat.phase = AttackPhase::Idle;
        combat.phaseTimer = 0.0f;
        combat.recoveryDelayTimer = 0.0f;
        combat.hitProcessed = false;
        combat.grabState = GrabState::None;
        combat.grabTarget = kInvalidEntity;
        combat.grabTimer = 0.0f;
    }

    // Desensitisation gain: check for zombie deaths after combat
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        mWorld.forEach<ZombieAI, Health>(
            [&](Entity, ZombieAI& zombieAI, Health& h)
            {
                // Zombie died since last check: only the player's own kills count.
                if (h.current <= 0.0f && h.current > -999.0f)
                {
                    if (zombieAI.lastDamagedBy == mPlayerEntity)
                    {
                        mental.desensitisation = std::min(100.0f, mental.desensitisation + 1.5f);
                        ++mRunStats.kills;
                    }
                    h.current = -1000.0f; // mark as counted
                }
            });
    }

    // Combat audio
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Combat>(mPlayerEntity))
    {
        const Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
        if (combat.phase == AttackPhase::Windup && prevPhase == AttackPhase::Idle)
        {
            mAudioManager.playSound(SoundId::WeaponSwing, 0.5f);
        }
    }
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const float curHp = mWorld.getComponent<Health>(mPlayerEntity).current;
        if (curHp < prevPlayerHp && curHp > 0.0f)
        {
            mAudioManager.playSound(SoundId::PlayerHurt, 0.6f);
        }
    }

    // Threat-mode runtime tuning layer for zombie behavior.
    {
        const SaveThreatMode threatMode = saveThreatModeFromIndex(mTitleModeIndex);
        const std::uint64_t runSeed = mSaveManager.currentRunSeed();
        mWorld.forEach<ZombieAI>(
            [&](Entity zombieEntity, ZombieAI& zombieAI)
            {
                const ThreatBehaviorTuning tuning = computeThreatBehaviorTuning(threatMode, mCurrentDay, zombieEntity, runSeed);
                zombieAI.moveSpeed = zombieAI.baseMoveSpeed * tuning.speedScale;
                zombieAI.visionRangeTiles = 8.0f * tuning.visionScale;
            });
    }

    // Threat director: migration toward the player's area and population growth.
    {
        std::vector<ThreatDirectorSystem::SpawnRequest> spawns;
        mThreatDirector.update(mWorld, mTileMap, mPlayerEntity, dtSeconds, mGameHours, mCurrentDay,
                               &mDistrictInfectionSystem, spawns);
        for (const auto& request : spawns)
        {
            EntityFactory::ZombieParams params;
            params.topLeftWorld = request.topLeftWorld;
            params.health = request.health;
            params.baseMoveSpeed = request.baseMoveSpeed;
            params.spawnDay = mCurrentDay;
            EntityFactory::spawnZombie(mWorld, mSpriteSheet, params);
        }
    }

    mZombieAISystem.setPlayerLit(mFlashlightOn);
    mZombieAISystem.update(
        mWorld,
        mTileMap,
        mNoiseModel,
        mPlayerEntity,
        dtSeconds,
        mGameHours,
        mCurrentDay,
        &mDistrictInfectionSystem);

    // Zombie audio: growls from chasing zombies, attack sounds
    mZombieGrowlTimer = std::max(0.0f, mZombieGrowlTimer - dtSeconds);
    if (mZombieGrowlTimer <= 0.0f && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
        const glm::vec2 pc(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);

        mWorld.forEach<ZombieAI, Transform, Health>(
            [&](Entity, ZombieAI& zai, Transform& zt, Health& zh)
            {
                if (mZombieGrowlTimer > 0.0f) return;
                if (zh.current <= 0.0f) return;
                if (zai.state != ZombieState::Chase && zai.state != ZombieState::Attack) return;

                const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                const float dist = glm::distance(pc, zc);
                if (dist < 256.0f)
                {
                    const SoundId sound = (zai.state == ZombieState::Attack) ? SoundId::ZombieAttack : SoundId::ZombieGrowl;
                    mAudioManager.playSoundAtWorld(sound, zc.x, pc.x, static_cast<float>(mWindowWidth), 0.5f);
                    mZombieGrowlTimer = 3.0f;
                }
            });
    }
    if (mShowCrafting && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        vel.dx = 0.0f;
        vel.dy = 0.0f;
    }

    mMovementSystem.update(mWorld, dtSeconds);
    mProjectileSystem.update(mWorld, mTileMap, mNoiseModel, dtSeconds);
    mCollisionSystem.update(mWorld, mTileMap, dtSeconds);

    // Post-physics vehicle state update.
    if (drivingVehicle &&
        mActiveVehicleEntity != kInvalidEntity &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Vehicle>(mActiveVehicleEntity) &&
        mWorld.hasComponent<Transform>(mActiveVehicleEntity) &&
        mWorld.hasComponent<Transform>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Vehicle& vehicle = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
        Transform& vehicleTransform = mWorld.getComponent<Transform>(mActiveVehicleEntity);
        const Transform& playerTransform = mWorld.getComponent<Transform>(mPlayerEntity);
        const Velocity& playerVelocity = mWorld.getComponent<Velocity>(mPlayerEntity);

        // Keep the vehicle entity aligned with the player's world position while occupied.
        vehicleTransform.x = playerTransform.x;
        vehicleTransform.y = playerTransform.y;

        const float speedSq = playerVelocity.dx * playerVelocity.dx + playerVelocity.dy * playerVelocity.dy;
        if (speedSq > 1.0f)
        {
            const float weatherWear = 1.0f + mWeatherState.rainIntensity * 0.6f;
            vehicle.fuel = std::max(0.0f, vehicle.fuel - dtSeconds * kVehicleFuelPerSecond * weatherWear);
            vehicle.condition = std::max(0.0f, vehicle.condition - dtSeconds * 0.18f * weatherWear);
        }

        vehicle.collisionDamageCooldown = std::max(0.0f, vehicle.collisionDamageCooldown - dtSeconds);

        const glm::vec2 vehicleCenter(vehicleTransform.x + kSpriteSize * 0.5f, vehicleTransform.y + kSpriteSize * 0.5f);
        if (speedSq > 20.0f && vehicle.collisionDamageCooldown <= 0.0f)
        {
            bool hitZombie = false;
            mWorld.forEach<ZombieAI, Transform, Health>(
                [&](Entity, ZombieAI& zai, Transform& zt, Health& zh)
                {
                    if (hitZombie || zh.current <= 0.0f)
                    {
                        return;
                    }

                    const glm::vec2 zombieCenter(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                    if (glm::distance(vehicleCenter, zombieCenter) <= kVehicleCollisionRangePixels)
                    {
                        zh.current = std::max(0.0f, zh.current - 65.0f);
                        zai.lastDamagedBy = mPlayerEntity; // only the player drives
                        vehicle.condition = std::max(0.0f, vehicle.condition - 6.0f);
                        vehicle.collisionDamageCooldown = 0.22f;
                        hitZombie = true;
                        mAudioManager.playSound(SoundId::WeaponHit, 0.35f);
                    }
                });
        }

        if (vehicle.fuel <= 0.01f || vehicle.condition < 10.0f)
        {
            vehicle.engineOn = false;
            vehicle.playerInside = false;
            mActiveVehicleEntity = kInvalidEntity;
            drivingVehicle = false;
            if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
            {
                Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
                vel.dx = 0.0f;
                vel.dy = 0.0f;
            }
            mInteractionMessage = (vehicle.condition < 10.0f) ? "Sedan broke down." : "Sedan ran out of fuel.";
            mInteractionMessageTimer = 2.2f;
        }
    }

    if (mRunState == RunState::Playing &&
        mTitleGameModeIndex == 0 &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Transform>(mPlayerEntity) &&
        !mCampaignObjective.completed)
    {
        if (mCampaignObjective.active &&
            mCampaignObjective.infectionDeadlineDay > 0 &&
            mCurrentDay > mCampaignObjective.infectionDeadlineDay)
        {
            mCampaignObjective.active = false;
            mCampaignObjective.completed = false;
            syncPresentationSettingsToSave();

            WorldMemoryEventRecord event;
            event.type = WorldMemoryEventType::Other;
            event.tileX = mCampaignObjective.tileX;
            event.tileY = mCampaignObjective.tileY;
            event.day = mCurrentDay;
            event.runIndex = mSaveManager.runNumber();
            event.detail = "campaign objective expired";
            mSaveManager.addWorldMemoryEvent(event);

            beginDeathState("Campaign failed: infection deadline reached on Day " + std::to_string(mCurrentDay));
            return;
        }

        if (mCampaignObjective.active)
        {
            const Transform& playerTransform = mWorld.getComponent<Transform>(mPlayerEntity);
            const int playerTileX = static_cast<int>(std::floor(playerTransform.x / static_cast<float>(std::max(1, mTileMap.tileWidth()))));
            const int playerTileY = static_cast<int>(std::floor(playerTransform.y / static_cast<float>(std::max(1, mTileMap.tileHeight()))));
            const int manhattan = std::abs(mCampaignObjective.tileX - playerTileX) + std::abs(mCampaignObjective.tileY - playerTileY);

            if (manhattan <= 1)
            {
                mCampaignObjective.active = false;
                mCampaignObjective.completed = true;
                syncPresentationSettingsToSave();

                WorldMemoryEventRecord event;
                event.type = WorldMemoryEventType::Other;
                event.tileX = mCampaignObjective.tileX;
                event.tileY = mCampaignObjective.tileY;
                event.day = mCurrentDay;
                event.runIndex = mSaveManager.runNumber();
                event.detail = "campaign objective secured";
                mSaveManager.addWorldMemoryEvent(event);

                beginRetirementState("Secured campaign objective on Day " + std::to_string(mCurrentDay));
                return;
            }
        }
    }

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        if (health.current <= 0.0f)
        {
            beginDeathState("Killed by zombies on Day " + std::to_string(mCurrentDay));
            return;
        }
    }

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Transform& playerTransform = mWorld.getComponent<Transform>(mPlayerEntity);
        const glm::vec2 playerCenter(
            playerTransform.x + kSpriteSize * 0.5f,
            playerTransform.y + kSpriteSize * 0.5f);

        // Track distance walked
        const float dx = playerTransform.x - mRunStats.prevX;
        const float dy = playerTransform.y - mRunStats.prevY;
        mRunStats.distanceWalked += std::sqrt(dx * dx + dy * dy);
        mRunStats.prevX = playerTransform.x;
        mRunStats.prevY = playerTransform.y;

        mCamera.follow(playerCenter, dtSeconds, 7.5f);
        mCamera.clampToBounds(mTileMap.pixelSize());
    }

    // Update player animation based on movement and facing
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Animation>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity) &&
        mWorld.hasComponent<Facing>(mPlayerEntity))
    {
        Animation& anim = mWorld.getComponent<Animation>(mPlayerEntity);
        const Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        const Facing& fac = mWorld.getComponent<Facing>(mPlayerEntity);

        const float speed = vel.dx * vel.dx + vel.dy * vel.dy;
        std::string desired = "player_idle";
        if (speed > 4.0f)
        {
            if (std::abs(vel.dx) > std::abs(vel.dy))
            {
                desired = "player_walk_right";
            }
            else if (vel.dy < 0.0f)
            {
                desired = "player_walk_up";
            }
            else
            {
                desired = "player_walk_down";
            }
        }
        else if (fac.crouching)
        {
            desired = "player_crouch";
        }

        if (anim.currentAnim != desired)
        {
            anim.currentAnim = desired;
            anim.currentFrame = 0;
            anim.frameTimer = 0.0f;
            anim.finished = false;
        }
    }

    // Update zombie animations based on movement
    for (const Entity entity : mWorld.livingEntities())
    {
        if (!mWorld.hasComponent<ZombieAI>(entity) ||
            !mWorld.hasComponent<Animation>(entity) ||
            !mWorld.hasComponent<Velocity>(entity))
        {
            continue;
        }

        Animation& anim = mWorld.getComponent<Animation>(entity);
        const Velocity& vel = mWorld.getComponent<Velocity>(entity);

        const float speed = vel.dx * vel.dx + vel.dy * vel.dy;
        std::string desired = "zombie_idle";
        if (speed > 1.0f)
        {
            if (std::abs(vel.dx) > std::abs(vel.dy))
            {
                desired = "zombie_walk_right";
            }
            else if (vel.dy < 0.0f)
            {
                desired = "zombie_walk_up";
            }
            else
            {
                desired = "zombie_walk_down";
            }
        }

        if (anim.currentAnim != desired)
        {
            anim.currentAnim = desired;
            anim.currentFrame = 0;
            anim.frameTimer = 0.0f;
            anim.finished = false;
        }
    }

    mAnimationSystem.update(mWorld, mSpriteSheet, dtSeconds);

    // Particle system update
    mParticleSystem.update(dtSeconds);

    // Spawn particles: blood on entity damage
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const float curHp = mWorld.getComponent<Health>(mPlayerEntity).current;
        if (curHp < prevPlayerHp && curHp > 0.0f)
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            mParticleSystem.spawnBlood(
                glm::vec2(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f), 10);
        }
    }

    // Spawn particles: dust when player moves
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Velocity>(mPlayerEntity) &&
        mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        const float speed = vel.dx * vel.dx + vel.dy * vel.dy;
        if (speed > 100.0f)
        {
            static float dustTimer = 0.0f;
            dustTimer += dtSeconds;
            if (dustTimer > 0.15f)
            {
                dustTimer = 0.0f;
                const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
                mParticleSystem.spawnDust(
                    glm::vec2(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize), 2);
            }
        }
    }

    // Spawn particles: campfire embers
    {
        static float emberTimer = 0.0f;
        emberTimer += dtSeconds;
        if (emberTimer > 0.3f)
        {
            emberTimer = 0.0f;
            mWorld.forEach<Structure, Transform>(
                [&](Entity, Structure& structure, Transform& transform)
                {
                    if (structure.type != StructureType::Campfire || structure.hp <= 0.0f)
                    {
                        return;
                    }
                    mParticleSystem.spawnEmbers(
                        glm::vec2(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f), 2);
                });
        }
    }

    updateNoiseModel(dtSeconds);
}

void Game::updateDistrictToast(float dtSeconds)
{
    mHudClock += dtSeconds;
    mDistrictToastTimer = std::max(0.0f, mDistrictToastTimer - dtSeconds);

    if (mRunState != RunState::Playing ||
        mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        return;
    }

    const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
    const int tileX = static_cast<int>(pt.x) / std::max(1, mTileMap.tileWidth());
    const int tileY = static_cast<int>(pt.y) / std::max(1, mTileMap.tileHeight());
    const int districtId = mTileMap.getDistrictId(tileX, tileY);
    if (districtId == mHudDistrictId)
    {
        return;
    }

    mHudDistrictId = districtId;
    if (districtId <= 0)
    {
        return;
    }

    int districtType = static_cast<int>(DistrictType::Wilderness);
    for (const auto& def : mMapGenerator.layoutData().districts)
    {
        if (def.id == districtId)
        {
            districtType = def.type;
            break;
        }
    }

    const int infection = static_cast<int>(std::round(mDistrictInfectionSystem.getInfection(districtId)));
    const bool overwhelmed = mDistrictInfectionSystem.isOverwhelmed(districtId);
    mDistrictToastText = std::string(districtTypeLabel(districtType)) + " district - "
        + (overwhelmed ? std::string("OVERWHELMED") : std::to_string(infection) + "% infected");
    mDistrictToastDanger = overwhelmed || infection >= 60;
    mDistrictToastTimer = 4.0f;
}

void Game::updateNoiseModel(float dtSeconds)
{
    mNoiseModel.update(dtSeconds);

    if (mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Transform>(mPlayerEntity) ||
        !mWorld.hasComponent<Velocity>(mPlayerEntity) ||
        !mWorld.hasComponent<Facing>(mPlayerEntity))
    {
        mMovementNoiseTier = NoiseTier::None;
        mNoiseEmitTimer = 0.0f;
        return;
    }

    const Transform& transform = mWorld.getComponent<Transform>(mPlayerEntity);
    const Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
    const Facing& facing = mWorld.getComponent<Facing>(mPlayerEntity);

    if (mActiveVehicleEntity != kInvalidEntity &&
        mWorld.hasComponent<Vehicle>(mActiveVehicleEntity) &&
        mWorld.hasComponent<Transform>(mActiveVehicleEntity))
    {
        Vehicle& vehicle = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
        if (vehicle.engineOn && vehicle.playerInside && vehicle.fuel > 0.01f && vehicle.condition >= 10.0f)
        {
            const Transform& vehicleTransform = mWorld.getComponent<Transform>(mActiveVehicleEntity);
            const glm::vec2 noisePos(vehicleTransform.x + kSpriteSize * 0.5f, vehicleTransform.y + kSpriteSize * 0.5f);

            vehicle.engineNoiseTimer += dtSeconds;
            const float interval = std::max(0.22f, 0.36f - std::clamp(vehicle.condition / 100.0f, 0.0f, 1.0f) * 0.10f);
            if (vehicle.engineNoiseTimer >= interval)
            {
                vehicle.engineNoiseTimer = 0.0f;
                const NoiseTier tier = (vehicle.condition < 40.0f) ? NoiseTier::Explosive : NoiseTier::Loud;
                mNoiseModel.emitNoise(noisePos, tier, 0.95f, mActiveVehicleEntity);
                mAudioManager.playSound(SoundId::FootstepLoud, 0.16f);
            }

            mMovementNoiseTier = NoiseTier::Loud;
            mNoiseEmitTimer = 0.0f;
            return;
        }
    }

    const float speedSquared = velocity.dx * velocity.dx + velocity.dy * velocity.dy;
    if (speedSquared < 4.0f)
    {
        mMovementNoiseTier = NoiseTier::None;
        mNoiseEmitTimer = 0.0f;
        return;
    }

    NoiseTier noiseTier = facing.running ? NoiseTier::Medium : NoiseTier::Soft;
    if (facing.crouching)
    {
        noiseTier = NoiseModel::adjustTier(noiseTier, -1);
    }

    const glm::vec2 feetPosition(
        transform.x + kSpriteSize * 0.5f,
        transform.y + kPlayerFootOffsetY);

    const int tileX = static_cast<int>(std::floor(feetPosition.x / static_cast<float>(mTileMap.tileWidth())));
    const int tileY = static_cast<int>(std::floor(feetPosition.y / static_cast<float>(mTileMap.tileHeight())));
    const SurfaceType surface = mTileMap.getSurfaceType(tileX, tileY);

    noiseTier = NoiseModel::adjustTier(noiseTier, surfaceNoiseModifier(surface));

    // Clumsy trait: +1 noise tier on footsteps
    if (mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
        if (traits.negative == NegativeTrait::Clumsy)
        {
            noiseTier = NoiseModel::adjustTier(noiseTier, 1);
        }
    }

    mMovementNoiseTier = noiseTier;

    if (noiseTier == NoiseTier::None)
    {
        mNoiseEmitTimer = 0.0f;
        return;
    }

    const float emissionInterval = facing.running ? 0.14f : (facing.crouching ? 0.28f : 0.22f);

    mNoiseEmitTimer += dtSeconds;
    if (mNoiseEmitTimer < emissionInterval)
    {
        return;
    }

    mNoiseEmitTimer = 0.0f;
    mNoiseModel.emitNoise(feetPosition, noiseTier, tierDurationSeconds(noiseTier), mPlayerEntity);

    // Footstep audio
    SoundId footstep = SoundId::FootstepNormal;
    if (noiseTier == NoiseTier::Soft || facing.crouching)
    {
        footstep = SoundId::FootstepSoft;
    }
    else if (noiseTier == NoiseTier::Loud || noiseTier == NoiseTier::Explosive || facing.running)
    {
        footstep = SoundId::FootstepLoud;
    }
    mAudioManager.playSound(footstep, 0.12f);
}
