#pragma once

#include <GL/glew.h>
#include <SDL.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "core/Input.h"
#include "audio/AudioManager.h"
#include "ecs/Types.h"
#include "ecs/World.h"
#include "items/ItemDatabase.h"
#include "map/MapGenerator.h"
#include "map/TileMap.h"
#include "map/TiledLoader.h"
#include "noise/NoiseModel.h"
#include "rendering/Camera.h"
#include "rendering/Shader.h"
#include "rendering/SpriteBatch.h"
#include "rendering/BitmapFont.h"
#include "rendering/SpriteSheet.h"
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
#include "components/Weather.h"

class Game
{
public:
    Game() = default;
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    int run();

private:
    enum class RunState
    {
        TitleScreen,
        Playing,
        Paused,
        Dead,
        Retired
    };

    enum class TitleRunMode
    {
        Survival = 0,
        Scavenger = 1,
        Overrun = 2
    };

    bool initialize();
    bool loadSlotAndWorld(const std::string& slotName);
    void registerComponents();
    void startNewRun();
    void shutdown();
    void setupScene();
    void beginDeathState(const std::string& cause);
    void beginRetirementState();
    void processInputEvents();
    void update(float dtSeconds);
    void render();
    void renderMap();
    void renderAttackArcOverlay();
    void renderNeedsHudOverlay();
    void renderDeathOverlay();
    void renderRetirementOverlay();
    void renderTextOverlays();
    void renderPauseOverlay();
    void renderTitleScreen();
    void renderControlsOverlay();
    void renderDayNightOverlay();
    void setupLightingUniforms();
    void snapshotPositions();
    void updateNoiseModel(float dtSeconds);
    void renderNoiseDebugOverlay();
    void updateWindowTitle(double frameSeconds);
    void renderInventoryOverlay();
    void renderBuildModeOverlay();
    void renderContainerOverlay();
    [[nodiscard]] glm::vec2 screenToWorld(const glm::vec2& screenPosition) const;

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
    ProjectileSystem mProjectileSystem;
    WoundSystem mWoundSystem;
    RenderSystem mRenderSystem;
    ParticleSystem mParticleSystem;
    SurvivorAISystem mSurvivorAISystem;
    SurvivorInteractionSystem mSurvivorInteractionSystem;
    WeatherSystem mWeatherSystem;
    DistrictInfectionSystem mDistrictInfectionSystem;

    WeatherState mWeatherState;

    Entity mPlayerEntity = kInvalidEntity;
    std::vector<Entity> mDebugEntities;
    RunState mRunState = RunState::TitleScreen;

    // Pause menu
    int mPauseSelection = 0; // 0=Resume, 1=Restart, 2=Quit

    // Title screen hub
    int mTitleMenuSelection = 0;
    int mTitleSaveSlotIndex = 0;
    int mTitleModeIndex = 0;
    bool mTitleHasCheckpoint = false;
    int mTitleCheckpointDay = 1;
    std::string mTitleCheckpointName;
    float mTitleCheckpointX = 0.0f;
    float mTitleCheckpointY = 0.0f;
    bool mShowTitleCredits = false;
    std::string mTitleStatusMessage;
    float mTitleStatusTimer = 0.0f;
    float mTitlePulseTimer = 0.0f;
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
    std::string mDeathCause;
    glm::vec2 mDeathWorldPosition{0.0f, 0.0f};
    std::string mCurrentCharacterName;
    std::string mInteractionMessage;
    float mInteractionMessageTimer = 0.0f;
    std::string mProximityPrompt;

    double mGameHours = 8.0;
    int mCurrentDay = 1;
    bool mShowInventory = false;
    int mInventoryCursor = 0; // navigable cursor across all 20 slots

    // Container view state
    bool mContainerOpen = false;
    Entity mOpenContainerEntity = kInvalidEntity;
    int mContainerCursor = 0;
    float mZombieGrowlTimer = 0.0f;

    double mFpsTimer = 0.0;
    int mFramesThisSecond = 0;
    float mInterpolationAlpha = 1.0f;
};
