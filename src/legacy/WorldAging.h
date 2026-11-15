#pragma once

// The town ages between lives (phase 10): one pure rule, run when a life
// ends, over what the world remembers. Built things a stage worse, boards a
// quarter weaker, unkept crates picked over, gardens gone to seed or dead,
// the dead in the street gone to bones, and the districts a little worse.
// No engine dependencies, so the rules are unit-tested directly.

#include <cstdint>
#include <utility>
#include <vector>

#include "save/SaveManager.h"

namespace aging
{

constexpr float kBoardWear = 0.25f;           // of one board's health, every board, every life
constexpr float kCrateFoodLoss = 0.2f;        // of the food stacks in a crate nobody keeps
constexpr float kStripChance = 0.1f;          // a crate nobody keeps, found and emptied by a survivor
constexpr float kGardenWateredHours = 72.0f;  // watered in the life's last three days: seedlings
constexpr std::size_t kMaxBones = 40;
constexpr float kInfectionPerDay = 0.5f;
constexpr float kInfectionLifeCleans = 10.0f;
constexpr float kPopulationPerLife = 0.04f;
constexpr float kPopulationCap = 0.6f;
constexpr int kWeatherLives = 5;              // a grave older than this, untended, is weathered
constexpr int kGrassRegrowLives = 2;          // burned grass comes back after this many lives

enum class Stage
{
    Intact,
    Damaged,
    Breaking,
    Gone
};
[[nodiscard]] Stage stageOf(float hpRatio);
[[nodiscard]] float ratioOf(Stage stage); // the health a stage starts with
[[nodiscard]] Stage worse(Stage stage);

// A thing someone built, as the world keeps it.
struct Built
{
    StructureRecord record;
    int building = 0;    // the building it stands in (index + 1), 0 outdoors
    bool wears = false;  // BuildSystem::wearsBetweenLives
    bool crate = false;
    bool garden = false;
};

struct WorldState
{
    std::vector<Built> built;
    std::vector<BoardedDoorRecord> doors;
    std::vector<WindowRecord> windows;
    float doorBoardHp = 450.0f;   // one board on a door (Doors::boardHp)
    float windowBoardHp = 300.0f; // one board on a window (Windows::kBoardHp)
    std::vector<int> keptBuildings; // buildings a living survivor calls home
    std::vector<int> foodItems;     // item ids that are food or drink
    // District infection at the life's end, and at its start (the floor).
    std::vector<std::pair<int, float>> infection;
    std::vector<std::pair<int, float>> lifeStart;
    // Bones in the street (tiles), and this life's dead to add to them.
    std::vector<std::pair<int, int>> bones;
    std::vector<std::pair<int, int>> bodies;
};

// What a life's ending did to the town.
struct Report
{
    int worn = 0;          // built things a stage worse
    int lost = 0;          // built things gone
    int boardsGone = 0;    // doors and windows whose last board went
    int foodLost = 0;      // food stacks gone from unkept crates
    int stripped = 0;      // crates emptied by a survivor
    std::vector<ContainerItemRecord> strippedItems; // what they took
    std::vector<std::pair<int, int>> strippedAt;    // the crates' tiles
};

// Runs the rules over `world` for a life of `daysLived` days. Deterministic
// from `seed` (the ended life's seed).
Report ageWorld(WorldState& world, int daysLived, std::uint32_t seed);

// Boards: every board loses a quarter of one board's health; at nothing,
// gone (0 returned).
[[nodiscard]] float wornBoards(float boardHp, float oneBoard);

// A district's infection for the next life.
[[nodiscard]] float agedInfection(float atEnd, float atStart, int daysLived);

// The starting dead grow by 4 % a life lived before, at most 60 %.
[[nodiscard]] float populationScale(int livesBefore);

// A grave is weathered when more than five lives have passed since it was
// dug or last tended.
[[nodiscard]] bool weathered(int graveRun, int tendedRun, int currentRun);

} // namespace aging
