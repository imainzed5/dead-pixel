#include "systems/MentalStateSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "components/Health.h"
#include "components/MentalState.h"
#include "components/Needs.h"
#include "components/Sleeping.h"
#include "components/SurvivorAI.h"
#include "components/Traits.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "map/TileMap.h"
#include "systems/NeedsSystem.h"
#include "systems/ZombieAISystem.h"

namespace
{
constexpr float kSpriteSize = 32.0f;
}

void MentalStateSystem::update(World& world, NoiseModel& noiseModel, Entity playerEntity, float dtSeconds,
                               double gameHours, int currentDay, const TileMap* tileMap)
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
    const bool indoors = tileMap != nullptr && tileMap->isIndoorsAtWorld(playerCenter.x, playerCenter.y);
    const bool isDark = (gameHours >= 20.0 || gameHours < 5.0);
    const bool sleeping = world.hasComponent<Sleeping>(playerEntity);

    // ------------------------------------------------------------------ Panic

    float panicGain = 0.0f;

    // Threats the character can actually see. Hunters count fully; wanderers
    // that haven't noticed you are unsettling but much less so.
    float threatWeight = 0.0f;
    bool hunterInSight = false;
    world.forEach<ZombieAI, Transform, Health>(
        [&](Entity, ZombieAI& ai, Transform& zt, Health& zh)
        {
            if (zh.current <= 0.0f) return;
            const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
            const glm::vec2 delta = playerCenter - zc;
            if (glm::dot(delta, delta) > kThreatSightRange * kThreatSightRange) return;
            if (tileMap != nullptr && !ZombieAISystem::hasLineOfSight(*tileMap, playerCenter, zc)) return;

            const bool hunting = ai.state == ZombieState::Chase || ai.state == ZombieState::Attack;
            threatWeight += hunting ? 1.0f : kUnawareThreatWeight;
            hunterInSight = hunterInSight || hunting;
        });

    if (threatWeight > 0.0f)
    {
        const float desensReduction = 1.0f - mental.desensitisation * 0.007f;
        panicGain += kZombieSightPanicGain * std::sqrt(threatWeight) * std::max(0.2f, desensReduction) * dtSeconds;
    }

    if (world.hasComponent<Health>(playerEntity))
    {
        const Health& health = world.getComponent<Health>(playerEntity);
        if (health.maximum > 0.0f && (health.current / health.maximum) < 0.3f)
        {
            panicGain += kLowHpPanicGain * dtSeconds;
        }
    }

    // The dark is unnerving outdoors: panic creeps up to a floor it won't calm
    // below, but darkness alone never tips you into a scream.
    const float calmFloor = (isDark && !indoors && !sleeping) ? kDarknessPanicCeiling : 0.0f;
    if (mental.panic < calmFloor)
    {
        panicGain += std::min(kDarknessPanicGain * dtSeconds, calmFloor - mental.panic);
    }

    if (world.hasComponent<Traits>(playerEntity) &&
        world.getComponent<Traits>(playerEntity).negative == NegativeTrait::Nervous)
    {
        panicGain *= 1.3f;
    }

    mental.panic = std::clamp(mental.panic + panicGain, 0.0f, 100.0f);
    if (threatWeight <= 0.0f && mental.panic > calmFloor)
    {
        mental.panic = std::max(calmFloor, mental.panic - mental.panicDecayPerSecond * dtSeconds);
    }

    // Full panic with a hunter in view: scream (loud noise that draws others).
    mental.screamCooldown = std::max(0.0f, mental.screamCooldown - dtSeconds);
    if (hunterInSight && mental.panic >= kPanicScreamThreshold && mental.screamCooldown <= 0.0f)
    {
        noiseModel.emitNoise(playerCenter, NoiseTier::Loud, 0.7f, playerEntity);
        mental.screamCooldown = kScreamCooldownSeconds;
    }

    // Panic drift: periodically change the random drift angle
    mental.panicDriftTimer -= dtSeconds;
    if (mental.panicDriftTimer <= 0.0f)
    {
        const float seed = playerCenter.x * 0.017f + playerCenter.y * 0.013f + mental.panic * 0.07f + mental.depression * 0.03f;
        mental.panicDriftAngle = seed - std::floor(seed / 6.2832f) * 6.2832f; // wrap to ~[0,2pi]
        mental.panicDriftTimer = kPanicDriftChangeInterval;
    }

    // ------------------------------------------------------------- Depression
    // Changes are accumulated and applied once (gain, relief, clamp) so the cap
    // check sees a stable value instead of cap-minus-decay.

    float depressionGain = 0.0f;
    float depressionRelief = kBaselineDepressionRelief * dtSeconds;

    // Company: a friendly survivor nearby.
    bool companyNearby = false;
    world.forEach<SurvivorAI, Transform, Health>(
        [&](Entity, SurvivorAI& ai, Transform& st, Health& sh)
        {
            if (companyNearby) return;
            if (!ai.alive || sh.current <= 0.0f) return;
            if (ai.attitude == SurvivorAttitude::Hostile) return;
            const glm::vec2 sc(st.x + kSpriteSize * 0.5f, st.y + kSpriteSize * 0.5f);
            if (glm::dot(sc - playerCenter, sc - playerCenter) < kSocialRecoveryRange * kSocialRecoveryRange)
            {
                companyNearby = true;
            }
        });

    float isolationGain = 0.0f;
    if (!companyNearby)
    {
        isolationGain = kIsolationDepressionGain * dtSeconds;
        depressionGain += isolationGain;
    }

    if (isDark && !sleeping)
    {
        depressionGain += kNightDepressionGain * dtSeconds;
    }

    // Unmet needs, and the comfort of having them all met under a roof.
    if (world.hasComponent<Needs>(playerEntity))
    {
        const Needs& needs = world.getComponent<Needs>(playerEntity);
        int unmet = 0;
        if (needs.hunger < NeedsSystem::kHungryBelow) ++unmet;
        if (needs.thirst < NeedsSystem::kThirstyBelow) ++unmet;
        if (needs.sleepDebt > NeedsSystem::kTiredAbove) ++unmet;
        if (needs.bodyTemperature < NeedsSystem::kColdBelow) ++unmet;
        if (needs.illness > 40.0f) ++unmet;
        depressionGain += kUnmetNeedDepressionGain * static_cast<float>(unmet) * dtSeconds;

        const bool comfortable = indoors &&
            needs.hunger >= 50.0f && needs.thirst >= 50.0f &&
            needs.sleepDebt < 50.0f && needs.bodyTemperature >= NeedsSystem::kChillyBelow;
        if (comfortable)
        {
            depressionRelief += kComfortDepressionRelief * dtSeconds;
        }
    }

    // Hardened bloodline trait: loneliness bites less while still holding together.
    if (world.hasComponent<Traits>(playerEntity) &&
        world.getComponent<Traits>(playerEntity).bloodline == BloodlineTrait::Hardened &&
        mental.depression < 60.0f)
    {
        depressionRelief += isolationGain * (1.0f - kHardenedDepressionFactor);
    }

    // Company also steadies the nerves.
    mental.socialRecoveryTimer = std::max(0.0f, mental.socialRecoveryTimer - dtSeconds);
    if (companyNearby)
    {
        depressionRelief += kSocialDepressionRelief * dtSeconds;
        if (mental.socialRecoveryTimer <= 0.0f)
        {
            mental.panic = std::max(0.0f, mental.panic - kSocialPanicRelief * kSocialRecoveryCooldown);
            mental.socialRecoveryTimer = kSocialRecoveryCooldown;
        }
    }

    // Numbness to violence has a cost.
    if (mental.desensitisation > 30.0f)
    {
        depressionGain += (mental.desensitisation - 30.0f) * kDesensitisationDepressionScale * dtSeconds;
    }

    mental.depression = std::clamp(mental.depression + depressionGain - depressionRelief, 0.0f, kDepressionRetirementThreshold);

    // --- Depression crisis window (M5) ---
    mForceRetire = false;
    if (mental.depression >= kCrisisEntryThreshold && !mental.inCrisis)
    {
        // Enter crisis: start the 24-hour countdown
        mental.inCrisis = true;
        mental.crisisStartHour = gameHours;
        mental.crisisStartDay = currentDay;
        mental.crisisRecoveryBuffer = 0.0f;
    }

    if (mental.inCrisis)
    {
        if (mental.depression < kCrisisRecoveryThreshold)
        {
            mental.inCrisis = false;
            mental.crisisRecoveryBuffer = 0.0f;
        }
        else
        {
            const int dayDelta = std::max(0, currentDay - mental.crisisStartDay);
            double elapsed = static_cast<double>(dayDelta) * 24.0 + (gameHours - mental.crisisStartHour);
            if (elapsed < 0.0)
            {
                elapsed += 24.0;
            }
            if (elapsed >= kCrisisWindowHours)
            {
                mForceRetire = true;
            }
        }
    }
}
