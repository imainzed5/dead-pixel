#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "legacy/Journal.h"
#include "map/LayoutData.h"
#include "world/Clues.h"
#include "world/Gate.h"
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
    // Phase 8: a companion who died at your side ("Travelled with <name>").
    // Not a life of the bloodline: not counted among those who came before.
    std::string travelledWith;
    // Phase 9: what their notebook held, when they carried one (the way
    // out's leads and what they had seen of the gate). Taking the notebook
    // from their pack teaches it.
    bool notebook = false;
    int leads = 0;
    clues::Seen seen;
    // Phase 10 (optional in the file): the marker and the cemetery. A grave
    // whose marker was set in the cemetery stands there (x, y move with it).
    bool markerTaken = false; // carried by the living, not yet set
    bool buried = false;      // a stone in the cemetery
    int burialOrder = -1;     // its place in the rows
    int tendedRun = 0;        // the life that last tended it (weathering)
    int restRun = 0;          // the life that laid the risen to rest
    bool survivor = false;    // a survivor the player buried by hand
    [[nodiscard]] bool ofBloodline() const { return travelledWith.empty() && !survivor; }
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
    // Phase 6 fields (optional in the file).
    float standingHours = 0.0f; // rain collector
    float growHours = 0.0f;     // garden bed
    float sinceWaterHours = 0.0f;
    bool regrowing = false;
    bool dead = false;
    int snareState = 0;
    int lastDawn = 0;
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
    int gateRole = 0;      // phase 9: 1 the Sergeant, 2 one of the dead with him
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
    // Phase 10 (optional): infection when this life began (the floor the town
    // ages from) and when the world's first life began.
    std::vector<std::pair<int, float>> lifeStart;
    std::vector<std::pair<int, float>> created;
};

