#pragma once

#include <glm/vec2.hpp>

#include "components/Weather.h"
#include "noise/NoiseModel.h"

// How the dead sense the living, as plain rules (overhaul phase 4). The
// system that applies them is ZombieAISystem (ZombieSenses.cpp).
namespace senses
{
// --- Sight ---
inline constexpr float kDarkScale = 0.6f;
inline constexpr float kLitScale = 1.3f;
inline constexpr float kCrouchStillScale = 0.5f;
inline constexpr float kCrouchMovingScale = 0.75f;
// An unaware zombie sees in a front cone; an alerted one all round.
inline constexpr float kConeDegrees = 160.0f;
// Sight is checked this often per zombie, staggered by entity id.
inline constexpr int kSightEveryTicks = 6; // 10 Hz at 60 ticks/s

struct SightInput
{
    float baseTiles = 8.0f;        // the kind's sight
    bool dark = false;             // night
    bool playerLit = false;        // flashlight, fire, a lit window behind them
    float weatherVisibility = 1.0f;
    bool crouched = false;
    bool moving = false;
    float extraScale = 1.0f;       // district infection, threat mode
};
[[nodiscard]] float sightRangeTiles(const SightInput& in);

// Whether `toTarget` lies inside a cone of `coneDegrees` about `facing`.
// A zero facing sees all round.
[[nodiscard]] bool inCone(const glm::vec2& facing, const glm::vec2& toTarget, float coneDegrees = kConeDegrees);

// --- Awareness ---
// Seeing the player fills a 0..1 meter; at 1 the zombie gives chase.
inline constexpr float kAwarenessDrainPerSecond = 0.5f;
inline constexpr float kHeadUpAwareness = 0.4f;
[[nodiscard]] float awarenessFillPerSecond(float distanceTiles, float rangeTiles, bool runner);
// One step of the meter: filling while seen, draining (unless `drains` is
// false: the Stalker) while not.
[[nodiscard]] float stepAwareness(float awareness, bool seen, float distanceTiles, float rangeTiles, bool runner,
                                  bool drains, float dtSeconds);

// --- Hearing ---
// Rain, heavy rain and a storm muffle every radius.
[[nodiscard]] float weatherHearingScale(WeatherCondition condition);
// Wind carries Loud and louder noises 20 % further downwind and 20 % less
// far upwind. `windTo` is the unit direction the wind blows toward.
[[nodiscard]] float windScale(NoiseTier tier, const glm::vec2& windTo, float windStrength, const glm::vec2& sourceToListener);
// A noise crossing a closed building's shell (all its doors and windows
// shut) drops one tier per shell. Same building (or both outdoors): none.
struct Shell
{
    int building = 0;   // 0 outdoors
    bool sealed = false;
};
[[nodiscard]] NoiseTier tierHeard(NoiseTier emitted, const Shell& source, const Shell& listener);
// How far (tiles) a noise reaches this listener.
[[nodiscard]] float heardRadiusTiles(NoiseTier emitted, const Shell& source, const Shell& listener,
                                     WeatherCondition weather, float hearing = 1.0f, float windFactor = 1.0f);
} // namespace senses
