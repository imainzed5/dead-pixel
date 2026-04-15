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
#include "components/Wounds.h"

struct GraveRecord
{
    std::string name;
    float x = 0.0f;
    float y = 0.0f;
    int day = 1;
    int runIndex = 0;
    std::string cause;
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
    int kills = 0;
    int itemsLooted = 0;
    int structuresBuilt = 0;
    float distanceWalked = 0.0f;
};

class SaveManager
{
public:
    static constexpr int kCurrentSaveVersion = 1;

    bool loadOrCreateSlot(const std::string& slotName);

    [[nodiscard]] std::uint32_t worldSeed() const { return mWorldSeed; }
    [[nodiscard]] int runNumber() const { return mRunNumber; }
    [[nodiscard]] std::uint32_t currentRunSeed() const;
    [[nodiscard]] const std::vector<GraveRecord>& graves() const { return mGraves; }
    [[nodiscard]] const std::vector<RetirementRecord>& retirements() const { return mRetirements; }
    [[nodiscard]] const std::vector<StructureRecord>& structures() const { return mStructures; }

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
    bool addLootedPosition(int tileX, int tileY);
    bool saveLootedPositions();
    bool loadLootedPositions();

    // Cumulative stats
    [[nodiscard]] int totalKills() const { return mTotalKills; }
    [[nodiscard]] int longestRunDays() const { return mLongestRunDays; }
    [[nodiscard]] int totalDaysSurvived() const { return mTotalDaysSurvived; }
    void addRunStats(int kills, int daysSurvived);

    bool recordDeath(const GraveRecord& record);
    bool recordRetirement(const RetirementRecord& record);
    bool saveStructures(const std::vector<StructureRecord>& structures);

    // NPC survivor persistence
    bool saveSurvivors(const std::vector<SurvivorRecord>& survivors);
    [[nodiscard]] std::vector<SurvivorRecord> loadSurvivors() const;

    // Bloodline query
    [[nodiscard]] bool hasRetirementHistory() const { return !mRetirements.empty(); }

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
};
