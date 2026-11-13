#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "map/LayoutData.h"
#include "components/Inventory.h"
#include "components/Needs.h"
#include "components/Health.h"
#include "components/MentalState.h"
#include "components/Skills.h"
#include "components/Traits.h"
#include "components/Wounds.h"

struct GraveRecord
{
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    int day = 1;
    int runIndex = 0;
    std::string cause;
    // What they carried. Lies by the grave until someone takes it, or is
    // carried by them while they walk.
    std::vector<std::pair<int, int>> items; // (itemId, count)
    bool risen = false;      // came back: walks near (x, y) as a zombie
    bool laidToRest = false; // a later survivor put them down
    // Phase 5: who they were. Trait keys ("strong_back"), and the top two
    // skills as (Skill id, level) for phase 10's journals.
    std::vector<std::string> positiveTraits;
    std::vector<std::string> negativeTraits;
    std::vector<std::pair<int, int>> skills;
};

struct RetirementRecord
{
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    int day = 1;
    int runIndex = 0;
};

struct StructureRecord
{
    int type = 0;          // StructureType as int
    float x = 0.0f;
    float y = 0.0f;
    int condition = 0;     // StructureCondition as int
    float hpRatio = 1.0f;  // fraction of max HP remaining
    std::vector<std::pair<int,int>> items; // {itemId, count} for storage crates
    // Phase 2 fields (optional in the file).
    float water = 0.0f;     // rain collector
    float burnHours = 0.0f; // campfire, light
    bool lit = false;
    bool lantern = false;
    int uses = 0;           // spike board
    int targetX = -1;       // can alarm: its door or window
    int targetY = -1;
};

// A window someone changed: glass broken, boards across it, a curtain drawn.
struct WindowRecord
{
    int x = 0;
    int y = 0;
    bool broken = false;
    float boardHp = 0.0f;
    bool covered = false;
    float toughness = 1.0f; // phase 5: how well its boards were nailed (absent: 1)
};

// Furniture the player moved or broke up, named by the tile the town put it
// on (see world/FurnitureUse.h). Kept in <slot>_furniture.json.
struct FurnitureMoveRecord
{
    int fromX = 0;
    int fromY = 0;
    int toX = 0;
    int toY = 0;
};

struct FurnitureMemoryRecord
{
    std::vector<FurnitureMoveRecord> moves;
    std::vector<std::pair<int, int>> removed;
};

// The last house a character held (three ways in closed, or a station
// built), and where the last character lived. The next life may start there
// or hear of it.
struct BaseRecord
{
    int building = 0;   // building id (index + 1), 0 = none
    float x = 0.0f;     // world centre of the house
    float y = 0.0f;
    int runIndex = 0;
};

struct LastHomeRecord
{
    int building = 0;
    bool diedAway = false; // the character died away from it
};

struct VehicleRecord
{
    float x = 0.0f;
    float y = 0.0f;
    float condition = 100.0f;
    float fuel = 0.0f;
    float maxFuel = 30.0f;
    bool engineOn = false;
    std::vector<std::pair<int,int>> items; // persisted trunk storage
};

// A zombie walking the town at a checkpoint.
struct ZombieRecord
{
    float x = 0.0f; // Transform position (sprite top-left)
    float y = 0.0f;
    float health = 50.0f;
    float maxHealth = 50.0f;
    float baseMoveSpeed = 112.0f;
    int spawnDay = 1;
    // Phase 4 (optional fields; older records are Shamblers).
    int kind = 0;          // ZombieKind
    bool dormant = false;  // a Crawler lying in wait
    bool follower = false; // the Stalker's follower
};

// A boarded-up door and what is left of its boards.
struct BoardedDoorRecord
{
    int x = 0;
    int y = 0;
    float boardHp = 0.0f;
    float toughness = 1.0f; // phase 5: how well they were nailed (absent: 1)
};

