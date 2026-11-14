// Game lifecycle: startup, runs, saving, input plumbing and small queries.
#include "core/GameShared.h"
#include "world/WeatherRules.h"

using namespace gameshared;

Game::Game(LaunchOptions options)
    : mLaunchOptions(std::move(options))
{
    if (mLaunchOptions.harnessEnabled())
    {
        mHarness = std::make_unique<DevHarness>(mLaunchOptions);
    }
}

Game::~Game()
{
    shutdown();
}

int Game::run()
{
    if (mHarness)
    {
        std::string error;
        if (!mHarness->loadScript(error))
        {
            std::cerr << "[harness] " << error << '\n';
            return 1;
        }
    }

    if (mLaunchOptions.worldSeed)
    {
        SaveManager::setSeedOverride(*mLaunchOptions.worldSeed);
        mCombatSystem.reseed(*mLaunchOptions.worldSeed);
    }

    if (!initialize())
    {
        return 1;
    }

    if (mHarness)
    {
        // Deterministic stepping: exactly one fixed tick per loop iteration,
        // as fast as the machine allows. Rendering happens on demand
        // (screenshots) or every tick with --watch.
        while (mIsRunning && !mHarness->finished())
        {
            processInputEvents();
            mHarness->beforeTick(*this);
            snapshotPositions();
            update(static_cast<float>(kFixedTimeStepSeconds));
            mInput.consumeTransitions();
            mHarness->afterTick(*this);

            if (mLaunchOptions.watch)
            {
                mInterpolationAlpha = 1.0f;
                render();
                SDL_GL_SwapWindow(mWindow);
            }
        }

        const int exitCode = mHarness->exitCode();
        mHarness.reset();
        shutdown();
        return exitCode;
    }

    using clock = std::chrono::steady_clock;
    auto previousTime = clock::now();
    double accumulator = 0.0;

    while (mIsRunning)
    {
        const auto currentTime = clock::now();
        double frameSeconds = std::chrono::duration<double>(currentTime - previousTime).count();
        previousTime = currentTime;

        if (frameSeconds > kMaxFrameStepSeconds)
        {
            frameSeconds = kMaxFrameStepSeconds;
        }

        accumulator += frameSeconds;
        processInputEvents();

        while (accumulator >= kFixedTimeStepSeconds)
        {
            snapshotPositions();
            update(static_cast<float>(kFixedTimeStepSeconds));
            mInput.consumeTransitions();
            accumulator -= kFixedTimeStepSeconds;
        }

        mInterpolationAlpha = static_cast<float>(accumulator / kFixedTimeStepSeconds);
        render();
        updateWindowTitle(frameSeconds);
        SDL_GL_SwapWindow(mWindow);
    }

    shutdown();
    return 0;
}

