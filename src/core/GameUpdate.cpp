// Per-tick simulation: input, systems, combat, sleep, animation and particles.
#include "core/GameShared.h"
#include "world/WeatherRules.h"
#include "world/CombatRules.h"
#include "world/WoundRules.h"

using namespace gameshared;

void Game::update(float dtSeconds)
{
    // What is heard and seen of the world as it stood after the last tick
    // (phase 12). Reads the world, changes nothing in it.
    updateSoundscape(dtSeconds);
    updateMood(dtSeconds);
    handleEscapeKey();

    // Combat weight runs on real time: the shake dies away, and a landed
    // blow holds the bodies almost still for a few frames. Only what is seen
    // moving slows (feelDt: combat, zombies, movement, animation, particles);
    // the clock, needs, wounds, weather and fire keep game time (dtSeconds).
    mShakeClock += dtSeconds;
    mShakeTrauma = std::max(0.0f, mShakeTrauma - dtSeconds * 2.6f);
    mPlayerHurtTimer = std::max(0.0f, mPlayerHurtTimer - dtSeconds);
    float feelDt = dtSeconds;
    if (mHitStop > 0.0f && mRunState == RunState::Playing)
    {
        mHitStop -= dtSeconds;
        feelDt = dtSeconds * 0.06f;
    }

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
                closeTalk();
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
                closeTalk();
            }
        }
        // The notebook's tabs (Left and Right), a journal being read (phase 10).
        updateNotebookInput();

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
    // The character screen, over the town at a standstill (phase 5).
    if (mRunState == RunState::CharacterCreation)
    {
        updateCharacterCreation();
        return;
    }

    // Pause menu input
    if (mRunState == RunState::Paused)
    {
        updatePauseMenu();
        return;
    }

    if (mRunState == RunState::Dead || mRunState == RunState::Retired || mRunState == RunState::Out)
    {
        mDeathStateTimer += static_cast<double>(dtSeconds);
        updateAnimations(dtSeconds);
        mParticleSystem.update(dtSeconds);

        const bool restartRequested =
            mInput.wasKeyPressed(SDL_SCANCODE_RETURN) ||
            mInput.wasKeyPressed(SDL_SCANCODE_SPACE) ||
            mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT);

        if (restartRequested && mDeathStateTimer >= kDeathRestartDelaySeconds)
        {
            mCreateCharacterOnStart = true;
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

    // Last tick's panic drift comes off before the legs steer again (it is an
    // offset, not a push that builds up from tick to tick).
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        vel.dx -= mPanicDrift.x;
        vel.dy -= mPanicDrift.y;
    }
    mPanicDrift = glm::vec2(0.0f);

    // What the body does to a walk and a swing this tick (phase 5).
    prepareBodyModifiers();

    // Knocked down by a Brute: no walking, no swinging, until you are up.
    // Eyes shut for a moment (a microsleep): nothing either.
    if (playerAlive && !mShowCrafting && !mContainerOpen && !mTalk.open && !playerKnockedDown() && mMicrosleepLeft <= 0.0f)
    {
        mInputSystem.update(mWorld, mInput, mMouseWorld, feelDt);
    }
    else if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
        velocity.dx = 0.0f;
        velocity.dy = 0.0f;
    }
    // Breaking: the legs take over and run, for a moment.
    if (playerAlive && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity) && mWorld.getComponent<MentalState>(mPlayerEntity).lostControlTimer > 0.0f &&
        !playerKnockedDown() && !playerHeld())
    {
        Velocity& velocity = mWorld.getComponent<Velocity>(mPlayerEntity);
        velocity.dx = mLostControlDir.x * 180.0f * 1.75f; // the run speed (InputSystem)
        velocity.dy = mLostControlDir.y * 180.0f * 1.75f;
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
    // Wounds, sleep debt, coffee, a deep breath, seeing things (GameBody.cpp).
    updateBody(dtSeconds);
    if (mRunState != RunState::Playing) return; // turned

    // Inventory food spoilage
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        // Heat and summer spoil food quicker (phase 7).
        const float gameHoursThisTick = static_cast<float>(dtSeconds * kGameHoursPerRealSecond) *
                                        weatherrules::spoilScale(mWeatherState.condition, weatherrules::seasonOf(mCurrentDay));
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        InventorySystem::degradeFood(inv, mItemDatabase, gameHoursThisTick);
        spoilContainers(gameHoursThisTick);
    }

    // Mental state system
    mMentalStateSystem.update(mWorld, mNoiseModel, mPlayerEntity, dtSeconds, mGameHours, mCurrentDay, &mTileMap);

    // Weather system
    mWeatherSystem.update(mWeatherState, dtSeconds, mGameHours, mCurrentDay);
    updateAtmosphere(dtSeconds);
    updateWeather(dtSeconds);

    // Flashlight toggle (Q). Needs a flashlight in the pack. In a car, Q is
    // the headlights (phase 11).
    const bool inCar = drivingVehicle && mWorld.hasComponent<Vehicle>(mActiveVehicleEntity) &&
                       vehiclerules::info(mWorld.getComponent<Vehicle>(mActiveVehicleEntity).kind).engine;
    if (playerAlive && inCar && mInput.wasKeyPressed(SDL_SCANCODE_Q))
    {
        Vehicle& car = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
        car.headlights = !car.headlights;
        mInteractionMessage = car.headlights ? "Headlights on. They will be seen." : "Headlights off.";
        mInteractionMessageTimer = 2.0f;
    }
    else if (playerAlive && !mShowCrafting && mInput.wasKeyPressed(SDL_SCANCODE_Q))
    {
        if (playerHasItem(mItems.flashlight) && !mFlashlightOn && flashlightCharge() <= 0.0f)
        {
            mInteractionMessage = "The flashlight is dead. It needs a battery.";
        }
        else if (playerHasItem(mItems.flashlight))
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
    if (mFlashlightOn && !playerHasItem(mItems.flashlight))
    {
        mFlashlightOn = false;
    }
    // Scavenging and power (phase 6): beds and snares, the lights, alarms,
    // batteries running down.
    updateScavenging(dtSeconds);
    updatePower(dtSeconds);
    updateAlarms(dtSeconds);
    updateFlashlightBattery(dtSeconds);
    updateRadio(dtSeconds);

    updateFire(dtSeconds, playerAlive, drivingVehicle);

    // The other people in town (GameSurvivors.cpp).
    updateSurvivors(dtSeconds);
    holdSurvivorsFromFire();

    // District infection daily update
    mDistrictInfectionSystem.dailyUpdate(mWorld, mMapGenerator.layoutData(), mCurrentDay);

    updateCraftingPanel(dtSeconds, drivingVehicle);

    // Held, hands are busy: no pack, no belt.
    if (!(playerAlive && playerHeld()))
    {
        updateInventoryInput(drivingVehicle);
    }

    // Panic drift: add random velocity jitter proportional to panic level
    if (mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        // It nudges a walk; someone standing still (boarding a door, stitching
        // a cut) stays where they are, and makes no footsteps for it.
        const bool walking = mInput.isKeyDown(SDL_SCANCODE_W) || mInput.isKeyDown(SDL_SCANCODE_A) ||
                             mInput.isKeyDown(SDL_SCANCODE_S) || mInput.isKeyDown(SDL_SCANCODE_D);
        if (mental.panic >= 25.0f && walking)
        {
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            // Scale drift strength: 0 at 25 panic, full at 100
            const float t = (mental.panic - 25.0f) / 75.0f; // 0..1
            const float driftStrength = t * 80.0f; // up to 80 px/s drift
            mPanicDrift = glm::vec2(std::cos(mental.panicDriftAngle), std::sin(mental.panicDriftAngle)) * driftStrength;
            vel.dx += mPanicDrift.x;
            vel.dy += mPanicDrift.y;
        }
    }

    // (Depression's slow walk is the Numb band's -5 %, in prepareBodyModifiers.)

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
        const float limit = playerCarryLimit();
        if (weight > limit)
        {
            const float overRatio = std::min((weight - limit) / 10.0f, 0.6f);
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
    updateFirstDays(dtSeconds);

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
        Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        // The harness's god mode: a crowd's bites in one tick cannot kill.
        if (mDevGodMode) health.current = std::max(health.current, health.maximum);
        playerAlive = health.current > 0.0f;
    }

    if (!playerAlive)
    {
        std::string cause = zombieDeathCause();

        bool diedInFire = false;
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            diedInFire = mFireSystem.burning(worldToTile(mTileMap, glm::vec2(pt.x + 16.0f, pt.y + 16.0f)));
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
        else if (!diedInFire && cause != "Dehydrated" && cause != "Starved" &&
                 mPlayerEntity != kInvalidEntity &&
                 mWorld.hasComponent<Wounds>(mPlayerEntity) &&
                 hasInfectedWound(mWorld.getComponent<Wounds>(mPlayerEntity)))
        {
            cause = "Succumbed to infection";
        }

        beginDeathState(cause + " on Day " + std::to_string(mCurrentDay));
        return;
    }

    mProximityPrompt.clear();

    // Held by the dead, there is no reaching for anything.
    const bool heldNow = playerAlive && playerHeld();
    if (playerAlive && !heldNow && !mShowCrafting && !mContainerOpen && !mTalk.open && !drivingVehicle &&
        !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        // Update proximity prompt for HUD display
        const ProximityPrompt prompt = mInteractionSystem.getProximityPrompt(mWorld, mPlayerEntity);
        mProximityPrompt = prompt.text;

        // A sleep spot says how safe the room is.
        if (prompt.entity != kInvalidEntity && mWorld.hasComponent<Interactable>(prompt.entity) &&
            mWorld.getComponent<Interactable>(prompt.entity).type == InteractableType::SleepSpot)
        {
            std::string openWay;
            const glm::ivec2 here = playerTile();
            const int safety = sleepSafety(mTileMap.getBuildingId(here.x, here.y), &openWay);
            mProximityPrompt += safety == 0 ? " (out in the open)"
                              : safety == 1 ? " (" + openWay + ")"
                              : safety == 3 ? " (the house is shut tight)"
                                            : "";
        }

        // A door within reach takes E before anything else nearby, then a
        // window, then furniture and what was built.
        bool interceptedContainer = mBaseAction.kind != BaseAction::Kind::None && mBaseAction.kind != BaseAction::Kind::Grab;
        // A vehicle at hand: fuel, mending, a pickup's load (even backed up
        // to a door) (phase 11). Its alarm comes last.
        if (!interceptedContainer) interceptedContainer = handleVehicleInteraction(prompt);
        const std::string vehiclePrompt = mProximityPrompt; // the alarm speaks only if nothing after this does
        if (!interceptedContainer) interceptedContainer = handleAlarmBox(prompt);
        if (!interceptedContainer) interceptedContainer = handleGateInteraction(prompt);
        if (!interceptedContainer) interceptedContainer = handleDoorInteraction(prompt);
        if (!interceptedContainer && doorInReach(prompt) == nullptr)
        {
            interceptedContainer = handleWindowInteraction(prompt);
            if (!interceptedContainer && windowInReach(prompt) == nullptr)
            {
                // Someone close enough to talk to comes before the furniture.
                interceptedContainer = talkToSurvivor();
                if (!interceptedContainer) interceptedContainer = handleLegacyInteraction(prompt);
                if (!interceptedContainer) interceptedContainer = handleReadable(prompt);
                if (!interceptedContainer) interceptedContainer = handleFieldStructure(prompt);
                if (!interceptedContainer) interceptedContainer = handleBaseInteraction(prompt);
                if (!interceptedContainer) interceptedContainer = handleFloorLamp(prompt);
                if (!interceptedContainer) interceptedContainer = handleTapInteraction(prompt);
                if (!interceptedContainer) interceptedContainer = handleWaterSource(prompt);
                if (!interceptedContainer) interceptedContainer = handleForage(prompt);
                if (!interceptedContainer && mProximityPrompt == vehiclePrompt) interceptedContainer = handleVehicleAlarm(prompt);
            }
        }
        if (!interceptedContainer && mInput.wasKeyPressed(SDL_SCANCODE_E) && prompt.entity != kInvalidEntity &&
            mWorld.hasComponent<Interactable>(prompt.entity))
        {
            const Interactable& inter = mWorld.getComponent<Interactable>(prompt.entity);
            if (inter.type == InteractableType::Container && mWorld.hasComponent<Inventory>(prompt.entity))
            {
                // Searching takes a moment (phase 6); the panel opens after.
                beginSearch(prompt.entity);
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
                    mAudioManager.play(interactionMessage.find("Drank") != std::string::npos ? SoundId::BodyDrink : SoundId::BodyEat);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("Picked up") != std::string::npos)
                {
                    mAudioManager.play(SoundId::PickupLight);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("bandage") != std::string::npos)
                {
                    mAudioManager.play(SoundId::Bandage);
                    ++mRunStats.itemsLooted;
                }
                else if (interactionMessage.find("Looted") != std::string::npos)
                {
                    mAudioManager.play(SoundId::PickupHeavy);
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

    // E with nothing else in reach: the nearest body on the ground.
    if (playerAlive && !mShowCrafting && !drivingVehicle && !mTalk.open && !mWorld.hasComponent<Sleeping>(mPlayerEntity) &&
        mInput.wasKeyPressed(SDL_SCANCODE_E) && mProximityPrompt.empty() && !mContainerOpen)
    {
        (void)searchNearestCorpse();
    }
    // A vehicle within reach: V gets in, whatever E does here (phase 11).
    if (playerAlive && !drivingVehicle && !mShowCrafting && !mContainerOpen && !mTalk.open &&
        mProximityPrompt.find("V: ") == std::string::npos)
    {
        const Entity near = vehicleNear(playerWorldCenter(), kVehicleReachPixels);
        if (near != kInvalidEntity)
        {
            const bool bicycle = mWorld.getComponent<Vehicle>(near).kind == VehicleKind::Bicycle;
            mProximityPrompt += std::string(mProximityPrompt.empty() ? "" : "   ") + (bicycle ? "V: Get on" : "V: Get in");
        }
    }

    // Held, throwing, reloading, clocks and flares, things in flight.
    updateFighting(feelDt, playerAlive, drivingVehicle);

    // Track pre-combat state for audio triggers
    const float prevPlayerHp = (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
        ? mWorld.getComponent<Health>(mPlayerEntity).current
        : 0.0f;
    const AttackPhase prevPhase = (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Combat>(mPlayerEntity))
        ? mWorld.getComponent<Combat>(mPlayerEntity).phase
        : AttackPhase::Idle;

    if (!mShowCrafting && !mContainerOpen && !mTalk.open && !drivingVehicle && !mBuildWheelOpen && mBuildChoice < 0 &&
        mBaseAction.kind == BaseAction::Kind::None && !playerKnockedDown())
    {
        mCombatSystem.update(mWorld, mInput, mTileMap, mNoiseModel, feelDt);
        syncEquippedWeapon();
        afterCombat();
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

    updateBlowFeedback(feelDt);

    // Desensitisation gain: check for zombie deaths after combat
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        int killsNow = 0;
        mWorld.forEach<ZombieAI, Health>(
            [&](Entity, ZombieAI& zombieAI, Health& h)
            {
                // Zombie died since last check: only the player's own kills count.
                if (h.current <= 0.0f && h.current > -999.0f)
                {
                    if (zombieAI.lastDamagedBy == mPlayerEntity)
                    {
                        ++mRunStats.kills;
                        ++killsNow;
                    }
                    h.current = -1000.0f; // mark as counted
                }
            });
        // Numbness, the first kill, Fighting XP (GameBody.cpp).
        for (int i = 0; i < killsNow; ++i) onPlayerKill();
    }

    // Clear away the dead however they died, once their kills are counted,
    // and leave a body where each one fell. (The risen become graves instead.)
    {
        struct Fallen
        {
            Entity zombie;
            bool leavesBody;
        };
        std::vector<Fallen> dead;
        mWorld.forEach<ZombieAI, Health>(
            [&](Entity zombie, ZombieAI&, Health& h)
            {
                if (h.current <= 0.0f)
                {
                    dead.push_back(Fallen{zombie, !mWorld.hasComponent<LegacyLink>(zombie) &&
                                                      mWorld.hasComponent<Transform>(zombie) &&
                                                      mWorld.hasComponent<Sprite>(zombie)});
                }
            });
        for (const Fallen& fallen : dead)
        {
            onZombieDeath(fallen.zombie);
            if (fallen.leavesBody)
            {
                const Transform& t = mWorld.getComponent<Transform>(fallen.zombie);
                const Sprite& s = mWorld.getComponent<Sprite>(fallen.zombie);
                const ZombieAI& ai = mWorld.getComponent<ZombieAI>(fallen.zombie);
                const glm::vec2 topLeft(t.x, t.y);
                const glm::vec4 tint = s.color;
                const int look = ai.look;
                const ZombieKind kind = ai.kind;
                const glm::vec2 slide = ai.knock * 0.7f;
                int dir = mWorld.hasComponent<Animation>(fallen.zombie)
                    ? mWorld.getComponent<Animation>(fallen.zombie).dir
                    : 1;
                // Knocked down, it goes over backward along the blow.
                if (glm::dot(slide, slide) > 1.0f)
                {
                    dir = facingDir(-slide, dir);
                }
                // A body lying straight toward or away from the viewer is
                // foreshortened to a lump; turn it to the nearest diagonal.
                if (dir == 2 || dir == 6)
                {
                    dir = (dir + ((look + static_cast<int>(topLeft.x)) % 2 == 0 ? 1 : 7)) & 7;
                }
                mWorld.destroyEntity(fallen.zombie);
                spawnCorpse(topLeft, look, dir, tint, slide, false, kind);
            }
            else
            {
                mWorld.destroyEntity(fallen.zombie);
            }
        }

        // Bodies stay for the run, up to a limit; the oldest go first.
        std::vector<std::pair<float, Entity>> bodies;
        mWorld.forEach<Corpse>(
            [&](Entity corpse, Corpse& c)
            {
                c.age += dtSeconds;
                bodies.emplace_back(c.age, corpse);

                // The body slides on what is left of the blow, and stops at walls.
                if ((c.slideX != 0.0f || c.slideY != 0.0f) && mWorld.hasComponent<Transform>(corpse))
                {
                    Transform& t = mWorld.getComponent<Transform>(corpse);
                    const auto blocked = [&](float x, float y)
                    {
                        const float cx = x + kSpriteSize * 0.5f;
                        const float cy = y + kSpriteSize * 0.5f + 6.0f;
                        return mTileMap.isSolidAtWorldDiamond(cx, cy, 9.0f, 9.0f);
                    };
                    const float nx = t.x + c.slideX * feelDt;
                    if (!blocked(nx, t.y)) t.x = nx; else c.slideX = 0.0f;
                    const float ny = t.y + c.slideY * feelDt;
                    if (!blocked(t.x, ny)) t.y = ny; else c.slideY = 0.0f;
                    const float keep = std::max(0.0f, 1.0f - 7.0f * feelDt);
                    c.slideX *= keep;
                    c.slideY *= keep;
                    if (c.slideX * c.slideX + c.slideY * c.slideY < 4.0f)
                    {
                        c.slideX = 0.0f;
                        c.slideY = 0.0f;
                    }
                }
            });
        if (bodies.size() > kMaxCorpses)
        {
            std::sort(bodies.begin(), bodies.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
            for (std::size_t i = 0; i < bodies.size() - kMaxCorpses; ++i)
            {
                mWorld.destroyEntity(bodies[i].second);
            }
        }
    }

    // Combat audio: the swing's whoosh comes with its noise (CombatSystem);
    // a blow taken draws a grunt by how hard it was.
    (void)prevPhase;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const float curHp = mWorld.getComponent<Health>(mPlayerEntity).current;
        if (curHp < prevPlayerHp - 0.5f && curHp > 0.0f)
        {
            mAudioManager.play(prevPlayerHp - curHp >= 10.0f ? SoundId::BodyGruntBig : SoundId::BodyGruntSmall);
        }
    }

    // Threat director: migration toward the player's area and population growth.
    {
        std::vector<ThreatDirectorSystem::SpawnRequest> spawns;
        mThreatDirector.update(mWorld, mTileMap, mPlayerEntity, dtSeconds, mGameHours, mCurrentDay,
                               &mDistrictInfectionSystem, spawns);
        if (!spawns.empty())
        {
            // The kind from the mode's shares and the district's infection.
            std::mt19937 rng(mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(mCurrentDay * 7919) ^
                             static_cast<std::uint32_t>(mGameHours * 3600.0));
            for (const auto& request : spawns)
            {
                const glm::ivec2 tile = worldToTile(mTileMap, request.topLeftWorld + glm::vec2(kSpriteSize * 0.5f));
                EntityFactory::spawnZombie(mWorld, mSpriteSheet, zombieParams(rollKind(rng, tile), request.topLeftWorld, rng));
            }
        }
    }

    updateDead(dtSeconds, feelDt);
    updateDoors(dtSeconds);
    updateWindows(dtSeconds);
    updateStructures(dtSeconds);
    updateTraps(dtSeconds);


    // The dead's own sounds are heard in GameSound.cpp (updateDeadSounds, and
    // the groans their noises carry).
    if (mShowCrafting && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        vel.dx = 0.0f;
        vel.dy = 0.0f;
    }

    updateBaseActions(dtSeconds);
    mMovementSystem.update(mWorld, feelDt);
    mProjectileSystem.update(mWorld, mTileMap, mNoiseModel, feelDt);
    mCollisionSystem.update(mWorld, mTileMap, feelDt);

    updateVehicleAfterPhysics(feelDt);
    drivingVehicle = mActiveVehicleEntity != kInvalidEntity;

    // The way out: the Sergeant, the last stand, walking out (phase 9).
    if (mRunState == RunState::Playing && updateGate(dtSeconds)) return;

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Health>(mPlayerEntity))
    {
        Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        if (mDevGodMode) health.current = std::max(health.current, health.maximum);
        if (health.current <= 0.0f)
        {
            beginDeathState(zombieDeathCause() + " on Day " + std::to_string(mCurrentDay));
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

        mCamera.follow(cameraTarget(playerCenter), dtSeconds, 7.5f); // runs ahead of a moving vehicle
        mCamera.clampToBounds(mTileMap.pixelSize());
    }

    updateLegacy(dtSeconds);
    updateJournal(dtSeconds);
    updateAnimations(feelDt);

    updateParticles(feelDt, prevPlayerHp);

    updateNoiseModel(dtSeconds);
}

// Sleep time-skip: fast-forward time and recover while asleep.
void Game::updateSleep(float dtSeconds)
{
    // Sleep time-skip: if player is sleeping, fast-forward time and recover needs
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        Sleeping& sleeping = mWorld.getComponent<Sleeping>(mPlayerEntity);
        // How safe the room is: it sets how well you sleep and what wakes you.
        if (sleeping.safety < 0)
        {
            const glm::ivec2 here = playerTile();
            const int building = mTileMap.getBuildingId(here.x, here.y);
            sleeping.safety = mTileMap.isIndoors(here.x, here.y) ? sleepSafety(building, nullptr) : 0;
            static constexpr float kRoomQuality[4] = {0.4f, 0.6f, 0.8f, 0.95f};
            bool inBed = false;
            for (const FurniturePiece& piece : mFurniture)
            {
                if (piece.kind == FurnitureKind::Bed && glm::length(glm::vec2(piece.tile - here)) <= 1.5f) inBed = true;
            }
            const float room = std::min(1.0f, kRoomQuality[sleeping.safety] + (inBed ? 0.05f : 0.0f));
            // A sleeping bag or a bedroll is no better than itself; a collapse is a collapse.
            sleeping.quality = inBed ? room : std::min(room, sleeping.quality);
            // Something to make you sleep makes worse sleep (phase 5).
            if (mWorld.hasComponent<Needs>(mPlayerEntity))
            {
                const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                const bool sedated = absoluteHour() - needs.lastSedativeHour <= sleeprules::kSedativeWindowHours;
                sleeping.quality = sleeprules::sleepQuality(sleeping.quality, sedated);
            }
            // Thunder makes for poor sleep (phase 7).
            sleeping.quality = std::clamp(sleeping.quality + weatherrules::sleepQualityShift(mWeatherState.condition), 0.1f, 1.0f);
            // Someone keeping watch (phase 8).
            if (companionNear(6.0f)) sleeping.quality = std::min(1.0f, sleeping.quality + 0.1f);
        }
        constexpr float kSleepTimeMultiplier = 8.0f;
        constexpr float kSleepWakeRange = 160.0f;
        constexpr float kSleepHealPerHour = 3.0f;
        const float extraDtSeconds = dtSeconds * (kSleepTimeMultiplier - 1.0f);
        const float gameHoursThisTick = static_cast<float>(dtSeconds * kGameHoursPerRealSecond * kSleepTimeMultiplier);
        sleeping.durationRemaining -= gameHoursThisTick;
        sleeping.hoursSlept += gameHoursThisTick;
        advanceGameClock(gameHoursThisTick, mGameHours, mCurrentDay);

        if (extraDtSeconds > 0.0f)
        {
            mNeedsSystem.update(mWorld, extraDtSeconds, buildNeedsEnvironment());
            mBleedingSystem.update(mWorld, extraDtSeconds);
            mWoundSystem.update(mWorld, mPlayerEntity, extraDtSeconds, mGameHours, woundContext());

            if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
                InventorySystem::degradeFood(inv, mItemDatabase, extraDtSeconds * static_cast<float>(kGameHoursPerRealSecond));
            }
        }

        if (mWorld.hasComponent<Needs>(mPlayerEntity))
        {
            Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
            // Low: a night does less (phase 5).
            const float rest = mWorld.hasComponent<MentalState>(mPlayerEntity)
                                   ? mindrules::restScale(mindrules::depressionBand(mWorld.getComponent<MentalState>(mPlayerEntity).depression))
                                   : 1.0f;
            needs.sleepDebt = std::max(0.0f, needs.sleepDebt - sleeping.debtRecoveryRate * sleeping.quality * rest * gameHoursThisTick);
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

        // Check for noise interruption (zombies nearby while sleeping). Out
        // in the open, or with a way in standing open, the dead wake you as
        // they come near; behind shut doors only when they are in the room
        // (or at the door: see updateDoors and updateWindows).
        bool interrupted = false;
        float nearestZombie = 1e9f;
        float wakeRange = sleeping.safety <= 1 ? 12.0f * static_cast<float>(mTileMap.tileWidth())
                                               : std::min(kSleepWakeRange, 2.0f * static_cast<float>(mTileMap.tileWidth()));
        if (mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            wakeRange *= traitrules::wakeRangeScale(mWorld.getComponent<Traits>(mPlayerEntity));
        }
        // A companion on watch wakes you while they are still further off.
        if (companionNear(6.0f)) wakeRange += 3.0f * static_cast<float>(mTileMap.tileWidth());
        // Light sleepers: woken a second sooner (phase 10).
        if (const Traits* traits = playerTraits(); traits != nullptr && traits->has(BloodlineTrait::Vigilant))
        {
            wakeRange += legacy::kVigilantWakeSeconds * legacy::kVigilantWalkerTiles * static_cast<float>(mTileMap.tileWidth());
        }
        if (mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            const glm::vec2 pc(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);
            mWorld.forEach<ZombieAI, Transform>(
                [&](Entity, ZombieAI&, Transform& zt)
                {
                    const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                    nearestZombie = std::min(nearestZombie, glm::distance(pc, zc));
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
                // Woken with one of them close: the worst way to wake.
                if (nearestZombie < mindrules::kWokenByZombieTiles * static_cast<float>(mTileMap.tileWidth()))
                {
                    addPanic(mindrules::kWokenByZombiePanic);
                }
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
                // A full night: Rested for a while (phase 5).
                if (sleeprules::fullSleep(sleeping.hoursSlept, sleeping.quality) && mWorld.hasComponent<Needs>(mPlayerEntity))
                {
                    mWorld.getComponent<Needs>(mPlayerEntity).restedHours = sleeprules::kRestedHours;
                    mInteractionMessage = "A whole night. You feel rested.";
                }

                // Checkpoint after a full sleep cycle
                if (mPlayerEntity != kInvalidEntity)
                {
                    flushGate();
                    mSaveManager.saveCheckpoint(buildRunCheckpoint());
                    refreshContainerRecords();
                    persistVehicles(false);
                    mSaveManager.saveWorldMemory();
                }
            }
            mInteractionMessageTimer = 3.0f;
            // Robbed in the night: you find out when you wake.
            if (mRobbedAsleep)
            {
                mRobbedAsleep = false;
                mInteractionMessage = "Someone has been here. Food and water gone from your pack.";
                mInteractionMessageTimer = 5.0f;
                addPanic(10.0f);
            }
            mWorld.removeComponent<Sleeping>(mPlayerEntity);
        }
    }
}

namespace
{
// The five drawn views, by facing (Animation::dir): the three right-hand ones
// are mirrored for the left.
const char* viewSuffix(int dir)
{
    static const char* const kViews[8] = {"_e", "_se", "_s", "_se", "_e", "_ne", "_n", "_ne"};
    return kViews[dir & 7];
}

bool viewFlipped(int dir)
{
    return dir >= 3 && dir <= 5;
}

// The dead's views: a Crawler is drawn in two facings (toward the viewer
// and away), mirrored like the rest.
const char* zombieView(ZombieKind kind, int dir)
{
    if (kind == ZombieKind::Crawler) return (dir & 7) >= 5 ? "_ne" : "_se";
    return viewSuffix(dir);
}

std::string zombieSet(ZombieKind kind, int look)
{
    return std::string(zombiekinds::spriteSet(kind)) + std::to_string(((look % 3) + 3) % 3);
}

void face(Animation& anim, Sprite& sprite, const glm::vec2& worldDir)
{
    anim.dir = facingDir(worldDir, anim.dir);
    sprite.flipX = viewFlipped(anim.dir);
}

// Switch animation. Turning mid-stride (walk_se to walk_e) keeps the step;
// anything else starts the new cycle from its first frame.
void playAnimation(Animation& anim, const std::string& desired, float frameDuration, bool looping = true)
{
    anim.driven = false;
    anim.frameDuration = frameDuration;
    anim.looping = looping;
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
        anim.finished = false;
    }
}

// Show one chosen frame of an animation (attacks follow their timers).
void showFrame(Animation& anim, const std::string& name, int frame)
{
    anim.currentAnim = name;
    anim.driven = true;
    anim.finished = false;
    anim.currentFrame = frame;
}

// Frame within one part of an attack: `count` frames spread over a timer
// that runs from `total` down to zero.
int partFrame(float timer, float total, int count)
{
    const float progress = 1.0f - std::clamp(timer / total, 0.0f, 1.0f);
    return std::min(count - 1, static_cast<int>(progress * static_cast<float>(count)));
}

// What the player carries decides which set of frames they wear.
const char* loadoutName(const Combat& combat)
{
    if (combat.weapon.itemId < 0)
    {
        return "none";
    }
    switch (combat.weapon.category)
    {
    case WeaponCategory::Unarmed: return "none";
    case WeaponCategory::Bladed: return "blade";
    case WeaponCategory::Ranged: return "gun";
    default: return "club";
    }
}

struct AttackFrames
{
    const char* name;
    int windup;
    int active;
    int recovery;
};

AttackFrames attackFrames(const Combat& combat)
{
    if (combat.weapon.itemId < 0)
    {
        return {"player_punch", 2, 1, 2};
    }
    switch (combat.weapon.category)
    {
    case WeaponCategory::Unarmed: return {"player_punch", 2, 1, 2};
    case WeaponCategory::Bladed: return {"player_stab", 2, 1, 2};
    case WeaponCategory::Ranged:
        // A shotgun goes to the shoulder; a pistol is held out.
        return combat.weapon.pellets > 1 ? AttackFrames{"player_shootlong", 1, 1, 1} : AttackFrames{"player_shoot", 1, 1, 1};
    default: return {"player_swing", 2, 2, 2};
    }
}
} // namespace

Entity Game::spawnCorpse(const glm::vec2& topLeft, int look, int dir, const glm::vec4& tint,
                         const glm::vec2& slide, bool settled, ZombieKind kind)
{
    Transform body{};
    body.x = body.prevX = topLeft.x;
    body.y = body.prevY = topLeft.y;

    Animation fall{};
    fall.currentAnim = zombieSet(kind, look) + "_death" + zombieView(kind, dir);
    if (mSpriteSheet.find(fall.currentAnim) == nullptr)
    {
        fall.currentAnim = "zombie" + std::to_string(((look % 3) + 3) % 3) + "_death" + viewSuffix(dir);
    }
    fall.dir = dir & 7;
    fall.frameDuration = 0.075f;
    fall.looping = false;
    const int frames = std::max(1, mSpriteSheet.frameCount(fall.currentAnim));
    if (settled)
    {
        fall.currentFrame = frames - 1;
        fall.finished = true;
    }

    Sprite sprite{};
    sprite.textureId = mSpriteSheet.textureId();
    sprite.uvRect = mSpriteSheet.frameUv(fall.currentAnim, fall.currentFrame);
    sprite.color = tint;
    sprite.layer = 3; // on the floor, under anything standing
    sprite.drawSize = SpriteSizes::kHumanoid;
    sprite.flipX = viewFlipped(dir);

    Corpse corpse{};
    corpse.slideX = slide.x;
    corpse.slideY = slide.y;
    corpse.look = look;
    corpse.dir = dir & 7;
    corpse.kind = kind;

    const Entity entity = mWorld.createEntity();
    mWorld.addComponent<Transform>(entity, body);
    mWorld.addComponent<Sprite>(entity, sprite);
    mWorld.addComponent<Animation>(entity, fall);
    mWorld.addComponent<Corpse>(entity, corpse);
    return entity;
}

// A blow that has just landed on a zombie: blood along the swing, and, for
// the player's own blows, a held frame and a shaken view.
void Game::updateBlowFeedback(float dtSeconds)
{
    mWorld.forEach<ZombieAI, Transform, Health>(
        [&](Entity, ZombieAI& zombie, Transform& transform, Health& health)
        {
            if (zombie.hitFlash < ZombieBlow::kFreshHit)
            {
                zombie.hitFlash = std::max(0.0f, zombie.hitFlash - dtSeconds);
                return;
            }
            zombie.hitFlash = ZombieBlow::kFlashSeconds;

            const bool killed = health.current <= 0.0f;
            const glm::vec2 centre(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f);
            const float speed = glm::length(zombie.knock);
            const glm::vec2 direction = speed > 1.0f ? zombie.knock / speed : glm::vec2(0.0f);

            // Each kind of blow reads differently: a blunt one stops the
            // world for a beat, a blade cuts straight through and leaves more
            // blood, a heavy one shakes the ground and raises dust, a shot
            // never pauses, and a shove draws no blood at all.
            const bool shove = zombie.lastBlow < 0;
            const bool bladed = zombie.lastBlow == static_cast<int>(WeaponCategory::Bladed);
            const bool shot = zombie.lastBlow == static_cast<int>(WeaponCategory::Ranged);
            const bool heavy = zombie.lastBlowHeavy;
            if (!shove || killed)
            {
                const int drops = bladed ? (killed ? 30 : 16) : (killed ? 22 : 10);
                mParticleSystem.spawnBloodSpray(centre, direction, drops, killed ? 1.25f : 1.0f);
            }
            if (heavy)
            {
                mParticleSystem.spawnDustRing(centre + glm::vec2(0.0f, 8.0f), 70.0f);
            }

            if (zombie.lastDamagedBy == mPlayerEntity)
            {
                float stop = killed ? 0.085f : 0.05f;
                if (shot || (bladed && killed)) stop = 0.0f;
                else if (shove) stop = 0.02f;
                else if (heavy) stop = 0.10f;
                mHitStop = std::max(mHitStop, stop);
                float trauma = killed ? 0.45f : 0.3f;
                if (heavy) trauma *= 2.0f;
                if (shove) trauma = 0.15f;
                mShakeTrauma = std::min(1.0f, mShakeTrauma + trauma);
            }
            // The contact, by the kind of blow (a shove's sound is its own).
            if (!shove) soundOfHit(zombie.lastBlow, heavy, killed, centre);
        });
}

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

        const bool dead = mWorld.hasComponent<Health>(mPlayerEntity) &&
                          mWorld.getComponent<Health>(mPlayerEntity).current <= 0.0f;
        if (!dead)
        {
            face(anim, sprite, glm::vec2(std::cos(fac.angleRadians), std::sin(fac.angleRadians)));
        }
        const std::string view = viewSuffix(anim.dir);

        const bool moving = vel.dx * vel.dx + vel.dy * vel.dy > 16.0f;
        const Combat* combat = mWorld.hasComponent<Combat>(mPlayerEntity)
            ? &mWorld.getComponent<Combat>(mPlayerEntity)
            : nullptr;
        const std::string set = std::string("player_") + (combat != nullptr ? loadoutName(*combat) : "none");
        // A broken arm is held in against the chest; a hurt leg limps (the
        // step onto it is quick, the step off it lingers).
        woundrules::BodyEffects body;
        if (mWorld.hasComponent<Wounds>(mPlayerEntity))
        {
            body = woundrules::effects(mWorld.getComponent<Wounds>(mPlayerEntity).active);
        }
        const char* armIn = body.armHeldIn ? "armin" : "";
        const auto limpTiming = [&]()
        {
            if (!body.limp) return;
            anim.frameDuration *= (anim.currentFrame % 4 < 2) ? 0.65f : 1.45f;
        };

        if (dead)
        {
            playAnimation(anim, "player_death" + view, 0.085f, false);
        }
        else if (combat != nullptr && combat->phase != AttackPhase::Idle)
        {
            // The swing follows the combat phases frame for frame.
            // A shove is a two-handed push: the punch frames serve.
            const AttackFrames frames = combat->shoving ? AttackFrames{"player_punch", 2, 1, 2} : attackFrames(*combat);
            int frame = 0;
            if (combat->phase == AttackPhase::Windup)
            {
                frame = partFrame(combat->phaseTimer, combat->windupTotal, frames.windup);
            }
            else if (combat->phase == AttackPhase::Active)
            {
                frame = frames.windup + partFrame(combat->phaseTimer, CombatSystem::kActiveSeconds, frames.active);
            }
            else
            {
                frame = frames.windup + frames.active +
                        partFrame(combat->phaseTimer, combat->recoveryTotal, frames.recovery);
            }
            showFrame(anim, frames.name + view, frame);
        }
        else if (mPlayerHurtTimer > 0.0f)
        {
            playAnimation(anim, set + "_hurt" + view, 0.2f);
        }
        else if (moving)
        {
            if (fac.crouching)
            {
                playAnimation(anim, set + "_crouchwalk" + view, 0.11f);
                limpTiming();
            }
            else if (fac.running)
            {
                playAnimation(anim, set + "_run" + armIn + view, 0.062f);
                limpTiming();
            }
            else
            {
                playAnimation(anim, set + "_walk" + armIn + view, 0.075f);
                limpTiming();
            }
        }
        else
        {
            // The body's state shows in how it stands (phase 12, with screen
            // effects on): shoulders down, a shiver, a hand on the knee; slower.
            const std::string mood = moodIdle();
            if (!fac.crouching && !body.armHeldIn && !mood.empty() &&
                mSpriteSheet.frameCount(set + "_idle" + mood + view) > 0)
            {
                playAnimation(anim, set + "_idle" + mood + view, mood == "cold" ? 0.12f : 0.95f);
            }
            else
            {
                playAnimation(anim, set + (fac.crouching ? std::string("_crouch") : std::string("_idle") + armIn) + view, 0.55f);
            }
        }
    }

    // Zombies face where they shamble, and turn to their prey for a lunge.
    mWorld.forEach<ZombieAI, Animation, Velocity, Sprite, Transform>(
        [&](Entity, ZombieAI& ai, Animation& anim, Velocity& vel, Sprite& sprite, Transform& transform)
        {
            const std::string set = zombieSet(ai.kind, ai.look);
            const float speed2 = vel.dx * vel.dx + vel.dy * vel.dy;
            const bool lunging = ai.attackWindup > 0.0f || ai.strikeTimer > 0.0f;
            // A Crawler low in grass or beside furniture cannot be made out
            // from more than 2 tiles off.
            if (ai.kind == ZombieKind::Crawler)
            {
                const glm::vec2 at(transform.x + 16.0f, transform.y + 16.0f);
                const glm::ivec2 tile(static_cast<int>(at.x) / 32, static_cast<int>(at.y) / 32);
                bool cover = false;
                if (glm::length(at - playerWorldCenter()) > zombiekinds::kCrawlerHiddenTiles * 32.0f)
                {
                    cover = mTileMap.getSurfaceType(tile.x, tile.y) == SurfaceType::Grass;
                    // Furniture stands indoors; only there is it worth the search.
                    for (int dy = -1; dy <= 1 && !cover && mTileMap.isIndoors(tile.x, tile.y); ++dy)
                    {
                        for (int dx = -1; dx <= 1 && !cover; ++dx) cover = furnitureAt(tile + glm::ivec2(dx, dy)) != nullptr;
                    }
                }
                sprite.visible = !cover;
            }
            const ZombieKind kind = ai.kind;

            if (ai.dormant)
            {
                // Lying in wait under something: still, low.
                showFrame(anim, set + "_idle" + zombieView(kind, anim.dir), 0);
                return;
            }
            if (ZombieBlow::isDown(ai))
            {
                // On the floor: the fall is the death fall held on its last
                // frame; getting up plays it backwards, slower.
                const std::string fall = set + "_death" + zombieView(kind, anim.dir);
                const int frames = std::max(1, mSpriteSheet.frameCount(fall));
                int frame = frames - 1;
                if (ai.downTimer <= 0.0f)
                {
                    const float progress = 1.0f - std::clamp(ai.riseTimer / combatrules::kRiseSeconds, 0.0f, 1.0f);
                    frame = std::clamp(static_cast<int>((1.0f - progress) * static_cast<float>(frames) * 0.8f), 0, frames - 1);
                }
                showFrame(anim, fall, frame);
                return;
            }
            if (ai.stagger > 0.0f)
            {
                // Thrown back: it keeps facing whoever hit it.
                showFrame(anim, set + "_stagger" + zombieView(kind, anim.dir),
                          kind == ZombieKind::Crawler ? 0 : (ai.stagger > 0.11f ? 0 : 1));
                return;
            }
            const glm::vec2 centre(transform.x + 16.0f, transform.y + 16.0f);
            const bool screaming = ai.screamTimer > 0.0f;
            const bool starting = kind == ZombieKind::Runner && ai.startTimer > 0.0f &&
                                  ai.state == ZombieState::Chase;
            if (lunging || ai.state == ZombieState::Attack || screaming || starting)
            {
                face(anim, sprite, ai.targetWorld - centre);
            }
            else if (speed2 > 1.0f)
            {
                face(anim, sprite, glm::vec2(vel.dx, vel.dy));
            }
            const std::string view = zombieView(kind, anim.dir);

            if (screaming)
            {
                // Head back, jaw open: three frames over the scream.
                const int frames = std::max(1, mSpriteSheet.frameCount(set + "_scream" + view));
                showFrame(anim, set + "_scream" + view,
                          partFrame(ai.screamTimer, zombiekinds::kScreamSeconds, frames));
            }
            else if (starting)
            {
                // The tell: the head snaps up before the sprint.
                showFrame(anim, set + "_alert" + view, 0);
            }
            else if (ai.attackWindup > 0.0f)
            {
                // Coiling back is the warning; the strike follows.
                const float windup = kind == ZombieKind::Runner ? zombiekinds::kRunnerWindupSeconds
                                                                : ZombieAISystem::kZombieAttackWindupSeconds;
                showFrame(anim, set + "_lunge" + view, partFrame(ai.attackWindup, windup, 2));
            }
            else if (ai.strikeTimer > 0.0f)
            {
                showFrame(anim, set + "_lunge" + view,
                          2 + partFrame(ai.strikeTimer, ZombieAISystem::kZombieStrikeSeconds, 3));
            }
            else if (speed2 > 1.0f)
            {
                // The shamble keeps time with how fast it is going; a Runner
                // in a sprint has a gait of its own.
                const float speed = std::sqrt(speed2);
                if (kind == ZombieKind::Runner && ai.sprintTimer > 0.0f && ai.state == ZombieState::Chase)
                {
                    playAnimation(anim, set + "_run" + view, std::clamp(9.0f / speed, 0.05f, 0.12f));
                }
                else
                {
                    playAnimation(anim, set + "_walk" + view, std::clamp(8.0f / speed, 0.07f, 0.2f));
                }
            }
            else if (ai.awareness > senses::kHeadUpAwareness && ai.state != ZombieState::Chase)
            {
                // Something caught its eye: the head comes up.
                showFrame(anim, set + "_alert" + view, 0);
            }
            else
            {
                playAnimation(anim, set + "_idle" + view, 0.6f);
            }
        });

    // Survivors: their own clothes, what they hold, and where they go.
    mWorld.forEach<SurvivorAI, Animation, Velocity, Sprite>(
        [&](Entity, SurvivorAI& ai, Animation& anim, Velocity& vel, Sprite& sprite)
        {
            const std::string set = "surv" + std::to_string(((ai.outfit % 3) + 3) % 3);
            if (mSpriteSheet.frameCount(set + "_none_idle_se") <= 0)
            {
                // An older sheet: the player's frames.
                const bool moving = vel.dx * vel.dx + vel.dy * vel.dy > 4.0f;
                if (moving) face(anim, sprite, glm::vec2(vel.dx, vel.dy));
                playAnimation(anim, std::string(ai.alive ? (moving ? "player_none_walk" : "player_none_idle") : "player_death") +
                                        viewSuffix(anim.dir),
                              moving ? 0.085f : 0.55f, ai.alive);
                return;
            }
            const std::string view = viewSuffix(anim.dir);
            if (!ai.alive || ai.state == SurvivorState::Dead)
            {
                playAnimation(anim, set + "_death" + view, 0.085f, false);
                return;
            }
            if (ai.state == SurvivorState::Sleep && ai.actionTimer > 0.0f)
            {
                showFrame(anim, set + "_sleep" + view, 0);
                return;
            }
            static const char* const kLoads[4] = {"_none", "_club", "_blade", "_gun"};
            const std::string load = kLoads[std::clamp(ai.loadout, 0, 3)];
            if (ai.swingTimer > 0.0f)
            {
                if (glm::length(ai.faceTo) > 0.01f) face(anim, sprite, ai.faceTo);
                const char* move = ai.shooting ? "_shoot" : ai.loadout == 2 ? "_stab" : ai.loadout == 0 ? "_punch" : "_swing";
                const float total = ai.shooting ? 0.3f : 0.4f;
                const std::string name = set + move + view;
                showFrame(anim, name, partFrame(ai.swingTimer, total, std::max(1, mSpriteSheet.frameCount(name))));
                return;
            }
            if (ai.hurtFlash > 0.0f)
            {
                showFrame(anim, set + load + "_hurt" + view, 0);
                return;
            }
            const float speed2 = vel.dx * vel.dx + vel.dy * vel.dy;
            if (speed2 > 4.0f)
            {
                face(anim, sprite, glm::vec2(vel.dx, vel.dy));
                const bool running = speed2 > 120.0f * 120.0f;
                playAnimation(anim, set + load + (running ? "_run" : "_walk") + viewSuffix(anim.dir), running ? 0.075f : 0.085f);
                return;
            }
            if (glm::length(ai.faceTo) > 0.01f) face(anim, sprite, ai.faceTo);
            playAnimation(anim, set + load + "_idle" + viewSuffix(anim.dir), 0.55f);
        });

    mAnimationSystem.update(mWorld, mSpriteSheet, dtSeconds);
}

void Game::updateAtmosphere(float dtSeconds)
{
    mMuzzleFlash = std::max(0.0f, mMuzzleFlash - dtSeconds);
    mAtmosphere.update(dtSeconds, mWeatherState, mCamera, mTileMap, mParticleSystem);
    if (mAtmosphere.takeThunder())
    {
        mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.18f);
    }

    // Everyone on their feet stirs the undergrowth and kicks what lies about.
    mSceneryMovers.clear();
    if (dtSeconds > 0.0f)
    {
        const auto add = [&](const Transform& transform)
        {
            mSceneryMovers.push_back(SceneryMover{
                glm::vec2(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f),
                glm::vec2(transform.x - transform.prevX, transform.y - transform.prevY) / dtSeconds});
        };
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            add(mWorld.getComponent<Transform>(mPlayerEntity));
        }
        mWorld.forEach<ZombieAI, Transform>([&](Entity, ZombieAI&, Transform& transform) { add(transform); });
    }
    mRustled.clear();
    mSceneryView.update(dtSeconds, mAtmosphere.wind(), mAtmosphere.gust(), mSceneryMovers, mTileMap, &mRustled);
    for (const glm::vec2& at : mRustled)
    {
        for (int i = 0; i < 2; ++i)
        {
            mParticleSystem.spawnLeaf(at, 8.0f, mAtmosphere.wind() * 0.2f, glm::vec4(0.36f, 0.48f, 0.28f, 1.0f));
        }
    }

    // Leaves come off the trees, a few in still air and a stream in a gale.
    const float gust = mAtmosphere.gust();
    mLeafDebt += (0.25f + gust * gust * 9.0f) * dtSeconds;
    if (mLeafDebt >= 1.0f)
    {
        mCanopies.clear();
        const glm::vec2 centre = mCamera.screenToWorld(mCamera.viewportSize() * 0.5f);
        mSceneryView.canopiesNear(centre, 520.0f, mCanopies);
        while (mLeafDebt >= 1.0f)
        {
            mLeafDebt -= 1.0f;
            if (mCanopies.empty())
            {
                continue;
            }
            // Autumn trees shed three times as freely.
            static std::uint32_t pick = 12345u;
            pick = pick * 1664525u + 1013904223u;
            const SceneryCanopy& tree = mCanopies[(pick >> 8) % mCanopies.size()];
            if (!tree.autumn && ((pick >> 20) % 3u) != 0u)
            {
                continue;
            }
            const glm::vec4 color = tree.autumn ? glm::vec4(0.80f, 0.46f, 0.18f, 1.0f)
                                                : glm::vec4(0.42f, 0.54f, 0.30f, 1.0f);
            mParticleSystem.spawnLeaf(tree.position, 38.0f + static_cast<float>((pick >> 12) % 24u),
                                      mAtmosphere.wind() * 0.55f, color);
        }
    }
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
                    if (structure.type != StructureType::Campfire || !structure.lit || structure.hp <= 0.0f)
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

    // A word, not a number: what the streets look like.
    const int level = ruinLevel(districtId);
    mDistrictToastText = std::string(districtTypeLabel(districtType)) + " district - " + ruinWord(level);
    mDistrictToastDanger = level >= 2;
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

    // In a vehicle there are no footsteps: its own noise is made in
    // updateVehicleAfterPhysics (GameVehicles.cpp).
    if (mActiveVehicleEntity != kInvalidEntity)
    {
        mNoiseEmitTimer = 0.0f;
        return;
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
    // Snow muffles footsteps outdoors: one tier quieter (phase 7).
    if (mTileMap.inBounds(tileX, tileY) && !mTileMap.isIndoors(tileX, tileY) &&
        (weatherrules::footstepTierShift(mWeatherState.condition) < 0 || mWeatherState.snowCover >= weatherrules::kSnowDeepEnough))
    {
        noiseTier = NoiseModel::adjustTier(noiseTier, -1);
    }

    // Clumsy trait: +1 noise tier on footsteps
    if (mWorld.hasComponent<Traits>(mPlayerEntity))
    {
        noiseTier = NoiseModel::adjustTier(noiseTier, traitrules::footstepTierBonus(mWorld.getComponent<Traits>(mPlayerEntity)));
    }
    // Shaken: every other step lands a tier louder (phase 5).
    if (noiseTier != NoiseTier::None && mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        mindrules::louderStep(mindrules::panicBand(mWorld.getComponent<MentalState>(mPlayerEntity).panic), mStepIndex))
    {
        noiseTier = NoiseModel::adjustTier(noiseTier, 1);
    }

    mMovementNoiseTier = noiseTier;

    if (noiseTier == NoiseTier::None)
    {
        mNoiseEmitTimer = 0.0f;
        // Too quiet for the dead, not for you: the step is still heard (its
        // own timer, so the noise's rhythm stays as it was).
        mQuietStepTimer += dtSeconds;
        if (mQuietStepTimer >= 0.3f)
        {
            mQuietStepTimer = 0.0f;
            playAt(footstepSound(glm::ivec2(tileX, tileY), 0), feetPosition, 0.7f);
        }
        return;
    }

    const float emissionInterval = facing.running ? 0.14f : (facing.crouching ? 0.28f : 0.22f);

    mNoiseEmitTimer += dtSeconds;
    if (mNoiseEmitTimer < emissionInterval)
    {
        return;
    }

    mNoiseEmitTimer = 0.0f;
    ++mStepIndex;
    // The step, on what it lands on (phase 12).
    const int stance = facing.crouching ? 0 : (facing.running ? 2 : 1);
    makeNoise(feetPosition, noiseTier, tierDurationSeconds(noiseTier), footstepSound(glm::ivec2(tileX, tileY), stance),
              mPlayerEntity);
}
