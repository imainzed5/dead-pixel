// How the dead move: wandering (and drifting into groups), going to what
// they heard, chasing down the shared flow field, keeping their distance
// (the Screamer), and not standing in one another (phase 4).
#include "systems/ZombieAISystem.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

#include "ai/Pathfinder.h"
#include "components/Health.h"
#include "components/Transform.h"

namespace
{
constexpr float kTwoPi = 6.2831853f;

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

void stop(Velocity& v)
{
    v.dx = 0.0f;
    v.dy = 0.0f;
}
} // namespace

void ZombieAISystem::requestPath(Entity entity, ZombieAI& ai)
{
    if (ai.pathQueued) return;
    ai.pathQueued = true;
    mPathQueue.push_back(entity);
}

void ZombieAISystem::runPathQueue(World& world, const TileMap& tileMap)
{
    int done = 0;
    while (!mPathQueue.empty() && done < kPathsPerTick)
    {
        const Entity e = mPathQueue.front();
        mPathQueue.pop_front();
        if (!world.hasComponent<ZombieAI>(e) || !world.hasComponent<Transform>(e)) continue;
        ZombieAI& ai = world.getComponent<ZombieAI>(e);
        ai.pathQueued = false;
        const Transform& t = world.getComponent<Transform>(e);
        const glm::ivec2 from = worldToTile(tileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
        ai.path = Pathfinder::findPath(tileMap, from, worldToTile(tileMap, ai.targetWorld));
        ai.pathIndex = ai.path.size() >= 2 ? 1 : 0;
        ai.pathGoal = ai.targetWorld;
        ++done;
        if (ai.path.empty() && ai.state == ZombieState::Investigate)
        {
            // Unreachable: give up and wander (a horde member drops out).
            ai.state = ZombieState::Wander;
            ai.migrating = false;
            ai.horde = 0;
            ai.stateTimer = 0.0f;
        }
    }
}

bool ZombieAISystem::followPath(const Ctx& c, One& z, Velocity& v, const glm::vec2& goal, float speed)
{
    ZombieAI& ai = *z.ai;
    const TileMap& tileMap = *c.tick->tileMap;
    const bool stale = ai.path.empty() || glm::length(ai.pathGoal - goal) > c.tileSize * 0.5f;
    if (stale)
    {
        if (!ai.pathQueued)
        {
            ai.targetWorld = goal;
            requestPath(z.entity, ai);
        }
        // Waiting a tick or two for its turn: head straight there if it can.
        if (hasLineOfSight(tileMap, z.center, goal)) setVelocityToward(v, z.center, goal, speed);
        else stop(v);
        return false;
    }
    while (ai.pathIndex < ai.path.size())
    {
        const glm::vec2 waypoint = tileCenterWorld(tileMap, ai.path[ai.pathIndex]);
        const glm::vec2 toWaypoint = waypoint - z.center;
        if (glm::dot(toWaypoint, toWaypoint) < 36.0f)
        {
            ++ai.pathIndex;
            continue;
        }
        setVelocityToward(v, z.center, waypoint, speed);
        return true;
    }
    setVelocityToward(v, z.center, goal, speed);
    return true;
}

void ZombieAISystem::wander(const Ctx& c, One& z, Velocity& v)
{
    ZombieAI& ai = *z.ai;
    const ZombieTick& t = *c.tick;
    const TileMap& tileMap = *t.tileMap;
    const float dt = z.dt;

    // Drawn to a thrown thing or a flare: it stands staring at it.
    if (ai.lureTimer > 0.0f)
    {
        stop(v);
        return;
    }
    // At the end of a noise: it turns in place, looking.
    if (ai.lookAroundTimer > 0.0f)
    {
        ai.lookAroundTimer -= dt;
        const float a = std::atan2(ai.facing.y, ai.facing.x) + dt * 1.4f;
        ai.facing = glm::vec2(std::cos(a), std::sin(a));
        stop(v);
        return;
    }
    // The Unknown: by day they stand where they are.
    if (t.dormantByDay && !c.night)
    {
        stop(v);
        return;
    }
    if (ai.idleTimer > 0.0f)
    {
        ai.idleTimer -= dt;
        stop(v);
        return;
    }
    ai.lingerTimer = std::max(0.0f, ai.lingerTimer - dt);

    const glm::vec2 toTarget = ai.targetWorld - z.center;
    const bool reached = glm::dot(toTarget, toTarget) < 16.0f;
    const bool stuck = ai.stateTimer >= kWanderStuckSeconds;
    if (reached || stuck || ai.targetWorld == glm::vec2(0.0f, 0.0f))
    {
        ai.stateTimer = 0.0f;

        // Sometimes pause, like a shambling body losing interest.
        if (reached && nextRandom(ai, z.entity) < 0.3f)
        {
            ai.idleTimer = 1.0f + nextRandom(ai, z.entity) * 3.0f;
            stop(v);
            return;
        }

        // Drift the heading; turn around harder when stuck.
        const float turn = stuck ? (1.6f + nextRandom(ai, z.entity) * 3.0f)
                                 : (nextRandom(ai, z.entity) - 0.5f) * 1.8f;
        ai.heading = std::fmod(ai.heading + turn + kTwoPi, kTwoPi);

        // Crowds: near three or more others, drift to the middle of them now
        // and then; otherwise walk the way the ones close by are going.
        if (!stuck)
        {
            const float clump = t.clumpTiles * c.tileSize;
            const float near = 4.0f * c.tileSize;
            int clumpCount = 0;
            glm::vec2 centre(0.0f);
            glm::vec2 headingSum(0.0f);
            int headingCount = 0;
            mGrid.query(z.center, clump, SpatialGrid::Zombie,
                        [&](const SpatialGrid::Item& item)
                        {
                            if (item.entity == z.entity) return;
                            ++clumpCount;
                            centre += item.position;
                            if (glm::length(item.position - z.center) <= near &&
                                c.world->hasComponent<ZombieAI>(item.entity))
                            {
                                const ZombieAI& other = c.world->getComponent<ZombieAI>(item.entity);
                                if (other.state == ZombieState::Wander)
                                {
                                    headingSum += glm::vec2(std::cos(other.heading), std::sin(other.heading));
                                    ++headingCount;
                                }
                            }
                        });
            if (clumpCount >= 3 && nextRandom(ai, z.entity) < 0.3f)
            {
                const glm::vec2 toCentre = centre / static_cast<float>(clumpCount) - z.center;
                if (glm::dot(toCentre, toCentre) > c.tileSize * c.tileSize)
                    ai.heading = std::atan2(toCentre.y, toCentre.x);
            }
            else if (headingCount > 0 && glm::dot(headingSum, headingSum) > 0.01f)
            {
                ai.heading = std::atan2(headingSum.y, headingSum.x) + (nextRandom(ai, z.entity) - 0.5f) * 0.4f;
            }
        }

        // The risen stay near where they fell; the ones at a loud noise stay
        // near it while it holds them.
        const auto pullBack = [&](const glm::vec2& home, float tiles)
        {
            const glm::vec2 toHome = home - z.center;
            const float leash = tiles * c.tileSize;
            if (glm::dot(toHome, toHome) > leash * leash) ai.heading = std::atan2(toHome.y, toHome.x);
        };
        if (ai.leashTiles > 0.0f) pullBack(ai.leashHome, ai.leashTiles);
        if (ai.lingerTimer > 0.0f) pullBack(ai.lingerAt, 3.0f);

        const float stepTiles = 2.0f + nextRandom(ai, z.entity) * 3.0f;
        const glm::vec2 dir(std::cos(ai.heading), std::sin(ai.heading));
        const glm::vec2 candidate = z.center + dir * stepTiles * c.tileSize;
        const glm::ivec2 candidateTile = worldToTile(tileMap, candidate);

        if (!tileMap.inBounds(candidateTile.x, candidateTile.y) || tileMap.isSolid(candidateTile.x, candidateTile.y))
        {
            // Blocked: reverse next time and stand for a beat.
            ai.heading = std::fmod(ai.heading + 3.14159f, kTwoPi);
            ai.targetWorld = z.center;
            stop(v);
            return;
        }
        ai.targetWorld = tileCenterWorld(tileMap, candidateTile);
    }

    setVelocityToward(v, z.center, ai.targetWorld, ai.moveSpeed * kWanderSpeedFactor * z.speedMultiplier);
}

void ZombieAISystem::investigate(const Ctx& c, One& z, Velocity& v)
{
    ZombieAI& ai = *z.ai;
    const ZombieTick& t = *c.tick;
    const TileMap& tileMap = *t.tileMap;

    const glm::vec2 toGoal = ai.targetWorld - z.center;
    if (glm::dot(toGoal, toGoal) < 64.0f)
    {
        stop(v);
        // A horde member waits at the waypoint for the game to send it on.
        if (ai.horde > 0) return;
        const bool trail = ai.followingTrail;
        const bool errand = ai.migrating || ai.nightCaller;
        ai.state = ZombieState::Wander;
        ai.stateTimer = 0.0f;
        ai.migrating = false;
        ai.path.clear();
        if (trail)
        {
            // Smelling for the next drop: a short pause.
            ai.idleTimer = 0.3f;
            return;
        }
        ai.idleTimer = 0.0f;
        if (!errand)
        {
            // It looks about where the noise was; a loud one holds it there.
            ai.lookAroundTimer = 6.0f + nextRandom(ai, z.entity) * 4.0f;
            if (t.noise != nullptr)
            {
                if (const NoiseAttractor* a = t.noise->attractorNear(ai.targetWorld, c.tileSize * 3.0f))
                {
                    ai.lingerTimer = a->remainingSeconds;
                    ai.lingerAt = a->worldPosition;
                }
            }
        }
        else
        {
            ai.idleTimer = 1.0f + nextRandom(ai, z.entity) * 2.0f;
        }
        return;
    }

    float factor = ai.migrating ? kMigrateSpeedFactor : kInvestigateSpeedFactor;
    if (ai.horde > 0) factor = kMigrateSpeedFactor;
    const float speed = ai.moveSpeed * factor * z.speedMultiplier;
    // Path when the goal is far, or when walls stand between: a straight
    // line would only press the zombie into them (and a path through a
    // closed door leads it to batter the door).
    const bool far = glm::length(toGoal) > kPathfindDistanceTiles * c.tileSize ||
                     !hasLineOfSight(tileMap, z.center, ai.targetWorld);
    if (far)
    {
        followPath(c, z, v, ai.targetWorld, speed);
    }
    else
    {
        ai.path.clear();
        setVelocityToward(v, z.center, ai.targetWorld, speed);
    }

    // Give up on goals that take far too long (a boarded door, a wall of
    // structures). Its own banging must not call it straight back, so it
    // stays deaf to noise for a while. A horde and the follower keep on.
    if (ai.stateTimer > t.giveUpSeconds && ai.horde == 0 && !ai.follower)
    {
        ai.state = ZombieState::Wander;
        ai.migrating = false;
        ai.nightCaller = false;
        ai.followingTrail = false;
        ai.path.clear();
        ai.stateTimer = 0.0f;
        ai.boredTimer = kBoredSeconds;
    }
}

void ZombieAISystem::chase(const Ctx& c, One& z, Velocity& v)
{
    ZombieAI& ai = *z.ai;
    const ZombieTick& t = *c.tick;
    const TileMap& tileMap = *t.tileMap;
    stop(v);
    ai.attackWindup = 0.0f;
    if (ai.prey == kInvalidEntity)
    {
        ai.state = ZombieState::Wander;
        return;
    }

    // How fast: a Runner starts, sprints, tires, and sprints again.
    float factor = 1.0f;
    if (ai.kind == ZombieKind::Runner)
    {
        if (ai.startTimer > 0.0f)
        {
            ai.startTimer -= z.dt;
            if (ai.startTimer <= 0.0f) ai.sprintTimer = zombiekinds::kRunnerSprintSeconds;
            return; // the head is up; it has not moved yet
        }
        if (ai.sprintTimer > 0.0f)
        {
            ai.sprintTimer -= z.dt;
            factor = zombiekinds::kRunnerSprintScale;
            if (ai.sprintTimer <= 0.0f) ai.windedTimer = zombiekinds::kRunnerWindedSeconds;
        }
        else if (ai.windedTimer > 0.0f)
        {
            ai.windedTimer -= z.dt;
            factor = zombiekinds::kRunnerWindedScale;
            if (ai.windedTimer <= 0.0f) ai.sprintTimer = zombiekinds::kRunnerSprintSeconds;
        }
        else
        {
            ai.sprintTimer = zombiekinds::kRunnerSprintSeconds;
            factor = zombiekinds::kRunnerSprintScale;
        }
    }
    const float speed = ai.moveSpeed * z.speedMultiplier * factor;
    const glm::vec2 preyCenter = ai.targetWorld;

    // Close and in sight: straight at it.
    if (glm::length(preyCenter - z.center) < c.tileSize * 1.5f && hasLineOfSight(tileMap, z.center, preyCenter))
    {
        setVelocityToward(v, z.center, preyCenter, speed);
        return;
    }

    // The player: downhill on the shared field.
    if (ai.prey == t.player && mFlow.valid() && mFlow.distance(z.tile) >= 0.0f)
    {
        glm::ivec2 next;
        if (mFlow.nextStep(tileMap, z.tile, next))
        {
            setVelocityToward(v, z.center, tileCenterWorld(tileMap, next), speed);
        }
        else
        {
            setVelocityToward(v, z.center, preyCenter, speed);
        }
        return;
    }

    // Off the field, or after someone else: a path, found in its turn.
    if (ai.pathRepathTimer >= kChaseRepathSeconds)
    {
        ai.pathRepathTimer = 0.0f;
        ai.path.clear();
    }
    followPath(c, z, v, preyCenter, speed);
}

void ZombieAISystem::keepAway(const Ctx& c, One& z, Velocity& v)
{
    ZombieAI& ai = *z.ai;
    stop(v);
    const glm::vec2 away = z.center - ai.targetWorld;
    const float distance = glm::length(away) / c.tileSize;
    const float speed = ai.moveSpeed * z.speedMultiplier;
    if (distance < zombiekinds::kScreamerKeepMinTiles)
    {
        // A rush: it backs off, not quite as fast as it came.
        if (glm::dot(away, away) > 1.0f)
        {
            const glm::vec2 dir = glm::normalize(away);
            const glm::vec2 to = z.center + dir * c.tileSize;
            const glm::ivec2 toTile = worldToTile(*c.tick->tileMap, to);
            if (!c.tick->tileMap->isSolid(toTile.x, toTile.y))
            {
                v.dx = dir.x * speed * zombiekinds::kScreamerFleeScale;
                v.dy = dir.y * speed * zombiekinds::kScreamerFleeScale;
            }
        }
    }
    else if (distance > zombiekinds::kScreamerKeepMaxTiles)
    {
        chase(c, z, v);
    }
    // Facing its prey while it stands.
    if (glm::dot(away, away) > 1.0f) ai.facing = -glm::normalize(away);
}

void ZombieAISystem::separate(const Ctx& c, One& z, Velocity& v)
{
    ZombieAI& ai = *z.ai;
    // Bones and little else: nothing to push against.
    if (ai.decomp == DecompStage::Skeletal) return;
    glm::vec2 push(0.0f);
    mGrid.query(z.center, kSeparationRadius, SpatialGrid::Zombie,
                [&](const SpatialGrid::Item& item)
                {
                    if (item.entity == z.entity) return;
                    const glm::vec2 away = z.center - item.position;
                    const float d2 = glm::dot(away, away);
                    if (d2 > 0.01f)
                    {
                        const float d = std::sqrt(d2);
                        push += (away / d) * (kSeparationRadius - d);
                    }
                });
    v.dx += push.x * 4.0f;
    v.dy += push.y * 4.0f;

    // And from standing inside the player.
    if (c.playerValid)
    {
        const glm::vec2 away = z.center - c.playerCenter;
        const float d2 = glm::dot(away, away);
        if (d2 > 0.01f && d2 < kPlayerBodyRadius * kPlayerBodyRadius)
        {
            const float d = std::sqrt(d2);
            v.dx += (away.x / d) * (kPlayerBodyRadius - d) * 10.0f;
            v.dy += (away.y / d) * (kPlayerBodyRadius - d) * 10.0f;
        }
    }
}
