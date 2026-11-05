#include "systems/NeedsSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>

#include "components/Facing.h"
#include "components/Health.h"
#include "components/Needs.h"
#include "components/Sleeping.h"
#include "components/Stamina.h"
#include "components/Traits.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "map/TileMap.h"

namespace
{
constexpr float kSpriteSize = 32.0f;

// Effective ambient below which the body starts losing heat, and how hard.
constexpr float kComfortableAmbient = 15.0f;
constexpr float kColdSlopePerDegree = 0.15f;
constexpr float kHotAmbient = 30.0f;
constexpr float kHotSlopePerDegree = 0.08f;

// Shelter keeps out wind and rain and holds some heat.
constexpr float kShelterBonus = 10.0f;
constexpr float kHeatSourceBonus = 18.0f;
constexpr float kRainChill = 4.0f;
constexpr float kWindChill = 4.0f;

constexpr float kWalkingHeat = 0.3f;
constexpr float kRunningHeat = 0.8f;

// Damage when a bar is empty (HP per real second; 60 s = 1 game hour).
constexpr float kStarvationDamage = 0.10f;  // ~16 game hours from full health
constexpr float kDehydrationDamage = 0.20f; // ~8 game hours from full health
constexpr float kHypothermiaDamage = 0.03f;
constexpr float kSevereHypothermiaDamage = 0.12f;
constexpr float kGravelyIllAbove = 80.0f;
constexpr float kGraveIllnessDamage = 0.04f; // ~40 game hours to die of untreated infection
} // namespace

float NeedsSystem::effectiveAmbient(float outdoorTemperature, bool indoors, float rainIntensity,
                                    float windStrength, bool nearHeat, float clothingInsulation)
{
    float ambient = outdoorTemperature;
    if (indoors)
    {
        ambient += kShelterBonus;
    }
    else
    {
        ambient -= std::clamp(rainIntensity, 0.0f, 1.0f) * kRainChill;
        ambient -= std::clamp(windStrength, 0.0f, 1.0f) * kWindChill;
    }
    if (nearHeat)
    {
        ambient += kHeatSourceBonus;
    }
    return ambient + clothingInsulation;
}

float NeedsSystem::targetBodyTemperature(float effectiveAmbientTemperature, float activityHeat)
{
    float target = 37.0f;
    if (effectiveAmbientTemperature < kComfortableAmbient)
    {
        target -= (kComfortableAmbient - effectiveAmbientTemperature) * kColdSlopePerDegree;
    }
    else if (effectiveAmbientTemperature > kHotAmbient)
    {
        target += (effectiveAmbientTemperature - kHotAmbient) * kHotSlopePerDegree;
    }
    return std::clamp(target + activityHeat, 30.0f, 40.5f);
}