// A body lying where a zombie fell.
struct CorpseRecord
{
    float x = 0.0f; // Transform position (sprite top-left)
    float y = 0.0f;
    bool flipX = false;
    int look = 0; // zombie look
    int dir = 1;  // facing as it fell (Animation::dir)
    int kind = 0; // ZombieKind (phase 4)
    bool searched = false;
    std::vector<std::pair<int, int>> items; // (item id, count) left in its pockets
};

// A blood marker on the ground (phase 4; checkpoint only).
struct BloodMarkerRecord
{
    float x = 0.0f; // world centre
    float y = 0.0f;
    float strength = 1.0f;
    float age = 0.0f;
    float life = 10.0f;
    int place = 0; // bloodtrail::Place
    bool pool = false;
    std::uint32_t seq = 0;
};

// What the districts remember between lives (phase 4, world memory).
struct DistrictMemoryRecord
{
    std::vector<std::pair<int, float>> infection; // district id, 0..100
    std::vector<std::pair<int, int>> kills;       // district id, the last life's kills there
    std::uint32_t appliedRunSeed = 0;             // the life the "-10 a life" was taken for
    std::vector<std::string> learnedRules;        // The Unknown's rules written in the notebook
};

// Where a run stands in its first three days (see world/FirstDays.h).
struct FirstDaysRecord
{
    std::vector<std::pair<int, int>> searched; // home containers opened
    std::vector<std::pair<int, int>> taken;    // home containers taken from: the home kit stays gone
    bool usedTap = false;
    int sleptNight = 0;  // the last night slept at home
    int night = 0;       // the night whose callers are under way
    int groupsSent = 0;
    int cardDay = 1;     // the last day whose dawn card has shown
};

// What a container holds, as the world remembers it. A container with a
// record is filled from it in every later life instead of being rolled.
struct ContainerItemRecord
{
    int itemId = -1;
    int count = 0;
    float condition = 1.0f;
    int loaded = 0; // a gun's rounds (phase 3; absent in older files)
};

struct ContainerRecord
{
    int tileX = 0;
    int tileY = 0;
    int runIndex = 0; // the run that last changed it
    std::vector<ContainerItemRecord> items;
};

// Weather as a checkpoint leaves it, so a resume continues the same weather.
struct WeatherRecord
{
    bool saved = false;
    int condition = 0;          // WeatherCondition
    float hoursLeft = 0.0f;     // game hours until the next condition
    std::string rngState;       // the weather generator, as text
};

struct FireTileRecord
{
    int tileX = 0;
    int tileY = 0;
    float intensity = 0.0f;
    float lifeRemaining = 0.0f;
};

enum class WorldMemoryEventType
{
    LootedContainer,
    BurnedTile,
    VehicleAbandoned,
    StructureAbandoned,
    GravePlaced,
    RetirementPlaced,
    Other
};

struct WorldMemoryEventRecord
{
    WorldMemoryEventType type = WorldMemoryEventType::Other;
    int tileX = 0;
    int tileY = 0;
    int day = 1;
    int runIndex = 0;
    std::string detail;
};

struct CharacterHistoryRecord
{
    std::string name;
    int runIndex = 0;
    int dayStarted = 1;
    int dayEnded = 1;
    bool retired = false;
    std::string cause;
    std::string locationTag;
    // Trait names; older records held one of each, read as one-item lists.
    std::vector<std::string> positiveTraits;
    std::vector<std::string> negativeTraits;
    std::string bloodlineTrait;
    std::string finalNote;
};

enum class SaveGameMode
{
    Campaign,
    Endless
};

enum class SaveThreatMode
{
    OverwhelmingSwarm,
    Stalker,
    MutatingThreat,
    InfectedSociety,
    Unknown,
    Hybrid
};

enum class SaveTonePreset
{
    Hopeless,
    GrimHuman,
    BrutalArcade
};

struct CampaignObjectiveState
{
    bool active = false;
    int tileX = 0;
    int tileY = 0;
    int districtId = 0;
    int infectionDeadlineDay = 0;
    bool completed = false;
};

