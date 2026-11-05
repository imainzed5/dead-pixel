#include "systems/ZombieAISystem.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "ai/Pathfinder.h"
#include "components/Health.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/Wounds.h"
#include "systems/DistrictInfectionSystem.h"

namespace
{
constexpr float kTwoPi = 6.2831853f;

glm::vec2 entityCenter(const Transform& transform)
{
    return glm::vec2(transform.x + 16.0f, transform.y + 16.0f);
}

glm::ivec2 worldToTile(const TileMap& tileMap, const glm::vec2& world)
{
    return glm::ivec2(
        static_cast<int>(std::floor(world.x / static_cast<float>(tileMap.tileWidth()))),
        static_cast<int>(std::floor(world.y / static_cast<float>(tileMap.tileHeight()))));
}

void setVelocityToward(Velocity& velocity, const glm::vec2& from, const glm::vec2& to, float speed)
{
    const glm::vec2 delta = to - from;
    const float len2 = glm::dot(delta, delta);
    if (len2 < 1.0f)
    {
        velocity.dx = 0.0f;
        velocity.dy = 0.0f;
        return;
    }
    const glm::vec2 dir = delta / std::sqrt(len2);
    velocity.dx = dir.x * speed;
    velocity.dy = dir.y * speed;
}
} // namespace

float ZombieAISystem::nextRandom(ZombieAI& zombie, Entity entity)
{
    if (zombie.rngState == 0)
    {
        zombie.rngState = 0x9E3779B9u ^ (static_cast<std::uint32_t>(entity) * 0x85EBCA6Bu);
        if (zombie.rngState == 0) zombie.rngState = 1;
    }
    // xorshift32
    std::uint32_t x = zombie.rngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    zombie.rngState = x;
    return static_cast<float>(x >> 8) / 16777216.0f;
}

void ZombieAISystem::sendTo(ZombieAI& zombie, const glm::vec2& targetWorld, bool migrating)
{
    zombie.state = ZombieState::Investigate;
    zombie.targetWorld = targetWorld;
    zombie.stateTimer = 0.0f;
    zombie.idleTimer = 0.0f;
    zombie.migrating = migrating;
    zombie.attackWindup = 0.0f;
    zombie.path.clear();
    zombie.pathIndex = 0;
}