bool Game::initialize()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    mWindowWidth = kDefaultWindowWidth;
    mWindowHeight = kDefaultWindowHeight;

    mWindow = SDL_CreateWindow(
        "Dead Pixel Survival",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        mWindowWidth,
        mWindowHeight,
        (mLaunchOptions.hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN) | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    if (mWindow == nullptr)
    {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    mGlContext = SDL_GL_CreateContext(mWindow);
    if (mGlContext == nullptr)
    {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    if (SDL_GL_MakeCurrent(mWindow, mGlContext) != 0)
    {
        std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    // The harness steps as fast as possible; vsync would throttle --watch runs.
    SDL_GL_SetSwapInterval(mHarness ? 0 : 1);

    glewExperimental = GL_TRUE;
    const GLenum glewResult = glewInit();
    glGetError();
    if (glewResult != GLEW_OK)
    {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewResult) << '\n';
        shutdown();
        return false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const std::filesystem::path assetsRoot = getExecutableDirectory() / "assets";

    mTitleSaveSlotIndex = 0;
    mActiveSaveSlot = kTitleSaveSlots[static_cast<std::size_t>(mTitleSaveSlotIndex)];
    if (!loadSlotAndWorld(mActiveSaveSlot))
    {
        std::cerr << "Failed to initialize save slot and world data." << '\n';
        shutdown();
        return false;
    }

    const std::string vertexShaderPath = (assetsRoot / "shaders" / "sprite.vert").string();
    const std::string fragmentShaderPath = (assetsRoot / "shaders" / "sprite.frag").string();

    const std::filesystem::path texturePath =
        (assetsRoot / "maps" / mTileMap.tileset().imagePath).lexically_normal();

    if (!mShader.loadFromFiles(vertexShaderPath, fragmentShaderPath))
    {
        shutdown();
        return false;
    }

    if (!mTexture.loadFromFile(texturePath.string()))
    {
        shutdown();
        return false;
    }

    if (!mSpriteBatch.initialize())
    {
        shutdown();
        return false;
    }
    // The mood pass (phase 12). Without it the game runs, only plainer.
    if (!mPostFx.initialize((assetsRoot / "shaders" / "postfx.vert").string(),
                            (assetsRoot / "shaders" / "postfx.frag").string()))
    {
        std::cerr << "Mood pass unavailable; screen effects off.\n";
    }

    const std::filesystem::path fontPng = assetsRoot / "fonts" / "pixel_font.png";
    const std::filesystem::path fontJson = assetsRoot / "fonts" / "pixel_font.json";
    if (!mFont.loadFromFiles(fontPng.string(), fontJson.string()))
    {
        std::cerr << "Failed to load bitmap font.\n";
        shutdown();
        return false;
    }

    const std::filesystem::path sheetPng = assetsRoot / "sprites" / "spritesheet.png";
    const std::filesystem::path sheetJson = assetsRoot / "sprites" / "spritesheet.json";
    if (!mSpriteSheet.loadFromFiles(sheetPng.string(), sheetJson.string()))
    {
        std::cerr << "Failed to load sprite sheet.\n";
        shutdown();
        return false;
    }
    mStructureSprites = StructureSprites::fromSheet(mSpriteSheet);

    setupSound(assetsRoot); // harness runs are silent (GameSound.cpp)

    if (!loadContent(assetsRoot))
    {
        shutdown();
        return false;
    }

    mProjection = buildProjection(mWindowWidth, mWindowHeight);
    mCamera.setViewportSize(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight));
    mCamera.clampToBounds(mTileMap.pixelSize());
    // Boot to title screen — do not start a run yet
    mRunState = RunState::TitleScreen;
    mTitleMenuSelection = 0;
    syncPresentationSettingsFromSave();
    mTitleHasCheckpoint = false;
    mTitleCheckpointDay = 1;
    mTitleCheckpointName.clear();
    mTitleCheckpointX = 0.0f;
    mTitleCheckpointY = 0.0f;
    mShowTitleCredits = false;
    mTitleStatusMessage.clear();
    mTitleStatusTimer = 0.0f;
    mTitlePulseTimer = 0.0f;
    mTitleConfirmNewWorld = false;
    mTitleConfirmNewWorldTimer = 0.0f;
    mIgnoreCheckpointRestoreOnce = false;

    mIsRunning = true;
    mShowNoiseDebug = false;
    mShowDistrictDebug = false;
    return true;
}

bool Game::loadSlotAndWorld(const std::string& slotName)
{
    if (!mSaveManager.loadOrCreateSlot(slotName))
    {
        return false;
    }

    if (mSaveManager.hasWorldLayout())
    {
        LayoutData layout = mSaveManager.loadWorldLayout();
        if (layout.width > 0 && layout.height > 0)
        {
            // A save from before phase 9: the gate goes where its objective stood.
            if (stampGate(layout)) mSaveManager.saveWorldLayout(layout);
            mMapGenerator.loadLayout(layout);
        }
        else
        {
            LayoutData regenerated = mMapGenerator.generateLayout(mSaveManager.worldSeed());
            stampGate(regenerated);
            mMapGenerator.loadLayout(regenerated);
            mSaveManager.saveWorldLayout(regenerated);
        }
    }
    else
    {
        LayoutData layout = mMapGenerator.generateLayout(mSaveManager.worldSeed());
        stampGate(layout);
        mMapGenerator.loadLayout(layout);
        mSaveManager.saveWorldLayout(layout);
    }

    mMapGenerator.applyToTileMap(mTileMap);
    placeWindows(); // before build: the walls are drawn with their windows
    mStructureView.build(mTileMap, mMapGenerator.layoutData());
    resetDoors(); // after build: closed doors are solid
    furnishWorld();
    plantScenery();
    mLayoutFingerprint = computeLayoutFingerprint(mMapGenerator.layoutData());
    mActiveSaveSlot = slotName;
    syncPresentationSettingsFromSave();
    mCamera.clampToBounds(mTileMap.pixelSize());

    std::cout << "[World] slot=" << slotName
              << " worldSeed=" << mSaveManager.worldSeed()
              << " runSeed=" << mSaveManager.currentRunSeed()
              << " layoutFingerprint=0x" << std::hex << mLayoutFingerprint << std::dec
              << '\n';

    return true;
}

void Game::syncPresentationSettingsFromSave()
{
    const RunPresentationSettings& settings = mSaveManager.presentationSettings();
    mTitleGameModeIndex = wrapIndex(indexFromSaveGameMode(settings.gameMode), static_cast<int>(kTitleGameModeNames.size()));
    mTitleModeIndex = wrapIndex(indexFromSaveThreatMode(settings.threatMode), static_cast<int>(kTitleModeNames.size()));
    mTitleToneIndex = wrapIndex(indexFromSaveTonePreset(settings.tonePreset), static_cast<int>(kTitleToneNames.size()));
}

void Game::syncPresentationSettingsToSave()
{
    RunPresentationSettings settings = mSaveManager.presentationSettings();
    settings.gameMode = saveGameModeFromIndex(mTitleGameModeIndex);
    settings.threatMode = saveThreatModeFromIndex(mTitleModeIndex);
    settings.tonePreset = saveTonePresetFromIndex(mTitleToneIndex);
    mSaveManager.setPresentationSettings(settings);
    mSaveManager.savePresentationSettings();
}

RunCheckpoint Game::buildRunCheckpoint()
{
    RunCheckpoint cp;
    // The town's people are kept beside the checkpoint (the survivors file).
    saveSurvivorRecords();
    cp.runSeed = mSaveManager.currentRunSeed();
    gateCheckpoint(cp); // what this character knows of the way out (phase 9)
    journalCheckpoint(cp); // and what they wrote down (phase 10)
    cp.characterName = mCurrentCharacterName;
    cp.currentDay = mCurrentDay;
    cp.gameHours = mGameHours;
    cp.weather.saved = true;
    cp.weather.condition = static_cast<int>(mWeatherState.condition);
    cp.weather.hoursLeft = mWeatherSystem.hoursLeft();
    cp.weather.rngState = mWeatherSystem.rngState();
    cp.weather.snowCover = mWeatherState.snowCover;

    if (mPlayerEntity != kInvalidEntity)
    {
        if (mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            const auto& t = mWorld.getComponent<Transform>(mPlayerEntity);
            cp.playerX = t.x;
            cp.playerY = t.y;
        }
        if (mWorld.hasComponent<Health>(mPlayerEntity))
        {
            const auto& h = mWorld.getComponent<Health>(mPlayerEntity);
            cp.healthCurrent = h.current;
            cp.healthMaximum = h.maximum;
            cp.healthBase = h.baseMaximum;
        }
        if (mWorld.hasComponent<Needs>(mPlayerEntity))
            cp.needs = mWorld.getComponent<Needs>(mPlayerEntity);
        if (mWorld.hasComponent<MentalState>(mPlayerEntity))
            cp.mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        if (mWorld.hasComponent<Wounds>(mPlayerEntity))
            cp.wounds = mWorld.getComponent<Wounds>(mPlayerEntity).active;
        cp.bleeding = mWorld.hasComponent<Bleeding>(mPlayerEntity);
        if (mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            cp.traitsSaved = true;
            for (PositiveTrait trait : traits.positives) cp.positiveTraits.emplace_back(traitrules::key(trait));
            for (NegativeTrait trait : traits.negatives) cp.negativeTraits.emplace_back(traitrules::key(trait));
        }
        if (mWorld.hasComponent<Skills>(mPlayerEntity))
        {
            cp.skillsSaved = true;
            cp.skills = mWorld.getComponent<Skills>(mPlayerEntity);
        }
        if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            cp.inventory = mWorld.getComponent<Inventory>(mPlayerEntity);
        cp.equippedSlot = mEquippedWeaponSlot;
    }

    cp.kills = mRunStats.kills;
    cp.itemsLooted = mRunStats.itemsLooted;
    cp.structuresBuilt = mRunStats.structuresBuilt;
    cp.distanceWalked = mRunStats.distanceWalked;

    for (const glm::ivec2& tile : mDoors.tilesIn(mTileMap, DoorState::Open))
        cp.openDoors.emplace_back(tile.x, tile.y);
    for (const glm::ivec2& tile : mDoors.tilesIn(mTileMap, DoorState::Broken))
        cp.brokenDoors.emplace_back(tile.x, tile.y);
    cp.boardsSaved = true;
    cp.boardedDoors = boardedDoorRecords();
    cp.burnedTiles = mBurnedTiles;
    cp.fires = fireRecords();
    recordTheDead(cp);
    cp.firstDaysSaved = true;
    cp.firstDays = firstDaysRecord();
    cp.windowsSaved = true;
    cp.windows = windowRecords();
    cp.structuresSaved = true;
    cp.structures = structureRecords(false);
    cp.scavenge = scavengeRunRecord();
    return cp;
}

bool Game::saveCurrentRunCheckpoint(const std::string& statusMessage)
{
    if (mPlayerEntity == kInvalidEntity)
    {
        return false;
    }

    syncLegacyToSave();
    saveDistrictMemory();
    flushGate();

    const bool ok = mSaveManager.saveCheckpoint(buildRunCheckpoint());
    if (ok)
    {
        refreshContainerRecords();
        persistVehicles(false);
        mSaveManager.saveWorldMemory();
        mInteractionMessage = statusMessage;
        mInteractionMessageTimer = 2.0f;
    }
    else
    {
        mInteractionMessage = "Checkpoint save failed.";
        mInteractionMessageTimer = 2.5f;
    }

    return ok;
}

void Game::registerComponents()
{
    mWorld.registerComponents<Transform, Sprite, Velocity, PlayerInput, Facing, Stamina, Combat, Health, ZombieAI, Needs,
                              Interactable, Animation, Bleeding, Sleeping, Inventory, MentalState, Structure, Vehicle,
                              TrunkOf, Fire, Projectile, Wounds, Traits, Skills, SurvivorAI, LegacyLink, Corpse>();
}

void Game::startNewRun()
{
    mWorld = World{};
    registerComponents();

    mDebugEntities.clear();
    mNoiseModel = NoiseModel{};
    mNoiseEmitTimer = 0.0f;
    mMovementNoiseTier = NoiseTier::None;

    mRunState = RunState::Playing;
    mDeathStateTimer = 0.0;
    mDeathCause.clear();
    mDeathWorldPosition = glm::vec2(0.0f, 0.0f);

    mGameHours = 8.0;
    mCurrentDay = 1;
    mShowInventory = false;
    mShowCrafting = false;
    mCraftingRecipeCursor = 0;
    mCraftingQuantity = 1;
    mCraftingCategory = 0;
    mSessionDiscoveredRecipes.clear();
    mCraftQueue.clear();
    mActiveCraftProgressSeconds = 0.0f;

    mRunStats = RunStats{};
    mContainersOpened = 0;

    mCurrentCharacterName = generateCharacterName(mSaveManager.currentRunSeed());

    mInteractionMessage.clear();
    mInteractionMessageTimer = 0.0f;
    mContainerOpen = false;
    mOpenContainerEntity = kInvalidEntity;
    mContainerCursor = 0;
    mTalk = TalkState{};
    mSinceWeaponOut = 99.0f;
    mRobbedAsleep = false;
    mSurvivorSmokeTimer = 0.0f;
    mLitHomes.clear();
    // Sound and mood (phase 12): a new town, a new clock; the dead as last heard.
    mHeardDead.clear();
    mMusic.reset();
    mTinnitusLeft = 0.0f;
    mListening = false;
    mListenStill = 0.0f;
    mClockBuilding = -2;
    mClockShows = -1.0;
    mActiveVehicleEntity = kInvalidEntity;
    clearFireAndWeatherMarks();
    mShowWorldMap = false;
    mShowNotebook = false;
    mNotebookPage = 0;
    resetJournal();

    // Generate run-variant spawns against the stable layout (map is NOT regenerated)
    mMapGenerator.generateSpawns(mSpawnData, mSaveManager.currentRunSeed());
    nudgeSpawnsOffFurniture();
    avoidGuttedStart();

    // The house an earlier life held: the new character starts there three
    // times in ten, and always when it was the last one's home and they died
    // away from it.
    {
        const BaseRecord& base = mSaveManager.baseRecord();
        const LastHomeRecord& last = mSaveManager.lastHome();
        std::mt19937 baseRng(mSaveManager.currentRunSeed() ^ 0xBA5Eu);
        const bool chance = std::uniform_real_distribution<float>(0.0f, 1.0f)(baseRng) < 0.3f;
        if (base.building > 0 && !guttedInMemory(base.building) && ((last.building == base.building && last.diedAway) || chance))
        {
            const glm::vec2 centre(base.x / static_cast<float>(mTileMap.tileWidth()),
                                   base.y / static_cast<float>(mTileMap.tileHeight()));
            glm::ivec2 best(-1);
            float bestDistance = 0.0f;
            for (int y = 0; y < mTileMap.height(); ++y)
            {
                for (int x = 0; x < mTileMap.width(); ++x)
                {
                    if (mTileMap.getBuildingId(x, y) != base.building || !mTileMap.isIndoors(x, y) || mTileMap.isSolid(x, y))
                        continue;
                    const float distance = glm::length(glm::vec2(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f) - centre);
                    if (best.x < 0 || distance < bestDistance)
                    {
                        best = glm::ivec2(x, y);
                        bestDistance = distance;
                    }
                }
            }
            if (best.x >= 0)
            {
                mSpawnData.playerStart = best;
                std::erase_if(mSpawnData.zombieSpawns, [&](const glm::ivec2& tile)
                              { return mTileMap.getBuildingId(tile.x, tile.y) == base.building; });
                std::cout << "[World] starting in the house held before (building " << base.building << ")" << std::endl;
            }
        }
    }

    // The town's people (phase 8): never a start in someone else's house.
    loadSurvivorRecords();
    avoidSurvivorHomes();

    // The mode, the tone, the dead the last life cleared, the town's growth.
    tuneRunSpawns();

    const std::uint64_t runLayoutFingerprint = computeLayoutFingerprint(mMapGenerator.layoutData());
    if (mLayoutFingerprint != 0 && runLayoutFingerprint != mLayoutFingerprint)
    {
        std::cerr << "[World] Layout fingerprint changed between runs. expected=0x"
                  << std::hex << mLayoutFingerprint << " actual=0x" << runLayoutFingerprint << std::dec << '\n';
    }
    else if (mLayoutFingerprint == 0)
    {
        mLayoutFingerprint = runLayoutFingerprint;
    }

    mCamera.clampToBounds(mTileMap.pixelSize());

    setupScene();

    const bool allowCheckpointRestore = !mIgnoreCheckpointRestoreOnce;
    mIgnoreCheckpointRestoreOnce = false;
    bool restoredCheckpoint = false;

    // Attempt to restore from checkpoint
    RunCheckpoint cp;
    if (allowCheckpointRestore)
    {
        cp = mSaveManager.loadCheckpoint();
    }

    if (allowCheckpointRestore && cp.valid && mPlayerEntity != kInvalidEntity)
    {
        if (mWorld.hasComponent<Transform>(mPlayerEntity))
        {
            auto& t = mWorld.getComponent<Transform>(mPlayerEntity);
            t.x = cp.playerX; t.y = cp.playerY;
        }
        // Who they were: the chosen traits, or (a checkpoint from before
        // the character screen) the seed's old pick; then health as saved.
        {
            Traits traits = traitrules::legacyPick(mSaveManager.currentRunSeed());
            if (cp.traitsSaved)
            {
                traits.positives.clear();
                traits.negatives.clear();
                for (const std::string& key : cp.positiveTraits)
                {
                    if (const auto trait = traitrules::parsePositive(key)) traits.positives.push_back(*trait);
                }
                for (const std::string& key : cp.negativeTraits)
                {
                    if (const auto trait = traitrules::parseNegative(key)) traits.negatives.push_back(*trait);
                }
            }
            applyCharacter(traits, true);
            if (cp.skillsSaved && mWorld.hasComponent<Skills>(mPlayerEntity))
            {
                mWorld.getComponent<Skills>(mPlayerEntity) = cp.skills;
            }
        }
        if (mWorld.hasComponent<Health>(mPlayerEntity))
        {
            auto& h = mWorld.getComponent<Health>(mPlayerEntity);
            h.current = cp.healthCurrent; h.maximum = cp.healthMaximum; h.baseMaximum = cp.healthBase;
        }
        if (mWorld.hasComponent<Needs>(mPlayerEntity))
            mWorld.getComponent<Needs>(mPlayerEntity) = cp.needs;
        if (mWorld.hasComponent<MentalState>(mPlayerEntity))
            mWorld.getComponent<MentalState>(mPlayerEntity) = cp.mental;
        if (mWorld.hasComponent<Wounds>(mPlayerEntity))
            mWorld.getComponent<Wounds>(mPlayerEntity).active = cp.wounds;
        if (cp.bleeding)
        {
            if (!mWorld.hasComponent<Bleeding>(mPlayerEntity))
                mWorld.addComponent<Bleeding>(mPlayerEntity, Bleeding{});
        }
        if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            mWorld.getComponent<Inventory>(mPlayerEntity) = cp.inventory;
        equipStartingWeapon(cp.equippedSlot);

        mCurrentDay = cp.currentDay;
        mGameHours = cp.gameHours;
        mCurrentCharacterName = cp.characterName;
        mRunStats.kills = cp.kills;
        mRunStats.itemsLooted = cp.itemsLooted;
        mRunStats.structuresBuilt = cp.structuresBuilt;
        mRunStats.distanceWalked = cp.distanceWalked;
        restoreTownFromCheckpoint(cp);
        if (cp.weather.saved)
        {
            mWeatherSystem.restore(mWeatherState, static_cast<WeatherCondition>(cp.weather.condition),
                                   cp.weather.hoursLeft, cp.weather.rngState);
            mWeatherState.snowCover = cp.weather.snowCover;
        }
        if (cp.firstDaysSaved)
        {
            restoreFirstDays(cp.firstDays);
        }

        mInteractionMessage = "Resumed " + mCurrentCharacterName + "'s run.";
        mInteractionMessageTimer = 3.0f;
        restoredCheckpoint = true;
    }
    else if (!allowCheckpointRestore)
    {
        mSaveManager.deleteCheckpoint();
    }
    // The way out (phase 9): what this character knew, then the key and the
    // fuse wherever they lie.
    if (restoredCheckpoint) restoreGate(cp);
    restoreJournal(cp, restoredCheckpoint); // and the markers carried (phase 10)
    ensureGateKey();
    ensureGateFuse();
    placeHomeKit();
    plantScenery(); // again, now that the run's people and things are in place
    for (const auto& [bx, by] : mBurnedTiles) mSceneryView.burnTree(glm::ivec2(bx, by));

    if (!restoredCheckpoint)
    {
        if (mTitleModeIndex >= 0 && mTitleModeIndex < static_cast<int>(kTitleModeNames.size()))
        {
            mInteractionMessage = "You are " + mCurrentCharacterName + ".";
            mInteractionMessageTimer = 3.5f;
        }
        // A new life from the menus: choose who first (phase 5).
        if (mCreateCharacterOnStart) beginCharacterCreation();
    }
    mCreateCharacterOnStart = false;
    mHallucinations.reset(mSaveManager.currentRunSeed() ^ 0x5EE1u);
    mPackTab = 0;
    mBodyCursor = 0;
    mBodyChoosing = false;
    mMicrosleepIn = -1.0f;
    mMicrosleepLeft = 0.0f;
    mRunXpPixels = 0.0f;
    mCarryXpHours = 0.0f;
    mPrevHolders = 0;
    mBodyRolls = 0;
    mWeekDayNoted = mCurrentDay;
    mXpSearched.clear();
    mRebleedRunSeconds = 0.0f;
}

void Game::returnToTitleScreen()
{
    // Navigation only: return to title without writing a new checkpoint/save
    // (a gate change kept back from a busy tick is written now).
    flushGate();
    mWorld = World{};
    registerComponents();

    mDebugEntities.clear();
    mPlayerEntity = kInvalidEntity;

    mNoiseModel = NoiseModel{};
    mNoiseEmitTimer = 0.0f;
    mMovementNoiseTier = NoiseTier::None;

    mBaseAction = BaseAction{};
    mBuildWheelOpen = false;
    mBuildChoice = -1;
    mWindowClimbs.clear();
    mParticleSystem = ParticleSystem{};

    mRunState = RunState::TitleScreen;
    mPauseSelection = 0;

    mDeathStateTimer = 0.0;
    mDeathCause.clear();
    mDeathWorldPosition = glm::vec2(0.0f, 0.0f);

    mGameHours = 8.0;
    mCurrentDay = 1;
    mShowInventory = false;
    mInventoryCursor = 0;
    mShowCrafting = false;
    mCraftingRecipeCursor = 0;
    mCraftingQuantity = 1;
    mCraftingCategory = 0;
    mSessionDiscoveredRecipes.clear();
    mCraftQueue.clear();
    mActiveCraftProgressSeconds = 0.0f;
    mContainerOpen = false;
    mOpenContainerEntity = kInvalidEntity;
    mContainerCursor = 0;
    mTalk = TalkState{};
    mSinceWeaponOut = 99.0f;
    mRobbedAsleep = false;
    mSurvivorSmokeTimer = 0.0f;
    mLitHomes.clear();
    // Sound and mood (phase 12): a new town, a new clock; the dead as last heard.
    mHeardDead.clear();
    mMusic.reset();
    mTinnitusLeft = 0.0f;
    mListening = false;
    mListenStill = 0.0f;
    mClockBuilding = -2;
    mClockShows = -1.0;
    mActiveVehicleEntity = kInvalidEntity;
    clearFireAndWeatherMarks();
    mShowWorldMap = false;
    mShowNotebook = false;
    mRunStats = RunStats{};
    mContainersOpened = 0;

    mInteractionMessage.clear();
    mInteractionMessageTimer = 0.0f;
    mProximityPrompt.clear();
    mCurrentCharacterName.clear();

    mShowControls = false;
    mShowTitleCredits = false;
    mTitleMenuSelection = 0;
    mTitleConfirmNewWorld = false;
    mTitleConfirmNewWorldTimer = 0.0f;
    mTitleStatusMessage = "Returned to title menu.";
    mTitleStatusTimer = 2.0f;

    refreshTitleCheckpoint();

    const auto& layout = mMapGenerator.layoutData();
    if (layout.width > 0 && layout.height > 0)
    {
        const glm::vec2 startCenter(
            (static_cast<float>(layout.playerStart.x) + 0.5f) * static_cast<float>(mTileMap.tileWidth()),
            (static_cast<float>(layout.playerStart.y) + 0.5f) * static_cast<float>(mTileMap.tileHeight()));
        mCamera.snapTo(startCenter);
    }
    mCamera.clampToBounds(mTileMap.pixelSize());
}

void Game::shutdown()
{
    flushGate();
    mDebugEntities.clear();
    mPlayerEntity = kInvalidEntity;
    mMovementNoiseTier = NoiseTier::None;

    mSpriteBatch.shutdown();
    mPostFx.shutdown();
    mTexture.unload();
    mAudioManager.shutdown();

    if (mGlContext != nullptr)
    {
        SDL_GL_DeleteContext(mGlContext);
        mGlContext = nullptr;
    }

    if (mWindow != nullptr)
    {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
    {
        SDL_Quit();
    }

    mIsRunning = false;
}

void Game::processInputEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        mInput.processEvent(event);

        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            mWindowWidth = event.window.data1;
            mWindowHeight = event.window.data2;
            mProjection = buildProjection(mWindowWidth, mWindowHeight);
            mCamera.setViewportSize(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight));
            mCamera.clampToBounds(mTileMap.pixelSize());
        }
    }

    if (!mHarness)
    {
        mInput.refreshMouseState();
    }

    if (mInput.quitRequested())
    {
        mIsRunning = false;
    }

    // Mouse wheel zoom (frame-level: scroll is consumed here, not per tick)
    if (const int scroll = mInput.consumeScroll(); scroll != 0)
    {
        const bool minimapZoomInput =
            mRunState == RunState::Playing &&
            !mShowWorldMap &&
            (mInput.isKeyDown(SDL_SCANCODE_LCTRL) || mInput.isKeyDown(SDL_SCANCODE_RCTRL));

        if (minimapZoomInput)
        {
            const float zoomFactor = 1.0f + static_cast<float>(scroll) * kMinimapZoomStep;
            mMinimapZoom = std::clamp(mMinimapZoom * zoomFactor, kMinimapZoomMin, kMinimapZoomMax);
        }
        else
        {
            constexpr float kZoomStep = 0.15f;
            const float currentZoom = mCamera.zoom();
            const float newZoom = currentZoom * (1.0f + static_cast<float>(scroll) * kZoomStep);
            const glm::vec2 mouseScreen(static_cast<float>(mInput.mouseX()), static_cast<float>(mInput.mouseY()));
            mCamera.zoomToward(newZoom, mouseScreen);
            mCamera.clampToBounds(mTileMap.pixelSize());
        }
    }
}

