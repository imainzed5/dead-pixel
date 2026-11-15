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
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "core/Input.h"
#include "core/Settings.h"
#include "core/Version.h"
#include "dev/LaunchOptions.h"
#include "audio/AudioManager.h"
#include "audio/Ambience.h"
#include "audio/AudioRules.h"
#include "ecs/Types.h"
#include "ecs/World.h"
#include "items/ItemDatabase.h"
#include "items/ItemIds.h"
#include "items/LootTables.h"
#include "legacy/LegacyRules.h"
#include "components/Skills.h"
#include "components/Traits.h"
#include "world/Doors.h"
#include "world/FurnitureUse.h"
#include "world/Windows.h"
#include "world/FirstDays.h"
#include "world/Aims.h"
#include "world/DayLength.h"
#include "world/Furniture.h"
#include "world/Forage.h"
#include "world/Clues.h"
#include "world/Gate.h"
#include "world/Scenery.h"
#include "world/BloodTrail.h"
#include "world/ZombieKinds.h"
#include "world/EntityFactory.h"
#include "world/Senses.h"
#include "map/MapGenerator.h"
#include "map/TileMap.h"
#include "map/TiledLoader.h"
#include "noise/NoiseModel.h"
#include "rendering/Camera.h"
#include "rendering/PostFx.h"
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
#include "systems/ThrownSystem.h"
#include "systems/RenderSystem.h"
#include "systems/WoundSystem.h"
#include "systems/HallucinationSystem.h"
#include "systems/ZombieAISystem.h"
#include "systems/SurvivorBrain.h"
#include "systems/WeatherSystem.h"
#include "systems/FireSystem.h"
#include "systems/Atmosphere.h"
#include "systems/DistrictInfectionSystem.h"
#include "systems/ThreatDirectorSystem.h"
#include "components/Weather.h"
#include "ui/Hud.h"

