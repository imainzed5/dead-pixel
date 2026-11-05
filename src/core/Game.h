#pragma once

#include <GL/glew.h>
#include <SDL.h>

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/Input.h"
#include "dev/LaunchOptions.h"
#include "audio/AudioManager.h"
#include "ecs/Types.h"
#include "ecs/World.h"
#include "items/ItemDatabase.h"
#include "legacy/LegacyRules.h"
#include "world/Doors.h"
#include "map/MapGenerator.h"
#include "map/TileMap.h"
#include "map/TiledLoader.h"
#include "noise/NoiseModel.h"
#include "rendering/Camera.h"
#include "rendering/Shader.h"
#include "rendering/SpriteBatch.h"
#include "rendering/BitmapFont.h"
#include "rendering/SpriteSheet.h"
#include "rendering/StructureView.h"
#include "rendering/Texture.h"
#include "save/SaveManager.h"
#include "systems/CollisionSystem.h"
#include "systems/CombatSystem.h"
#include "systems/InputSystem.h"
#include "systems/InteractionSystem.h"
#include "systems/MovementSystem.h"
#include "systems/NeedsSystem.h"
#include "systems/AnimationSystem.h"
#include "systems/BleedingSystem.h"
#include "systems/BuildSystem.h"
#include "systems/CraftingSystem.h"
#include "systems/InventorySystem.h"
#include "systems/MentalStateSystem.h"
#include "systems/ParticleSystem.h"
#include "systems/ProjectileSystem.h"
#include "systems/RenderSystem.h"
#include "systems/WoundSystem.h"
#include "systems/ZombieAISystem.h"
#include "systems/SurvivorAISystem.h"
#include "systems/SurvivorInteractionSystem.h"
#include "systems/WeatherSystem.h"
#include "systems/DistrictInfectionSystem.h"
#include "systems/ThreatDirectorSystem.h"
#include "components/Weather.h"
#include "ui/Hud.h"

class DevHarness;

class Game
{
public:
    explicit Game(LaunchOptions options = {});
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    int run();

private:
    friend class DevHarness;

    enum class RunState
    {
        TitleScreen,
        Playing,
        Paused,
        Dead,
        Retired
    };

    bool initialize();
    bool loadSlotAndWorld(const std::string& slotName);
    void registerComponents();
    void startNewRun();
    void returnToTitleScreen();
    void shutdown();
    void setupScene();
    void beginDeathState(const std::string& cause);
    void beginRetirementState(const std::string& customCause = "");
    void stopPlayerAtRunEnd();
    [[nodiscard]] CharacterHistoryRecord runEndHistory(bool retired, int runIndex) const;
    void addRunEndMemoryEvent(WorldMemoryEventType type, int runIndex);
    void persistWorldForNextRun(int runIndex, const SurvivorRecord* retiree = nullptr);

