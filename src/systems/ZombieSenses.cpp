// What the dead see, hear and smell (phase 4), and how that turns them.
#include "systems/ZombieAISystem.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

#include "components/Health.h"
#include "components/Transform.h"
#include "world/BloodTrail.h"

namespace
{
glm::vec2 entityCenter(const Transform& transform)
{
    return glm::vec2(transform.x + 16.0f, transform.y + 16.0f);
}
} // namespace

void ZombieAISystem::see(const Ctx& c, One& z)
{
    ZombieAI& ai = *z.ai;
    const ZombieTick& t = *c.tick;
    World& world = *c.world;
    const TileMap& tileMap = *t.tileMap;
    ai.seesPrey = false;

    float extra = z.visionMultiplier;
    if (t.blindHearing) extra *= 0.3f;
    if (t.dormantByDay && !c.night) extra *= 0.5f;
    if (t.rainCalm && t.raining) extra *= 0.5f;
    const bool alerted = ai.state == ZombieState::Chase || ai.state == ZombieState::Attack || ai.awareness >= 1.0f;

    if (c.playerValid)
    {
        senses::SightInput in;
        in.baseTiles = ai.visionRangeTiles;
        in.dark = c.night;
        in.playerLit = t.playerLit;
        in.weatherVisibility = t.visibility;
        in.crouched = t.playerCrouched;
        in.moving = t.playerMoving;
        in.extraScale = extra;
        const float range = senses::sightRangeTiles(in);
        const glm::vec2 toPlayer = c.playerCenter - z.center;
        const float distance = glm::length(toPlayer) / c.tileSize;
        if (distance <= range && (alerted || senses::inCone(ai.facing, toPlayer)) &&
            hasLineOfSight(tileMap, z.center, c.playerCenter))
        {
            ai.seesPrey = true;
            ai.prey = t.player;
            ai.seenDistanceTiles = distance;
            ai.seenRangeTiles = range;
            ai.seenAt = c.playerCenter;
            return;
        }
    }

    // The other living: no crouching or lights to read, just the dark. They
    // have lived this long by keeping out of sight: seen at 0.6 of the range.
    const float range = ai.visionRangeTiles * extra * (c.night ? senses::kDarkScale : 1.0f) * t.visibility * 0.6f;
    float best = range + 1.0f;
    mGrid.query(z.center, range * c.tileSize, SpatialGrid::Survivor,
                [&](const SpatialGrid::Item& item)
                {
                    const glm::vec2 to = item.position - z.center;
                    const float d = glm::length(to) / c.tileSize;
                    if (d >= best) return;
                    if (!alerted && !senses::inCone(ai.facing, to)) return;
                    if (!hasLineOfSight(tileMap, z.center, item.position)) return;
                    best = d;
                    ai.seesPrey = true;
                    ai.prey = item.entity;
                    ai.seenDistanceTiles = d;
                    ai.seenRangeTiles = range;
                    ai.seenAt = item.position;
                });
    (void)world;
}