void Game::handleEscapeKey()
{
    if (mInput.wasKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        // While playing, Escape closes the topmost open panel; it only pauses
        // when nothing is open. Panel handlers later in the tick see the same
        // press, but their panels are already closed so it is a no-op.
        if (mRunState == RunState::Playing)
        {
            if (mShowCrafting)
            {
                mShowCrafting = false;
            }
            else if (mContainerOpen)
            {
                mContainerOpen = false;
                mOpenContainerEntity = kInvalidEntity;
            }
            else if (mTalk.open)
            {
                // Out of trade or give first, then the talk.
                if (mTalk.view != TalkState::View::Talk) mTalk.view = TalkState::View::Talk;
                else closeTalk();
            }
            else if (mBuildWheelOpen || mBuildChoice >= 0)
            {
                mBuildWheelOpen = false;
                mBuildChoice = -1;
            }
            else if (mShowWorldMap)
            {
                mShowWorldMap = false;
            }
            else if (mShowNotebook)
            {
                mShowNotebook = false;
            }
            else if (mShowInventory && mBodyChoosing)
            {
                mBodyChoosing = false;
            }
            else if (mShowInventory)
            {
                mShowInventory = false;
            }
            else
            {
                mRunState = RunState::Paused;
                mPauseSelection = 0;
            }
        }
        else if (mRunState == RunState::Paused)
        {
            mRunState = RunState::Playing;
        }
        else if (mRunState == RunState::TitleScreen)
        {
            if (mShowTitleCredits)
            {
                mShowTitleCredits = false;
            }
            else if (mShowControls)
            {
                mShowControls = false;
            }
            else
            {
                mIsRunning = false;
            }
        }
    }
}