    // Legacy layer (GameLegacy.cpp)
    struct RisenTrack
    {
        Entity entity = kInvalidEntity;
        int graveIndex = -1;
        glm::vec2 lastTopLeft{0.0f};
    };
    struct LegacyCardLayout
    {
        glm::vec2 pos{0.0f};
        glm::vec2 size{0.0f};
        float titleScale = 2.0f;
        float lineScale = 2.0f;
    };
    struct CraftingLayout
    {
        glm::vec2 pos{0.0f};
        glm::vec2 size{0.0f};
        std::array<glm::vec4, 3> tabs{}; // x, y, w, h
        glm::vec2 listPos{0.0f};
        float listW = 0.0f;
        float rowH = 0.0f;
        int maxRows = 1;
        glm::vec2 detailPos{0.0f};
        float detailW = 0.0f;
        float detailBottom = 0.0f;
        float footerY = 0.0f;
    };
    // Shared by drawing and mouse hit-testing.
    [[nodiscard]] CraftingLayout craftingLayout() const;
    // First visible row of the recipe list, keeping the cursor in view.
    [[nodiscard]] int craftingListScroll(int count, int maxRows) const;
    void updateDoors(float dtSeconds);
    // Every door closed, except those broken down in earlier runs.
    void resetDoors();
    // E at a door: opens or closes it. False when no door is in reach.
    bool handleDoorInteraction(const ProximityPrompt& entityPrompt);
    void spawnLegacy();
    void spawnGrave(int graveIndex);
    void spawnLegacyPack(int graveIndex, const glm::vec2& topLeftWorld);
    void spawnRisen(int graveIndex);
    void updateLegacy(float dtSeconds);
    void syncLegacyToSave();
    void renderLegacyCardShapes();
    void renderLegacyText();
    [[nodiscard]] LegacyCardLayout legacyCardLayout() const;
    [[nodiscard]] std::vector<legacy::Lead> currentLegacyLeads();
    [[nodiscard]] glm::vec2 playerWorldCenter() const;
    [[nodiscard]] DistrictType districtTypeAt(int tileX, int tileY) const;
    void processInputEvents();
    void handleEscapeKey();
    void update(float dtSeconds);
    // Pieces of update(), in the order it calls them.
    void updateTitleScreen(float dtSeconds);
    void updatePauseMenu();
    void handleVehicleKey(bool playerAlive);
    void updateFire(float dtSeconds, bool playerAlive, bool drivingVehicle);
    void updateCraftingPanel(float dtSeconds, bool drivingVehicle);
    void updateInventoryInput(bool drivingVehicle);
    void applyVehicleDriving();
    void updateSleep(float dtSeconds);
    void updateVehicleAfterPhysics(float dtSeconds);
    void updateAnimations(float dtSeconds);
    void updateParticles(float dtSeconds, float prevPlayerHp);
    void render();
    void renderMap();
    void renderAttackArcOverlay();
    void buildHudState();
    void updateDistrictToast(float dtSeconds);
    void renderRunEndOverlay();
    void renderTextOverlays();
    void renderPauseOverlay();
    void renderTitleScreen();
    void renderControlsPanel();
    void renderControlsOverlay();
    void renderDayNightOverlay();
    void renderMinimapOverlay();
    void renderWorldMapOverlay();
    void renderNotebookOverlay();
    void setupLightingUniforms();
    [[nodiscard]] float nightFactor() const;
    [[nodiscard]] glm::ivec2 playerTile() const;
    [[nodiscard]] bool playerHasItem(const char* itemName) const;
    void snapshotPositions();
    void updateNoiseModel(float dtSeconds);
    [[nodiscard]] NeedsEnvironment buildNeedsEnvironment();
    void refreshTitleCheckpoint();
    void renderNoiseDebugOverlay();
    void updateWindowTitle(double frameSeconds);
    void renderInventoryPanel();
    void renderInventoryOverlay();
    void renderCraftingOverlay();
    void renderBuildModeOverlay();
    void renderContainerOverlay();
    void syncPresentationSettingsFromSave();
    void syncPresentationSettingsToSave();
    void ensureCampaignObjective();
    bool saveCurrentRunCheckpoint(const std::string& statusMessage = "Run checkpoint saved.");
    [[nodiscard]] RunCheckpoint buildRunCheckpoint();
    // The town as the checkpoint left it: doors, scars, fires.
    void restoreTownFromCheckpoint(const RunCheckpoint& cp);
    void spawnScorchMark(int tileX, int tileY);
    void spawnFireTile(const FireTileRecord& record, float spreadPhase);
    [[nodiscard]] glm::vec2 screenToWorld(const glm::vec2& screenPosition) const;

    LaunchOptions mLaunchOptions;
    std::unique_ptr<DevHarness> mHarness;

    SDL_Window* mWindow = nullptr;
    SDL_GLContext mGlContext = nullptr;
    int mWindowWidth = 1280;
    int mWindowHeight = 720;
    bool mIsRunning = false;

    Input mInput;
    Shader mShader;
    Texture mTexture;
    SpriteSheet mSpriteSheet;
    BitmapFont mFont;
    SpriteBatch mSpriteBatch;
    Camera mCamera;
    TileMap mTileMap;
    TiledLoader mTiledLoader;
    MapGenerator mMapGenerator;
    SpawnData mSpawnData;
    NoiseModel mNoiseModel;
    SaveManager mSaveManager;
    AudioManager mAudioManager;
    ItemDatabase mItemDatabase;

    World mWorld;
    InputSystem mInputSystem;
    InteractionSystem mInteractionSystem;
    CombatSystem mCombatSystem;
    CollisionSystem mCollisionSystem;
    MovementSystem mMovementSystem;
    NeedsSystem mNeedsSystem;
    ZombieAISystem mZombieAISystem;
    AnimationSystem mAnimationSystem;
    BleedingSystem mBleedingSystem;
    MentalStateSystem mMentalStateSystem;
    BuildSystem mBuildSystem;
    CraftingSystem mCraftingSystem;
    ProjectileSystem mProjectileSystem;
    WoundSystem mWoundSystem;
    RenderSystem mRenderSystem;
    ParticleSystem mParticleSystem;
    SurvivorAISystem mSurvivorAISystem;
    SurvivorInteractionSystem mSurvivorInteractionSystem;
    WeatherSystem mWeatherSystem;
    DistrictInfectionSystem mDistrictInfectionSystem;
    ThreatDirectorSystem mThreatDirector;

    WeatherState mWeatherState;

    Entity mPlayerEntity = kInvalidEntity;
    std::vector<Entity> mDebugEntities;
    RunState mRunState = RunState::TitleScreen;

    // Pause menu
    int mPauseSelection = 0; // 0=Resume, 1=Save, 2=Back to Menu, 3=Restart, 4=Quit