void ZombieAISystem::update(
    World& world,
    const TileMap& tileMap,
    const NoiseModel& noiseModel,
    Entity playerEntity,
    float dtSeconds,
    double gameHours,
    int currentDay,
    const DistrictInfectionSystem* infectionSystem) const
{
    const bool isNight = (gameHours >= 20.0 || gameHours < 5.0);
    const float visionMultiplier = isNight ? (mPlayerLit ? kNightVisionLit : kNightVisionDark) : 1.0f;
    const float tileSize = static_cast<float>(tileMap.tileWidth());

    const bool playerValid =
        playerEntity != kInvalidEntity &&
        world.hasComponent<Transform>(playerEntity) &&
        world.hasComponent<Health>(playerEntity);

    Health* playerHealth = nullptr;
    glm::vec2 playerCenter(0.0f, 0.0f);

    if (playerValid)
    {
        playerHealth = &world.getComponent<Health>(playerEntity);
        playerCenter = entityCenter(world.getComponent<Transform>(playerEntity));
    }

    // Snapshot living zombie positions for separation.
    std::vector<std::pair<Entity, glm::vec2>> zombieCenters;
    world.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI&, Transform& t)
        {
            if (world.hasComponent<Health>(e) && world.getComponent<Health>(e).current <= 0.0f) return;
            zombieCenters.emplace_back(e, entityCenter(t));
        });

    world.forEach<ZombieAI, Transform, Velocity>(
        [&](Entity zombieEntity, ZombieAI& zombieAI, Transform& zombieTransform, Velocity& zombieVelocity)
        {
            if (world.hasComponent<Health>(zombieEntity) && world.getComponent<Health>(zombieEntity).current <= 0.0f)
            {
                zombieVelocity.dx = 0.0f;
                zombieVelocity.dy = 0.0f;
                return;
            }

            zombieAI.stateTimer += dtSeconds;
            zombieAI.pathRepathTimer += dtSeconds;
            zombieAI.attackCooldown = std::max(0.0f, zombieAI.attackCooldown - dtSeconds);

            // --- Decomposition stage from age ---
            const int age = currentDay - zombieAI.spawnDay;
            if (age <= 3) zombieAI.decomp = DecompStage::Fresh;
            else if (age <= 10) zombieAI.decomp = DecompStage::Bloated;
            else if (age <= 20) zombieAI.decomp = DecompStage::Decayed;
            else zombieAI.decomp = DecompStage::Skeletal;

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
            const glm::ivec2 zombieTile = worldToTile(tileMap, zombieCenter);
            const int districtId = tileMap.getDistrictId(zombieTile.x, zombieTile.y);

            float localInfection = 0.0f;
            bool overwhelmedDistrict = false;
            if (infectionSystem != nullptr && districtId > 0)
            {
                localInfection = infectionSystem->getInfection(districtId);
                overwhelmedDistrict = infectionSystem->isOverwhelmed(districtId);
            }

            const float infectionFactor = glm::clamp(localInfection / 100.0f, 0.0f, 1.0f);
            speedMultiplier *= overwhelmedDistrict ? (1.0f + infectionFactor * 0.35f) : (1.0f + infectionFactor * 0.20f);
            damageMultiplier *= overwhelmedDistrict ? (1.0f + infectionFactor * 0.25f) : (1.0f + infectionFactor * 0.12f);
            const float localVisionMultiplier = visionMultiplier * (overwhelmedDistrict
                ? (1.0f + infectionFactor * 0.65f)
                : (1.0f + infectionFactor * 0.35f));

            const float sensedNoise = noiseModel.getNoiseAtPosition(zombieCenter);
            const float investigateThreshold = std::max(0.02f, kZombieNoiseInvestigateThreshold - infectionFactor * 0.04f);
            const bool canHearNoise = sensedNoise >= investigateThreshold;

            const bool canSeePlayer = [&]()
            {
                if (!playerValid) return false;
                const float distanceTiles = glm::length(playerCenter - zombieCenter) / tileSize;
                if (distanceTiles > zombieAI.visionRangeTiles * localVisionMultiplier) return false;
                return hasLineOfSight(tileMap, zombieCenter, playerCenter);
            }();

            const float distanceToPlayer = playerValid ? glm::length(playerCenter - zombieCenter) : 999999.0f;
            const bool isInAttackRange = playerValid && distanceToPlayer <= kZombieAttackRange;

            if (canSeePlayer)
            {
                if (zombieAI.state != ZombieState::Chase && zombieAI.state != ZombieState::Attack)
                {
                    zombieAI.pathRepathTimer = kZombieRepathSeconds; // repath immediately
                }
                // A lunge in progress finishes even if the player steps back.
                const bool lunging = zombieAI.state == ZombieState::Attack && zombieAI.attackWindup > 0.0f;
                zombieAI.state = (isInAttackRange || lunging) ? ZombieState::Attack : ZombieState::Chase;
                zombieAI.targetWorld = playerCenter;
                zombieAI.stateTimer = 0.0f;
                zombieAI.migrating = false;
                zombieAI.idleTimer = 0.0f;
            }
            else if (zombieAI.state == ZombieState::Chase)
            {
                // Lost sight: go to the last known position.
                sendTo(zombieAI, zombieAI.targetWorld, false);
            }
            else if (canHearNoise && (zombieAI.state == ZombieState::Wander || zombieAI.state == ZombieState::Investigate))
            {
                const NoiseEvent* bestEvent = nullptr;
                float bestDistanceSq = 0.0f;
                for (const NoiseEvent& event : noiseModel.activeEvents())
                {
                    const float radiusWorld = static_cast<float>(event.radiusTiles) * tileSize;
                    const glm::vec2 toEvent = event.worldPosition - zombieCenter;
                    const float distSq = glm::dot(toEvent, toEvent);
                    if (distSq > radiusWorld * radiusWorld) continue;
                    if (bestEvent == nullptr || distSq < bestDistanceSq)
                    {
                        bestEvent = &event;
                        bestDistanceSq = distSq;
                    }
                }

                // Re-target only when the noise is meaningfully somewhere else.
                if (bestEvent != nullptr &&
                    (zombieAI.state == ZombieState::Wander ||
                     glm::length(bestEvent->worldPosition - zombieAI.targetWorld) > tileSize * 2.0f))
                {
                    sendTo(zombieAI, bestEvent->worldPosition, false);
                }
            }

            switch (zombieAI.state)
            {
            case ZombieState::Wander:
            {
                if (zombieAI.idleTimer > 0.0f)
                {
                    zombieAI.idleTimer -= dtSeconds;
                    zombieVelocity.dx = 0.0f;
                    zombieVelocity.dy = 0.0f;
                    break;
                }

                const glm::vec2 toTarget = zombieAI.targetWorld - zombieCenter;
                const bool reached = glm::dot(toTarget, toTarget) < 16.0f;
                const bool stuck = zombieAI.stateTimer >= kWanderStuckSeconds;
                if (reached || stuck || zombieAI.targetWorld == glm::vec2(0.0f, 0.0f))
                {
                    zombieAI.stateTimer = 0.0f;

                    // Sometimes pause, like a shambling body losing interest.
                    if (reached && nextRandom(zombieAI, zombieEntity) < 0.3f)
                    {
                        zombieAI.idleTimer = 1.0f + nextRandom(zombieAI, zombieEntity) * 3.0f;
                        zombieVelocity.dx = 0.0f;
                        zombieVelocity.dy = 0.0f;
                        break;
                    }

                    // Drift the heading; turn around harder when stuck.
                    const float turn = stuck ? (1.6f + nextRandom(zombieAI, zombieEntity) * 3.0f)
                                             : (nextRandom(zombieAI, zombieEntity) - 0.5f) * 1.8f;
                    zombieAI.heading = std::fmod(zombieAI.heading + turn + kTwoPi, kTwoPi);

                    if (zombieAI.leashTiles > 0.0f)
                    {
                        const glm::vec2 toHome = zombieAI.leashHome - zombieCenter;
                        const float leash = zombieAI.leashTiles * tileSize;
                        if (glm::dot(toHome, toHome) > leash * leash)
                        {
                            zombieAI.heading = std::atan2(toHome.y, toHome.x);
                        }
                    }

                    const float stepTiles = 2.0f + nextRandom(zombieAI, zombieEntity) * 3.0f;
                    const glm::vec2 dir(std::cos(zombieAI.heading), std::sin(zombieAI.heading));
                    glm::vec2 candidate = zombieCenter + dir * stepTiles * tileSize;
                    const glm::ivec2 candidateTile = worldToTile(tileMap, candidate);

                    if (!tileMap.inBounds(candidateTile.x, candidateTile.y) || tileMap.isSolid(candidateTile.x, candidateTile.y))
                    {
                        // Blocked: reverse next time and stand for a beat.
                        zombieAI.heading = std::fmod(zombieAI.heading + 3.14159f, kTwoPi);
                        zombieAI.targetWorld = zombieCenter;
                        zombieVelocity.dx = 0.0f;
                        zombieVelocity.dy = 0.0f;
                        break;
                    }
                    zombieAI.targetWorld = tileCenterWorld(tileMap, candidateTile);
                }

                setVelocityToward(zombieVelocity, zombieCenter, zombieAI.targetWorld,
                                  zombieAI.moveSpeed * kWanderSpeedFactor * speedMultiplier);
                break;
            }

            case ZombieState::Investigate:
            {
                const glm::vec2 toGoal = zombieAI.targetWorld - zombieCenter;
                if (glm::dot(toGoal, toGoal) < 64.0f)
                {
                    zombieAI.state = ZombieState::Wander;
                    zombieAI.stateTimer = 0.0f;
                    zombieAI.migrating = false;
                    zombieAI.path.clear();
                    zombieAI.idleTimer = 1.0f + nextRandom(zombieAI, zombieEntity) * 2.0f;
                    zombieVelocity.dx = 0.0f;
                    zombieVelocity.dy = 0.0f;
                    break;
                }

                const float speedFactor = zombieAI.migrating ? kMigrateSpeedFactor : kInvestigateSpeedFactor;
                const float speed = zombieAI.moveSpeed * speedFactor * speedMultiplier;
                const bool far = glm::length(toGoal) > kPathfindDistanceTiles * tileSize;

                if (far)
                {
                    const bool needPath = zombieAI.path.empty() ||
                        glm::length(zombieAI.pathGoal - zombieAI.targetWorld) > tileSize * 0.5f;
                    if (needPath)
                    {
                        zombieAI.path = Pathfinder::findPath(tileMap, zombieTile, worldToTile(tileMap, zombieAI.targetWorld));
                        zombieAI.pathIndex = zombieAI.path.size() >= 2 ? 1 : 0;
                        zombieAI.pathGoal = zombieAI.targetWorld;
                        if (zombieAI.path.empty())
                        {
                            // Unreachable: give up and wander.
                            zombieAI.state = ZombieState::Wander;
                            zombieAI.migrating = false;
                            zombieAI.stateTimer = 0.0f;
                            break;
                        }
                    }

                    while (zombieAI.pathIndex < zombieAI.path.size())
                    {
                        const glm::vec2 waypoint = tileCenterWorld(tileMap, zombieAI.path[zombieAI.pathIndex]);
                        const glm::vec2 toWaypoint = waypoint - zombieCenter;
                        if (glm::dot(toWaypoint, toWaypoint) < 36.0f)
                        {
                            ++zombieAI.pathIndex;
                            continue;
                        }
                        setVelocityToward(zombieVelocity, zombieCenter, waypoint, speed);
                        break;
                    }
                    if (zombieAI.pathIndex >= zombieAI.path.size())
                    {
                        setVelocityToward(zombieVelocity, zombieCenter, zombieAI.targetWorld, speed);
                    }
                }
                else
                {
                    zombieAI.path.clear();
                    setVelocityToward(zombieVelocity, zombieCenter, zombieAI.targetWorld, speed);
                }

                // Give up on goals that take far too long (blocked by structures, etc.).
                if (zombieAI.stateTimer > 90.0f)
                {
                    zombieAI.state = ZombieState::Wander;
                    zombieAI.migrating = false;
                    zombieAI.path.clear();
                    zombieAI.stateTimer = 0.0f;
                }
                break;
            }

            case ZombieState::Chase:
            {
                zombieVelocity.dx = 0.0f;
                zombieVelocity.dy = 0.0f;
                zombieAI.attackWindup = 0.0f;

                if (!playerValid)
                {
                    zombieAI.state = ZombieState::Wander;
                    break;
                }

                if (zombieAI.pathRepathTimer >= kZombieRepathSeconds)
                {
                    zombieAI.pathRepathTimer = 0.0f;
                    const std::vector<glm::ivec2> path =
                        Pathfinder::findPath(tileMap, zombieTile, worldToTile(tileMap, playerCenter));
                    zombieAI.targetWorld = path.size() >= 2 ? tileCenterWorld(tileMap, path[1]) : playerCenter;
                }

                // With a clear line, close in directly instead of tile-stepping.
                const glm::vec2 chaseTarget = glm::length(playerCenter - zombieCenter) < tileSize * 1.5f
                    ? playerCenter
                    : zombieAI.targetWorld;
                setVelocityToward(zombieVelocity, zombieCenter, chaseTarget, zombieAI.moveSpeed * speedMultiplier);
                break;
            }

            case ZombieState::Attack:
            {
                zombieVelocity.dx = 0.0f;
                zombieVelocity.dy = 0.0f;

                // Mid-lunge the zombie commits; otherwise it follows a retreating player.
                if (zombieAI.attackWindup <= 0.0f && (!playerValid || !isInAttackRange))
                {
                    zombieAI.state = ZombieState::Chase;
                    zombieAI.pathRepathTimer = kZombieRepathSeconds;
                    zombieAI.stateTimer = 0.0f;
                    break;
                }

                if (zombieAI.attackWindup <= 0.0f)
                {
                    if (zombieAI.attackCooldown <= 0.0f)
                    {
                        zombieAI.attackWindup = kZombieAttackWindupSeconds;
                    }
                    break;
                }

                zombieAI.attackWindup -= dtSeconds;
                if (zombieAI.attackWindup > 0.0f)
                {
                    break;
                }

                // Resolve the lunge.
                zombieAI.attackWindup = 0.0f;
                zombieAI.attackCooldown = kZombieAttackCooldownSeconds * (0.85f + nextRandom(zombieAI, zombieEntity) * 0.3f);
                const bool stillInReach = playerValid && distanceToPlayer <= kZombieAttackRange * kZombieAttackReachScale;
                if (!stillInReach || playerHealth == nullptr || nextRandom(zombieAI, zombieEntity) >= kZombieHitChance)
                {
                    break; // dodged or missed
                }

                playerHealth->current = std::max(0.0f, playerHealth->current - kZombieAttackDamage * damageMultiplier);

                if (world.hasComponent<Wounds>(playerEntity) && nextRandom(zombieAI, zombieEntity) < kZombieWoundChance)
                {
                    Wounds& wounds = world.getComponent<Wounds>(playerEntity);
                    Wound w{};
                    const float roll = nextRandom(zombieAI, zombieEntity);
                    if (roll < kZombieBiteShare)
                    {
                        w.type = WoundType::Bite;
                        w.contaminated = nextRandom(zombieAI, zombieEntity) < kBiteContaminationChance;
                    }
                    else if (roll < kZombieBiteShare + 0.4f)
                    {
                        w.type = WoundType::Cut;
                    }
                    else
                    {
                        w.type = WoundType::Bruise;
                    }
                    w.severity = 0.5f + nextRandom(zombieAI, zombieEntity) * 0.5f;
                    wounds.active.push_back(w);
                }
                break;
            }
            }

            // Separation: keep bodies from stacking into one sprite.
            glm::vec2 push(0.0f);
            for (const auto& [other, otherCenter] : zombieCenters)
            {
                if (other == zombieEntity) continue;
                const glm::vec2 away = zombieCenter - otherCenter;
                const float d2 = glm::dot(away, away);
                if (d2 > 0.01f && d2 < kSeparationRadius * kSeparationRadius)
                {
                    const float d = std::sqrt(d2);
                    push += (away / d) * (kSeparationRadius - d);
                }
            }
            zombieVelocity.dx += push.x * 4.0f;
            zombieVelocity.dy += push.y * 4.0f;
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
