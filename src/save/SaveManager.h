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
    std::vector<std::pair<int,int>> items; // {itemId, count} for supply caches
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
};

// A boarded-up door and what is left of its boards.
struct BoardedDoorRecord
{
    int x = 0;
    int y = 0;
    float boardHp = 0.0f;
};

// A body lying where a zombie fell.
struct CorpseRecord
{
    float x = 0.0f; // Transform position (sprite top-left)
    float y = 0.0f;
    bool flipX = false;
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
    std::string positiveTrait;
    std::string negativeTrait;
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
    Needs needs{};
    MentalState mental{};
    std::vector<Wound> wounds;
    bool bleeding = false;
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
};

class SaveManager
{
public:
    // Dev/test hook: newly created slots use this world seed instead of a random one.
    static void setSeedOverride(std::uint32_t seed);

    static constexpr int kCurrentSaveVersion = 1;

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

    // World memory (looted containers)
    [[nodiscard]] const std::vector<std::pair<int,int>>& lootedPositions() const { return mLootedPositions; }
    [[nodiscard]] const std::vector<std::pair<int,int>>& discoveredRecipes() const { return mDiscoveredRecipes; }
    bool addLootedPosition(int tileX, int tileY);
    bool addDiscoveredRecipe(int itemA, int itemB);
    [[nodiscard]] bool isRecipeDiscovered(int itemA, int itemB) const;
    bool saveLootedPositions();
    bool saveDiscoveredRecipes();
    bool loadLootedPositions();
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
    bool writeMeta() const;
    bool writeLegacy() const;
    bool writeStructures() const;
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

    int mSaveVersion = 0;
    std::uint32_t mWorldSeed = 0;
    int mRunNumber = 1;
    int mTotalKills = 0;
    int mLongestRunDays = 0;
    int mTotalDaysSurvived = 0;
    std::vector<GraveRecord> mGraves;
    std::vector<RetirementRecord> mRetirements;
    std::vector<StructureRecord> mStructures;
    std::vector<std::pair<int,int>> mLootedPositions;
    std::vector<std::pair<int,int>> mDiscoveredRecipes;
    std::vector<VehicleRecord> mVehicles;
    std::vector<FireTileRecord> mFireTiles;
    std::vector<std::pair<int,int>> mBurnedTiles;
    std::vector<std::pair<int,int>> mBrokenDoors;
    std::vector<BoardedDoorRecord> mBoardedDoors;
    std::vector<CharacterHistoryRecord> mCharacterHistory;
    std::vector<WorldMemoryEventRecord> mWorldEvents;
    RunPresentationSettings mPresentationSettings{};
};