    // Title screen hub
    int mTitleMenuSelection = 0;
    int mTitleSaveSlotIndex = 0;
    int mTitleModeIndex = 0;     // threat mode index
    int mTitleGameModeIndex = 1; // 0=Campaign, 1=Endless
    int mTitleToneIndex = 1;     // 0=Hopeless, 1=Grim Human, 2=Brutal Arcade
    bool mTitleHasCheckpoint = false;
    int mTitleCheckpointDay = 1;
    std::string mTitleCheckpointName;
    float mTitleCheckpointX = 0.0f;
    float mTitleCheckpointY = 0.0f;
    float mTitleCheckpointRefreshTimer = 0.0f;
    bool mShowTitleCredits = false;
    std::string mTitleStatusMessage;
    float mTitleStatusTimer = 0.0f;
    float mTitlePulseTimer = 0.0f;
    bool mTitleConfirmNewWorld = false;
    float mTitleConfirmNewWorldTimer = 0.0f;
    bool mIgnoreCheckpointRestoreOnce = false;
    std::string mActiveSaveSlot = "default_slot";

    // Controls overlay
    bool mShowControls = false;

    // Run statistics
    struct RunStats
    {
        int kills = 0;
        int itemsLooted = 0;
        int structuresBuilt = 0;
        float distanceWalked = 0.0f;
        float prevX = 0.0f;
        float prevY = 0.0f;
    };
    RunStats mRunStats{};

    glm::mat4 mProjection{1.0f};
    glm::vec2 mMouseWorld{0.0f, 0.0f};
    bool mShowNoiseDebug = false;
    bool mShowDistrictDebug = false;
    float mNoiseEmitTimer = 0.0f;
    NoiseTier mMovementNoiseTier = NoiseTier::None;
    std::uint64_t mLayoutFingerprint = 0;

    double mDeathStateTimer = 0.0;
    // What this run leaves for the next, shown on the run-end screen.
    std::string mRunEndLegacyLine;
    std::string mDeathCause;
    glm::vec2 mDeathWorldPosition{0.0f, 0.0f};
    std::string mCurrentCharacterName;
    std::string mInteractionMessage;
    float mInteractionMessageTimer = 0.0f;
    std::string mProximityPrompt;

    Hud mHud;
    HudState mHudState;

    StructureView mStructureView;
    StructureSprites mStructureSprites;
    std::vector<StaticSprite> mStaticSprites;
    float mHudClock = 0.0f;
    int mHudDistrictId = -1;
    float mDistrictToastTimer = 0.0f;
    std::string mDistrictToastText;
    bool mDistrictToastDanger = false;

    bool mShowWorldMap = false;
    bool mShowNotebook = false;
    CampaignObjectiveState mCampaignObjective{};
    float mMinimapZoom = 1.0f;

    double mGameHours = 8.0;
    int mCurrentDay = 1;
    bool mShowInventory = false;
    int mInventoryCursor = 0; // navigable cursor across all 20 slots

    // Crafting panel state
    bool mShowCrafting = false;
    int mCraftingRecipeCursor = 0;
    int mCraftingQuantity = 1;
    int mCraftingCategory = 0; // 0=ALL, 1=TOOLS, 2=MED
    std::vector<std::pair<int, int>> mSessionDiscoveredRecipes;
    glm::vec2 mCraftingLastMouse{-1.0f};

    struct CraftQueueEntry
    {
        int recipeIndex = -1;
        int remaining = 0;
    };
    std::deque<CraftQueueEntry> mCraftQueue;
    float mActiveCraftProgressSeconds = 0.0f;

    // Container view state
    bool mContainerOpen = false;
    Entity mOpenContainerEntity = kInvalidEntity;
    int mContainerCursor = 0;

    // M8 narrow slice state
    Entity mActiveVehicleEntity = kInvalidEntity;
    std::vector<std::pair<int, int>> mBurnedTiles;
    Doors mDoors;
    std::vector<glm::ivec2> mDoorBlows; // this tick's zombie blows on closed doors

    // This run's view of the grave records; written back by syncLegacyToSave.
    std::vector<GraveRecord> mLegacyGraves;
    std::vector<bool> mLegacyPackSpawned;
    std::vector<RisenTrack> mRisenTracks;
    float mLegacyCardTimer = 0.0f;
    std::string mLegacyCardTitle;
    std::vector<std::string> mLegacyCardLines;
    std::size_t mBurnedTilesAtRunStart = 0; // tiles before this index burned in earlier runs

    float mZombieGrowlTimer = 0.0f;

    // Flashlight (Q): a beam at night that zombies can see from afar.
    bool mFlashlightOn = false;
    static constexpr float kFlashlightRangeTiles = 9.0f;
    static constexpr float kFlashlightHalfAngleDegrees = 26.0f;

    double mFpsTimer = 0.0;
    int mFramesThisSecond = 0;
    float mInterpolationAlpha = 1.0f;
};
