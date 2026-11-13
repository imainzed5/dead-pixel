#pragma once

#include <array>
#include <optional>
#include <string>

#include "components/Skills.h"

// Skills as rules (phase 5, content tables section 7): thresholds, earning,
// slow forgetting, and every per-level effect as a function, so systems call
// `boardSeconds(level)` rather than testing levels themselves.
namespace skillrules
{
constexpr int kMaxLevel = 5;
// XP needed for each level, from level 1 to 5.
constexpr std::array<float, kMaxLevel> kThresholds = {100.0f, 250.0f, 500.0f, 900.0f, 1500.0f};
// Unused this long, a skill loses XP (never its level).
constexpr double kForgetAfterHours = 72.0;
constexpr float kForgetXpPerHour = 2.0f;

// --- What earns XP --------------------------------------------------------
constexpr float kXpBoard = 10.0f;
constexpr float kXpBreakFurniture = 8.0f;
constexpr float kXpBuild = 20.0f;
constexpr float kXpRepair = 6.0f;
constexpr float kXpBlowLanded = 2.0f;
constexpr float kXpKill = 8.0f;
constexpr float kXpShoveSave = 4.0f;
constexpr float kXpRunPer20Tiles = 1.0f;
constexpr float kXpHeavyCarryPerHour = 1.0f;
constexpr float kHeavyCarryKg = 20.0f;
constexpr float kXpSearch = 3.0f;
constexpr float kXpRareFind = 15.0f;
constexpr float kXpTreat = 10.0f;
constexpr float kXpSplintOrStitch = 20.0f;
constexpr float kXpHotwire = 25.0f;
constexpr float kXpVehicleRepair = 15.0f;
constexpr float kXpRefuel = 3.0f;
constexpr float kXpTrap = 6.0f;
constexpr float kXpForage = 5.0f;
constexpr float kXpCook = 4.0f;
constexpr float kXpSnareCatch = 12.0f;
constexpr float kXpHarvest = 15.0f;

[[nodiscard]] const char* name(Skill skill);
[[nodiscard]] std::optional<Skill> parse(const std::string& text);

[[nodiscard]] int levelForXp(float xp);
// XP at which a level starts (0 for level 0).
[[nodiscard]] float xpForLevel(int level);
// How far from this level to the next, 0..1 (1 at level 5).
[[nodiscard]] float progress(float xp);

struct XpResult
{
    bool leveledUp = false;
    int level = 0;
};
// Earn XP; returns the new level when it went up.
XpResult awardXp(Skills& skills, Skill skill, float amount, double nowHour);
// Set a level outright (traits at the start, the harness).
void setLevel(Skills& skills, Skill skill, int level, double nowHour);
// Forgetting: called each tick with the game hours that passed.
void decay(Skills& skills, double nowHour, float hoursPassed);

// --- Carpentry --------------------------------------------------------------
constexpr float kBoardBaseSeconds = 4.0f;
[[nodiscard]] float boardSeconds(int level);
[[nodiscard]] float buildTimeScale(int level);
[[nodiscard]] float boardStrengthScale(int level);
[[nodiscard]] bool salvagesNails(int level);
[[nodiscard]] bool canReinforceBoards(int level);
constexpr float kReinforcedBoardScale = 1.5f;
[[nodiscard]] int extraPlanksFromFurniture(int level);

// --- Fighting ---------------------------------------------------------------
[[nodiscard]] float swingStaminaScale(int level);
[[nodiscard]] float windupScale(int level);
[[nodiscard]] bool shoveFellsWeakened(int level);
constexpr float kWeakenedShoveShare = 0.5f; // health share a level-2 shove fells below
[[nodiscard]] float wearScale(int level);
[[nodiscard]] float doubleBlowChance(int level);

// --- Fitness ----------------------------------------------------------------
[[nodiscard]] float staminaBonus(int level);
[[nodiscard]] float runSpeedScale(int level);
[[nodiscard]] float carryBonusKg(int level);

// --- Scavenging -------------------------------------------------------------
[[nodiscard]] float searchTimeScale(int level);
[[nodiscard]] bool seesWorthSearching(int level);
constexpr float kWorthSearchingTiles = 3.0f;
[[nodiscard]] float extraItemChance(int level);

// --- Medicine ---------------------------------------------------------------
[[nodiscard]] float treatmentTimeScale(int level);
[[nodiscard]] float healingScale(int level);
[[nodiscard]] bool seesInfectionEarly(int level);
[[nodiscard]] bool antibioticsHalfDose(int level);

// --- Mechanics --------------------------------------------------------------
[[nodiscard]] float hotwireTimeScale(int level);
[[nodiscard]] float repairScale(int level);
[[nodiscard]] bool canRunGenerator(int level);

// --- Living off the land ----------------------------------------------------
[[nodiscard]] float forageScale(int level);
[[nodiscard]] bool knowsMushrooms(int level);
[[nodiscard]] float snareScale(int level);
[[nodiscard]] float cropScale(int level);

// --- Recipes: the soft gate --------------------------------------------------
// Below a recipe's skill level it takes twice as long and fails 40 % of the
// time ("It comes apart in your hands.").
constexpr float kUnskilledCraftTimeScale = 2.0f;
constexpr float kUnskilledFailChance = 0.4f;
[[nodiscard]] float craftTimeScale(int level, int required);
[[nodiscard]] float craftFailChance(int level, int required);
} // namespace skillrules
