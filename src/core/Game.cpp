// Game lifecycle: startup, runs, saving, input plumbing and small queries.
#include "core/GameShared.h"

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

    if (!mAudioManager.initialize(assetsRoot.string()))
    {
        std::cerr << "Warning: Audio init failed, continuing without sound.\n";
    }
    mAudioManager.playAmbient(AmbientId::Wind, 0.15f);

    const std::filesystem::path itemsJson = assetsRoot / "data" / "items.json";
    if (!mItemDatabase.loadFromFile(itemsJson.string()))
    {
        std::cerr << "Failed to load item database.\n";
        shutdown();
        return false;
    }

    const std::filesystem::path recipesJson = assetsRoot / "data" / "recipes.json";
    if (!mCraftingSystem.loadRecipesFromFile(recipesJson.string()))
    {
        std::cerr << "Failed to load crafting recipes.\n";
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
            mMapGenerator.loadLayout(layout);
        }
        else
        {
            LayoutData regenerated = mMapGenerator.generateLayout(mSaveManager.worldSeed());
            mSaveManager.saveWorldLayout(regenerated);
        }
    }
    else
    {
        LayoutData layout = mMapGenerator.generateLayout(mSaveManager.worldSeed());
        mSaveManager.saveWorldLayout(layout);
    }

    mMapGenerator.applyToTileMap(mTileMap);
    mStructureView.build(mTileMap, mMapGenerator.layoutData());
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
    mCampaignObjective = settings.campaignObjective;
}

void Game::syncPresentationSettingsToSave()
{
    RunPresentationSettings settings = mSaveManager.presentationSettings();
    settings.gameMode = saveGameModeFromIndex(mTitleGameModeIndex);
    settings.threatMode = saveThreatModeFromIndex(mTitleModeIndex);
    settings.tonePreset = saveTonePresetFromIndex(mTitleToneIndex);
    settings.campaignObjective = mCampaignObjective;
    mSaveManager.setPresentationSettings(settings);
    mSaveManager.savePresentationSettings();
}

