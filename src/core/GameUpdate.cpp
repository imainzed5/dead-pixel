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
        updateTitleScreen(dtSeconds);
        return;
    }

    // Pause menu input
    if (mRunState == RunState::Paused)
    {
        updatePauseMenu();
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

    handleVehicleKey(playerAlive);
    drivingVehicle = mActiveVehicleEntity != kInvalidEntity;

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

    updateFire(dtSeconds, playerAlive, drivingVehicle);

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

    updateCraftingPanel(dtSeconds, drivingVehicle);

    updateInventoryInput(drivingVehicle);

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

    applyVehicleDriving();
    drivingVehicle = mActiveVehicleEntity != kInvalidEntity;

    // Retirement: depression crisis window expired
    if (mMentalStateSystem.shouldForceRetire())
    {
        mMentalStateSystem.clearForceRetire();
        beginRetirementState();
        return;
    }

    updateSleep(dtSeconds);

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
        else if (cause == "Killed by zombies" && mPlayerEntity != kInvalidEntity &&
                 mWorld.hasComponent<Wounds>(mPlayerEntity) &&
                 hasInfectedWound(mWorld.getComponent<Wounds>(mPlayerEntity)))
        {
            cause = "Succumbed to infection";
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

        // A door within reach takes E before anything else nearby.
        bool interceptedContainer = handleDoorInteraction(prompt);
        if (!interceptedContainer && mInput.wasKeyPressed(SDL_SCANCODE_E) && prompt.entity != kInvalidEntity &&
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
        syncEquippedWeapon();
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

    // Clear away the dead however they died. Melee kills are removed by the
    // combat system; burned or shot zombies used to stay standing, frozen,
    // and kept anyone nearby from sleeping.
    {
        std::vector<Entity> dead;
        mWorld.forEach<ZombieAI, Health>(
            [&](Entity zombie, ZombieAI&, Health& h)
            {
                if (h.current <= 0.0f) dead.push_back(zombie);
            });
        for (const Entity zombie : dead)
        {
            mWorld.destroyEntity(zombie);
        }
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
            params.health = request.health * toneTuning(saveTonePresetFromIndex(mTitleToneIndex)).zombieHealthScale;
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
        &mDistrictInfectionSystem,
        &mDoorBlows);
    updateDoors(dtSeconds);

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

    updateVehicleAfterPhysics(dtSeconds);
    drivingVehicle = mActiveVehicleEntity != kInvalidEntity;

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

    updateLegacy(dtSeconds);
    updateAnimations(dtSeconds);

    updateParticles(dtSeconds, prevPlayerHp);

    updateNoiseModel(dtSeconds);
}

// Sleep time-skip: fast-forward time and recover while asleep.
void Game::updateSleep(float dtSeconds)
{
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
                    mSaveManager.saveCheckpoint(buildRunCheckpoint());
                    mSaveManager.saveLootedPositions();
                }
            }
            mInteractionMessageTimer = 3.0f;
            mWorld.removeComponent<Sleeping>(mPlayerEntity);
        }
    }
}

namespace
{
// Which way a character faces on screen, from a direction in world space. The
// iso view puts world (x, y) at screen (x - y, (x + y) / 2). Frames are drawn
// facing right: away picks the back view, flip mirrors them to face left.
// Near straight up or down the flip is kept, and near straight left or right
// the view is kept, so a character walking a screen axis does not flicker.
void faceToward(const glm::vec2& worldDir, bool& away, bool& flip)
{
    const float sx = worldDir.x - worldDir.y;
    const float sy = (worldDir.x + worldDir.y) * 0.5f;
    if (sx * sx + sy * sy < 1e-6f)
    {
        return;
    }
    if (std::abs(sy) > 0.2f * std::abs(sx))
    {
        away = sy < 0.0f;
    }
    if (std::abs(sx) > 0.2f * std::abs(sy))
    {
        flip = sx < 0.0f;
    }
}

// Switch animation. Turning mid-stride (walk_front to walk_back) keeps the
// step; anything else starts the new cycle from its first frame.
void playAnimation(Animation& anim, const std::string& desired)
{
    if (anim.currentAnim == desired)
    {
        return;
    }
    const auto cycle = [](const std::string& name) { return name.substr(0, name.rfind('_')); };
    const bool sameCycle = cycle(anim.currentAnim) == cycle(desired);
    anim.currentAnim = desired;
    if (!sameCycle)
    {
        anim.currentFrame = 0;
        anim.frameTimer = 0.0f;
    }
    anim.finished = false;
}

const char* viewSuffix(bool away)
{
    return away ? "_back" : "_front";
}
} // namespace

