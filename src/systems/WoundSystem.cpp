#include "systems/WoundSystem.h"
#include "core/Tuning.h"

#include <algorithm>
#include <cmath>

#include "components/Bleeding.h"
#include "components/Health.h"
#include "components/Needs.h"
#include "components/Wounds.h"
#include "world/WoundRules.h"

namespace
{
constexpr float kStableRecoveryPerSecond = 0.02f;

std::uint32_t hash(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
} // namespace

float WoundSystem::roll(std::uint32_t seed)
{
    ++mRolls;
    return static_cast<float>(hash(seed ^ (mRolls * 0x9E3779B9u)) >> 8) / static_cast<float>(1u << 24);
}

void WoundSystem::update(World& world, Entity playerEntity, float dtSeconds, double /*gameHours*/,
                         const WoundContext& context)
{
    mTurnedToDeath = false;
    mDressingsDirtied = 0;
    mNewlyInfected = 0;
    mNewlyTurning = 0;
    if (playerEntity == kInvalidEntity || !world.hasComponent<Wounds>(playerEntity))
    {
        return;
    }

    Wounds& wounds = world.getComponent<Wounds>(playerEntity);
    const float hours = dtSeconds / 60.0f; // 1 real second is 1 game minute
    const float healRate = woundrules::healRate(context.bedRest, context.medicineLevel, context.healTimeScale);

    for (auto it = wounds.active.begin(); it != wounds.active.end();)
    {
        Wound& w = *it;

        // Healing.
        w.healTimer += dtSeconds * healRate;
        const float healHours = woundrules::healHours(w);
        if (healHours > 0.0f && w.healTimer >= healHours * 60.0f)
        {
            it = wounds.active.erase(it);
            continue;
        }

        // An old dressing goes dirty.
        if (w.dressed)
        {
            w.dressingHours += hours;
            if (w.dressingHours >= woundrules::kDressingDirtyHours)
            {
                w.dressed = false;
                w.contaminated = true;
                w.dressingHours = 0.0f;
                ++mDressingsDirtied;
            }
        }

        // Infection: a dirty bite festers; an open deep or dirty cut may take it.
        if (woundrules::festers(w))
        {
            w.infectionTimer += hours;
            if (w.infectionTimer >= woundrules::kBiteInfectionHours)
            {
                w.infected = true;
                ++mNewlyInfected;
            }
        }
        else if (woundrules::mayInfect(w))
        {
            const float chance = 1.0f - std::pow(1.0f - woundrules::kOpenCutInfectionPerHour, hours);
            if (roll(context.seed) < chance)
            {
                w.infected = true;
                ++mNewlyInfected;
            }
        }

        if (w.infected)
        {
            if (world.hasComponent<Needs>(playerEntity))
            {
                Needs& needs = world.getComponent<Needs>(playerEntity);
                needs.illness = std::min(100.0f, needs.illness + woundrules::kInfectionIllnessPerSecond * dtSeconds);
            }
            // Hopeless: a bite infected and untreated a whole day turns.
            if (context.hopeless && w.type == WoundType::Bite && !w.turning)
            {
                w.infectedHours += hours;
                if (w.infectedHours >= woundrules::kTurningAfterHours)
                {
                    w.turning = true;
                    w.turningHours = 0.0f;
                    w.turnDeathHours = woundrules::turnDeathHours(roll(context.seed));
                    ++mNewlyTurning;
                }
            }
        }
        if (w.turning)
        {
            w.turningHours += hours;
            if (w.turningHours >= w.turnDeathHours) mTurnedToDeath = true;
        }

        ++it;
    }

    if (world.hasComponent<Health>(playerEntity))
    {
        Health& health = world.getComponent<Health>(playerEntity);
        health.maximum = woundrules::maxHealth(health.baseMaximum, wounds.active);
        health.current = std::min(health.current, health.maximum);

        // A deep torso or head wound, not yet stitched, bleeds slowly.
        const woundrules::BodyEffects fx = woundrules::effects(wounds.active);
        if (fx.slowBleedPerHour > 0.0f)
        {
            health.current = std::max(0.0f, health.current - fx.slowBleedPerHour * hours);
        }

        if (world.hasComponent<Needs>(playerEntity) && !world.hasComponent<Bleeding>(playerEntity))
        {
            const Needs& needs = world.getComponent<Needs>(playerEntity);
            const bool stable = needs.hunger > 55.0f &&
                                needs.thirst > 55.0f &&
                                needs.illness < 20.0f &&
                                std::none_of(wounds.active.begin(), wounds.active.end(),
                                             [](const Wound& wound) { return wound.infected; });

            if (stable && health.current < health.maximum)
            {
                health.current = std::min(health.maximum, health.current +
                                          Tuning::get("health.recovery.perSecond", kStableRecoveryPerSecond) * dtSeconds);
            }
        }
    }
}
