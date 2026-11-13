#pragma once

#include <cstdint>

#include "ecs/World.h"

// What the body around the wounds brings to them (set by the game each tick).
struct WoundContext
{
    std::uint32_t seed = 0x5EEDu; // the run's seed: infection rolls come from it
    bool hopeless = false;        // tone Hopeless: an infected bite left a day turns
    bool bedRest = false;         // asleep in a bed: heals x1.5
    int medicineLevel = 0;
    float healTimeScale = 1.0f;   // Slow Healer: 1.5
};

// Wounds heal, fester, go dirty under old dressings, and (deep in the torso
// or head) bleed slowly. Rules in world/WoundRules.h.
class WoundSystem
{
public:
    void update(World& world, Entity playerEntity, float dtSeconds, double gameHours,
                const WoundContext& context = WoundContext{});

    // A turning ran its course this tick: the character dies and will rise.
    [[nodiscard]] bool turnedToDeath() const { return mTurnedToDeath; }
    // Dressings that went dirty this tick, wounds that took infection, and
    // wounds that began turning (for the message line).
    [[nodiscard]] int dressingsDirtied() const { return mDressingsDirtied; }
    [[nodiscard]] int newlyInfected() const { return mNewlyInfected; }
    [[nodiscard]] int newlyTurning() const { return mNewlyTurning; }

private:
    float roll(std::uint32_t seed);

    std::uint32_t mRolls = 0;
    bool mTurnedToDeath = false;
    int mDressingsDirtied = 0;
    int mNewlyInfected = 0;
    int mNewlyTurning = 0;
};