struct RunPresentationSettings
{
    SaveGameMode gameMode = SaveGameMode::Endless;
    SaveThreatMode threatMode = SaveThreatMode::OverwhelmingSwarm;
    SaveTonePreset tonePreset = SaveTonePreset::GrimHuman;
    CampaignObjectiveState campaignObjective{};
};

struct SurvivorRecord
{
    std::string name;
    bool alive = true;
    int spawnDay = 1;
    int deathDay = 0;
    float x = 0.0f;
    float y = 0.0f;
    float hunger = 80.0f;
    float thirst = 80.0f;
    float aggression = 0.5f;
    float cooperation = 0.5f;
    float competence = 0.5f;
    int homeDistrictId = 0;
};

struct RunCheckpoint
{
    bool valid = false;
    std::uint32_t runSeed = 0;
    std::string characterName;
    int currentDay = 1;
    double gameHours = 8.0;
    float playerX = 0.0f;
    float playerY = 0.0f;
    float healthCurrent = 100.0f;
    float healthMaximum = 100.0f;
    float healthBase = 100.0f; // maximum with no wounds (Tough: 120)
    Needs needs{};
    MentalState mental{};
    std::vector<Wound> wounds;
    bool bleeding = false;
    // Phase 5: the chosen traits (keys) and what has been learned. Older
    // checkpoints leave these false: traits come from the seed's old pick.
    bool traitsSaved = false;
    std::vector<std::string> positiveTraits;
    std::vector<std::string> negativeTraits;
    bool skillsSaved = false;
    Skills skills{};
    Inventory inventory{};
    int equippedSlot = -1; // inventory slot of the weapon in hand
    int kills = 0;
    int itemsLooted = 0;
    int structuresBuilt = 0;
    float distanceWalked = 0.0f;
    // The town as this run has changed it so far.
    std::vector<std::pair<int,int>> openDoors;
    std::vector<std::pair<int,int>> brokenDoors;
    // Boards on closed doors. Checkpoints from before boards leave
    // boardsSaved false, and the doors keep what the world remembers.
    bool boardsSaved = false;
    std::vector<BoardedDoorRecord> boardedDoors;
    std::vector<std::pair<int,int>> burnedTiles; // all scars, earlier runs' included
    std::vector<FireTileRecord> fires;
    // The dead still walking (not the risen, who come back with their graves).
    // Checkpoints from before zombies were saved leave this false.
    bool zombiesSaved = false;
    std::vector<ZombieRecord> zombies;
    std::vector<CorpseRecord> corpses;
    std::vector<BloodMarkerRecord> blood;
    bool firstDaysSaved = false;
    FirstDaysRecord firstDays;
    WeatherRecord weather;
    // Phase 2: windows and built things as this run left them. Older
    // checkpoints leave the flags false and the world memory stands.
    bool windowsSaved = false;
    std::vector<WindowRecord> windows;
    bool structuresSaved = false;
    std::vector<StructureRecord> structures;
};

class SaveManager
{
public:
    // Dev/test hook: newly created slots use this world seed instead of a random one.
    static void setSeedOverride(std::uint32_t seed);

    // 1: the first revival saves. 2: container records replace looted
    // positions; known molotov recipes use the Empty Bottle. Version 1 slots
    // are migrated on load and their old files kept as *.v1.backup.
    static constexpr int kCurrentSaveVersion = 2;

    bool loadOrCreateSlot(const std::string& slotName);
    bool resetCurrentSlotWorld(bool keepPresentationSettings = true);

