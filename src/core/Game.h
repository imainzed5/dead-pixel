#pragma once

#include <GL/glew.h>
#include <SDL.h>

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <filesystem>
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
#include "items/ItemIds.h"
#include "items/LootTables.h"
#include "legacy/LegacyRules.h"
#include "world/Doors.h"
#include "world/FirstDays.h"
#include "world/Furniture.h"
#include "world/Scenery.h"
#include "map/MapGenerator.h"
#include "map/TileMap.h"
#include "map/TiledLoader.h"
#include "noise/NoiseModel.h"
#include "rendering/Camera.h"
#include "rendering/Shader.h"
#include "rendering/SpriteBatch.h"
#include "rendering/BitmapFont.h"
#include "rendering/SpriteSheet.h"
#include "rendering/FurnitureView.h"
#include "rendering/SceneryView.h"
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
#include "systems/Atmosphere.h"
#include "systems/DistrictInfectionSystem.h"
#include "systems/ThreatDirectorSystem.h"
#include "components/Weather.h"
#include "ui/Hud.h"

class DevHarness;

// The container panel: docked right at the crafting width, pack on the
// left, the container on the right.
struct ContainerLayout
{
    static constexpr float kPadding = 24.0f;
    static constexpr float kListY = 112.0f;
    static constexpr float kRowH = 24.0f;
    float panelX = 0.0f;
    float panelW = 0.0f;
    float panelH = 0.0f;
    float columnW = 0.0f;
    float packX = 0.0f;
    float boxX = 0.0f;
};

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
    friend class Autoplayer;

    enum class RunState
    {
        TitleScreen,
        Playing,
        Paused,
        Dead,
        Retired
    };

    bool initialize();
    // Items, recipes, loot classes and tuning from assets/data (GameContent.cpp).
    bool loadContent(const std::filesystem::path& assetsRoot);
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
    // Furnishes the town from the layout; furniture is part of the world,
    // the same every run.
    void furnishWorld();
    // Plants the outdoor scenery, leaving clear any tile something already stands on.
    void plantScenery();
    // The weapon in hand is an inventory item. Its wear is kept in the item's
    // condition, so putting it away and taking it out again does not mend it.
    void equipWeaponFromSlot(int slotIndex);
    // Equips `preferredSlot` if it holds a weapon, else the first weapon
    // carried, else bare hands.
    void equipStartingWeapon(int preferredSlot);
    // Writes the weapon's wear back to its slot; bare hands if it is gone.
    void syncEquippedWeapon();
    // Moves run spawns that landed on furniture to a free tile beside it.
    void nudgeSpawnsOffFurniture();
    // The door E and R act on, if any: the nearest open or closed door within
    // a tile, unless something else to use is nearer and you are not at it.
    [[nodiscard]] const Doors::Door* doorInReach(const ProximityPrompt& entityPrompt) const;
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
    [[nodiscard]] bool playerHasItem(int itemId) const;
    void snapshotPositions();
    void updateNoiseModel(float dtSeconds);
    [[nodiscard]] NeedsEnvironment buildNeedsEnvironment();
    void refreshTitleCheckpoint();
    void renderNoiseDebugOverlay();
    void updateWindowTitle(double frameSeconds);
    void renderInventoryPanel();
    // Backing and the PACK / MAKE names shared by the panels docked to the right edge.
    void renderDockedHeader(const glm::vec2& pos, float width, bool crafting);
    void renderCraftingOverlay();
    void renderBuildModeOverlay();
    void renderContainerOverlay();
    // Containers (GameStorage.cpp): the two-column panel, world memory, spoilage.
    void openContainer(Entity container);
    void closeContainer();
    void handleContainerInput();
    // Moves the `row`th thing (one, or the stack) between pack and container.
    int containerMove(bool fromPack, int row, bool wholeStack);
    [[nodiscard]] ContainerLayout containerLayout() const;
    [[nodiscard]] std::string containerName(Entity container) const;
    [[nodiscard]] FurnitureKind containerKindAt(const glm::ivec2& tile) const;
    // Writes this container's contents to world memory (and saves it).
    void recordContainer(Entity container);
    // Brings every remembered container's record up to date (before a save).
    void refreshContainerRecords();
    void spoilContainers(float gameHours);
    void syncPresentationSettingsFromSave();
    void syncPresentationSettingsToSave();
    void ensureCampaignObjective();
    bool saveCurrentRunCheckpoint(const std::string& statusMessage = "Run checkpoint saved.");
    [[nodiscard]] RunCheckpoint buildRunCheckpoint();
    // The town as the checkpoint left it: doors, scars, fires.
    void restoreTownFromCheckpoint(const RunCheckpoint& cp);
    void spawnScorchMark(int tileX, int tileY);
    void spawnFireTile(const FireTileRecord& record, float spreadPhase);
    // R at a closed door: nail a board across it (hammer and a plank).
    void boardUpDoor(const glm::ivec2& tile);
    // Every door with boards on it, for saving.
    [[nodiscard]] std::vector<BoardedDoorRecord> boardedDoorRecords() const;
    // A body where a zombie fell (sprite top-left). It wears the zombie's
    // look, falls facing `dir` (Animation::dir) and slides on what is left of
    // the blow; `settled` skips the fall (bodies restored from a save).
    void spawnCorpse(const glm::vec2& topLeft, int look, int dir, const glm::vec4& tint,
                     const glm::vec2& slide, bool settled);
    // Blood, a held frame and a shaken screen when a blow lands on a zombie.
    void updateBlowFeedback(float dtSeconds);

    // The first three days (GameFirstDays.cpp): home, the taps, the day plan
    // and the night's callers.
    void setupHome();
    void restoreFirstDays(const FirstDaysRecord& record);
    [[nodiscard]] FirstDaysRecord firstDaysRecord() const;
    // This character's things, in the home's cupboards (not those taken from
    // before a resume).
    void placeHomeKit();
    void updateFirstDays(float dtSeconds);
    // R at a kitchen counter or a bathroom sink: drink, fill empty bottles.
    bool handleTapInteraction(const ProximityPrompt& entityPrompt);
    // A container was opened, or something was taken from it.
    void noteContainerSearched(Entity container, bool took);
    [[nodiscard]] firstdays::Facts firstDaysFacts();
    [[nodiscard]] bool playerAtHome() const;
    void sendNightCallers(int count);
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
    ItemIds mItems;     // the items code names, resolved by key
    LootTables mLoot;

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
    Atmosphere mAtmosphere;
    // Wind in the trees, feet in the bushes, leaves coming down, lamp light.
    void updateAtmosphere(float dtSeconds);
    std::vector<SceneryMover> mSceneryMovers;
    std::vector<glm::vec2> mRustled;
    std::vector<SceneryCanopy> mCanopies;
    float mLeafDebt = 0.0f;
    float mMuzzleFlash = 0.0f; // seconds of muzzle light left
    bool mHideUi = false;      // dev harness: screenshots of the bare world
    glm::vec2 mMuzzleWorld{0.0f};
    // Combat weight: a landed blow slows time for a few frames (mHitStop,
    // real seconds left) and shakes the view (mShakeTrauma, 0..1).
    float mHitStop = 0.0f;
    float mShakeTrauma = 0.0f;
    float mShakeClock = 0.0f;
    float mPlayerHurtTimer = 0.0f;
    std::vector<glm::vec2> mPlayerHits; // directions of blows the player took this tick
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
    std::vector<FurniturePiece> mFurniture;
    FurnitureView mFurnitureView;
    SceneryView mSceneryView;
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
    int mEquippedWeaponSlot = -1;

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
    int mPackCursor = 0;
    bool mContainerPackSide = false;
    float mContainerSpoilHours = 0.0f;
    int mContainersOpened = 0; // this run, for the harness metrics

    // M8 narrow slice state
    Entity mActiveVehicleEntity = kInvalidEntity;
    std::vector<std::pair<int, int>> mBurnedTiles;
    Doors mDoors;
    std::vector<glm::ivec2> mDoorBlows; // this tick's zombie blows on closed doors

    struct FirstDaysState
    {
        int homeBuilding = 0;                       // where the run started
        std::vector<glm::ivec2> homeDoors;
        std::vector<std::pair<int, int>> homeContainers;
        FirstDaysRecord record;
        float cardTimer = 0.0f;
        bool tapsDryNoted = false;                  // tried a dead tap
        bool waterOffAnnounced = false;
    };
    FirstDaysState mFirstDays;

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
    static constexpr std::size_t kMaxCorpses = 60; // bodies left lying in a run
    static constexpr float kFlashlightHalfAngleDegrees = 26.0f;

    double mFpsTimer = 0.0;
    int mFramesThisSecond = 0;
    float mInterpolationAlpha = 1.0f;
};