// Character animation: which pose, which way they face on screen.
void Game::updateAnimations(float dtSeconds)
{
    // The player looks where they aim: the flashlight and every swing follow
    // the mouse, so the body does too, even while backing away.
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Animation>(mPlayerEntity) &&
        mWorld.hasComponent<Sprite>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity) &&
        mWorld.hasComponent<Facing>(mPlayerEntity))
    {
        Animation& anim = mWorld.getComponent<Animation>(mPlayerEntity);
        Sprite& sprite = mWorld.getComponent<Sprite>(mPlayerEntity);
        const Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        const Facing& fac = mWorld.getComponent<Facing>(mPlayerEntity);

        faceToward(glm::vec2(std::cos(fac.angleRadians), std::sin(fac.angleRadians)), anim.facingAway, sprite.flipX);

        const bool moving = vel.dx * vel.dx + vel.dy * vel.dy > 4.0f;
        const bool dead = mWorld.hasComponent<Health>(mPlayerEntity) &&
                          mWorld.getComponent<Health>(mPlayerEntity).current <= 0.0f;
        bool swinging = false;
        if (mWorld.hasComponent<Combat>(mPlayerEntity))
        {
            const AttackPhase phase = mWorld.getComponent<Combat>(mPlayerEntity).phase;
            swinging = phase == AttackPhase::Windup || phase == AttackPhase::Active;
        }

        std::string desired;
        if (dead)
        {
            desired = "player_dead";
        }
        else
        {
            if (swinging)
            {
                desired = "player_attack";
            }
            else if (moving)
            {
                desired = fac.crouching ? "player_crouchwalk" : "player_walk";
            }
            else
            {
                desired = fac.crouching ? "player_crouch" : "player_idle";
            }
            desired += viewSuffix(anim.facingAway);
        }
        playAnimation(anim, desired);
    }

    // Zombies face where they shamble, and turn to their prey for a lunge.
    mWorld.forEach<ZombieAI, Animation, Velocity, Sprite, Transform>(
        [&](Entity, ZombieAI& ai, Animation& anim, Velocity& vel, Sprite& sprite, Transform& transform)
        {
            const bool moving = vel.dx * vel.dx + vel.dy * vel.dy > 1.0f;
            const bool lunging = ai.attackWindup > 0.0f;
            if (lunging || ai.state == ZombieState::Attack)
            {
                const glm::vec2 centre(transform.x + 16.0f, transform.y + 16.0f);
                faceToward(ai.targetWorld - centre, anim.facingAway, sprite.flipX);
            }
            else if (moving)
            {
                faceToward(glm::vec2(vel.dx, vel.dy), anim.facingAway, sprite.flipX);
            }
            const char* pose = lunging ? "zombie_attack" : (moving ? "zombie_walk" : "zombie_idle");
            playAnimation(anim, std::string(pose) + viewSuffix(anim.facingAway));
        });

    // Survivors walk like the player and face where they go.
    mWorld.forEach<SurvivorAI, Animation, Velocity, Sprite>(
        [&](Entity, SurvivorAI& ai, Animation& anim, Velocity& vel, Sprite& sprite)
        {
            if (ai.state == SurvivorState::Dead)
            {
                playAnimation(anim, "player_dead");
                return;
            }
            const bool moving = vel.dx * vel.dx + vel.dy * vel.dy > 4.0f;
            if (moving)
            {
                faceToward(glm::vec2(vel.dx, vel.dy), anim.facingAway, sprite.flipX);
            }
            playAnimation(anim, std::string(moving ? "player_walk" : "player_idle") + viewSuffix(anim.facingAway));
        });

    mAnimationSystem.update(mWorld, mSpriteSheet, dtSeconds);
}

// Particle update and spawning (blood, dust, embers).
void Game::updateParticles(float dtSeconds, float prevPlayerHp)
{
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
