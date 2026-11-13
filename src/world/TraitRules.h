#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "components/Skills.h"
#include "components/Traits.h"

// Traits as rules (phase 5, content tables section 8): what each costs or
// gives, what it does, the starting numbers a choice leads to, and the
// seed's quick pick. Systems ask these instead of testing traits themselves.
namespace traitrules
{
constexpr int kStartingPoints = 0;
constexpr float kBaseHealth = 100.0f;
constexpr float kBaseCarryKg = 25.0f;

[[nodiscard]] const std::array<PositiveTrait, 10>& allPositive();
[[nodiscard]] const std::array<NegativeTrait, 10>& allNegative();

[[nodiscard]] int cost(PositiveTrait trait);
[[nodiscard]] int gain(NegativeTrait trait);
// One line of effect, for the character screen.
[[nodiscard]] const char* effect(PositiveTrait trait);
[[nodiscard]] const char* effect(NegativeTrait trait);
// Save keys ("strong_back") and parsing of keys or names ("Strong Back").
[[nodiscard]] const char* key(PositiveTrait trait);
[[nodiscard]] const char* key(NegativeTrait trait);
[[nodiscard]] std::optional<PositiveTrait> parsePositive(const std::string& text);
[[nodiscard]] std::optional<NegativeTrait> parseNegative(const std::string& text);

// Points left: the start, plus every bad trait's gain, less every good
// trait's cost.
[[nodiscard]] int points(const Traits& traits);
// Can this character begin: no more than three of each, points not below 0.
[[nodiscard]] bool canBegin(const Traits& traits);
// Toggle a trait on or off; refuses a fourth of a column.
bool toggle(Traits& traits, PositiveTrait trait);
bool toggle(Traits& traits, NegativeTrait trait);

// --- Starting numbers ---------------------------------------------------
[[nodiscard]] float maxHealth(const Traits& traits);
[[nodiscard]] float carryLimitKg(const Traits& traits, int fitnessLevel);
[[nodiscard]] std::array<int, kSkillCount> startingLevels(const Traits& traits);

// --- The seed's pick ----------------------------------------------------
// Quick start: one bad trait from the seed, then one good trait whose cost
// that bad trait pays for.
[[nodiscard]] Traits quickStart(std::uint32_t seed);
// What runs before phase 5 rolled (one of the first three of each): used to
// rebuild a character from an old checkpoint, which did not save traits.
[[nodiscard]] Traits legacyPick(std::uint32_t seed);

// --- Effects ------------------------------------------------------------
[[nodiscard]] float moveScale(const Traits& traits);          // Quick
[[nodiscard]] float panicGainScale(const Traits& traits);     // Steady, Nervous
[[nodiscard]] float hungerScale(const Traits& traits);        // Hungry
[[nodiscard]] float wakeRangeScale(const Traits& traits);     // Light / Heavy Sleeper
[[nodiscard]] float sleepDebtGainScale(const Traits& traits); // Night Owl
[[nodiscard]] float darkSightScale(const Traits& traits);     // Night Owl
[[nodiscard]] float temperatureBiteScale(const Traits& traits); // Thin-Skinned
[[nodiscard]] float rainChillScale(const Traits& traits);     // Outdoorsy
[[nodiscard]] float healTimeScale(const Traits& traits);      // Slow Healer
[[nodiscard]] float companyScale(const Traits& traits);       // Loner
[[nodiscard]] float badFoodIllnessScale(const Traits& traits); // Weak Stomach
[[nodiscard]] float killPanic(const Traits& traits);          // Squeamish
[[nodiscard]] float desensitisationScale(const Traits& traits); // Squeamish
[[nodiscard]] int footstepTierBonus(const Traits& traits);    // Clumsy
} // namespace traitrules