void ZombieAISystem::hear(const Ctx& c, One& z)
{
    ZombieAI& ai = *z.ai;
    const ZombieTick& t = *c.tick;
    if (t.noise == nullptr) return;
    if (ai.state != ZombieState::Wander && ai.state != ZombieState::Investigate) return;
    if (ai.boredTimer > 0.0f) return;

    bool listenerKnown = false;
    senses::Shell listener;
    const NoiseEvent* bestEvent = nullptr;
    int bestTier = 0;
    float bestDistance = 0.0f;
    const float hearing = ai.hearing * (t.blindHearing ? 2.0f : 1.0f);
    for (const NoiseEvent& event : t.noise->activeEvents())
    {
        // The dead pay no mind to their own, but for a scream or a groan.
        if (event.source == NoiseSource::Zombie) continue;
        // A horde on the move does not stop for footsteps; nor do the dead
        // calmed by rain (The Unknown).
        if (ai.horde > 0 && static_cast<int>(event.tier) <= static_cast<int>(NoiseTier::Soft)) continue;
        if (t.rainCalm && t.raining && static_cast<int>(event.tier) <= static_cast<int>(NoiseTier::Soft)) continue;
        const glm::vec2 toListener = z.center - event.worldPosition;
        const float distance = glm::length(toListener);
        // Quick reject before the shells: nothing carries past this.
        const float maxRadius = static_cast<float>(NoiseModel::tierRadiusTiles(event.tier)) * 1.2f * hearing * t.hearingScale * c.tileSize;
        if (distance > maxRadius) continue;
        if (!listenerKnown)
        {
            listener = t.shellAt ? t.shellAt(z.tile) : senses::Shell{};
            listenerKnown = true;
        }
        const senses::Shell source{event.building, event.sealed};
        const float wind = senses::windScale(event.tier, t.windTo, t.windStrength, toListener);
        const float radius = senses::heardRadiusTiles(event.tier, source, listener, t.hearingScale, hearing, wind) * c.tileSize;
        if (distance > radius) continue;
        const int tier = static_cast<int>(event.tier);
        if (bestEvent == nullptr || tier > bestTier || (tier == bestTier && distance < bestDistance))
        {
            bestEvent = &event;
            bestTier = tier;
            bestDistance = distance;
        }
    }
    if (bestEvent == nullptr) return;

    const bool loud = bestTier >= static_cast<int>(NoiseTier::Loud);
    if (ai.horde > 0)
    {
        // A loud noise turns the whole crowd in reach; the living, heard,
        // draw off the ones that heard them.
        if (!loud && !(bestEvent->sourceEntity == t.player && t.player != kInvalidEntity)) return;
        ai.horde = 0;
        sendTo(ai, bestEvent->worldPosition, false);
        return;
    }
    // Re-target only when the noise is meaningfully somewhere else. A night
    // caller keeps to its errand: the din at the door does not turn it.
    if (ai.state == ZombieState::Wander ||
        (!ai.nightCaller && !ai.follower && (ai.followingTrail || !ai.migrating || loud) &&
         glm::length(bestEvent->worldPosition - ai.targetWorld) > c.tileSize * 2.0f))
    {
        sendTo(ai, bestEvent->worldPosition, false);
    }
}

void ZombieAISystem::smell(const Ctx& c, One& z)
{
    ZombieAI& ai = *z.ai;
    const ZombieTick& t = *c.tick;
    if (t.trail == nullptr || t.trail->markers().empty()) return;
    // Only one with nothing better to do: not chasing, not after a noise,
    // not on an errand.
    if (ai.horde > 0 || ai.nightCaller || ai.follower) return;
    if (ai.state == ZombieState::Chase || ai.state == ZombieState::Attack) return;
    if (ai.state == ZombieState::Investigate && !ai.followingTrail) return;
    const bloodtrail::Marker* marker =
        t.trail->freshestNear(z.center, bloodtrail::kSmellTiles * c.tileSize, ai.trailSeq);
    if (marker == nullptr) return;
    sendTo(ai, marker->world, false);
    ai.followingTrail = true;
    ai.trailSeq = marker->seq;
}

