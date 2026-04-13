#include "systems/WoundSystem.h"

#include <algorithm>

#include "components/Bleeding.h"
#include "components/Health.h"
#include "components/Needs.h"
#include "components/Wounds.h"

namespace
{
constexpr float kWoundHealSeconds = 120.0f;    // 2 min real = wound heals naturally
constexpr float kBiteInfectionHours = 3.0f;    // game-hours until bite becomes infected
constexpr float kInfectionIllnessPerSecond = 2.0f;
constexpr float kMaxHpPenaltyPerWound = 5.0f;
constexpr float kStableRecoveryPerSecond = 0.02f;
}

void WoundSystem::update(World& world, Entity playerEntity, float dtSeconds, double /*gameHours*/)
{
    if (playerEntity == kInvalidEntity || !world.hasComponent<Wounds>(playerEntity))
    {
        return;
    }

    Wounds& wounds = world.getComponent<Wounds>(playerEntity);

    const float gameHoursThisTick = dtSeconds / 60.0f; // rough: 1 real-sec ~ 1 game-min

    for (auto it = wounds.active.begin(); it != wounds.active.end();)
    {
        Wound& w = *it;

        // Heal timer ticks down
        w.healTimer += dtSeconds;
        if (w.healTimer >= kWoundHealSeconds && !w.infected)
        {
            it = wounds.active.erase(it);
            continue;
        }

        // Bite infection progress
        if (w.type == WoundType::Bite && !w.infected)
        {
            w.infectionTimer += gameHoursThisTick;
            if (w.infectionTimer >= kBiteInfectionHours)
            {
                w.infected = true;
            }
        }

        // Infected wounds cause illness
        if (w.infected && world.hasComponent<Needs>(playerEntity))
        {
            Needs& needs = world.getComponent<Needs>(playerEntity);
            needs.illness = std::min(100.0f, needs.illness + kInfectionIllnessPerSecond * dtSeconds);
        }

        ++it;
    }

    // Apply max HP penalty for untreated wounds
    if (world.hasComponent<Health>(playerEntity))
    {
        Health& health = world.getComponent<Health>(playerEntity);
        const float penalty = static_cast<float>(wounds.active.size()) * kMaxHpPenaltyPerWound;
        health.maximum = std::max(20.0f, 100.0f - penalty);
        health.current = std::min(health.current, health.maximum);

        if (world.hasComponent<Needs>(playerEntity) && !world.hasComponent<Bleeding>(playerEntity))
        {
            const Needs& needs = world.getComponent<Needs>(playerEntity);
            const bool stable = needs.hunger > 55.0f &&
                                needs.thirst > 55.0f &&
                                needs.illness < 20.0f &&
                                std::none_of(
                                    wounds.active.begin(),
                                    wounds.active.end(),
                                    [](const Wound& wound)
                                    {
                                        return wound.infected;
                                    });

            if (stable && health.current < health.maximum)
            {
                health.current = std::min(health.maximum, health.current + kStableRecoveryPerSecond * dtSeconds);
            }
        }
    }
}
