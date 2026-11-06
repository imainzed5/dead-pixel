#pragma once

#include "ecs/World.h"
#include "noise/NoiseModel.h"

class TileMap;

// Panic (short-term fear) and depression (long-term despair).
//
// Panic comes from threats the character can actually see: zombies in line of
// sight (walls block), weighted up when they are hunting you, plus low health
// and being outdoors in the dark. At full panic with a hunter in view the
// character screams, which brings more of them.
//
// Depression builds over days, not minutes: loneliness, unmet needs, cold and
// numbness to violence push it up; shelter with needs met, company and a good
// night's sleep bring it down. Holding it at the cap for a full day retires the
// character.
class MentalStateSystem
{
public:
    // tileMap is optional: without it every zombie in range counts as seen and
    // the character is treated as outdoors.
    void update(World& world, NoiseModel& noiseModel, Entity playerEntity, float dtSeconds,
                double gameHours = 12.0, int currentDay = 1, const TileMap* tileMap = nullptr);

    // Returns true if the player should be force-retired this frame
    [[nodiscard]] bool shouldForceRetire() const { return mForceRetire; }
    void clearForceRetire() { mForceRetire = false; }

    // --- Panic ---
    static constexpr float kThreatSightRange = 224.0f;   // 7 tiles
    static constexpr float kUnawareThreatWeight = 0.35f; // a wanderer that hasn't noticed you
    static constexpr float kZombieSightPanicGain = 3.0f;
    static constexpr float kLowHpPanicGain = 1.5f;
    static constexpr float kDarknessPanicGain = 0.5f;    // outdoors at night only
    static constexpr float kDarknessPanicCeiling = 20.0f;
    static constexpr float kPanicScreamThreshold = 90.0f;
    static constexpr float kScreamCooldownSeconds = 14.0f;
    static constexpr float kPanicDriftChangeInterval = 0.4f; // seconds between drift angle changes

    // --- Depression (per real second; a game day is 1440 s) ---
    static constexpr float kDepressionRetirementThreshold = 100.0f;
    static constexpr float kIsolationDepressionGain = 0.010f; // ~14 per day alone
    static constexpr float kNightDepressionGain = 0.006f;     // awake at night
    static constexpr float kUnmetNeedDepressionGain = 0.02f;  // per unmet need
    static constexpr float kComfortDepressionRelief = 0.012f; // sheltered, fed, rested, warm
    static constexpr float kBaselineDepressionRelief = 0.002f;
    static constexpr float kDesensitisationDepressionScale = 0.0003f;

    // M5: Crisis window
    static constexpr double kCrisisWindowHours = 24.0;
    static constexpr float kCrisisRecoveryThreshold = 90.0f;
    // Float-safe "at the cap" check for entering the crisis.
    static constexpr float kCrisisEntryThreshold = 99.5f;

    // M5: Social recovery
    static constexpr float kSocialRecoveryRange = 160.0f;
    static constexpr float kSocialDepressionRelief = 0.05f;  // per second when a friendly NPC is near
    static constexpr float kSocialPanicRelief = 2.0f;
    static constexpr float kSocialRecoveryCooldown = 0.5f;

    // M5: Hardened trait: early depression builds slower
    static constexpr float kHardenedDepressionFactor = 0.7f;

private:
    bool mForceRetire = false;
};