    [[nodiscard]] std::uint32_t worldSeed() const { return mWorldSeed; }
    [[nodiscard]] int runNumber() const { return mRunNumber; }
    [[nodiscard]] std::uint32_t currentRunSeed() const;
    [[nodiscard]] const std::vector<GraveRecord>& graves() const { return mGraves; }
    [[nodiscard]] const std::vector<RetirementRecord>& retirements() const { return mRetirements; }
    [[nodiscard]] const std::vector<StructureRecord>& structures() const { return mStructures; }
    [[nodiscard]] const std::vector<VehicleRecord>& vehicles() const { return mVehicles; }
    [[nodiscard]] const std::vector<FireTileRecord>& fireTiles() const { return mFireTiles; }
    [[nodiscard]] const std::vector<std::pair<int,int>>& burnedTiles() const { return mBurnedTiles; }
    // Doors the dead broke down in earlier runs stay broken.
    [[nodiscard]] const std::vector<std::pair<int,int>>& brokenDoors() const { return mBrokenDoors; }
    // Doors boarded up in earlier runs stay boarded.
    [[nodiscard]] const std::vector<BoardedDoorRecord>& boardedDoors() const { return mBoardedDoors; }
    // Windows broken, boarded or curtained in earlier runs.
    [[nodiscard]] const std::vector<WindowRecord>& windows() const { return mWindows; }
    void setWindows(std::vector<WindowRecord> windows) { mWindows = std::move(windows); }
    // Furniture moved or broken up, and the house the last lives held.
    [[nodiscard]] const FurnitureMemoryRecord& furnitureMemory() const { return mFurnitureMemory; }
    void setFurnitureMemory(FurnitureMemoryRecord memory) { mFurnitureMemory = std::move(memory); }
    bool saveFurnitureMemory() const;
    [[nodiscard]] const DistrictMemoryRecord& districtMemory() const { return mDistrictMemory; }
    void setDistrictMemory(DistrictMemoryRecord memory) { mDistrictMemory = std::move(memory); }
    bool saveDistrictMemory() const;
    [[nodiscard]] const BaseRecord& baseRecord() const { return mBase; }
    void setBaseRecord(const BaseRecord& base) { mBase = base; }
    [[nodiscard]] const LastHomeRecord& lastHome() const { return mLastHome; }
    void setLastHome(const LastHomeRecord& home) { mLastHome = home; }
    [[nodiscard]] const std::vector<CharacterHistoryRecord>& characterHistory() const { return mCharacterHistory; }
    [[nodiscard]] const std::vector<WorldMemoryEventRecord>& worldMemoryEvents() const { return mWorldEvents; }
    [[nodiscard]] const RunPresentationSettings& presentationSettings() const { return mPresentationSettings; }

    // World layout persistence
    [[nodiscard]] bool hasWorldLayout() const;
    [[nodiscard]] LayoutData loadWorldLayout() const;
    bool saveWorldLayout(const LayoutData& layout);

    // Run-state checkpointing
    bool saveCheckpoint(const RunCheckpoint& cp);
    [[nodiscard]] RunCheckpoint loadCheckpoint() const;
    void deleteCheckpoint();

    // World memory: what each changed container holds, recipes written down.
    [[nodiscard]] const std::vector<ContainerRecord>& containerRecords() const { return mContainers; }
    [[nodiscard]] const ContainerRecord* containerRecord(int tileX, int tileY) const;
    // Adds or replaces the record for (record.tileX, record.tileY). Call
    // saveWorldMemory() to write it.
    void setContainerRecord(const ContainerRecord& record);
    // Forgets the record at a tile (a piece broken up or pushed away).
    void removeContainerRecord(int tileX, int tileY);
    [[nodiscard]] const std::vector<std::pair<int,int>>& discoveredRecipes() const { return mDiscoveredRecipes; }
    // The version this slot had on disk before it was migrated (0: not migrated).
    [[nodiscard]] int migratedFromVersion() const { return mMigratedFrom; }
    bool addDiscoveredRecipe(int itemA, int itemB);
    [[nodiscard]] bool isRecipeDiscovered(int itemA, int itemB) const;
    bool saveWorldMemory();
    bool saveDiscoveredRecipes();
    bool loadWorldMemory();
    void setVehicleRecords(std::vector<VehicleRecord> vehicles);
    void setFireTileRecords(std::vector<FireTileRecord> fireTiles);
    void setBurnedTiles(std::vector<std::pair<int,int>> burnedTiles);
    void setBrokenDoors(std::vector<std::pair<int,int>> brokenDoors);
    void setBoardedDoors(std::vector<BoardedDoorRecord> boardedDoors);
    bool addWorldMemoryEvent(const WorldMemoryEventRecord& record);