// Phase 10: what the aging town keeps (<slot>_aging.json): bones in the
// street where the dead of earlier lives lay.
struct AgingRecord
{
    std::vector<std::pair<int, int>> bones; // tiles
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

// Phase 6: things left lying on a floor (a pile), as the world or a
// checkpoint remembers them.
struct FloorItemRecord
{
    int x = 0;
    int y = 0;
    std::vector<ContainerItemRecord> items;
};

// Phase 6 world memory: what later lives inherit of scavenging.
struct ScavengeMemoryRecord
{
    std::vector<std::pair<int, int>> unlockedDoors; // forced, keyed or opened from inside
    std::vector<std::pair<int, int>> searched;      // containers searched in any life (drawn ajar)
    std::vector<int> seenBuildings;                 // building ids whose kind the map shows
    std::vector<std::pair<int, int>> usedWater;     // toilets drawn from (one fill each per world)
    std::vector<std::pair<int, int>> usedKeyRings;  // kitchen containers whose keys were found
    std::vector<FloorItemRecord> floorItems;        // piles indoors
};

// Phase 6 run state, kept in the checkpoint.
struct ScavengeRunRecord
{
    bool saved = false;
    int powerForced = -1;          // -1 none, 0 off, 1 on (harness)
    bool dipAnnounced = false;
    bool outAnnounced = false;
    std::vector<std::pair<int, int>> searched;      // opened this run: no wait
    std::vector<std::array<int, 3>> foraged;        // x, y, day searched
    std::vector<FloorItemRecord> floorItems;        // every pile
    std::vector<std::pair<int, int>> litLamps;
    std::vector<int> alarmsSpent;                   // buildings whose alarm has gone off
    bool radioOn = false;
};

// Weather as a checkpoint leaves it, so a resume continues the same weather.
struct WeatherRecord
{
    bool saved = false;
    int condition = 0;          // WeatherCondition
    float hoursLeft = 0.0f;     // game hours until the next condition
    float snowCover = 0.0f;     // phase 7, optional
    std::string rngState;       // the weather generator, as text
};

// A burning tile of the fire grid (phase 7): `intensity` is its heat 0..1
// and `lifeRemaining` the fuel it has left 0..1 (older saves held seconds;
// both are clamped on restore). `byPlayer`: kills it makes are the player's.
struct FireTileRecord
{
    int tileX = 0;
    int tileY = 0;
    float intensity = 0.0f;
    float lifeRemaining = 0.0f;
    bool byPlayer = false;
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
    bool escaped = false; // phase 9: got out by the gate
    // Phase 10 (optional in the file): the journal they kept, whether a later
    // life read it, and what made the bloodline's traits.
    std::vector<journal::Entry> journal;
    bool journalRead = false;
    bool chosen = false;      // stayed for good (a retirement chosen, not a crisis)
    float depression = 0.0f;  // at the end
    int tilesBurned = 0;      // in that life
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

struct RunPresentationSettings
{
    SaveGameMode gameMode = SaveGameMode::Endless;
    SaveThreatMode threatMode = SaveThreatMode::OverwhelmingSwarm;
    SaveTonePreset tonePreset = SaveTonePreset::GrimHuman;
    // Phase 10, set when a world is made: everything carries over (Living
    // World), or nothing does (Fresh Start).
    bool livingWorld = true;
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
    // Version 4 (phase 8): who they are to the town and to you. Older files
    // load with no home (one is found for them), empty stock and trust 0.
    int id = 0;
    int role = 0;           // survivorrules::Role
    int partner = -1;       // the other half of a pair (id)
    int group = 0;          // 1 the ones at the garage, 2 the society
    int outfit = 0;
    bool stayer = false;    // keeps the house while the other goes out
    bool retired = false;   // an earlier character of the bloodline
    int homeBuilding = 0;   // index + 1; 0 none yet
    int homeX = -1;         // a floor tile of home
    int homeY = -1;
    float trust = 0.0f;     // -100..100, toward the current character
    float sleepDebt = 0.0f;
    float health = 60.0f;
    int wounds = 0;
    bool companion = false; // travelling with the current character
    bool buried = false;    // dead with a grave, not a body
    int helloDay = -1;      // the day "Say hello" last counted
    int lastSeenDay = 0;    // the day last seen; 0 never
    bool met = false;       // in the notebook (kept across lives: someone wrote it down)
    int metRun = 0;         // the run that last met them (an earlier one: "Someone wrote:")
    bool homeKnown = false;
    bool avoiding = false;  // robbed you and keeps away
    int angryDay = -1;
    bool warnedHome = false;
    float ignoredHours = 0.0f;
    int outDay = -1;
    std::vector<std::string> told; // leads already told
    std::vector<ContainerItemRecord> stock;
    // Phase 9: a retired character's leads on the way out (they teach them),
    // and one who went out by the gate with you (gone, not dead).
    int gateLeads = 0;
    bool left = false;
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
    // Phase 6: power, piles, searched containers, foraging.
    ScavengeRunRecord scavenge;
    // Phase 9: the way out as this character knows it, and how far the gate
    // ground open this life (the next life finds it closed).
    // Phase 10: this life's journal so far, and which of its once-a-life
    // entries are written (journal flags, the last ten days noted, the season).
    bool journalSaved = false;
    std::vector<journal::Entry> journal;
    int journalFlags = 0;
    int journalDays = 0;
    int journalSeason = -1;
    bool gateSaved = false;
    int leads = 0;
    clues::Seen seen;
    bool broadcastHeard = false;
    bool standStarted = false;
    float standOpened = 0.0f;
    float standElapsed = 0.0f;
    int standWaves = 0;
};

class SaveManager
{
public:
    // Dev/test hook: newly created slots use this world seed instead of a random one.
    static void setSeedOverride(std::uint32_t seed);

