#include "systems/SurvivorAISystem.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "components/Health.h"
#include "components/SurvivorAI.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"
#include "components/Interactable.h"

namespace
{
constexpr float kSpriteHalf = 16.0f;

glm::vec2 entityCenter(const Transform& t)
{
    return glm::vec2(t.x + kSpriteHalf, t.y + kSpriteHalf);
}

float urgency(float need, float threshold)
{
    if (need >= threshold) return 0.0f;
    return 1.0f - (need / threshold);
}
}

void SurvivorAISystem::update(World& world, const TileMap& tileMap, const NoiseModel& /*noiseModel*/,
                               Entity playerEntity, float dtSeconds, double /*gameHours*/, int /*currentDay*/) const
{
    world.forEach<SurvivorAI, Transform, Velocity, Health>(
        [&](Entity entity, SurvivorAI& ai, Transform& transform, Velocity& vel, Health& health)
        {
            if (!ai.alive || health.current <= 0.0f)
            {
                ai.alive = false;
                ai.state = SurvivorState::Dead;
                vel.dx = 0.0f;
                vel.dy = 0.0f;
                return;
            }

            // --- Needs decay ---
            ai.hunger = std::max(0.0f, ai.hunger - kNeedsDecayHunger * dtSeconds);
            ai.thirst = std::max(0.0f, ai.thirst - kNeedsDecayThirst * dtSeconds);

            if (ai.hunger <= 0.0f)
                health.current -= kStarvationDamage * dtSeconds;
            if (ai.thirst <= 0.0f)
                health.current -= kDehydrationDamage * dtSeconds;

            health.current = std::max(0.0f, health.current);

            // --- Attitude update based on desperation ---
            const float desperationScore = urgency(ai.hunger, 40.0f) + urgency(ai.thirst, 40.0f);
            if (ai.personality.aggression > 0.7f)
            {
                ai.attitude = SurvivorAttitude::Hostile;
            }
            else if (desperationScore > 1.2f)
            {
                ai.attitude = SurvivorAttitude::Desperate;
            }
            else if (ai.personality.cooperation > 0.6f)
            {
                ai.attitude = SurvivorAttitude::Organised;
            }
            else
            {
                ai.attitude = SurvivorAttitude::Neutral;
            }

            // --- Decision cycle ---
            ai.decisionTimer -= dtSeconds;
            if (ai.decisionTimer > 0.0f && ai.state != SurvivorState::Fight && ai.state != SurvivorState::Flee)
            {
                // Continue current behavior between decisions
            }
            else
            {
                ai.decisionTimer = kDecisionInterval;

                const glm::vec2 myCenter = entityCenter(transform);

                // 1. Assess threats: find nearest zombie
                float nearestZombieDist2 = kFleeRange * kFleeRange;
                glm::vec2 nearestZombiePos{0.0f, 0.0f};
                bool zombieNearby = false;
                int zombieCount = 0;

                world.forEach<ZombieAI, Transform, Health>(
                    [&](Entity, ZombieAI& zai, Transform& zt, Health& zh)
                    {
                        if (zh.current <= 0.0f) return;
                        if (zai.state == ZombieState::Wander) {} // check all
                        const glm::vec2 zc = entityCenter(zt);
                        const float d2 = glm::dot(zc - myCenter, zc - myCenter);
                        if (d2 < kFleeRange * kFleeRange)
                        {
                            ++zombieCount;
                            if (d2 < nearestZombieDist2)
                            {
                                nearestZombieDist2 = d2;
                                nearestZombiePos = zc;
                                zombieNearby = true;
                            }
                        }
                    });

                // 2. Decide
                if (zombieNearby)
                {
                    const float d = std::sqrt(nearestZombieDist2);
                    if (d < kAttackRange * 2.0f && ai.personality.aggression > 0.4f && zombieCount <= 2)
                    {
                        ai.state = SurvivorState::Fight;
                        ai.targetWorld = nearestZombiePos;
                    }
                    else
                    {
                        ai.state = SurvivorState::Flee;
                        // Flee away from zombie
                        const glm::vec2 away = myCenter - nearestZombiePos;
                        const float len = glm::length(away);
                        if (len > 0.001f)
                            ai.targetWorld = myCenter + (away / len) * kFleeRange;
                        else
                            ai.targetWorld = myCenter + glm::vec2(kFleeRange, 0.0f);
                    }
                }
                else if (ai.hunger < 30.0f || ai.thirst < 30.0f)
                {
                    ai.state = SurvivorState::Scavenge;
                    // Move toward a random nearby position to "search"
                    const float angle = static_cast<float>(entity % 360) * 0.0174533f + dtSeconds;
                    ai.targetWorld = myCenter + glm::vec2(std::cos(angle), std::sin(angle)) * kScavengeRange * 0.5f;
                }
                else
                {
                    ai.state = SurvivorState::Wander;
                    const float angle = static_cast<float>((entity * 7 + 13) % 360) * 0.0174533f;
                    ai.targetWorld = myCenter + glm::vec2(std::cos(angle), std::sin(angle)) * kWanderRadius;
                }
            }

            // --- Execute current state ---
            ai.stateTimer += dtSeconds;
            ai.attackCooldown = std::max(0.0f, ai.attackCooldown - dtSeconds);

            const glm::vec2 myCenter = entityCenter(transform);
            const glm::vec2 toTarget = ai.targetWorld - myCenter;
            const float distToTarget = glm::length(toTarget);

            switch (ai.state)
            {
            case SurvivorState::Wander:
            case SurvivorState::Scavenge:
            case SurvivorState::Flee:
            {
                if (distToTarget > 4.0f)
                {
                    const glm::vec2 dir = toTarget / distToTarget;
                    const float speed = (ai.state == SurvivorState::Flee) ? ai.moveSpeed * 1.3f : ai.moveSpeed;
                    vel.dx = dir.x * speed;
                    vel.dy = dir.y * speed;
                }
                else
                {
                    vel.dx = 0.0f;
                    vel.dy = 0.0f;
                    if (ai.state == SurvivorState::Flee)
                        ai.state = SurvivorState::Wander;
                }
                break;
            }
            case SurvivorState::Fight:
            {
                if (distToTarget > kAttackRange)
                {
                    const glm::vec2 dir = toTarget / distToTarget;
                    vel.dx = dir.x * ai.moveSpeed;
                    vel.dy = dir.y * ai.moveSpeed;
                }
                else
                {
                    vel.dx = 0.0f;
                    vel.dy = 0.0f;

                    if (ai.attackCooldown <= 0.0f)
                    {
                        // Attack nearest zombie
                        world.forEach<ZombieAI, Transform, Health>(
                            [&](Entity, ZombieAI&, Transform& zt, Health& zh)
                            {
                                if (zh.current <= 0.0f) return;
                                const glm::vec2 zc = entityCenter(zt);
                                if (glm::distance(myCenter, zc) < kAttackRange * 1.5f)
                                {
                                    zh.current -= kAttackDamage;
                                    ai.attackCooldown = kAttackCooldown;
                                }
                            });
                    }
                }
                break;
            }
            case SurvivorState::Sleep:
            {
                vel.dx = 0.0f;
                vel.dy = 0.0f;
                ai.fatigue = std::max(0.0f, ai.fatigue - 5.0f * dtSeconds);
                if (ai.fatigue <= 0.0f)
                    ai.state = SurvivorState::Wander;
                break;
            }
            case SurvivorState::Trade:
            case SurvivorState::Idle:
            {
                vel.dx = 0.0f;
                vel.dy = 0.0f;
                break;
            }
            case SurvivorState::Dead:
                vel.dx = 0.0f;
                vel.dy = 0.0f;
                break;
            }

            // Clamp position to map bounds
            const float maxX = static_cast<float>(tileMap.pixelSize().x - 32);
            const float maxY = static_cast<float>(tileMap.pixelSize().y - 32);
            transform.x = std::clamp(transform.x, 0.0f, maxX);
            transform.y = std::clamp(transform.y, 0.0f, maxY);
        });
}
