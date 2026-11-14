#pragma once

#include <cstdint>

#include <glm/vec2.hpp>

#include "components/Weather.h"

// Living off the land (phase 6): what a bush, a tree base or a tuft of grass
// gives, snares at dawn, and the food rules that go with them. Plain rules,
// apart from the game, so they can be tested.
namespace forage
{
enum class Spot
{
    None,
    Bush,     // berries, spring to autumn
    TreeBase, // mushrooms, after rain
    Tuft,     // herbs, any time
    Wild      // open wilderness ground: any of the three
};

enum class Find
{
    Nothing,
    Berries,
    Mushrooms,
    Herbs
};

struct Result
{
    Find find = Find::Nothing;
    int count = 0;
};

constexpr float kSearchSeconds = 3.0f;
constexpr int kRegrowDays = 3;          // a spot gives again after this
constexpr float kNothingChance = 0.6f;
constexpr float kBadMushroomChance = 0.25f;
constexpr int kKnowsMushroomsLevel = 2; // Living off the land

// What searching `spot` at `tile` on `day` gives. `rainedLately`: rain in the
// last two days. Each level of Living off the land adds 10 % to the yield.
[[nodiscard]] Result search(Spot spot, const glm::ivec2& tile, int day, Season season, bool rainedLately,
                            int livingSkill, std::uint32_t runSeed);
// A spot searched on `lastDay` can be searched again on `day`.
[[nodiscard]] bool regrown(int lastDay, int day);
// Eating mushrooms makes you ill: one in four is bad unless you know them.
[[nodiscard]] bool badMushroom(int livingSkill, float roll01);

// Snares: at dawn, Raw Meat (25 %, 50 % at Living off the land 3), sprung
// empty (10 %), or still set.
enum class SnareDawn
{
    Set,
    Caught,
    Sprung
};
constexpr int kSnareMinTilesFromBuildings = 15;
[[nodiscard]] SnareDawn snareAtDawn(const glm::ivec2& tile, int day, int livingSkill, std::uint32_t runSeed);
} // namespace forage

// Food in the pack (phase 6): fresh, stale or spoiled by its condition.
namespace food
{
enum class State
{
    Fresh,   // above 0.6
    Stale,   // 0.3 to 0.6: 75 % of the food value
    Spoiled  // below 0.3: half value, and illness
};
[[nodiscard]] State stateOf(float condition);
[[nodiscard]] const char* stateName(State state); // "fresh", "stale", "spoiled"
[[nodiscard]] float valueScale(float condition);
// Illness from eating it: spoiled food (as before phase 6) and raw meat.
[[nodiscard]] float illnessFrom(float condition, float rawIllness, float weakStomachScale);

// Dirty water (phase 6): thirst as water, illness, and maybe a bad stomach.
constexpr float kDirtyWaterIllness = 12.0f;
constexpr float kBadStomachChance = 0.25f;
constexpr float kBadStomachHours = 6.0f;
constexpr float kBadStomachDrain = 1.5f; // hunger and thirst drain
// A hot meal or drink: +5 degrees for two hours.
constexpr float kHotMealWarmth = 5.0f;
constexpr float kHotMealHours = 2.0f;
} // namespace food