    // 1: the first revival saves. 2: container records replace looted
    // positions; known molotov recipes use the Empty Bottle. Version 1 slots
    // are migrated on load and their old files kept as *.v1.backup.
    // 4: survivors with homes, stock and trust (phase 8); version 3
    // survivors are kept and given homes on load.
    // 5: the way out (phase 9): the gate file, leads in checkpoints and
    // graves; an old campaign objective becomes the gate's district.
    static constexpr int kCurrentSaveVersion = 5;

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
    // Gutted buildings (phase 7): one floor tile of each.
    [[nodiscard]] const std::vector<std::pair<int,int>>& guttedBuildings() const { return mGutted; }
    void addGuttedBuilding(int x, int y);
    // Doors boarded up in earlier runs stay boarded.
    [[nodiscard]] const std::vector<BoardedDoorRecord>& boardedDoors() const { return mBoardedDoors; }
    // Windows broken, boarded or curtained in earlier runs.
    [[nodiscard]] const std::vector<WindowRecord>& windows() const { return mWindows; }
    void setWindows(std::vector<WindowRecord> windows) { mWindows = std::move(windows); }
    // Furniture moved or broken up, and the house the last lives held.
    [[nodiscard]] const FurnitureMemoryRecord& furnitureMemory() const { return mFurnitureMemory; }
    void setFurnitureMemory(FurnitureMemoryRecord memory) { mFurnitureMemory = std::move(memory); }
    bool saveFurnitureMemory() const;
    [[nodiscard]] const ScavengeMemoryRecord& scavengeMemory() const { return mScavenge; }
    void setScavengeMemory(ScavengeMemoryRecord memory) { mScavenge = std::move(memory); }
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
    // A grave that is not a life of the bloodline (a companion, a survivor
    // buried by their own). Does not advance the run.
    bool addGrave(const GraveRecord& record);
    bool addCharacterHistoryRecord(const CharacterHistoryRecord& record);
    bool saveStructures(const std::vector<StructureRecord>& structures);

    // NPC survivor persistence
    bool saveSurvivors(const std::vector<SurvivorRecord>& survivors);
    [[nodiscard]] std::vector<SurvivorRecord> loadSurvivors() const;

    // Bloodline query (the traits themselves: legacy::bloodlineFor, phase 10)
    [[nodiscard]] bool hasRetirementHistory() const { return !mRetirements.empty(); }

    // Mode and campaign state
    void setPresentationSettings(const RunPresentationSettings& settings) { mPresentationSettings = settings; }
    bool savePresentationSettings();

    // The service gate (phase 9): world memory, <slot>_gate.json.
    [[nodiscard]] const gate::State& gateState() const { return mGate; }
    void setGateState(gate::State state) { mGate = std::move(state); }
    bool saveGateState() const;
    // A save from before phase 9 that had a campaign objective: its district,
    // where the gate is to stand (0 none). Cleared once the gate is placed.
    [[nodiscard]] int legacyObjectiveDistrict() const { return mLegacyObjectiveDistrict; }
    void clearLegacyObjectiveDistrict();
    // A life that got out by the gate: no grave, no survivor. Advances the run.
    bool recordEscape();

    // Phase 10: the aging town (bones) and a Fresh Start's forgetting.
    [[nodiscard]] const AgingRecord& aging() const { return mAging; }
    void setAging(AgingRecord aging) { mAging = std::move(aging); }
    bool saveAging() const;
    // A Fresh Start world at a life's end: everything the town remembered goes
    // (graves, built things, containers, survivors, the districts, the gate's
    // work, bones). The town, its seed, the run count and the history stay.
    bool forgetWorld();
    [[nodiscard]] bool livingWorld() const { return mPresentationSettings.livingWorld; }
    // The history record of a life (nullptr: none). Non-const: reading a
    // journal marks it read; call flushLegacy() after.
    CharacterHistoryRecord* lifeRecord(int runIndex, const std::string& name);

private:
    bool loadMeta();
    bool loadLegacy();
    bool loadStructures();
    void loadFurnitureMemory();
    void loadDistrictMemory();
    void loadGateState();
    void loadAging();
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
    std::string mGatePath;
    std::string mAgingPath;
    AgingRecord mAging;
    DistrictMemoryRecord mDistrictMemory;
    gate::State mGate;
    int mLegacyObjectiveDistrict = 0;

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
    std::vector<std::pair<int,int>> mGutted;
    std::vector<BoardedDoorRecord> mBoardedDoors;
    std::vector<WindowRecord> mWindows;
    ScavengeMemoryRecord mScavenge;
    FurnitureMemoryRecord mFurnitureMemory;
    BaseRecord mBase;
    LastHomeRecord mLastHome;
    std::vector<CharacterHistoryRecord> mCharacterHistory;
    std::vector<WorldMemoryEventRecord> mWorldEvents;
    RunPresentationSettings mPresentationSettings{};
};
