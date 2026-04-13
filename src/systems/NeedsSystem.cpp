#include "systems/NeedsSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>

#include "components/Facing.h"
#include "components/Health.h"
#include "components/Needs.h"
#include "components/Stamina.h"
#include "components/Traits.h"
#include "components/Velocity.h"

void NeedsSystem::update(World& world, float dtSeconds, double gameHours) const
{
    const bool isNight = (gameHours >= 20.0 || gameHours < 5.0);

    world.forEach<Needs>(
        [&](Entity entity, Needs& needs)
        {
            if (world.hasComponent<Health>(entity))
            {
                const Health& health = world.getComponent<Health>(entity);
                if (health.current <= 0.0f)
                {
                    return;
                }
            }

            // --- Movement state ---
            float hungerDecayMultiplier = 1.0f;
            float thirstDecayMultiplier = 1.0f;
            bool isMoving = false;
            bool isRunning = false;
            bool isCrouching = false;

            if (world.hasComponent<Velocity>(entity))
            {
                const Velocity& velocity = world.getComponent<Velocity>(entity);
                isMoving = glm::dot(glm::vec2(velocity.dx, velocity.dy), glm::vec2(velocity.dx, velocity.dy)) > 1.0f;

                if (isMoving && world.hasComponent<Facing>(entity))
                {
                    const Facing& facing = world.getComponent<Facing>(entity);
                    isRunning = facing.running;
                    isCrouching = facing.crouching;

                    if (isRunning)
                    {
                        hungerDecayMultiplier = 2.0f;
                        thirstDecayMultiplier = 2.5f;
                    }
                    else if (isCrouching)
                    {
                        hungerDecayMultiplier = 0.5f;
                        thirstDecayMultiplier = 0.6f;
                    }
                }
            }

            // Illness accelerates all decay
            const float illnessMultiplier = 1.0f + needs.illness * 0.01f;

            // --- Hunger decay ---
            float traitHungerMul = 1.0f;
            if (world.hasComponent<Traits>(entity))
            {
                const Traits& traits = world.getComponent<Traits>(entity);
                if (traits.negative == NegativeTrait::Hungry) traitHungerMul = 1.5f;
            }
            const float hungerDecay = needs.hungerDecayPerSecond * hungerDecayMultiplier * illnessMultiplier * traitHungerMul * dtSeconds;
            needs.hunger = std::max(0.0f, needs.hunger - hungerDecay);

            // --- Thirst decay ---
            const float thirstDecay = needs.thirstDecayPerSecond * thirstDecayMultiplier * illnessMultiplier * dtSeconds;
            needs.thirst = std::max(0.0f, needs.thirst - thirstDecay);

            // --- Fatigue ---
            if (isRunning)
            {
                needs.fatigue = std::min(needs.maxFatigue, needs.fatigue + 8.0f * dtSeconds);
            }
            else if (!isMoving)
            {
                needs.fatigue = std::max(0.0f, needs.fatigue - needs.fatigueRecoveryPerSecond * dtSeconds);
            }
            else if (isCrouching)
            {
                needs.fatigue = std::max(0.0f, needs.fatigue - needs.fatigueRecoveryPerSecond * 0.5f * dtSeconds);
            }

            // --- Sleep debt (always grows, only sleeping recovers) ---
            needs.sleepDebt = std::min(needs.maxSleepDebt, needs.sleepDebt + needs.sleepDebtGainPerSecond * illnessMultiplier * dtSeconds);

            // --- Body temperature (night = cold) ---
            const float targetTemp = isNight ? 34.5f : 37.0f;
            const float tempDrift = (targetTemp - needs.bodyTemperature) * 0.02f * dtSeconds;
            needs.bodyTemperature += tempDrift;

            // --- Illness (exposure / cold slowly increases, warmth slowly decreases) ---
            if (needs.bodyTemperature < 35.5f)
            {
                needs.illness = std::min(100.0f, needs.illness + 0.005f * dtSeconds);
            }
            else if (needs.illness > 0.0f && needs.bodyTemperature > 36.5f)
            {
                needs.illness = std::max(0.0f, needs.illness - 0.002f * dtSeconds);
            }

            // --- Stamina ceiling from fatigue ---
            if (world.hasComponent<Stamina>(entity))
            {
                Stamina& stamina = world.getComponent<Stamina>(entity);
                if (stamina.baseMaximum <= 0.0f)
                {
                    stamina.baseMaximum = std::max(1.0f, stamina.maximum);
                }

                float cap = stamina.baseMaximum;

                // Low hunger reduces cap
                if (needs.hunger < 20.0f)
                {
                    cap *= 0.6f;
                }
                // Low thirst reduces cap
                if (needs.thirst < 20.0f)
                {
                    cap *= 0.65f;
                }
                // High fatigue reduces cap (above 60%)
                if (needs.fatigue > 60.0f)
                {
                    const float fatigueRatio = (needs.fatigue - 60.0f) / (needs.maxFatigue - 60.0f);
                    cap *= (1.0f - fatigueRatio * 0.5f);
                }
                // High sleep debt reduces cap
                if (needs.sleepDebt > 50.0f)
                {
                    const float debtRatio = (needs.sleepDebt - 50.0f) / (needs.maxSleepDebt - 50.0f);
                    cap *= (1.0f - debtRatio * 0.4f);
                }

                stamina.maximum = std::max(1.0f, cap);
                stamina.current = std::min(stamina.current, stamina.maximum);
            }

            // --- Starvation / dehydration damage ---
            if (world.hasComponent<Health>(entity))
            {
                Health& health = world.getComponent<Health>(entity);
                if (needs.hunger <= 0.0f)
                {
                    health.current = std::max(0.0f, health.current - needs.starvationDamagePerSecond * dtSeconds);
                }
                if (needs.thirst <= 0.0f)
                {
                    health.current = std::max(0.0f, health.current - needs.dehydrationDamagePerSecond * dtSeconds);
                }
            }
        });
}