bool Game::saveCurrentRunCheckpoint(const std::string& statusMessage)
{
    if (mPlayerEntity == kInvalidEntity)
    {
        return false;
    }

    RunCheckpoint cp;
    cp.runSeed = mSaveManager.currentRunSeed();
    cp.characterName = mCurrentCharacterName;
    cp.currentDay = mCurrentDay;
    cp.gameHours = mGameHours;

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
    }

    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        cp.needs = mWorld.getComponent<Needs>(mPlayerEntity);
    }

    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        cp.mental = mWorld.getComponent<MentalState>(mPlayerEntity);
    }

    if (mWorld.hasComponent<Wounds>(mPlayerEntity))
    {
        cp.wounds = mWorld.getComponent<Wounds>(mPlayerEntity).active;
    }

    cp.bleeding = mWorld.hasComponent<Bleeding>(mPlayerEntity);

    if (mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        cp.inventory = mWorld.getComponent<Inventory>(mPlayerEntity);
    }

    cp.kills = mRunStats.kills;
    cp.itemsLooted = mRunStats.itemsLooted;
    cp.structuresBuilt = mRunStats.structuresBuilt;
    cp.distanceWalked = mRunStats.distanceWalked;

    const bool ok = mSaveManager.saveCheckpoint(cp);
    if (ok)
    {
        mSaveManager.saveLootedPositions();
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

void Game::ensureCampaignObjective()
{
    if (mTitleGameModeIndex != 0)
    {
        mCampaignObjective = CampaignObjectiveState{};
        syncPresentationSettingsToSave();
        return;
    }

    if (mCampaignObjective.active)
    {
        return;
    }

    if (mCampaignObjective.completed)
    {
        mCampaignObjective = CampaignObjectiveState{};
    }

    const LayoutData& layout = mMapGenerator.layoutData();
    if (layout.width <= 0 || layout.height <= 0 || layout.districts.empty())
    {
        return;
    }

    const int startX = layout.playerStart.x;
    const int startY = layout.playerStart.y;

    int bestDistrictId = 0;
    int bestTileX = startX;
    int bestTileY = startY;
    float bestDistance2 = -1.0f;

    for (const auto& district : layout.districts)
    {
        if (district.id == 0)
        {
            continue;
        }

        const int cx = (district.minX + district.maxX) / 2;
        const int cy = (district.minY + district.maxY) / 2;
        const float dx = static_cast<float>(cx - startX);
        const float dy = static_cast<float>(cy - startY);
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestDistance2)
        {
            bestDistance2 = distance2;
            bestDistrictId = district.id;
            bestTileX = cx;
            bestTileY = cy;
        }
    }

    bestTileX = std::clamp(bestTileX, 0, mTileMap.width() - 1);
    bestTileY = std::clamp(bestTileY, 0, mTileMap.height() - 1);
    if (mTileMap.isSolid(bestTileX, bestTileY))
    {
        bool found = false;
        for (int radius = 1; radius <= 8 && !found; ++radius)
        {
            for (int oy = -radius; oy <= radius && !found; ++oy)
            {
                for (int ox = -radius; ox <= radius; ++ox)
                {
                    const int nx = std::clamp(bestTileX + ox, 0, mTileMap.width() - 1);
                    const int ny = std::clamp(bestTileY + oy, 0, mTileMap.height() - 1);
                    if (!mTileMap.isSolid(nx, ny))
                    {
                        bestTileX = nx;
                        bestTileY = ny;
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    mCampaignObjective.active = true;
    mCampaignObjective.completed = false;
    mCampaignObjective.tileX = bestTileX;
    mCampaignObjective.tileY = bestTileY;
    mCampaignObjective.districtId = bestDistrictId;
    mCampaignObjective.infectionDeadlineDay = 60;

    syncPresentationSettingsToSave();
}

void Game::registerComponents()
{
    mWorld.registerComponent<Transform>();
    mWorld.registerComponent<Sprite>();
    mWorld.registerComponent<Velocity>();
    mWorld.registerComponent<PlayerInput>();
    mWorld.registerComponent<Facing>();
    mWorld.registerComponent<Stamina>();
    mWorld.registerComponent<Combat>();
    mWorld.registerComponent<Health>();
    mWorld.registerComponent<ZombieAI>();
    mWorld.registerComponent<Needs>();
    mWorld.registerComponent<Interactable>();
    mWorld.registerComponent<Animation>();
    mWorld.registerComponent<Bleeding>();
    mWorld.registerComponent<Sleeping>();
    mWorld.registerComponent<Inventory>();
    mWorld.registerComponent<MentalState>();
    mWorld.registerComponent<Structure>();
    mWorld.registerComponent<Vehicle>();
    mWorld.registerComponent<Fire>();
    mWorld.registerComponent<Projectile>();
    mWorld.registerComponent<Wounds>();
    mWorld.registerComponent<Traits>();
    mWorld.registerComponent<SurvivorAI>();
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
    mCraftingNotebookView = false;
    mSessionDiscoveredRecipes.clear();
    mCraftQueue.clear();
    mActiveCraftProgressSeconds = 0.0f;

    mRunStats = RunStats{};

    mCurrentCharacterName = generateCharacterName(mSaveManager.currentRunSeed());

    mInteractionMessage.clear();
    mInteractionMessageTimer = 0.0f;
    mContainerOpen = false;
    mOpenContainerEntity = kInvalidEntity;
    mContainerCursor = 0;
    mActiveVehicleEntity = kInvalidEntity;
    mBurnedTiles.clear();
    mShowWorldMap = false;
    mShowNotebook = false;

    // Generate run-variant spawns against the stable layout (map is NOT regenerated)
    mMapGenerator.generateSpawns(mSpawnData, mSaveManager.currentRunSeed());

    // Threat-mode spawn pressure tuning.
    {
        std::mt19937 runRng(mSaveManager.currentRunSeed() ^ 0xA11CEu);
        auto reduceTo = [](std::vector<glm::ivec2>& list, float ratio)
        {
            if (list.empty()) return;
            const std::size_t target = std::max<std::size_t>(1, static_cast<std::size_t>(std::round(static_cast<float>(list.size()) * ratio)));
            if (target < list.size())
            {
                list.resize(target);
            }
        };
        auto duplicateTo = [](std::vector<glm::ivec2>& list, float ratio)
        {
            if (list.empty() || ratio <= 1.0f) return;
            const std::size_t base = list.size();
            const std::size_t target = static_cast<std::size_t>(std::round(static_cast<float>(base) * ratio));
            for (std::size_t i = 0; list.size() < target; ++i)
            {
                list.push_back(list[i % base]);
            }
        };

        switch (saveThreatModeFromIndex(mTitleModeIndex))
        {
        case SaveThreatMode::OverwhelmingSwarm:
            duplicateTo(mSpawnData.zombieSpawns, 1.45f);
            reduceTo(mSpawnData.foodSpawns, 0.88f);
            reduceTo(mSpawnData.waterSpawns, 0.90f);
            break;
        case SaveThreatMode::Stalker:
            reduceTo(mSpawnData.zombieSpawns, 0.68f);
            duplicateTo(mSpawnData.weaponSpawns, 1.10f);
            break;
        case SaveThreatMode::MutatingThreat:
            duplicateTo(mSpawnData.zombieSpawns, 1.18f);
            reduceTo(mSpawnData.bandageSpawns, 0.85f);
            break;
        case SaveThreatMode::InfectedSociety:
            reduceTo(mSpawnData.zombieSpawns, 0.82f);
            reduceTo(mSpawnData.foodSpawns, 0.92f);
            break;
        case SaveThreatMode::Unknown:
        {
            std::uniform_real_distribution<float> ratioDist(0.75f, 1.35f);
            const float zRatio = ratioDist(runRng);
            const float foodRatio = ratioDist(runRng);
            const float waterRatio = ratioDist(runRng);
            if (zRatio >= 1.0f) duplicateTo(mSpawnData.zombieSpawns, zRatio); else reduceTo(mSpawnData.zombieSpawns, zRatio);
            if (foodRatio >= 1.0f) duplicateTo(mSpawnData.foodSpawns, foodRatio); else reduceTo(mSpawnData.foodSpawns, foodRatio);
            if (waterRatio >= 1.0f) duplicateTo(mSpawnData.waterSpawns, waterRatio); else reduceTo(mSpawnData.waterSpawns, waterRatio);
            break;
        }
        case SaveThreatMode::Hybrid:
            duplicateTo(mSpawnData.zombieSpawns, 1.32f);
            reduceTo(mSpawnData.foodSpawns, 0.82f);
            reduceTo(mSpawnData.bandageSpawns, 0.82f);
            break;
        }
    }

    ensureCampaignObjective();

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
        if (mWorld.hasComponent<Health>(mPlayerEntity))
        {
            auto& h = mWorld.getComponent<Health>(mPlayerEntity);
            h.current = cp.healthCurrent; h.maximum = cp.healthMaximum;
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

        mCurrentDay = cp.currentDay;
        mGameHours = cp.gameHours;
        mCurrentCharacterName = cp.characterName;
        mRunStats.kills = cp.kills;
        mRunStats.itemsLooted = cp.itemsLooted;
        mRunStats.structuresBuilt = cp.structuresBuilt;
        mRunStats.distanceWalked = cp.distanceWalked;

        mInteractionMessage = "Resumed " + mCurrentCharacterName + "'s run.";
        mInteractionMessageTimer = 3.0f;
        restoredCheckpoint = true;
    }
    else if (!allowCheckpointRestore)
    {
        mSaveManager.deleteCheckpoint();
    }

    if (!restoredCheckpoint)
    {
        if (mTitleModeIndex >= 0 && mTitleModeIndex < static_cast<int>(kTitleModeNames.size()))
        {
            mInteractionMessage = "You are " + mCurrentCharacterName + ".";
            mInteractionMessageTimer = 3.5f;
        }
    }
}

void Game::returnToTitleScreen()
{
    // Navigation only: return to title without writing a new checkpoint/save.
    mWorld = World{};
    registerComponents();

    mDebugEntities.clear();
    mPlayerEntity = kInvalidEntity;

    mNoiseModel = NoiseModel{};
    mNoiseEmitTimer = 0.0f;
    mMovementNoiseTier = NoiseTier::None;

    mBuildSystem = BuildSystem{};
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
    mCraftingNotebookView = false;
    mSessionDiscoveredRecipes.clear();
    mCraftQueue.clear();
    mActiveCraftProgressSeconds = 0.0f;
    mContainerOpen = false;
    mOpenContainerEntity = kInvalidEntity;
    mContainerCursor = 0;
    mActiveVehicleEntity = kInvalidEntity;
    mBurnedTiles.clear();
    mShowWorldMap = false;
    mShowNotebook = false;
    mRunStats = RunStats{};

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
    mDebugEntities.clear();
    mPlayerEntity = kInvalidEntity;
    mMovementNoiseTier = NoiseTier::None;

    mSpriteBatch.shutdown();
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
                mCraftingNotebookView = false;
            }
            else if (mContainerOpen)
            {
                mContainerOpen = false;
                mOpenContainerEntity = kInvalidEntity;
            }
            else if (mBuildSystem.isBuildMode())
            {
                mBuildSystem.exitBuildMode();
            }
            else if (mShowWorldMap)
            {
                mShowWorldMap = false;
            }
            else if (mShowNotebook)
            {
                mShowNotebook = false;
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
    // 0 = full day, 1 = full night, with dusk 18-20 and dawn 5-6.
    const double hour = mGameHours;
    if (hour >= 6.0 && hour < 18.0) return 0.0f;
    if (hour >= 18.0 && hour < 20.0) return static_cast<float>((hour - 18.0) / 2.0);
    if (hour >= 5.0 && hour < 6.0) return 1.0f - static_cast<float>(hour - 5.0);
    return 1.0f;
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

NeedsEnvironment Game::buildNeedsEnvironment()
{
    NeedsEnvironment environment;
    environment.gameHours = mGameHours;
    environment.outdoorTemperature = mWeatherState.temperature;
    environment.rainIntensity = mWeatherState.rainIntensity;
    environment.windStrength = mWeatherState.windStrength;
    environment.tileMap = &mTileMap;

    mWorld.forEach<Structure, Transform>(
        [&](Entity, Structure& structure, Transform& st)
        {
            if (structure.type != StructureType::Campfire || structure.hp <= 0.0f) return;
            environment.heatSources.emplace_back(st.x + kSpriteSize * 0.5f, st.y + kSpriteSize * 0.5f);
        });
    mWorld.forEach<Fire, Transform>(
        [&](Entity, Fire& fire, Transform& ft)
        {
            if (fire.intensity <= 0.1f) return;
            environment.heatSources.emplace_back(ft.x + kSpriteSize * 0.5f, ft.y + kSpriteSize * 0.5f);
        });

    // Clothing: a jacket in the pack counts as worn.
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        constexpr float kJacketInsulation = 5.0f;
        mWorld.getComponent<Needs>(mPlayerEntity).clothingInsulation =
            playerHasItem("Jacket") ? kJacketInsulation : 0.0f;
    }
    return environment;
}

bool Game::playerHasItem(const char* itemName) const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return false;
    }
    const ItemDef* item = mItemDatabase.findByName(itemName);
    if (item == nullptr)
    {
        return false;
    }
    for (const auto& slot : mWorld.getComponent<Inventory>(mPlayerEntity).slots)
    {
        if (slot.itemId == item->id && slot.count > 0)
        {
            return true;
        }
    }
    return false;
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
