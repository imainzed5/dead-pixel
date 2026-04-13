#include "systems/ZombieAISystem.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

#include "ai/Pathfinder.h"
#include "components/Health.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/Wounds.h"
#include "components/ZombieAI.h"

namespace
{
glm::vec2 entityCenter(const Transform& transform)
{
    return glm::vec2(transform.x + 16.0f, transform.y + 16.0f);
}
}

void ZombieAISystem::update(
    World& world,
    const TileMap& tileMap,
    const NoiseModel& noiseModel,
    Entity playerEntity,
    float dtSeconds,
    double gameHours,
    int currentDay) const
{
    const bool isNight = (gameHours >= 20.0 || gameHours < 5.0);
    const float visionMultiplier = isNight ? 1.5f : 1.0f;

    const bool playerValid =
        playerEntity != kInvalidEntity &&
        world.hasComponent<Transform>(playerEntity) &&
        world.hasComponent<Health>(playerEntity);

    Transform* playerTransform = nullptr;
    Health* playerHealth = nullptr;
    glm::vec2 playerCenter(0.0f, 0.0f);

    if (playerValid)
    {
        playerTransform = &world.getComponent<Transform>(playerEntity);
        playerHealth = &world.getComponent<Health>(playerEntity);
        playerCenter = entityCenter(*playerTransform);
    }

    world.forEach<ZombieAI, Transform, Velocity>(
        [&](Entity zombieEntity, ZombieAI& zombieAI, Transform& zombieTransform, Velocity& zombieVelocity)
        {
            zombieAI.stateTimer += dtSeconds;
            zombieAI.pathRepathTimer += dtSeconds;
            zombieAI.attackCooldown = std::max(0.0f, zombieAI.attackCooldown - dtSeconds);

            // --- Decomposition stage from age ---
            const int age = currentDay - zombieAI.spawnDay;
            if (age <= 3) zombieAI.decomp = DecompStage::Fresh;
            else if (age <= 10) zombieAI.decomp = DecompStage::Bloated;
            else if (age <= 20) zombieAI.decomp = DecompStage::Decayed;
            else zombieAI.decomp = DecompStage::Skeletal;

            // Decomposition modifiers: speed and damage
            float speedMultiplier = 1.0f;
            float damageMultiplier = 1.0f;
            switch (zombieAI.decomp)
            {
            case DecompStage::Fresh:    speedMultiplier = 1.0f;  damageMultiplier = 1.0f;  break;
            case DecompStage::Bloated:  speedMultiplier = 0.75f; damageMultiplier = 0.9f;  break;
            case DecompStage::Decayed:  speedMultiplier = 0.5f;  damageMultiplier = 0.6f;  break;
            case DecompStage::Skeletal: speedMultiplier = 0.3f;  damageMultiplier = 0.35f; break;
            }

            const glm::vec2 zombieCenter = entityCenter(zombieTransform);

            const float sensedNoise = noiseModel.getNoiseAtPosition(zombieCenter);
            const bool canHearNoise = sensedNoise >= kZombieNoiseInvestigateThreshold;

            const bool canSeePlayer = [&]()
            {
                if (!playerValid)
                {
                    return false;
                }

                const glm::vec2 delta = playerCenter - zombieCenter;
                const float distanceTiles = glm::length(delta) / static_cast<float>(tileMap.tileWidth());
                if (distanceTiles > zombieAI.visionRangeTiles * visionMultiplier)
                {
                    return false;
                }

                return hasLineOfSight(tileMap, zombieCenter, playerCenter);
            }();

            const float distanceToPlayer = playerValid ? glm::length(playerCenter - zombieCenter) : 999999.0f;
            const bool isInAttackRange = playerValid && distanceToPlayer <= kZombieAttackRange;

            if (canSeePlayer)
            {
                zombieAI.state = isInAttackRange ? ZombieState::Attack : ZombieState::Chase;
                zombieAI.targetWorld = playerCenter;
                zombieAI.stateTimer = 0.0f;
            }
            else if (zombieAI.state == ZombieState::Chase && !canSeePlayer)
            {
                zombieAI.state = ZombieState::Investigate;
                zombieAI.stateTimer = 0.0f;
            }
            else if (canHearNoise && (zombieAI.state == ZombieState::Wander || zombieAI.state == ZombieState::Investigate))
            {
                const auto& events = noiseModel.activeEvents();
                const NoiseEvent* bestEvent = nullptr;
                float bestDistanceSq = 0.0f;

                for (const NoiseEvent& event : events)
                {
                    const float radiusWorld = static_cast<float>(event.radiusTiles * tileMap.tileWidth());
                    const glm::vec2 toEvent = event.worldPosition - zombieCenter;
                    const float distSq = glm::dot(toEvent, toEvent);
                    if (distSq > radiusWorld * radiusWorld)
                    {
                        continue;
                    }

                    if (bestEvent == nullptr || distSq < bestDistanceSq)
                    {
                        bestEvent = &event;
                        bestDistanceSq = distSq;
                    }
                }

                if (bestEvent != nullptr)
                {
                    zombieAI.state = ZombieState::Investigate;
                    zombieAI.targetWorld = bestEvent->worldPosition;
                }
            }

            switch (zombieAI.state)
            {
            case ZombieState::Wander:
            {
                if (zombieAI.stateTimer >= 1.3f || zombieAI.targetWorld == glm::vec2(0.0f, 0.0f))
                {
                    const glm::ivec2 selfTile(
                        static_cast<int>(zombieCenter.x / static_cast<float>(tileMap.tileWidth())),
                        static_cast<int>(zombieCenter.y / static_cast<float>(tileMap.tileHeight())));

                    // Deterministic pseudo-random selection around each zombie using its entity ID and timer.
                    const int selector = static_cast<int>(zombieEntity * 13 + zombieAI.stateTimer * 1000.0f) % 16;
                    const glm::ivec2 offsets[16] = {
                        glm::ivec2(2, 0), glm::ivec2(-2, 0), glm::ivec2(0, 2), glm::ivec2(0, -2),
                        glm::ivec2(3, 1), glm::ivec2(-3, -1), glm::ivec2(1, 3), glm::ivec2(-1, -3),
                        glm::ivec2(4, 0), glm::ivec2(0, 4), glm::ivec2(-4, 0), glm::ivec2(0, -4),
                        glm::ivec2(2, 2), glm::ivec2(-2, 2), glm::ivec2(2, -2), glm::ivec2(-2, -2)};

                    glm::ivec2 targetTile = selfTile + offsets[selector];
                    targetTile.x = glm::clamp(targetTile.x, 0, tileMap.width() - 1);
                    targetTile.y = glm::clamp(targetTile.y, 0, tileMap.height() - 1);

                    if (!tileMap.isSolid(targetTile.x, targetTile.y))
                    {
                        zombieAI.targetWorld = tileCenterWorld(tileMap, targetTile);
                    }

                    zombieAI.stateTimer = 0.0f;
                }

                const glm::vec2 toTarget = zombieAI.targetWorld - zombieCenter;
                if (glm::dot(toTarget, toTarget) < 9.0f)
                {
                    zombieVelocity.dx = 0.0f;
                    zombieVelocity.dy = 0.0f;
                }
                else
                {
                    const glm::vec2 moveDir = glm::normalize(toTarget);
                    zombieVelocity.dx = moveDir.x * zombieAI.moveSpeed * 0.55f * speedMultiplier;
                    zombieVelocity.dy = moveDir.y * zombieAI.moveSpeed * 0.55f * speedMultiplier;
                }
                break;
            }

            case ZombieState::Investigate:
            {
                const glm::vec2 toTarget = zombieAI.targetWorld - zombieCenter;
                if (glm::dot(toTarget, toTarget) < 25.0f)
                {
                    zombieAI.state = ZombieState::Wander;
                    zombieAI.stateTimer = 0.0f;
                    zombieVelocity.dx = 0.0f;
                    zombieVelocity.dy = 0.0f;
                }
                else
                {
                    const glm::vec2 moveDir = glm::normalize(toTarget);
                    zombieVelocity.dx = moveDir.x * zombieAI.moveSpeed * 0.8f * speedMultiplier;
                    zombieVelocity.dy = moveDir.y * zombieAI.moveSpeed * 0.8f * speedMultiplier;
                }
                break;
            }

            case ZombieState::Chase:
            {
                zombieVelocity.dx = 0.0f;
                zombieVelocity.dy = 0.0f;

                if (!playerValid)
                {
                    zombieAI.state = ZombieState::Wander;
                    break;
                }

                if (zombieAI.pathRepathTimer >= kZombieRepathSeconds)
                {
                    zombieAI.pathRepathTimer = 0.0f;

                    const glm::ivec2 startTile(
                        static_cast<int>(zombieCenter.x / static_cast<float>(tileMap.tileWidth())),
                        static_cast<int>(zombieCenter.y / static_cast<float>(tileMap.tileHeight())));
                    const glm::ivec2 goalTile(
                        static_cast<int>(playerCenter.x / static_cast<float>(tileMap.tileWidth())),
                        static_cast<int>(playerCenter.y / static_cast<float>(tileMap.tileHeight())));

                    const std::vector<glm::ivec2> path = Pathfinder::findPath(tileMap, startTile, goalTile);
                    if (path.size() >= 2)
                    {
                        zombieAI.targetWorld = tileCenterWorld(tileMap, path[1]);
                    }
                    else
                    {
                        zombieAI.targetWorld = playerCenter;
                    }
                }

                const glm::vec2 toTarget = zombieAI.targetWorld - zombieCenter;
                if (glm::dot(toTarget, toTarget) > 1.0f)
                {
                    const glm::vec2 moveDir = glm::normalize(toTarget);
                    zombieVelocity.dx = moveDir.x * zombieAI.moveSpeed * speedMultiplier;
                    zombieVelocity.dy = moveDir.y * zombieAI.moveSpeed * speedMultiplier;
                }

                break;
            }

            case ZombieState::Attack:
            {
                zombieVelocity.dx = 0.0f;
                zombieVelocity.dy = 0.0f;

                if (!playerValid || !isInAttackRange)
                {
                    zombieAI.state = ZombieState::Investigate;
                    zombieAI.stateTimer = 0.0f;
                    break;
                }

                if (zombieAI.attackCooldown <= 0.0f && playerHealth != nullptr)
                {
                    playerHealth->current = std::max(0.0f, playerHealth->current - kZombieAttackDamage * damageMultiplier);
                    zombieAI.attackCooldown = kZombieAttackCooldownSeconds;

                    // Inflict wound on player
                    if (world.hasComponent<Wounds>(playerEntity))
                    {
                        Wounds& wounds = world.getComponent<Wounds>(playerEntity);
                        Wound w{};
                        // Random wound type: 50% bite, 30% cut, 20% bruise
                        const float r = static_cast<float>(zombieAI.attackCooldown * 100.0f);
                        const int roll = static_cast<int>(r) % 10;
                        if (roll < 5) w.type = WoundType::Bite;
                        else if (roll < 8) w.type = WoundType::Cut;
                        else w.type = WoundType::Bruise;
                        w.severity = 0.5f + (static_cast<float>(roll) / 20.0f);
                        wounds.active.push_back(w);
                    }
                }
                break;
            }
            }
        });
}

glm::vec2 ZombieAISystem::tileCenterWorld(const TileMap& tileMap, const glm::ivec2& tile)
{
    return glm::vec2(
        (static_cast<float>(tile.x) + 0.5f) * static_cast<float>(tileMap.tileWidth()),
        (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(tileMap.tileHeight()));
}

bool ZombieAISystem::hasLineOfSight(const TileMap& tileMap, const glm::vec2& worldStart, const glm::vec2& worldEnd)
{
    const glm::vec2 delta = worldEnd - worldStart;
    const float distance = glm::length(delta);
    if (distance < 0.001f)
    {
        return true;
    }

    const float stepSize = static_cast<float>(tileMap.tileWidth()) * 0.4f;
    const int steps = std::max(1, static_cast<int>(distance / stepSize));

    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const glm::vec2 p = worldStart + delta * t;

        const int tileX = static_cast<int>(std::floor(p.x / static_cast<float>(tileMap.tileWidth())));
        const int tileY = static_cast<int>(std::floor(p.y / static_cast<float>(tileMap.tileHeight())));

        if (tileMap.isSolid(tileX, tileY))
        {
            return false;
        }
    }

    return true;
}