void NeedsSystem::update(World& world, float dtSeconds, const NeedsEnvironment& environment) const
{
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

            const bool sleeping = world.hasComponent<Sleeping>(entity);

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
            if (sleeping)
            {
                hungerDecayMultiplier *= kSleepingDrainScale;
                thirstDecayMultiplier *= kSleepingDrainScale;
            }

            // --- Body temperature ---
            bool indoors = false;
            bool nearHeat = false;
            if (world.hasComponent<Transform>(entity))
            {
                const Transform& transform = world.getComponent<Transform>(entity);
                const glm::vec2 center(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f);
                if (environment.tileMap != nullptr)
                {
                    indoors = environment.tileMap->isIndoorsAtWorld(center.x, center.y);
                }
                for (const glm::vec2& heat : environment.heatSources)
                {
                    const glm::vec2 delta = heat - center;
                    if (glm::dot(delta, delta) < kHeatSourceRadius * kHeatSourceRadius)
                    {
                        nearHeat = true;
                        break;
                    }
                }
            }

            const float ambient = effectiveAmbient(environment.outdoorTemperature, indoors, environment.rainIntensity,
                                                   environment.windStrength, nearHeat, needs.clothingInsulation);
            const float activityHeat = isRunning ? kRunningHeat : (isMoving ? kWalkingHeat : 0.0f);
            const float targetTemp = targetBodyTemperature(ambient, activityHeat);
            const float drift = std::clamp(kTemperatureDriftPerSecond * dtSeconds, 0.0f, 1.0f);
            needs.bodyTemperature += (targetTemp - needs.bodyTemperature) * drift;

            const bool overheated = needs.bodyTemperature > kOverheatedAbove;
            if (overheated)
            {
                thirstDecayMultiplier *= 1.5f;
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
                // Recovering from exertion is slower when exhausted or freezing.
                float recovery = needs.fatigueRecoveryPerSecond;
                if (needs.sleepDebt > kTiredAbove) recovery *= 0.6f;
                if (needs.bodyTemperature < kColdBelow) recovery *= 0.6f;
                needs.fatigue = std::max(0.0f, needs.fatigue - recovery * dtSeconds);
            }
            else if (isCrouching)
            {
                needs.fatigue = std::max(0.0f, needs.fatigue - needs.fatigueRecoveryPerSecond * 0.5f * dtSeconds);
            }

            // --- Sleep debt (grows while awake; sleeping recovers it in Game) ---
            if (!sleeping)
            {
                needs.sleepDebt = std::min(needs.maxSleepDebt, needs.sleepDebt + needs.sleepDebtGainPerSecond * illnessMultiplier * dtSeconds);
            }

            // --- Illness: cold, starvation and dehydration wear the body down ---
            float illnessGain = 0.0f;
            if (needs.bodyTemperature < kHypothermicBelow) illnessGain += 0.02f;
            else if (needs.bodyTemperature < kColdBelow) illnessGain += 0.01f;
            if (needs.hunger < kStarvingBelow) illnessGain += 0.005f;
            if (needs.thirst < kParchedBelow) illnessGain += 0.008f;

            if (illnessGain > 0.0f)
            {
                needs.illness = std::min(100.0f, needs.illness + illnessGain * dtSeconds);
            }
            else if (needs.illness > 0.0f && needs.bodyTemperature > 36.5f &&
                     needs.hunger > 40.0f && needs.thirst > 40.0f)
            {
                const float recovery = sleeping ? 0.012f : 0.004f;
                needs.illness = std::max(0.0f, needs.illness - recovery * dtSeconds);
            }

            // --- Stamina ceiling: every need shaves the cap a little at a time ---
            if (world.hasComponent<Stamina>(entity))
            {
                Stamina& stamina = world.getComponent<Stamina>(entity);
                if (stamina.baseMaximum <= 0.0f)
                {
                    stamina.baseMaximum = std::max(1.0f, stamina.maximum);
                }

                float cap = stamina.baseMaximum;

                if (needs.hunger < kStarvingBelow) cap *= 0.65f;
                else if (needs.hunger < kHungryBelow) cap *= 0.85f;

                if (needs.thirst < kParchedBelow) cap *= 0.6f;
                else if (needs.thirst < kThirstyBelow) cap *= 0.85f;

                if (needs.fatigue > 60.0f)
                {
                    const float fatigueRatio = (needs.fatigue - 60.0f) / (needs.maxFatigue - 60.0f);
                    cap *= (1.0f - fatigueRatio * 0.5f);
                }
                if (needs.sleepDebt > 50.0f)
                {
                    const float debtRatio = (needs.sleepDebt - 50.0f) / (needs.maxSleepDebt - 50.0f);
                    cap *= (1.0f - debtRatio * 0.4f);
                }

                if (needs.bodyTemperature < kHypothermicBelow) cap *= 0.7f;
                else if (needs.bodyTemperature < kColdBelow) cap *= 0.85f;
                if (overheated) cap *= 0.85f;

                if (needs.illness > 40.0f)
                {
                    cap *= 1.0f - std::min(0.35f, (needs.illness - 40.0f) / 60.0f * 0.35f);
                }

                stamina.maximum = std::max(1.0f, cap);
                stamina.current = std::min(stamina.current, stamina.maximum);
            }

            // --- Health damage: slow countdowns, not cliffs ---
            if (world.hasComponent<Health>(entity))
            {
                Health& health = world.getComponent<Health>(entity);
                float damage = 0.0f;
                if (needs.hunger <= 0.0f) damage += kStarvationDamage;
                if (needs.thirst <= 0.0f) damage += kDehydrationDamage;
                if (needs.illness >= kGravelyIllAbove) damage += kGraveIllnessDamage;
                if (needs.bodyTemperature < kSevereHypothermiaBelow) damage += kSevereHypothermiaDamage;
                else if (needs.bodyTemperature < kHypothermicBelow) damage += kHypothermiaDamage;
                if (damage > 0.0f)
                {
                    health.current = std::max(0.0f, health.current - damage * dtSeconds);
                }
            }
        });
}