void Game::snapshotPositions()
{
    mWorld.forEach<Transform>(
        [](Entity, Transform& t)
        {
            t.prevX = t.x;
            t.prevY = t.y;
        });
}

glm::ivec2 Game::playerTile() const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        return glm::ivec2(-1000, -1000);
    }
    const Transform& t = mWorld.getComponent<Transform>(mPlayerEntity);
    const float tileW = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float tileH = static_cast<float>(std::max(1, mTileMap.tileHeight()));
    return glm::ivec2(
        static_cast<int>(std::floor((t.x + kSpriteSize * 0.5f) / tileW)),
        static_cast<int>(std::floor((t.y + kSpriteSize * 0.5f) / tileH)));
}

float Game::nightFactor() const
{
    // 0 = full day, 1 = full night; dusk and dawn move with the seasons.
    return weatherrules::nightFactor(mCurrentDay, mGameHours);
}

void Game::refreshTitleCheckpoint()
{
    const RunCheckpoint titleCheckpoint = mSaveManager.loadCheckpoint();
    mTitleHasCheckpoint = titleCheckpoint.valid;
    mTitleCheckpointDay = titleCheckpoint.valid ? titleCheckpoint.currentDay : 1;
    mTitleCheckpointName = titleCheckpoint.valid ? titleCheckpoint.characterName : std::string{};
    mTitleCheckpointX = titleCheckpoint.valid ? titleCheckpoint.playerX : 0.0f;
    mTitleCheckpointY = titleCheckpoint.valid ? titleCheckpoint.playerY : 0.0f;
    mTitleCheckpointRefreshTimer = 1.0f;
}