void ZombieAISystem::sense(const Ctx& c, One& z)
{
    ZombieAI& ai = *z.ai;
    const ZombieTick& t = *c.tick;
    World& world = *c.world;
    const std::uint64_t phase = mTick + static_cast<std::uint64_t>(z.entity);
    const bool lookNow = z.sleeping || phase % static_cast<std::uint64_t>(senses::kSightEveryTicks) == 0;
    if (lookNow) see(c, z);

    // Where its prey is now (it may have moved since the last look).
    glm::vec2 preyCenter = ai.seenAt;
    bool preyAlive = false;
    if (ai.prey != kInvalidEntity && world.hasComponent<Transform>(ai.prey))
    {
        preyAlive = !world.hasComponent<Health>(ai.prey) || world.getComponent<Health>(ai.prey).current > 0.0f;
        if (preyAlive) preyCenter = entityCenter(world.getComponent<Transform>(ai.prey));
    }
    if (!preyAlive) ai.seesPrey = false;

    // The meter. The Unknown's "keep still" rule: a still player fills nothing.
    bool filling = ai.seesPrey;
    if (filling && t.ignoreStill && ai.prey == t.player && !t.playerMoving && ai.awareness < 1.0f)
    {
        filling = false;
        if (t.events != nullptr && lookNow && ai.seenDistanceTiles < 8.0f)
            t.events->push_back({ZombieEvent::Type::UnknownSeen, z.entity, z.center,
                                 static_cast<int>(zombiekinds::UnknownRule::IgnoreStill)});
    }
    const bool chasing = ai.state == ZombieState::Chase || ai.state == ZombieState::Attack;
    if (!(chasing && ai.seesPrey))
    {
        ai.awareness = senses::stepAwareness(ai.awareness, filling, ai.seenDistanceTiles, ai.seenRangeTiles,
                                             ai.kind == ZombieKind::Runner, t.awarenessDrains, z.dt);
    }
    if (filling && ai.awareness >= 1.0f)
    {
        if (!chasing)
        {
            // It has you. A Runner's head snaps up; a Screamer screams.
            if (ai.kind == ZombieKind::Runner)
            {
                ai.startTimer = zombiekinds::kRunnerStartSeconds;
                ai.sprintTimer = 0.0f;
                ai.windedTimer = 0.0f;
                if (t.events != nullptr) t.events->push_back({ZombieEvent::Type::StartRun, z.entity, z.center, 0});
            }
            ai.pathRepathTimer = kChaseRepathSeconds;
            ai.groanTimer = 1.0f + nextRandom(ai, z.entity) * 2.0f;
        }
        ai.horde = 0;
        ai.migrating = false;
        ai.followingTrail = false;
        ai.lingerTimer = 0.0f;
        ai.lookAroundTimer = 0.0f;
        ai.idleTimer = 0.0f;
        ai.stateTimer = 0.0f;
        ai.targetWorld = preyCenter;
        const float distance = glm::length(preyCenter - z.center);
        const bool lunging = ai.state == ZombieState::Attack && ai.attackWindup > 0.0f;
        ai.state = (distance <= kZombieAttackRange || lunging) ? ZombieState::Attack : ZombieState::Chase;
        if (ai.kind == ZombieKind::Screamer && ai.screamCooldown <= 0.0f && !lunging)
        {
            ai.screamTimer = zombiekinds::kScreamSeconds;
            ai.screamCooldown = zombiekinds::kScreamRepeatSeconds;
            if (t.noise != nullptr)
                t.noise->emitNoise(z.center, NoiseTier::Loud, zombiekinds::kScreamSeconds, z.entity, NoiseSource::Scream);
            if (t.events != nullptr) t.events->push_back({ZombieEvent::Type::Scream, z.entity, z.center, 0});
        }
    }
    else if (ai.seesPrey && !chasing)
    {
        // Something there: it turns and shuffles toward what it saw. A horde
        // walks on until one is sure.
        if (ai.horde == 0 && !ai.nightCaller &&
            (ai.state == ZombieState::Wander || glm::length(ai.targetWorld - ai.seenAt) > c.tileSize * 1.5f))
        {
            sendTo(ai, ai.seenAt, false);
        }
    }
    else if (chasing && !ai.seesPrey && lookNow)
    {
        // Lost sight (at a look, not between them): go to where it was last seen.
        if (!(ai.state == ZombieState::Attack && ai.attackWindup > 0.0f))
        {
            sendTo(ai, ai.seenAt, false);
        }
    }
    else if (!chasing)
    {
        if (lookNow) hear(c, z);
        if (phase % 30 == 0 && ai.state != ZombieState::Chase) smell(c, z);
    }
    if (chasing && ai.seesPrey) ai.targetWorld = preyCenter;

    // The chase is loud: a groan every few seconds, which is how one chase
    // becomes a crowd.
    if (ai.state == ZombieState::Chase || ai.state == ZombieState::Attack)
    {
        ai.groanTimer -= z.dt;
        if (ai.groanTimer <= 0.0f)
        {
            ai.groanTimer = 3.0f + nextRandom(ai, z.entity) * 2.0f;
            if (t.noise != nullptr)
                t.noise->emitNoise(z.center, NoiseTier::Soft, 0.45f, z.entity, NoiseSource::Groan);
            if (t.events != nullptr) t.events->push_back({ZombieEvent::Type::Groan, z.entity, z.center, 0});
        }
    }
}