    // Cumulative stats
    [[nodiscard]] int totalKills() const { return mTotalKills; }
    [[nodiscard]] int longestRunDays() const { return mLongestRunDays; }
    [[nodiscard]] int totalDaysSurvived() const { return mTotalDaysSurvived; }
    void addRunStats(int kills, int daysSurvived);

    bool recordDeath(const GraveRecord& record);
    // Replace an existing grave (pack taken, risen put to rest, ...). Call
    // flushLegacy() afterwards to write the change.
    void updateGrave(std::size_t index, const GraveRecord& record);
    bool flushLegacy() const { return writeLegacy(); }
    bool recordRetirement(const RetirementRecord& record);
    bool addCharacterHistoryRecord(const CharacterHistoryRecord& record);
    bool saveStructures(const std::vector<StructureRecord>& structures);

    // NPC survivor persistence
    bool saveSurvivors(const std::vector<SurvivorRecord>& survivors);
    [[nodiscard]] std::vector<SurvivorRecord> loadSurvivors() const;

    // Bloodline query
    [[nodiscard]] bool hasRetirementHistory() const { return !mRetirements.empty(); }
    [[nodiscard]] BloodlineTrait selectBloodlineForNewRun() const;

    // Mode and campaign state
    void setPresentationSettings(const RunPresentationSettings& settings) { mPresentationSettings = settings; }
    bool savePresentationSettings();

private:
    bool loadMeta();
    bool loadLegacy();
    bool loadStructures();
    void loadFurnitureMemory();
    void loadDistrictMemory();
    bool writeMeta() const;
    bool writeLegacy() const;
    bool writeStructures() const;
    // Copies every file of this slot to <file>.v<version>.backup.
    void backupSlotFiles(int version) const;
    bool writeJsonAtomic(const std::string& targetPath, const std::string& backupPath, const std::string& jsonText) const;

    std::string mSlotName;
    std::string mSaveDir;
    std::string mMetaPath;
    std::string mMetaBackupPath;
    std::string mLegacyPath;
    std::string mLegacyBackupPath;
    std::string mStructurePath;
    std::string mStructureBackupPath;
    std::string mLayoutPath;
    std::string mLayoutBackupPath;
    std::string mCheckpointPath;
    std::string mWorldMemoryPath;
    std::string mSurvivorPath;
    std::string mFurniturePath;
    std::string mDistrictPath;
    DistrictMemoryRecord mDistrictMemory;

    int mSaveVersion = 0;
    std::uint32_t mWorldSeed = 0;
    int mRunNumber = 1;
    int mTotalKills = 0;
    int mLongestRunDays = 0;
    int mTotalDaysSurvived = 0;
    std::vector<GraveRecord> mGraves;
    std::vector<RetirementRecord> mRetirements;
    std::vector<StructureRecord> mStructures;
    std::vector<ContainerRecord> mContainers;
    int mMigratedFrom = 0;
    std::vector<std::pair<int,int>> mDiscoveredRecipes;
    std::vector<VehicleRecord> mVehicles;
    std::vector<FireTileRecord> mFireTiles;
    std::vector<std::pair<int,int>> mBurnedTiles;
    std::vector<std::pair<int,int>> mBrokenDoors;
    std::vector<BoardedDoorRecord> mBoardedDoors;
    std::vector<WindowRecord> mWindows;
    FurnitureMemoryRecord mFurnitureMemory;
    BaseRecord mBase;
    LastHomeRecord mLastHome;
    std::vector<CharacterHistoryRecord> mCharacterHistory;
    std::vector<WorldMemoryEventRecord> mWorldEvents;
    RunPresentationSettings mPresentationSettings{};
};