class DevHarness;
struct Vehicle;

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
    bool mDevGodMode = false; // set by the harness's `god on`
    friend class Autoplayer;

    enum class RunState
    {
        TitleScreen,
        Playing,
        Paused,
        Dead,
        Retired,
        CharacterCreation, // the character screen, over the dimmed town (phase 5)
        Out                // got out by the gate (phase 9)
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
        std::array<glm::vec4, 4> tabs{}; // x, y, w, h
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

    // --- Phase 10: the journal and the notebook (GameJournal.cpp) ---
    void loadJournalLines(const std::filesystem::path& assetsRoot);
    // Everything the save holds of the bloodline, for the rules.
    [[nodiscard]] legacy::LegacyState legacyState() const;
    // Living World (the default) or Fresh Start (nothing carries over).
    [[nodiscard]] bool livingWorld() const { return mSaveManager.livingWorld(); }
    // Writes an entry, if a notebook is carried.
    void noteJournal(journal::Kind kind, const std::string& a = {}, const std::string& b = {}, int n = 0);
    // Things noticed as they happen: the first night, the lights, a wound...
    void updateJournal(float dtSeconds);
    // A new life's empty journal.
    void resetJournal();
    // This life's journal with its last entry, for the history record.
    [[nodiscard]] std::vector<journal::Entry> finishJournal(journal::Kind end, const std::string& lastLine = {});
    // The notebook (J): six tabs, docked right; Left and Right turn them.
    void updateNotebookInput();
    void renderNotebookTabs(float panelX, float panelW, float y);
    void renderLeadsPage(float left, float textW, float top, float bottom);
    void renderPeoplePage(float left, float textW, float top, float bottom);
    void renderBeforeYouPage(float left, float textW, float top, float bottom);
    void renderKnowHowPage(float left, float textW, float top, float bottom);
    void renderJournalPage(float left, float textW, float top, float bottom);
    void renderReaderPage(float left, float textW, float top, float bottom);
    // Opens a dead character's journal: at their grave (graveIndex), or from
    // the notebook's "Before you" page (history index).
    void openReader(int graveIndex, int historyIndex, bool atGrave);
    void closeReader();
    // The first reading of a journal: what it costs and what it teaches.
    void markJournalRead(int historyIndex, int graveIndex);
    // An earlier character's notebook: its leads on the way out (once).
    bool learnFromGraveNotebook(int graveIndex);
    [[nodiscard]] std::string markerName(const InventorySlot& slot) const;

    // --- Phase 10: the cemetery, markers, burial, the aging town (GameCemetery.cpp) ---
    bool stampCemetery(LayoutData& layout);
    void setupCemetery();
    [[nodiscard]] bool cemeteryPlaced() const;
    [[nodiscard]] glm::ivec2 graveSlotTile(const GraveRecord& grave) const;
    [[nodiscard]] bool graveWeathered(const GraveRecord& grave) const;
    // E at a grave, a stone, or a dead survivor's emptied body.
    bool handleLegacyInteraction(const ProximityPrompt& entityPrompt);
    // Whether the grave's marker can be taken now (its pack gone, or laid to rest).
    [[nodiscard]] bool markerAvailable(int graveIndex) const;
    bool takeMarker(int graveIndex);
    // F with a marker in hand: set it in the cemetery as a headstone.
    bool placeMarker(int slot);
    void beginTend(int graveIndex);
    bool beginBurial(Entity body);
    void finishLegacyAction();
    // At a resume: a marker the checkpoint does not carry goes back to its grave.
    void reconcileMarkers();
    // At a life's end: markers carried go back to their graves.
    void releaseMarkers();
    [[nodiscard]] int nextBurialOrder() const;
    // The town ages between lives (legacy::ageWorld over the save's records).
    void ageWorldForNextLife(int daysLived, int runIndex);
    // Stay here for good (the pause menu): why not yet ("" when it is offered).
    [[nodiscard]] std::string stayForGoodBlocker() const;
    void stayForGood();
    [[nodiscard]] std::vector<std::string> pauseOptions() const;
    void spawnBones();
    // The checkpoint's part (Game.cpp keeps to one line each).
    void journalCheckpoint(RunCheckpoint& cp) const;
    void restoreJournal(const RunCheckpoint& cp, bool restored);
    // Threat mode, tone, earlier lives' kills, the town's growth (GameDead.cpp).
    void tuneRunSpawns();
    void processInputEvents();
    void handleEscapeKey();
    void update(float dtSeconds);
    // Pieces of update(), in the order it calls them.
    void updateTitleScreen(float dtSeconds);
    void updatePauseMenu();
    void handleVehicleKey(bool playerAlive);
    // --- Fire on a grid (phase 7, GameFire.cpp over systems/FireSystem) ---
    void updateFire(float dtSeconds, bool playerAlive, bool drivingVehicle);
    // A molotov coming down on `tile`, thrown from `from`: three tiles
    // across the throw at heat 0.8. False where nothing could burn.
    bool igniteTile(const glm::ivec2& tile, const glm::vec2& from);
    // One tile alight at `heat`; false where nothing burns.
    bool igniteFire(const glm::ivec2& tile, float heat, bool byPlayer);
    // The fire grid as checkpoint records (heat, fuel, who lit it).
    [[nodiscard]] std::vector<FireTileRecord> fireRecords() const;
    // A new run or the title: no fire, scars, prints or flash carried over.
    void clearFireAndWeatherMarks();
    bool douseFireAhead();
    // Fuel of a tile not burning (firerules::fuelOf on what lies there).
    [[nodiscard]] float tileFuel(int tileX, int tileY);
    [[nodiscard]] FireSystem::Env fireEnv();
    [[nodiscard]] bool tileBurned(int tileX, int tileY) const;
    void markTileBurned(int tileX, int tileY);
    // A tile that used up its fuel: scorched, its furniture and tree gone.
    void tileBurnedOut(const glm::ivec2& tile);
    // Per building: floor tiles, and how many have burned (for gutting).
    void countBuildingFloors();
    // A wooden building burned out: open, empty, charred. `fresh`: it
    // happened now (a message, world memory), not restored at run start.
    void gutBuilding(int building, bool fresh);
    [[nodiscard]] bool buildingGutted(int building) const;
    // A gutted house is never a start home (world memory, read before the
    // run counts its buildings).
    [[nodiscard]] bool guttedInMemory(int building) const;
    void avoidGuttedStart();
    // Burning tiles inside each building, this tick (smoke).
    [[nodiscard]] int burningIn(int building) const;
    // Fire is a wall to the living: survivors stop short of a burning tile
    // (the dead walk through and burn).
    void holdSurvivorsFromFire();
    // Where the fire light goes: one light per 3x3 block, at most 8.
    [[nodiscard]] std::vector<glm::vec2> fireLights() const;

    // --- Weather and seasons (phase 7, GameWeather.cpp) ---
    // After the weather moves: lightning, snow, frozen ponds, the season's
    // card, wet and smoke, footprints.
    void updateWeather(float dtSeconds);
    void lightningStrike();
    void updateFootprints(float dtSeconds);
    void drawFootprints();
    // Firewise, holding a Molotov or Matches: what would burn shimmers.
    void drawFuelShimmer();
    // Flames on burning tiles, sorted with walls and people.
    void collectFireSprites();
    [[nodiscard]] bool pondFrozen() const;
    // The dead drawn at all (fog hides them beyond 6 tiles).
    [[nodiscard]] bool deadDrawnAt(const glm::vec2& world) const;
    // The `weatherinfo` line: noise, sight and body target as the rules read them.
    [[nodiscard]] std::string weatherInfoLine() const;

    // --- Body and mind (phase 5, GameBody.cpp) ---
    // Give the player these traits: health, carry limit and starting skills
    // follow. `fresh`: a new character (full health, skills from the traits).
    void applyCharacter(const Traits& traits, bool fresh);
    [[nodiscard]] const Traits* playerTraits() const;
    [[nodiscard]] int skillLevel(Skill skill) const;
    // Earn XP; a level gained is posted in the message line.
    void awardXp(Skill skill, float amount);
    [[nodiscard]] float playerCarryLimit() const;
    [[nodiscard]] double absoluteHour() const { return static_cast<double>(mCurrentDay) * 24.0 + mGameHours; }
    void addPanic(float amount);
    void addDepression(float amount);
    // Before input and combat each tick: what the body does to a walk and a swing.
    void prepareBodyModifiers();
    // What the wound system needs to know of you (seed, tone, bed, Medicine).
    [[nodiscard]] WoundContext woundContext() const;
    // After the needs and mind: sleep debt, coffee, microsleeps, a deep
    // breath, running blind, numbness fading, seeing things, wounds bleeding
    // again, skills forgetting.
    void updateBody(float dtSeconds);
    // A kill of the player's: XP, the first kill's fright, numbness.
    void onPlayerKill();
    // F with a medical item (or Enter in the BODY tab): `woundIndex` -1 is
    // the worst it can help. True when the item was a treatment.
    bool treatWithItem(int slotIndex, int woundIndex);
    void finishTreatment();
    // F on a book in shelter: 20 game minutes; F on a keepsake.
    bool useComfortItem(int slotIndex);
    void finishReading();
    // R at a door or window: the held boarding (timed, Carpentry).
    void startBoarding(const glm::ivec2& tile, bool window);
    void finishBoarding();
    [[nodiscard]] float heldActionScale() const; // shaking hands
    // Things that are not there: where a sign may show.
    [[nodiscard]] std::optional<glm::vec2> placeFalseSign(FalseSign::Kind kind, float a, float b);
    void renderBodyEffects();      // vignette, closing eyes, figures (shape pass)
    void renderBodyTab();          // the BODY tab of the docked panel
    void updateBodyTabInput();
    // The character screen (GameCharacter.cpp).
    void beginCharacterCreation();
    void updateCharacterCreation();
    void renderCharacterCreation();
    void finishCharacterCreation();

    // --- Fighting (phase 3, GameFighting.cpp) ---
    // Before combat: held by the dead, throwing (G), reloading (R), wound
    // clocks and flares, things in flight.
    void updateFighting(float dtSeconds, bool playerAlive, bool drivingVehicle);
    // After combat: what the swing, shove or shot did, shown and heard.
    void afterCombat();
    [[nodiscard]] bool playerHeld() const;
    // Throw the selected belt item at a world point (clamped to its range).
    bool throwSelected(const glm::vec2& targetWorld);
    void onThrowLanded(const ThrowLanding& landing);
    // The dead in `radiusTiles` (and in sight of it, if `needSight`) go to
    // `where` and stay near it for `seconds`.
    int lureTo(const glm::vec2& where, float radiusTiles, float seconds, bool needSight);
    // F on a fighting item: wind an alarm clock, douse a fire. True if used.
    bool useFightingItem(int slotIndex);
    // A gun's prompt and reload (R), when nothing in reach takes R.
    void handleReloadKey();
    void renderFightingOverlay();
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
    // The build wheel and the label of a held action (text pass), and the
    // held action's progress line (shape pass).
    void renderBuildWheel();
    // Screen position of build wheel entry `index`.
    [[nodiscard]] glm::vec2 buildWheelEntry(int index) const;
    void renderBaseActionBar();
    [[nodiscard]] glm::vec2 baseActionAnchor() const;
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
    // write: false keeps it in memory for the next save (a survivor's
    // search, mid-tick: written at the next checkpoint) (phase 13).
    void recordContainer(Entity container, bool write = true);
    // Brings every remembered container's record up to date (before a save).
    void refreshContainerRecords();
    void spoilContainers(float gameHours);
    void syncPresentationSettingsFromSave();
    void syncPresentationSettingsToSave();
    bool saveCurrentRunCheckpoint(const std::string& statusMessage = "Run checkpoint saved.");
    [[nodiscard]] RunCheckpoint buildRunCheckpoint();
    // The town as the checkpoint left it: doors, scars, fires.
    void restoreTownFromCheckpoint(const RunCheckpoint& cp);
    void spawnScorchMark(int tileX, int tileY);
    // R at a closed door: nail a board across it (hammer and a plank).
    void boardUpDoor(const glm::ivec2& tile);
    // Every door with boards on it, for saving.
    [[nodiscard]] std::vector<BoardedDoorRecord> boardedDoorRecords() const;
    // A body where a zombie fell (sprite top-left). It wears the zombie's
    // look, falls facing `dir` (Animation::dir) and slides on what is left of
    // the blow; `settled` skips the fall (bodies restored from a save).
    Entity spawnCorpse(const glm::vec2& topLeft, int look, int dir, const glm::vec4& tint,
                     const glm::vec2& slide, bool settled, ZombieKind kind = ZombieKind::Shambler);
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

    // Windows (GameWindows.cpp): glass, boards, curtains, climbing through,
    // the dead at them, and light that shows through them at night.
    // The town's windows, whole (before StructureView.build).
    void placeWindows();
    // Whole again, then as world memory left them (each run).
    void resetWindows();
    [[nodiscard]] std::vector<WindowRecord> windowRecords() const;
    void restoreWindows(const std::vector<WindowRecord>& records);
    // The window E, R and X act on: the nearest within a tile.
    [[nodiscard]] const Windows::Window* windowInReach(const ProximityPrompt& entityPrompt) const;
    bool handleWindowInteraction(const ProximityPrompt& entityPrompt);
    void boardUpWindow(const glm::ivec2& tile);
    // A zombie's blow at a window this tick (glass, boards, or a climb).
    void windowBlow(const glm::ivec2& tile, Entity zombie, float dtSeconds, float damageScale = 1.0f);
    void breakWindow(const glm::ivec2& tile, bool loud);
    void updateWindows(float dtSeconds);
    // True when (world) is on the inside of the window's wall.
    [[nodiscard]] bool insideOf(const windows::Spot& spot, const glm::vec2& world) const;

    // The home base (GameBase.cpp): furniture pushed and broken up, the
    // build wheel and its stations, traps and lights, sleep and safety,
    // and the house the next life inherits.
    // Furniture and solid built things set the tile furniture flags.
    void refreshFurnitureLayer();
    // Writes the furniture changes to world memory at once.
    void rememberFurniture();
    [[nodiscard]] FurniturePiece* furnitureAt(const glm::ivec2& tile);
    // Slides the piece at `tile` one tile in `dir`, by the push rules.
    // Returns an empty string on success, else why not.
    std::string pushFurniture(const glm::ivec2& tile, const glm::ivec2& dir, bool chargeStamina);
    // Moves the piece at `tile` to `target` with its cupboard and records,
    // and remembers it (pushing, a pickup's load).
    void moveFurniturePiece(const glm::ivec2& tile, const glm::ivec2& target);
    void noteDoorwayBlocked(const glm::ivec2& target);

    // --- Vehicles (phase 11, GameVehicles.cpp over systems/VehicleSystem) ---
    // The town's sedans, pickups and bicycles from world memory, placed the
    // first time (vehiclerules::place).
    void spawnVehicles(std::mt19937& runRng, float suppliesScale);
    Entity spawnVehicle(const VehicleRecord& record);
    // Every vehicle into world memory; at a life's end, the ones used a stage worse.
    void persistVehicles(bool lifeOver);
    void enterVehicle(Entity vehicle);
    // Out beside it (`thrown`: off a bicycle, onto the ground). False if there is no room.
    bool exitVehicle(bool thrown);
    // E at a vehicle on foot: refuel, mend, load, unload.
    bool handleVehicleInteraction(const ProximityPrompt& entityPrompt);
    // E: set off a parked car's live alarm, when nothing else in reach wants E.
    bool handleVehicleAlarm(const ProximityPrompt& entityPrompt);
    void finishVehicleAction();
    // The keys found in a drawer of the house a car belongs to.
    void noteCarKeys(Entity container);
    // What the dead know of the player's vehicle this tick.
    void fillVehicleTick(ZombieTick& tick) const;
    [[nodiscard]] Entity vehicleNear(const glm::vec2& at, float reach) const;
    [[nodiscard]] std::string vehicleName(const Vehicle& vehicle) const;
    [[nodiscard]] glm::vec2 vehicleCentre(Entity vehicle) const;
    void refreshVehicleSprite(Entity vehicle);
    // What a tile is to a vehicle's body.
    [[nodiscard]] int vehicleTileKind(int x, int y) const;
    void knockDownScenery(const glm::ivec2& tile);
    // The view: leads the vehicle and pulls back at speed.
    [[nodiscard]] glm::vec2 cameraTarget(const glm::vec2& playerCenter) const;
    // Seat controls while inside: E to start, hold E to hot-wire.
    void updateVehicleSeat(float dtSeconds);
    void driveVehicle(Entity vehicle, float dtSeconds);
    void keepClearOfVehicles();
    void updateVehicleAlarms(float dtSeconds);
    void fallOffBicycle(const char* why);
    [[nodiscard]] bool alarmLive(const Vehicle& vehicle) const;
    // Breaks the piece up into the pack (overflow to the floor).
    void breakFurniture(const glm::ivec2& tile);
    // The best tool carried for breaking things up.
    [[nodiscard]] furnitureuse::Tool breakingTool() const;
    void dropOnFloor(const glm::ivec2& tile, int itemId, int count, float condition = 1.0f, int loaded = 0);
    // A blow on the piece blocking a doorway.
    void furnitureBlow(const glm::ivec2& tile, float damage);
    // E/X/R/F in reach of furniture, stations and things set down.
    bool handleBaseInteraction(const ProximityPrompt& entityPrompt);
    // Held actions (push, break, climb, build) and the build wheel.
    void updateBaseActions(float dtSeconds);
    void updateStructures(float dtSeconds);
    void updateTraps(float dtSeconds);
    // F with a placeable selected. True when it was one.
    bool placeItem(int slotIndex);
    Entity spawnStructure(StructureType type, const glm::ivec2& tile, const StructureRecord* record);
    // Takes the materials and starts building a station on `tile`.
    void startBuild(const StationRecipe& recipe, const glm::ivec2& tile);
    void finishBuild();
    void cancelBuild(const char* why);
    [[nodiscard]] bool canAffordStation(const StationRecipe& recipe, std::string* missing) const;
    [[nodiscard]] std::vector<glm::ivec2> occupiedTiles() const;
    // An alarm hung on this door or window rattles, if any.
    void rattleAlarm(const glm::ivec2& tile);
    [[nodiscard]] bool alarmOn(const glm::ivec2& tile) const;
    // The ways into a building: doors and windows, with how they stand.
    struct WayIn
    {
        glm::ivec2 tile{0};
        bool window = false;
        bool open = false;     // open or broken door, empty frame
        bool closed = false;   // shut (door), whole (window)
        bool boarded = false;  // boarded, or blocked by furniture
        bool covered = false;  // a curtain drawn
        float strength = 0.0f; // what the dead must break
    };
    [[nodiscard]] std::vector<WayIn> waysIn(int building) const;
    // 0 outdoors, 1 a way in open, 2 all closed, 3 all boarded or blocked.
    [[nodiscard]] int sleepSafety(int building, std::string* openWay) const;
    void noteAccomplishment();
    // Wakes the player if asleep somewhere no safer than `wakeAtOrBelow`
    // (Sleeping::safety) and within `rangeTiles` of `where`.
    void wakeSleeper(const glm::vec2& where, float rangeTiles, int wakeAtOrBelow, const std::string& message);
    // At run end: the house held, and where this life lived.
    void recordBase(bool died);
    [[nodiscard]] std::vector<StructureRecord> structureRecords(bool runEnd) const;
    void restoreStructures(const std::vector<StructureRecord>& records);
    // A light inside the building shows through its uncovered windows at night.
    [[nodiscard]] bool buildingLit(int building) const;

    // --- Scavenging (phase 6, GameScavenge.cpp) ---
    // Each run: locks, alarms, piles on the floor, what was searched.
    void setupScavenging();
    // The small box inside a building's street door that stops its alarm.
    void addAlarmBox(int building);
    [[nodiscard]] ScavengeRunRecord scavengeRunRecord() const;
    void restoreScavenging(const ScavengeRunRecord& record);
    // World memory: piles indoors, doors unlocked, containers searched.
    void saveScavengeMemory();
    // A v2 save: shops refurnished; records of containers no longer there go.
    void migrateScavenging();
    [[nodiscard]] bool doorLocked(const glm::ivec2& tile) const;
    // E, X at a locked door. True when it was one (and the key was used).
    bool handleLockedDoor(const glm::ivec2& tile);
    void unlockDoor(const glm::ivec2& tile, const char* how);
    // A building broken into while its alarm works: Loud every 2 s for 45 s.
    void tripAlarm(int building, const glm::ivec2& door);
    void updateAlarms(float dtSeconds);
    bool handleAlarmBox(const ProximityPrompt& entityPrompt);
    // E at a container: open at once if searched this run, else search it
    // (held, Soft). The armoury locker wants a crowbar.
    void beginSearch(Entity container);
    void finishSearch();
    [[nodiscard]] bool searchedThisRun(const glm::ivec2& tile) const;
    // R at a pond, a toilet or a stale collector: Dirty Water.
    bool handleWaterSource(const ProximityPrompt& entityPrompt);
    // E at a bush, a tree base, a tuft of grass or open wild ground.
    bool handleForage(const ProximityPrompt& entityPrompt);
    void finishForage();
    // F on food or drink. True when it was eaten or drunk.
    bool eatOrDrink(int slotIndex);
    // F on a Battery (into the emptiest light or radio) or a Radio (on/off).
    bool useBatteryOrRadio(int slotIndex);
    // Backspace in the pack: onto the floor where you stand.
    void dropFromPack(int slotIndex, bool wholeStack);
    // F with a Snare selected: set it on grass far from buildings.
    bool placeSnare(int slotIndex);
    // E, R at a garden bed or a snare. True when it handled the key.
    bool handleFieldStructure(const ProximityPrompt& entityPrompt);
    void updateScavenging(float dtSeconds);
    // Food with its state ("Bread (stale)"), anything else by its name.
    [[nodiscard]] std::string packItemName(const InventorySlot& slot) const;
    // What is left to find: containers not yet searched, per district and in all.
    void countTownStock();
    [[nodiscard]] float districtStock(int district) const; // 0..1, -1 unknown
    // The pile entity on `tile`, or kInvalidEntity.
    [[nodiscard]] Entity pileAt(const glm::ivec2& tile) const;
    [[nodiscard]] std::vector<FloorItemRecord> floorPiles(bool indoorsOnly) const;
    void restoreFloorPiles(const std::vector<FloorItemRecord>& piles);
    void markSeenBuildings();
    // Searched containers stand ajar, lit floor lamps glow (after each build).
    void applyFurnitureAlts();
    // A held scavenging action has run its time (search, force, smash, tape,
    // the alarm box, foraging).
    void finishScavengeAction();
    // Scavenging 2: marks over unsearched containers close by (shape pass).
    void renderScavengeMarks();

    // --- Power (phase 6, GamePower.cpp) ---
    [[nodiscard]] bool powerOn() const;
    void updatePower(float dtSeconds);
    // E at a floor lamp: on or off (with the power on).
    bool handleFloorLamp(const ProximityPrompt& entityPrompt);
    [[nodiscard]] bool lampLit(const glm::ivec2& tile) const;
    // The flashlight's battery: drains while on; dead, it will not light.
    void updateFlashlightBattery(float dtSeconds);
    [[nodiscard]] float flashlightCharge() const;
    // The radio, on: the forecast once a game hour, music in shelter.
    void updateRadio(float dtSeconds);

    // The dead (overhaul phase 4), GameDead.cpp: kinds, senses, blood,
    // hordes, threat modes and what the districts remember.
    [[nodiscard]] const zombiekinds::ModeRules& modeRules() const;
    // A kind for a new one on `tile` today, from the mode's shares and the
    // district's infection.
    [[nodiscard]] ZombieKind rollKind(std::mt19937& rng, const glm::ivec2& tile) const;
    // Spawn parameters for one of `kind` (health and speed rolled, the
    // mode's and the tone's health scales applied).
    [[nodiscard]] EntityFactory::ZombieParams zombieParams(ZombieKind kind, const glm::vec2& topLeft, std::mt19937& rng) const;
    void resetDead();
    void updateDead(float dtSeconds, float feelDt);
    void runZombieAI(float feelDt);
    void updateHordes(float dtSeconds);
    void updateFollower();
    void updateBloodTrail(float dtSeconds);
    void handleZombieEvents();
    void applyPlayerBlows();
    // A zombie has just died: the district, its blood and its burst.
    void onZombieDeath(Entity zombie);
    [[nodiscard]] senses::Shell shellAt(const glm::ivec2& tile);
    [[nodiscard]] bool buildingSealed(int building);
    // The same worked out afresh, without the tick's cache (the mixer asks
    // this, so that listening never changes what the simulation sees).
    [[nodiscard]] bool buildingSealedNow(int building) const;
    [[nodiscard]] int ruinLevel(int districtId) const;
    void refreshRuinLevels();
    void noticeUnknownRules();
    void recordTheDead(RunCheckpoint& cp); // the dead, bodies and blood into a checkpoint
    [[nodiscard]] std::string zombieDeathCause();
    ZombieKind mLastBlowKind = ZombieKind::Shambler;
    std::string mLastBlowBy; // a survivor's blow last: "Killed by NAME" (phase 8)
    bool mLastBlowFollower = false;
    void buildRuinMarks();   // litter, smears and bodies by each district's ruin level
    void drawBloodTrail();
    [[nodiscard]] static const char* ruinWord(int level); // quiet, uneasy, turning, overrun
    [[nodiscard]] bool isHordeNight(int night) const;
    [[nodiscard]] bool hordeDay() const; // a horde comes tonight (or is passing)
    [[nodiscard]] float barrierDamageScale(Entity zombie) const;
    [[nodiscard]] bool playerKnockedDown() const { return mPlayerDownTimer > 0.0f; }
    // Search the body of the dead in reach (E): rolled once, then a container.
    bool searchNearestCorpse();
    void loadDistrictMemory(bool newLife);
    void saveDistrictMemory() const;
    void startHorde();
    void reduceStartingDead();
    void placeDormantCrawlers(std::mt19937& rng);
    [[nodiscard]] bool nightNow() const; // dusk to dawn
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

    // Fighting (phase 3). Not saved: things in flight, burning flares and
    // ringing clocks end with the session (a checkpoint drops them).
    ThrownSystem mThrownSystem;
    bool mThrowAiming = false;             // G held with a throwable selected
    float mArmedAlarm = -1.0f;             // a wound clock in the pack: seconds before it rings
    struct Lure
    {
        glm::vec2 position{0.0f};
        float delay = 0.0f;      // seconds before it starts (a clock still ticking)
        float remaining = 0.0f;  // seconds it rings or burns
        float noiseTimer = 0.0f;
        float pullTimer = 0.0f;  // seconds before it pulls the dead again
        bool flare = false;
        bool clockSurvives = false;
        bool carried = false;    // a wound clock still in the pack: it rings where you are
        Entity marker = kInvalidEntity; // what lies on the ground (a flare, a clock)
    };
    std::vector<Lure> mLures;
    std::vector<std::pair<glm::vec2, glm::vec2>> mShotLines; // the last shot's lines, drawn for a frame
    float mShotLineTimer = 0.0f;
    float mShoveArcTimer = 0.0f;           // the white arc of a shove
    float mShoveArcAngle = 0.0f;
    glm::vec2 mLastThrowLanding{-1.0f};    // for the harness
    bool mHeldZoomed = false;              // the view pulled in while held
    SurvivorBrain mSurvivorBrain;
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
    int mNotebookPage = 0; // 0 the notebook, 1 the way out (Left/Right)
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
    glm::vec2 mPanicDrift{0.0f}; // the drift added to the player's velocity this tick

    // Body and mind (phase 5). Not saved unless said.
    HallucinationSystem mHallucinations;
    int mPackTab = 0;               // 0 PACK, 1 BODY (MAKE is the crafting panel)
    int mBodyCursor = 0;            // the wound picked in the BODY tab
    bool mBodyChoosing = false;     // Enter on a wound: what you carry that helps
    int mBodyChoice = 0;
    float mMicrosleepIn = -1.0f;    // seconds to the next microsleep (< 0: not counting)
    float mMicrosleepLeft = 0.0f;   // eyes shut, input lost
    float mRunXpPixels = 0.0f;      // running, toward Fitness XP
    float mCarryXpHours = 0.0f;     // carrying heavy, toward Fitness XP
    int mStepIndex = 0;             // footsteps, for "a tier louder half the time"
    glm::vec2 mLostControlDir{0.0f};
    int mPrevHolders = 0;           // grabbed this tick: +12 panic
    float mRebleedRunSeconds = 0.0f; // running on a deep leg wound
    int mRebleeds = 0;               // deep wounds opened again (the harness counts them)
    std::uint32_t mBodyRolls = 0;
    float bodyRoll();               // the run's seed: deterministic body rolls
    int mWeekDayNoted = 0;
    bool mWokeToZombieCheck = false; // set when sleep ends; the next tick checks
    float mBodyMessageCooldown = 0.0f;
    std::vector<Entity> mXpSearched; // containers already looked through (Scavenging XP)

    // Scavenging and power (phase 6). Checkpointed as ScavengeRunRecord
    // unless said.
    struct DoorLockState
    {
        glm::ivec2 tile{0};
        int building = 0;
        bool alarm = false;
        float damage = 0.0f; // blows on it (not saved)
        bool edge = true;    // in the outer wall (inside, it unlocks); a stock room's is not
    };
    std::vector<DoorLockState> mLocks;      // follows from the layout and world memory
    struct AlarmRinging
    {
        int building = 0;
        glm::ivec2 door{0};
        float left = 0.0f;
        float noiseTimer = 0.0f;
    };
    std::vector<AlarmRinging> mAlarms;      // ringing now (not saved)
    std::vector<std::pair<glm::ivec2, int>> mAlarmBoxes; // box tile, building
    std::vector<int> mAlarmsSpent;
    std::vector<glm::ivec2> mSearchedThisRun;
    std::vector<glm::ivec2> mLitLamps;
    struct ForagedSpot
    {
        glm::ivec2 tile{0};
        int day = 0;
    };
    std::vector<ForagedSpot> mForaged;
    glm::ivec2 mForageTile{-1};             // the spot being searched (not saved)
    int mPowerForced = -1;
    bool mPowerWasOn = true;                // not saved: follows the clock
    bool mDipAnnounced = false;
    bool mPowerOutAnnounced = false;
    bool mPowerWentAsleep = false;          // not saved
    bool mRadioOn = false;
    float mRadioTimer = 0.0f;               // seconds to the next forecast (not saved)
    std::vector<int> mDistrictContainers;   // per district id, at the life's start (not saved)
    std::vector<int> mDistrictStocked;
    int mTownContainers = 0;
    int mTownUnopened = 0;
    float mSeenTimer = 0.0f;
    double mLastRainHour = -1000.0;         // for mushrooms (not saved)
    std::vector<std::pair<glm::ivec2, forage::Spot>> mForageSpots; // from the scenery
    std::vector<std::pair<Entity, glm::ivec2>> mOnBeds; // the dead standing in a garden bed
    // The character screen.
    struct CharacterDraft
    {
        Traits traits;
        std::string name;
        int column = 1; // 0 the name, 1 good at, 2 bad at, 3 Begin
        int rerolls = 0;  // Enter on the name: another
        int row = 0;
        bool typed = false; // typing replaced the seeded name
        float refusedTimer = 0.0f;
        std::string refusedWhy; // what was refused, shown while the timer runs
    } mDraft;
    bool mCreateCharacterOnStart = false;

    // Vehicles (phase 11): the one the player sits in, and its state.
    Entity mActiveVehicleEntity = kInvalidEntity;
    Entity mRidingCompanion = kInvalidEntity;
    std::vector<Entity> mVehicleBlows;                 // the dead that struck the body this tick
    std::vector<std::pair<Entity, float>> mBatterers;  // who has struck it lately, and when
    float mVehicleClock = 0.0f;
    float mWireTimer = 0.0f;   // hot-wiring: seconds of E held
    bool mWireHeld = false;
    bool mVehicleZoomed = false;
    bool mVehicleTheftNoted = false;
    float mVehicleHitCooldown = 0.0f;
    std::mt19937 mVehicleRng{0x7EC1u};
    int mVehicleScrapes = 0; // walls touched while driving (counted for the harness)
    int mVehicleRunDowns = 0; // the dead thrown by a car, and cars stopped by one (harness)
    int mVehicleStops = 0;
    std::vector<std::pair<int, int>> mBurnedTiles;
    std::unordered_set<std::int64_t> mBurnedLookup; // the same, by FireSystem::key

    // Fire and weather (phase 7).
    FireSystem mFireSystem;
    std::vector<int> mBuildingFloors;        // per building id: floor tiles
    std::vector<int> mBuildingBurned;        // per building id: floor tiles burned
    std::vector<int> mBuildingBurning;       // per building id: burning now
    std::vector<char> mBuildingGutted;       // per building id
    float mFireNoiseTimer = 0.0f;
    float mLightningFlash = 0.0f;            // seconds of flash left
    float mHoursSinceRain = 99.0f;           // wet grass burns poorly
    float mSmokePuffTimer = 0.0f;
    struct Footprint
    {
        glm::vec2 position{0.0f};
        float angle = 0.0f;
        float life = 0.0f;
        bool snow = false;
    };
    std::vector<Footprint> mFootprints;
    std::unordered_map<Entity, glm::vec2> mLastPrint;
    Doors mDoors;
    std::vector<BarrierBlow> mDoorBlows; // this tick's zombie blows on doors, windows, blocked doorways

    // The dead (phase 4). Blood markers are kept by a checkpoint; the
    // districts' infection, kills and the rules of The Unknown seen are
    // world memory (save/districts.json); the rest is not saved.
    bloodtrail::Trail mBloodTrail;
    glm::vec2 mTrailLast{-1.0f};
    std::vector<ZombieEvent> mZombieEvents;
    std::vector<PlayerBlow> mPlayerBlows;
    float mPlayerDownTimer = 0.0f;   // knocked down by a Brute
    float mHobbleTimer = 0.0f;       // bitten in the leg by a Crawler
    float mThunderTimer = 60.0f;
    float mRainStoppedTimer = 0.0f;  // The Unknown: frenzied for a while after rain
    bool mWasRaining = false;
    float mBruteShakeTimer = 0.0f;
    struct Horde
    {
        int id = 0;
        int night = 0;
        std::vector<glm::ivec2> route;
        bool active = false;
        float checkTimer = 0.0f;
        int members = 0;
    } mHorde;
    int mNextHordeId = 1;
    int mHordeNightStarted = 0;
    std::vector<int> mRuinLevels;               // by district id, set at run start and each dawn
    int mRuinDay = -1;
    std::vector<int> mRuinDistrictOf;           // by tile: the district whose ruin it shows
    std::vector<int> mDistrictKills;            // this life's kills, by district id
    std::vector<int> mStartKills;               // the last life's, which thin the start
    std::array<int, zombiekinds::kUnknownRuleCount> mUnknownSeen{};
    std::array<float, zombiekinds::kUnknownRuleCount> mUnknownSeenCooldown{};
    std::vector<std::string> mLearnedRules;     // rule keys written in the notebook (world memory)
    std::pair<zombiekinds::UnknownRule, zombiekinds::UnknownRule> mUnknownRules{};
    float mFollowerTimer = 0.0f;
    bool mFollowerChosen = false;               // one follower a run (Stalker)
    std::uint64_t mShellTick = 0;
    std::vector<std::int8_t> mShellSealed;     // by building id: -1 not yet known this tick
    float mDecayedExposure = 0.0f;
    Windows mWindows;
    std::vector<windows::Spot> mWindowSpots;
    std::vector<glm::ivec2> mRemovedFurniture; // origins of pieces broken up (world memory)

    // A held action of the player's: pushing, breaking up, climbing, building.
    struct BaseAction
    {
        enum class Kind
        {
            None,
            Grab,     // E held at a movable piece
            Break,    // X at a piece
            Climb,    // through a window, over a piece
            Build,    // a station going up
            Repair,   // mending a weapon at a workbench
            Board,    // nailing a board across a door or window (phase 5)
            Treat,    // stitching or splinting a wound (phase 5)
            Read,     // a book, in shelter (phase 5)
            // Phase 6.
            Search,   // a container, before its panel opens
            Force,    // a locked door, with a crowbar
            Smash,    // a window's glass, from outside
            Tape,     // a window's glass, taped and knocked out quietly
            StopAlarm, // the box by a shop's door
            Forage,   // a bush, a tree base, a tuft of grass
            // Phase 9: the way out.
            ReadClue,      // a note, a board, a book (quiet)
            FitFuse,       // the gate box
            PourFuel,      // a can into the generator
            FreeGenerator, // working the seized generator loose (Loud every 2 s)
            // Phase 10.
            Tend,          // a weathered grave (quiet)
            Bury,          // a dead survivor, by hand (Soft)
            // Phase 11: vehicles (the target is the vehicle).
            Refuel,        // a can into the tank
            MendVehicle,   // toolbox and scrap metal
            LoadCargo,     // a piece into a pickup's bed
            UnloadCargo    // and out again
        } kind = Kind::None;
        glm::ivec2 tile{0};       // the piece, window or build site
        glm::ivec2 to{0};         // climb: where you come out
        float timer = 0.0f;
        float total = 0.0f;
        float noiseTimer = 0.0f;
        float holdTimer = 0.0f;   // E held before a grab
        float pushProgress = 0.0f;
        bool grabbed = false;
        StructureType building = StructureType::Campfire;
        glm::vec2 startPos{0.0f};
        float startHp = 0.0f;
        std::string label;
        bool window = false;   // board: a window, not a door
        int slot = -1;         // treat, read: the pack slot of the item
        int wound = -1;        // treat: which wound
        int treatment = 0;     // treat: a woundrules::Treatment
        Entity target = kInvalidEntity; // search: the container
    };
    BaseAction mBaseAction;
    bool mBuildWheelOpen = false;
    int mBuildChoice = -1;        // chosen station (index into kRecipes), -1 none
    glm::vec2 mBuildWheelCentre{0.0f};
    bool mBuildGhostValid = false;
    glm::ivec2 mBuildGhostTile{0};
    int mAccomplishedDay = 0;     // last game day an accomplishment counted
    float mLitWindowTimer = 0.0f;
    struct WindowClimb
    {
        glm::ivec2 tile{0};
        Entity zombie = kInvalidEntity;
        float timer = 0.0f;
        float seen = 0.0f; // seconds since the climber last pressed
    };
    std::vector<WindowClimb> mWindowClimbs;

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

    // --- Phase 10 ---
    journal::Lines mJournalLines;                 // assets/data/journal_lines.json
    std::vector<journal::Entry> mJournal;         // this life's, kept while a notebook is carried
    // Once-a-life entries already written (bits of JournalFlag), the last ten
    // days noted, the season the life is in.
    int mJournalFlags = 0;
    int mJournalDays = 0;
    int mJournalSeason = -1;
    std::size_t mJournalWounds = 0;               // wounds seen (new ones are written)
    bool mJournalHorde = false;                   // a horde was on its way
    bool mJournalCrisis = false;
    std::array<int, kSkillCount> mSkillEchoes{};  // what runs in the family
    struct ReaderState
    {
        bool open = false;
        int grave = -1;     // the grave read at (-1: from the notebook)
        int history = -1;   // the life whose journal it is (-1: none kept)
        bool atGrave = false;
        int scroll = 0;
    } mReader;
    int mNotebookCursor = 0;                      // the "Before you" row
    std::vector<int> mGravesFound;                // graves whose finding this life wrote down
    std::vector<Entity> mCemeteryEntities;        // fence posts and old stones
    int mStayConfirm = 0;                         // pause menu: presses of "Stay here for good"
    std::string mStayWhy;                         // pause menu: why staying is not offered yet
    bool mTitleFreshStart = false;                // title: the next new world is a Fresh Start
    bool mStayedForGood = false;                  // the run ended by staying
    int mInfectionCheck = 0;                      // harness: infection above world creation

    // Flashlight (Q): a beam at night that zombies can see from afar.
    bool mFlashlightOn = false;
    static constexpr float kFlashlightRangeTiles = 9.0f;
    static constexpr std::size_t kMaxCorpses = 60; // bodies left lying in a run
    static constexpr float kFlashlightHalfAngleDegrees = 26.0f;

    double mFpsTimer = 0.0;
    int mFramesThisSecond = 0;
    float mInterpolationAlpha = 1.0f;

    // --- The other people in town (phase 8, GameSurvivors.cpp) ---
    // Every survivor the world has (alive and dead), as the survivors file
    // keeps them; the entity of a live one carries SurvivorAI::id = index.
    std::vector<SurvivorRecord> mSurvivors;
    // assets/data/survivor_lines.json: pools of lines by situation.
    std::unordered_map<std::string, std::vector<std::string>> mSurvivorLines;
    class SurvivorHooks;
    friend class SurvivorHooks;
    // The talk panel (E on a person) and its trade view.
    struct TalkState
    {
        bool open = false;
        Entity who = kInvalidEntity;
        int cursor = 0;
        std::string line;          // what they said last
        enum class View
        {
            Talk,
            Trade,
            Give
        } view = View::Talk;
        bool packSide = true;      // trade: the cursor is in your pack
        int packCursor = 0;
        int theirCursor = 0;
        std::vector<int> offer;    // trade: per pack slot, how many you put in
        std::vector<int> ask;      // trade: per stock slot, how many you ask for
    } mTalk;
    float mSinceWeaponOut = 99.0f;   // seconds since a swing, or a gun in hand
    bool mRobbedAsleep = false;      // "Someone has been here." on waking
    float mSurvivorSmokeTimer = 0.0f;
    std::vector<int> mLitHomes;      // buildings with a survivor's lamp lit (this tick)
    // The town's cupboards as survivors see them: where each stands, and
    // where to stand to search it.
    struct SurvivorContainer
    {
        Entity entity = kInvalidEntity;
        glm::ivec2 tile{0};
        glm::ivec2 stand{-1};
        int building = 0;
    };
    std::vector<SurvivorContainer> mSurvivorContainers;
    std::unordered_map<int, std::vector<glm::ivec2>> mSurvivorHomeDoors; // doors to the outside, by building
    std::vector<Entity> mTheftNoted; // containers already counted as theft this life
    void buildSurvivorContainers();
    [[nodiscard]] const std::vector<glm::ivec2>& survivorHomeDoors(int building);
    // The container a survivor would go to next (the harness fills it).
    [[nodiscard]] Entity survivorNextContainer(Entity survivor);
    // A way into the building a survivor living there would board.
    [[nodiscard]] bool survivorCouldBoard(int building);
    [[nodiscard]] std::vector<survivorrules::StockLine> survivorStockLines(Entity survivor) const;
    // The rules' advance of everyone alive over `days`; `apply` writes it
    // into the records and the containers (else a dry run, for the harness).
    survivorrules::BetweenLivesResult advanceSurvivors(int days, bool apply);
    // A container emptied by a survivor: remembered as searched.
    void markSearchedBySurvivor(const glm::ivec2& tile, bool write = true);

    void loadSurvivorLines(const std::filesystem::path& assetsRoot);
    [[nodiscard]] const std::string& survivorLine(const std::string& pool, const std::string& name, std::uint32_t salt) const;
    // Before the start is chosen: the records, and never a start in someone's house.
    void loadSurvivorRecords();
    void avoidSurvivorHomes();
    // In setupScene: cast the town's people once (world creation), find
    // homes for any without one (an old save), and put them in the world.
    void setupSurvivors();
    void castSurvivors();
    void giveSurvivorHome(SurvivorRecord& record, std::vector<int>& taken);
    Entity spawnSurvivor(std::size_t index);
    [[nodiscard]] Entity survivorEntity(int id) const;
    [[nodiscard]] Entity companionEntity() const;
    [[nodiscard]] bool survivorLivesIn(int building) const;
    [[nodiscard]] glm::ivec2 homeTileOf(int building) const;
    // The brain, then what the game makes of it: deaths, the companion,
    // weapons drawn, trespass, robbery, being seen.
    void updateSurvivors(float dtSeconds);
    void onSurvivorDeath(Entity survivor);
    void survivorSays(Entity survivor, const std::string& pool, std::uint32_t salt = 0);
    // The live survivors back into their records, and the file written.
    void syncSurvivorRecords();
    void saveSurvivorRecords();
    // At a run's end: the days the dead character lived pass for everyone.
    void survivorsBetweenLives(int days, bool retired);
    // Taking from a container in someone's house is theft.
    void noteTakenFrom(Entity container);
    // Something of theirs taken from a lived-in house's people (a container, a car).
    void noteTheft(int building);
    // A companion close by (for sleep and company).
    [[nodiscard]] bool companionNear(float tiles) const;
    // E with nothing else in reach: talk to the person nearest, or search
    // a body. True when it did something.
    bool talkToSurvivor();
    void openTalk(Entity survivor);
    void closeTalk();
    void handleTalkInput();
    void talkChoose(int choice);
    [[nodiscard]] std::vector<std::string> talkChoices() const;
    void tradeAgree();
    void renderTalkPanel();
    void renderSurvivorSay();
    [[nodiscard]] std::string survivorSeems(Entity survivor) const;
    [[nodiscard]] float survivorWorth(Entity survivor, int itemId) const;
    [[nodiscard]] std::vector<int> survivorSpare(Entity survivor) const;
    [[nodiscard]] std::string survivorLead(Entity survivor);
    void updateSurvivorSmoke(float dtSeconds);

    // --- The way out (phase 9, GameGate.cpp and GameClues.cpp) ---
    // The gate stamped on the layout once: a new world, or an old save at
    // first load (where its campaign objective stood). True when it changed.
    bool stampGate(LayoutData& layout);
    // Each life, after the scene: the gate tiles shut, the Sergeant and the
    // dead with him, the key and the fuse where they lie, the readables.
    void setupGate();
    // After a checkpoint: what this character knew and how far it ground open.
    void restoreGate(const RunCheckpoint& cp);
    void gateCheckpoint(RunCheckpoint& cp) const;
    // Each tick: the Sergeant's walk, the last stand, the gate seen, the
    // way out walked. True when the run ended (got out).
    bool updateGate(float dtSeconds);
    // E at the box, the generator or a leaver's marker.
    bool handleGateInteraction(const ProximityPrompt& entityPrompt);
    void finishGateAction();
    void collectGateSprites();
    void saveGate();
    // A change kept back from a busy tick (the Sergeant's key): written at
    // the next checkpoint, the run's end or the title.
    void flushGate();
    bool mGateDirty = false;
    void creditGate(const std::string& deed);
    // How many of an item there are in every pack, container, pile and
    // grave in town (the key and the fuse are one of a kind).
    [[nodiscard]] int itemsInTown(int itemId) const;
    void ensureGateKey();
    void ensureGateFuse();
    void spawnSergeant(const glm::ivec2& tile, const ZombieRecord* record);
    void updateSergeant(float dtSeconds);
    void updateStand(float dtSeconds);
    void stopStand(const std::string& why);
    void spawnStandDead(int count);
    // A zombie fell: the Sergeant drops the key.
    void gateZombieDied(Entity zombie);
    // Walking through the open gate: the run ends "Out".
    void beginEscape();
    // At a run's end: where the key lies, saved.
    void persistGate();
    [[nodiscard]] bool gateKeyInHand() const;
    [[nodiscard]] bool gatePlaced() const;
    [[nodiscard]] glm::vec2 gateBoxWorld() const;
    [[nodiscard]] int gateFuelNeeded() const { return mStand.started ? 1 : gate::kFuelNeeded; }
    // Clues (GameClues.cpp).
    void loadClueContent(const std::filesystem::path& assetsRoot);
    void learnLeads(clues::Mask leads, const char* how);
    void noteGateSeen();
    // E at a readable: a held, quiet read.
    bool handleReadable(const ProximityPrompt& entityPrompt);
    void finishReadClue();
    // The radio's looped voice (with a battery in it).
    void updateBroadcast(float dtSeconds);
    // The objective line's way out step ("" in Endless).
    [[nodiscard]] std::string gateObjective() const;
    [[nodiscard]] bool wayOutPageShown() const;
    void renderWayOutPage(float left, float textW, float top, float bottom);
    // A survivor's hello: what they know of the way out ("" nothing new).
    std::string survivorGateLead(SurvivorRecord& record, float trust, float competence);
    // An earlier character's notebook taken from their pack.
    void notebookTaken(Entity container);
    // On the town map: the gate, the hardware store, the Sergeant's streets.
    struct WayOutMark
    {
        glm::vec2 world{0.0f};
        std::string label;
    };
    [[nodiscard]] std::vector<WayOutMark> wayOutMarks() const;

    gate::State mGate;                   // world memory (<slot>_gate.json), mirrored here
    gate::Stand mStand;                  // this life (checkpoint)
    clues::Content mClueContent;         // assets/data/clues.json
    clues::Names mClueNames;             // from the world seed and the gate's side
    clues::Mask mLeads = 0;              // what this character knows (checkpoint)
    clues::Seen mGateSeen;               // the gate as this character last knew it (checkpoint)
    std::vector<clues::Readable> mReadables; // follows from the layout (every life)
    std::vector<std::size_t> mReadablesRead; // read this life: then the piece is furniture again (not saved)
    glm::ivec2 mFuseCabinet{-1};         // follows from the layout
    std::vector<glm::ivec2> mSergeantLoop; // follows from the layout and the world seed
    int mSergeantBuilding = 0;
    std::size_t mSergeantLeg = 0;        // not saved
    float mSergeantTimer = 0.0f;         // not saved
    float mStandPullTimer = 0.0f;        // not saved
    int mStandStage = 0;                 // not saved: the third last said
    float mBroadcastTimer = 1.0f;        // not saved
    int mBroadcastLine = 0;              // not saved
    bool mBroadcastHeard = false;        // checkpoint
    float mGateCheckTimer = 0.0f;        // not saved
    std::vector<std::string> mOutLines;  // the Out screen (not saved)
    std::string mOutCompanion;           // who went with them (not saved)

    // --- Sound and mood (phase 12, GameSound.cpp and GameMood.cpp) ---
    // Nothing here is saved, makes an entity, or draws on the simulation's
    // random numbers: what is heard and seen of the mood only reads the world.
    // The table, the device (not under the harness or --mute), the settings.
    void setupSound(const std::filesystem::path& assetsRoot);
    // At the top of every tick: the listener, the sounds the noises made,
    // ambience, music, the body and the dead heard about you.
    void updateSoundscape(float dtSeconds);
    // One call for a noise the dead hear and the sound the player hears.
    void makeNoise(const glm::vec2& at, NoiseTier tier, float seconds, SoundId sound, Entity source = kInvalidEntity,
                   NoiseSource kind = NoiseSource::World, float reach = 1.0f);
    // A sound with no noise behind it (too quiet for the dead, or the body).
    void playAt(SoundId sound, const glm::vec2& at, float volume = 1.0f);
    void playUi(SoundId sound);
    // The step a foot makes on this tile: 0 crouched, 1 walking, 2 running.
    [[nodiscard]] SoundId footstepSound(const glm::ivec2& tile, int stance) const;
    // A loop heard from a place: volume by distance, side by screen.
    void placeLayer(LayerId layer, const glm::vec2& at, float rangeTiles, float volume);
    void updateAmbience();
    void updateDeadSounds();
    void updateBodySounds(float dtSeconds);
    void updateListening(float dtSeconds);
    void updateStings();
    void startTinnitus();
    [[nodiscard]] audiorules::Shell soundShellAt(const glm::vec2& world) const;
    [[nodiscard]] music::Tone musicTone() const;
    // Called where things happen.
    void soundOfDoor(const glm::ivec2& tile, bool opening, bool gently);
    void soundOfHit(int weaponCategory, bool heavy, bool killed, const glm::vec2& at);

    music::Director mMusic;
    float mAmbienceTimer = 0.0f;
    float mDeadSoundTimer = 0.0f;
    float mHeartbeatTimer = 0.0f;
    float mTinnitusLeft = 0.0f;
    float mListenStill = 0.0f;
    bool mListening = false;            // ear to a door (crouched, still, a door at hand)
    int mNightKnown = -1;               // -1 not yet; else 1 night, 0 day (stings)
    int mCallerStingNight = -1;         // the day of the night whose caller sting played
    float mThunderFarTimer = 30.0f;     // distant thunder before a storm
    float mDogTimer = 120.0f;
    float mFliesTimer = 0.0f;
    bool mWasHeld = false;
    bool mWasBurning = false;
    bool mWasPowerOn = true;
    float mQuietStepTimer = 0.0f;       // steps too quiet to be a noise are still heard
    std::mt19937 mSoundRng{0xA0D10u};   // which groan, when a dog barks: never the simulation's
    std::unordered_map<Entity, std::uint8_t> mHeardDead; // the dead's state as last heard
    bool mSoundReady = false;

    // Mood on the screen (GameMood.cpp). 1 full, 0.5 reduced, 0 off; the
    // harness starts at 0 (`fx on` sets 1) so older screenshots stay the same.
    float mScreenEffects = 1.0f;
    float mBlinkTimer = 30.0f;
    float mBlinkLeft = 0.0f;
    float mMoodClock = 0.0f;
    float mBreathPuffTimer = 0.0f;
    float mBeatClock = 0.0f;         // the heart, seen: 0-1 through a beat
    float mHeartbeatSeen = 0.0f;     // 0-1, 1 on the beat
    std::uint32_t mMoodSeed = 0x4D00Du; // blinks: its own numbers, never the simulation's
    moodrules::Params mMoodParams;   // this frame's pass (neutral: the pass is skipped)
    void updateMood(float dtSeconds);
    [[nodiscard]] float moodLevel() const;
    [[nodiscard]] float moodRoll();  // 0-1 from mMoodSeed
    // The idle a body falls into by its state ("" for none): low, cold, knee.
    [[nodiscard]] const char* moodIdle() const;
    void renderMoodWorld();          // in the world pass: wet drops, shadows, flies, motes, chimneys, the clock
    bool beginPostFx();              // before the world: draw into the pass's texture
    void endPostFx();                // after the world, before the HUD
    PostFx mPostFx;
    int mClockBuilding = -2;         // the town clock's building index (-2 not chosen, -1 none)
    double mClockShows = -1.0;       // the absolute hour its hands show (-1 not yet)

    // --- Run flow, settings and teaching (phase 13) ---
    // Settings (GameSettings.cpp): settings.json beside the slots, read at
    // start and written on every change. A harness run starts from the
    // defaults whatever the file says, so a player's keys never change what a
    // script or the bot does (`settings reload` reads the file on purpose).
    void loadSettings();
    void applySettings();
    void saveSettings();
    void applyWindowMode();
    // The window's drawable size changed: the world's projection, the camera
    // and the interface's size (mWindowWidth/Height are interface units).
    void onPixelSize(int width, int height);
    [[nodiscard]] glm::vec2 uiAt(const glm::vec2& world) const; // a world point on the interface
    [[nodiscard]] float uiZoom() const;                         // the camera's zoom in interface units
    [[nodiscard]] glm::vec2 uiMouse() const;
    // F1, F3 and K: the dev build's, hidden in a packaged one.
    [[nodiscard]] static bool devKeys();
    void openSettings();
    void updateSettingsScreen(float dtSeconds);
    void renderSettingsShapes();
    void settingsChange(int delta); // Left/Right on the chosen row
    [[nodiscard]] int settingsRows() const;
    settings::Settings mSettings;
    std::string mSettingsPath;
    int mPixelWidth = 1280;          // the window in pixels
    int mPixelHeight = 720;
    float mUiScale = 1.0f;           // interface units to pixels
    float mShakeScale = 1.0f;
    bool mShowSettings = false;
    int mSettingsTab = 0;            // Sound, Screen, Game, Keys
    int mSettingsRow = -1;           // -1: the tabs
    int mSettingsScroll = 0;
    int mRebinding = -1;             // the action waiting for its key
    std::string mSettingsNote;       // "Shove is F now. Use item took Space."
    float mSettingsNoteTimer = 0.0f;

    // The title (GameTitle.cpp): the menu, New World, The Town, Credits.
    enum class TitleView
    {
        Main,
        NewWorld,
        Town,
        Credits
    };
    void updateTitleMain();
    void updateNewWorld();
    void updateTown();
    void startNewWorld();
    void renderTitleMain();
    void renderNewWorld();
    void renderTown();
    void renderTitleCredits();
    TitleView mTitleView = TitleView::Main;
    int mNewWorldRow = 0;
    std::string mNewWorldSeed;       // digits; blank is random
    int mNewWorldDayMinutes = 24;
    int mTownCursor = 0;
    bool mTownReading = false;
    bool mQuitConfirm = false;       // the pause menu's Quit to desktop, asked once

    // Day length (set per world): game seconds per real second.
    float mDayScale = 1.0f;
    [[nodiscard]] double hoursPerSecond() const;

    // Standing aims after day 3 (GameAims.cpp, rules in world/Aims).
    aims::Facts aimFacts();
    void updateAims(float dtSeconds);
    void chooseAim();                // at dawn
    [[nodiscard]] aims::Aim currentAim();
    [[nodiscard]] firstdays::DayCard dawnCard();
    [[nodiscard]] int firstDayWithoutPower() const;
    aims::Kind mAimKind = aims::Kind::None; // checkpoint
    int mAimDay = 0;                 // the dawn it was chosen (checkpoint)
    int mAimsRewarded = 0;           // a bit per kind: "Done." given this life (checkpoint)
    float mAimDoneTimer = 0.0f;      // "Done." showing
    int mBoiledSinceAim = 0;         // checkpoint
    double mLoudSince = -1.0;        // the absolute hour of the player's last loud noise
    std::vector<glm::ivec2> mLeadsVisited; // this life (not saved)
    aims::Aim mAimShown;             // as the facts stood at the last look
    float mAimRefresh = 0.0f;        // seconds to the next look at the facts
    aims::DayCards mDayCards = aims::defaultCards();
    std::vector<aims::DeathHint> mDeathHints = aims::defaultDeathHints();

    // Teaching through play (GameTeach.cpp). Prompts are shown once per save
    // (the seen set lives in the settings file, by slot); the harness keeps
    // them off unless a script says `teach on`, so older shots stay the same.
    void teach(const std::string& id);   // queue a prompt, unless seen
    void taught(const std::string& id);  // used: never shown again
    [[nodiscard]] bool wasTaught(const std::string& id) const;
    void updateTeaching(float dtSeconds);
    [[nodiscard]] bool teachingOn() const;
    [[nodiscard]] bool lookAroundStep() const; // the day-1 "Look around" step shows
    void noteMessage();                   // the message log (last ten)
    void chooseDeathHint();
    bool mTeachAllowed = true;            // false under the harness until `teach on`
    std::deque<std::string> mTeachQueue;
    std::string mTeachLine;               // the prompt showing
    std::string mTeachId;
    float mTeachTimer = 0.0f;
    float mLookAroundTiles = 0.0f;        // walked on day 1 (not saved)
    bool mLookAroundUsed = false;         // E pressed (not saved)
    bool mLookAroundFirstLife = false;    // set at run start
    int mShovesThisLife = 0;
    std::deque<std::string> mMessageLog;
    std::string mLastLogged;
    std::string mDeathHint;               // the run-end screen's one line

    // The demo (--demo or demo.txt): a life ends at dawn on day 7.
    bool mDemo = false;
    bool mDemoEnded = false;
    void updateDemo();
    // Checkpoints at dawn, on coming home and on quitting to the title.
    int mDawnSavedDay = 0;
    bool mWasAtHome = false;
    float mHomeSaveCooldown = 0.0f;
    bool mAutosaveAllowed = true;    // false under the harness until `autosave on`
    void updateAutosave(float dtSeconds);
    // The window's icon: assets/icons/icon_32.png (none if it is missing).
    void setWindowIcon(const std::filesystem::path& assetsRoot);
};