bool Game::playerHasItem(int itemId) const
{
    return itemId >= 0 && mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity) &&
           InventorySystem::countItem(mWorld.getComponent<Inventory>(mPlayerEntity), itemId) > 0;
}

void Game::updateWindowTitle(double frameSeconds)
{
    mFpsTimer += frameSeconds;
    mFramesThisSecond += 1;

    if (mFpsTimer < 1.0)
    {
        return;
    }

    const double fps = static_cast<double>(mFramesThisSecond) / mFpsTimer;

    std::size_t zombieCount = 0;
    for (const Entity entity : mWorld.livingEntities())
    {
        if (mWorld.hasComponent<ZombieAI>(entity))
        {
            ++zombieCount;
        }
    }

    std::ostringstream title;
    title << "Dead Pixel Survival | FPS: "
          << std::fixed << std::setprecision(1) << fps
          << " | Entities: " << mWorld.livingEntities().size()
          << " | Zombies: " << zombieCount;

    if (mRunState == RunState::Dead)
    {
        title << " | DEAD";
    }

    title << " | F1 Noise: " << (mShowNoiseDebug ? "ON" : "OFF");

    if (!mDebugEntities.empty())
    {
        title << " | K: remove test entity";
    }

    SDL_SetWindowTitle(mWindow, title.str().c_str());

    mFpsTimer = 0.0;
    mFramesThisSecond = 0;
}

glm::vec2 Game::screenToWorld(const glm::vec2& screenPosition) const
{
    return mCamera.screenToWorld(screenPosition);
}
