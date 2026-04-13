#include "systems/MentalStateSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "components/Health.h"
#include "components/MentalState.h"
#include "components/Needs.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"
#include "components/Traits.h"
#include "components/SurvivorAI.h"

namespace
{
constexpr float kSpriteSize = 32.0f;
constexpr float kZombieSightRange = 192.0f;
constexpr float kMinDirectionLength2 = 0.0001f;
}

void MentalStateSystem::update(World& world, NoiseModel& noiseModel, Entity playerEntity, float dtSeconds, double gameHours)
{
    if (playerEntity == kInvalidEntity ||
        !world.hasComponent<MentalState>(playerEntity) ||
        !world.hasComponent<Transform>(playerEntity))
    {
        return;
    }

    MentalState& mental = world.getComponent<MentalState>(playerEntity);
    const Transform& playerTransform = world.getComponent<Transform>(playerEntity);
    const glm::vec2 playerCenter(playerTransform.x + kSpriteSize * 0.5f, playerTransform.y + kSpriteSize * 0.5f);

    // --- Panic triggers ---
    float panicGain = 0.0f;

    // Zombie proximity panic
    int zombiesInSight = 0;
    glm::vec2 nearestFleeDirection(0.0f, 0.0f);
    float nearestThreatDistance2 = kZombieSightRange * kZombieSightRange;
    world.forEach<ZombieAI, Transform, Health>(
        [&](Entity, ZombieAI&, Transform& zt, Health& zh)
        {
            if (zh.current <= 0.0f) return;
            const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
            const glm::vec2 awayFromZombie = playerCenter - zc;
            const float distance2 = glm::dot(awayFromZombie, awayFromZombie);
            if (distance2 < kZombieSightRange * kZombieSightRange)
            {
                ++zombiesInSight;
                if (distance2 < nearestThreatDistance2)
                {
                    nearestThreatDistance2 = distance2;
                    nearestFleeDirection = awayFromZombie;
                }
            }
        });

    if (zombiesInSight > 0)
    {
        // Desensitisation reduces panic gain
        const float desensReduction = 1.0f - mental.desensitisation * 0.007f;
        const float effectiveCount = std::sqrt(static_cast<float>(zombiesInSight));
        panicGain += kZombieSightPanicGain * effectiveCount * std::max(0.2f, desensReduction) * dtSeconds;
    }

    // Low HP panic
    if (world.hasComponent<Health>(playerEntity))
    {
        const Health& health = world.getComponent<Health>(playerEntity);
        if (health.maximum > 0.0f && (health.current / health.maximum) < 0.3f)
        {
            panicGain += kLowHpPanicGain * dtSeconds;
        }
    }

    // Darkness panic and depression (nighttime hours 20:00-05:00)
    const bool isDark = (gameHours >= 20.0 || gameHours < 5.0);
    if (isDark)
    {
        panicGain += kDarknessPanicGain * dtSeconds;
        mental.depression = std::min(100.0f, mental.depression + 0.15f * dtSeconds);
    }

    // Low needs depression + panic from low needs
    if (world.hasComponent<Needs>(playerEntity))
    {
        const Needs& needs = world.getComponent<Needs>(playerEntity);
        float lowNeedsCount = 0.0f;
        if (needs.hunger < 20.0f) lowNeedsCount += 1.0f;
        if (needs.thirst < 20.0f) lowNeedsCount += 1.0f;
        if (needs.sleepDebt > 70.0f) lowNeedsCount += 1.0f;

        if (lowNeedsCount > 0.0f)
        {
            mental.depression = std::min(100.0f, mental.depression + kLowNeedsDepressionGain * lowNeedsCount * dtSeconds);
        }
    }

    // Nervous trait: +30% panic gain
    if (world.hasComponent<Traits>(playerEntity))
    {
        const Traits& traits = world.getComponent<Traits>(playerEntity);
        if (traits.negative == NegativeTrait::Nervous)
        {
            panicGain *= 1.3f;
        }
    }

    // Apply panic gain and decay
    mental.panic = std::clamp(mental.panic + panicGain, 0.0f, 100.0f);

    if (zombiesInSight == 0)
    {
        mental.panic = std::max(0.0f, mental.panic - mental.panicDecayPerSecond * dtSeconds);
    }

    // --- Panic effects ---
    mental.screamCooldown = std::max(0.0f, mental.screamCooldown - dtSeconds);

    // 75+ panic → scream (emits loud noise)
    if (mental.panic >= kPanicScreamThreshold && mental.screamCooldown <= 0.0f)
    {
        noiseModel.emitNoise(playerCenter, NoiseTier::Loud, 0.7f, playerEntity);
        mental.screamCooldown = 8.0f;
    }

    // Panic drift: periodically change the random drift angle
    mental.panicDriftTimer -= dtSeconds;
    if (mental.panicDriftTimer <= 0.0f)
    {
        // Use a deterministic-ish seed that changes over time
        const float seed = playerCenter.x * 0.017f + playerCenter.y * 0.013f + mental.panic * 0.07f + mental.depression * 0.03f;
        mental.panicDriftAngle = seed - std::floor(seed / 6.2832f) * 6.2832f; // wrap to ~[0,2pi]
        mental.panicDriftTimer = kPanicDriftChangeInterval;
    }

    // --- Depression ---
    // Isolation: no zombies means alone, slowly increases depression
    if (zombiesInSight == 0)
    {
        mental.depression = std::min(100.0f, mental.depression + kIsolationDepressionGain * dtSeconds);
    }

    // Hardened bloodline trait: reduce early depression accumulation
    if (world.hasComponent<Traits>(playerEntity))
    {
        const Traits& traits = world.getComponent<Traits>(playerEntity);
        if (traits.bloodline == BloodlineTrait::Hardened && mental.depression < 60.0f)
        {
            // Offset some of the depression gained this frame
            const float relief = kIsolationDepressionGain * (1.0f - kHardenedDepressionFactor) * dtSeconds;
            mental.depression = std::max(0.0f, mental.depression - relief);
        }
    }

    // Depression natural decay (very slow)
    mental.depression = std::max(0.0f, mental.depression - mental.depressionDecayPerSecond * dtSeconds);

    // --- Social recovery: NPC proximity reduces depression and panic ---
    mental.socialRecoveryTimer = std::max(0.0f, mental.socialRecoveryTimer - dtSeconds);
    if (mental.socialRecoveryTimer <= 0.0f)
    {
        bool npcNearby = false;
        world.forEach<SurvivorAI, Transform, Health>(
            [&](Entity, SurvivorAI& ai, Transform& st, Health& sh)
            {
                if (npcNearby) return;
                if (!ai.alive || sh.current <= 0.0f) return;
                if (ai.attitude == SurvivorAttitude::Hostile) return;
                const glm::vec2 sc(st.x + kSpriteSize * 0.5f, st.y + kSpriteSize * 0.5f);
                if (glm::dot(sc - playerCenter, sc - playerCenter) < kSocialRecoveryRange * kSocialRecoveryRange)
                {
                    npcNearby = true;
                }
            });

        if (npcNearby)
        {
            mental.depression = std::max(0.0f, mental.depression - kSocialDepressionRelief * kSocialRecoveryCooldown);
            mental.panic = std::max(0.0f, mental.panic - kSocialPanicRelief * kSocialRecoveryCooldown);
            mental.socialRecoveryTimer = kSocialRecoveryCooldown;
        }
    }

    // --- Desensitisation ---
    // Desensitisation increases depression gain
    if (mental.desensitisation > 30.0f)
    {
        const float extraDepression = (mental.desensitisation - 30.0f) * 0.005f * dtSeconds;
        mental.depression = std::min(100.0f, mental.depression + extraDepression);
    }

    // --- Depression crisis window (M5) ---
    mForceRetire = false;
    if (mental.depression >= kDepressionRetirementThreshold)
    {
        if (!mental.inCrisis)
        {
            // Enter crisis: start 24-hour countdown
            mental.inCrisis = true;
            mental.crisisStartHour = gameHours;
            mental.crisisRecoveryBuffer = 0.0f;
        }
        // Crisis is active but depression still at max — check timer
    }

    if (mental.inCrisis)
    {
        // Check if depression dropped below recovery threshold
        if (mental.depression < kCrisisRecoveryThreshold)
        {
            mental.inCrisis = false;
            mental.crisisRecoveryBuffer = 0.0f;
        }
        else
        {
            // Track elapsed crisis time
            double elapsed = gameHours - mental.crisisStartHour;
            if (elapsed < 0.0) elapsed += 24.0; // wrapped past midnight
            if (elapsed >= kCrisisWindowHours)
            {
                mForceRetire = true;
            }
        }
    }
}
