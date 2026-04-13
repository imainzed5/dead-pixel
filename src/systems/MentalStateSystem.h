#pragma once

#include "ecs/World.h"
#include "noise/NoiseModel.h"

class MentalStateSystem
{
public:
    void update(World& world, NoiseModel& noiseModel, Entity playerEntity, float dtSeconds, double gameHours = 12.0);

    // Returns true if the player should be force-retired this frame
    [[nodiscard]] bool shouldForceRetire() const { return mForceRetire; }
    void clearForceRetire() { mForceRetire = false; }

private:
    static constexpr float kPanicAimWobbleThreshold = 25.0f;
    static constexpr float kPanicSpeedBurstThreshold = 50.0f;
    static constexpr float kPanicScreamThreshold = 75.0f;
    static constexpr float kPanicFullThreshold = 100.0f;
    static constexpr float kPanicDriftChangeInterval = 0.4f; // seconds between drift angle changes

    static constexpr float kDepressionRetirementThreshold = 100.0f;

    static constexpr float kZombieSightPanicGain = 3.0f;
    static constexpr float kDamagePanicGainPerHP = 0.6f;
    static constexpr float kLowHpPanicGain = 1.5f;
    static constexpr float kDarknessPanicGain = 0.5f;

    static constexpr float kIsolationDepressionGain = 0.3f;
    static constexpr float kGraveDepressionGain = 2.0f;
    static constexpr float kLowNeedsDepressionGain = 0.5f;

    static constexpr float kKillDesensitisationGain = 1.5f;

    // M5: Crisis window
    static constexpr double kCrisisWindowHours = 24.0;
    static constexpr float kCrisisRecoveryThreshold = 90.0f;

    // M5: Social recovery
    static constexpr float kSocialRecoveryRange = 160.0f;
    static constexpr float kSocialDepressionRelief = 1.0f;   // per second when NPC nearby
    static constexpr float kSocialPanicRelief = 2.0f;
    static constexpr float kSocialRecoveryCooldown = 0.5f;

    // M5: Hardened trait multiplier
    static constexpr float kHardenedDepressionFactor = 0.7f;

    bool mForceRetire = false;
};
