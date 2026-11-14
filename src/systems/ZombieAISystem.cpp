// The dead: what each one does this tick. Senses, movement and blows live
// in ZombieSenses.cpp, ZombieMove.cpp and ZombieAttack.cpp; this file runs
// them in order and keeps the shared state (the spatial grid, the flow field
// toward the player, and the queue of paths).
#include "systems/ZombieAISystem.h"
#include "world/WeatherRules.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "components/Combat.h"
#include "components/Corpse.h"
#include "components/Facing.h"
#include "components/Health.h"
#include "components/LegacyLink.h"
#include "components/Stamina.h"
#include "components/SurvivorAI.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "systems/DistrictInfectionSystem.h"
#include "world/CombatRules.h"

namespace
{
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
    zombie.lookAroundTimer = 0.0f;
    zombie.lingerTimer = 0.0f;
    zombie.followingTrail = false;
    zombie.path.clear();
    zombie.pathIndex = 0;
}

void ZombieAISystem::reset()
{
    mFlow.clear();
    mPathQueue.clear();
    mTick = 0;
}

void ZombieAISystem::update(World& world, const ZombieTick& tick)
{
    ++mTick;
    if (tick.tileMap == nullptr) return;
    const TileMap& tileMap = *tick.tileMap;

    Ctx c;
    c.world = &world;
    c.tick = &tick;
    c.tileSize = static_cast<float>(tileMap.tileWidth());
    c.night = weatherrules::isNight(tick.day, tick.hours);
    c.playerValid = tick.player != kInvalidEntity && world.hasComponent<Transform>(tick.player) &&
                    world.hasComponent<Health>(tick.player) && world.getComponent<Health>(tick.player).current > 0.0f;
    if (c.playerValid)
    {
        c.playerCenter = entityCenter(world.getComponent<Transform>(tick.player));
        if (world.hasComponent<Facing>(tick.player))
        {
            const float a = world.getComponent<Facing>(tick.player).angleRadians;
            c.playerFacing = glm::vec2(std::cos(a), std::sin(a));
        }
        if (world.hasComponent<Stamina>(tick.player))
        {
            const Stamina& st = world.getComponent<Stamina>(tick.player);
            c.playerExhausted = combatrules::exhausted(st.current, st.maximum);
        }
        if (world.hasComponent<Combat>(tick.player))
        {
            const Combat& pc = world.getComponent<Combat>(tick.player);
            c.playerImmune = pc.grabImmunity > 0.0f;
            c.playerHeld = pc.holders > 0;
        }
    }

    // Who is where, for every "near here" question this tick.
    mGrid.reset(static_cast<float>(tileMap.width()) * c.tileSize, static_cast<float>(tileMap.height()) * c.tileSize,
                4.0f * c.tileSize);
    world.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI&, Transform& t)
        {
            if (world.hasComponent<Health>(e) && world.getComponent<Health>(e).current <= 0.0f) return;
            mGrid.insert(e, entityCenter(t), SpatialGrid::Zombie);
        });
    world.forEach<SurvivorAI, Transform>(
        [&](Entity e, SurvivorAI& s, Transform& t)
        {
            if (s.state == SurvivorState::Dead) return;
            if (world.hasComponent<Health>(e) && world.getComponent<Health>(e).current <= 0.0f) return;
            mGrid.insert(e, entityCenter(t), SpatialGrid::Survivor);
        });
    world.forEach<Corpse, Transform>(
        [&](Entity e, Corpse&, Transform& t) { mGrid.insert(e, entityCenter(t), SpatialGrid::Corpse); });
    mGrid.finish();

    // One field toward the player for every chase.
    if (c.playerValid)
    {
        const glm::ivec2 playerTile = worldToTile(tileMap, c.playerCenter);
        if (!mFlow.valid() || mTick % kFlowEveryTicks == 0 || glm::length(glm::vec2(playerTile - mFlow.goal())) > 6.0f)
        {
            mFlow.build(tileMap, playerTile);
        }
    }
    else
    {
        mFlow.clear();
    }
    runPathQueue(world, tileMap);

    world.forEach<ZombieAI, Transform, Velocity>(
        [&](Entity zombieEntity, ZombieAI& ai, Transform& zombieTransform, Velocity& velocity)
        {
            if (world.hasComponent<Health>(zombieEntity) && world.getComponent<Health>(zombieEntity).current <= 0.0f)
            {
                velocity.dx = 0.0f;
                velocity.dy = 0.0f;
                return;
            }

            One z;
            z.entity = zombieEntity;
            z.ai = &ai;
            z.center = entityCenter(zombieTransform);
            z.tile = worldToTile(tileMap, z.center);
            z.dt = tick.dt;

            // The far dead think less often (and with the time they missed),
            // unless they are going somewhere on purpose.
            const float playerDistanceTiles =
                c.playerValid ? glm::length(c.playerCenter - z.center) / c.tileSize : 1.0e6f;
            z.sleeping = playerDistanceTiles > kSleepTiles && !ai.migrating && ai.horde == 0 && !ai.nightCaller &&
                         !ai.follower && ai.state != ZombieState::Chase && ai.state != ZombieState::Attack &&
                         !ZombieBlow::isDown(ai) && ai.stagger <= 0.0f && !ai.holding;
            if (z.sleeping)
            {
                if ((mTick + static_cast<std::uint64_t>(zombieEntity)) % kSleepEveryTicks != 0) return;
                z.dt = tick.dt * static_cast<float>(kSleepEveryTicks);
            }
            const float dt = z.dt;

            // On the floor: it slides on what is left of the blow, lies a
            // while, then takes its time getting up. It can do nothing else.
            if (ZombieBlow::isDown(ai))
            {
                ai.holding = false;
                ai.attackWindup = 0.0f;
                ai.stagger = 0.0f;
                velocity.dx = ai.knock.x;
                velocity.dy = ai.knock.y;
                ai.knock *= std::max(0.0f, 1.0f - ZombieBlow::kKnockDecay * dt);
                if (ai.downTimer > 0.0f)
                {
                    ai.downTimer -= dt;
                    if (ai.downTimer <= 0.0f)
                    {
                        ai.downTimer = 0.0f;
                        ai.riseTimer = combatrules::kRiseSeconds;
                    }
                }
                else
                {
                    ai.riseTimer = std::max(0.0f, ai.riseTimer - dt);
                }
                return;
            }

            // Reeling from a blow: carried by the knockback, unable to act.
            if (ai.stagger > 0.0f)
            {
                ai.holding = false;
                ai.stagger -= dt;
                velocity.dx = ai.knock.x;
                velocity.dy = ai.knock.y;
                ai.knock *= std::max(0.0f, 1.0f - ZombieBlow::kKnockDecay * dt);
                return;
            }
            ai.knock = glm::vec2(0.0f);
            ai.strikeTimer = std::max(0.0f, ai.strikeTimer - dt);
            ai.lureTimer = std::max(0.0f, ai.lureTimer - dt);

            // --- Decomposition stage from age ---
            const int age = tick.day - ai.spawnDay;
            if (age <= 3) ai.decomp = DecompStage::Fresh;
            else if (age <= 10) ai.decomp = DecompStage::Bloated;
            else if (age <= 20) ai.decomp = DecompStage::Decayed;
            else ai.decomp = DecompStage::Skeletal;
            switch (ai.decomp)
            {
            case DecompStage::Fresh:    z.speedMultiplier = 1.0f;  z.damageMultiplier = 1.0f;  break;
            case DecompStage::Bloated:  z.speedMultiplier = 0.75f; z.damageMultiplier = 0.9f;  break;
            case DecompStage::Decayed:  z.speedMultiplier = 0.5f;  z.damageMultiplier = 0.6f;  break;
            case DecompStage::Skeletal: z.speedMultiplier = 0.3f;  z.damageMultiplier = 0.35f; break;
            }

            // The district: the worse it is, the worse they are.
            const int districtId = tileMap.getDistrictId(z.tile.x, z.tile.y);
            float localInfection = 0.0f;
            bool overwhelmed = false;
            if (tick.infection != nullptr && districtId > 0)
            {
                localInfection = tick.infection->getInfection(districtId);
                overwhelmed = tick.infection->isOverwhelmed(districtId);
            }
            const float infectionFactor = glm::clamp(localInfection / 100.0f, 0.0f, 1.0f);
            z.speedMultiplier *= overwhelmed ? (1.0f + infectionFactor * 0.35f) : (1.0f + infectionFactor * 0.20f);
            z.damageMultiplier *= overwhelmed ? (1.0f + infectionFactor * 0.25f) : (1.0f + infectionFactor * 0.12f);
            z.visionMultiplier = overwhelmed ? (1.0f + infectionFactor * 0.65f) : (1.0f + infectionFactor * 0.35f);

            // The threat mode, and The Unknown's rules.
            z.speedMultiplier *= tick.speedScale;
            if (tick.dormantByDay && c.night) z.speedMultiplier *= 1.5f;
            if (tick.rainCalm && tick.raining) z.speedMultiplier *= 0.7f;
            // The weather and the season (phase 7): a blizzard, summer
            // nights, and snow lying in the open.
            z.speedMultiplier *= tick.weatherSpeed;
            if (c.night) z.speedMultiplier *= tick.nightSpeed;
            if (tick.snowCover >= weatherrules::kSnowDeepEnough && !tileMap.isIndoors(z.tile.x, z.tile.y))
            {
                z.speedMultiplier *= weatherrules::kSnowDeadScale;
            }
            if (tick.frenzyAfterRain) z.speedMultiplier *= 1.5f;
            ai.moveSpeed = ai.baseMoveSpeed;

            ai.stateTimer += dt;
            ai.boredTimer = std::max(0.0f, ai.boredTimer - dt);
            ai.pathRepathTimer += dt;
            ai.attackCooldown = std::max(0.0f, ai.attackCooldown - dt);
            ai.screamCooldown = std::max(0.0f, ai.screamCooldown - dt);

            // Holding on: it keeps its grip and bites until it is shaken off.
            if (ai.holding)
            {
                hold(c, z, velocity);
                return;
            }

            // A Crawler under a bed or a car: still until the living come
            // close, then a scrape, then it moves.
            if (ai.dormant)
            {
                velocity.dx = 0.0f;
                velocity.dy = 0.0f;
                if (ai.wakeTimer > 0.0f)
                {
                    ai.wakeTimer -= dt;
                    if (ai.wakeTimer <= 0.0f)
                    {
                        ai.dormant = false;
                        ai.wakeTimer = 0.0f;
                        ai.awareness = 1.0f;
                        ai.prey = tick.player;
                        ai.state = ZombieState::Chase;
                        ai.targetWorld = c.playerCenter;
                    }
                }
                else if (c.playerValid && glm::length(c.playerCenter - z.center) <=
                                              zombiekinds::kCrawlerWakeTiles * c.tileSize)
                {
                    ai.wakeTimer = zombiekinds::kCrawlerScrapeSeconds;
                    if (tick.events != nullptr)
                        tick.events->push_back({ZombieEvent::Type::Scrape, zombieEntity, z.center, 0});
                }
                return;
            }

            // Screaming: it stands, head back, until the scream is done.
            if (ai.screamTimer > 0.0f)
            {
                ai.screamTimer -= dt;
                velocity.dx = 0.0f;
                velocity.dy = 0.0f;
                return;
            }

            sense(c, z);

            switch (ai.state)
            {
            case ZombieState::Wander: wander(c, z, velocity); break;
            case ZombieState::Investigate: investigate(c, z, velocity); break;
            case ZombieState::Chase:
                if (ai.kind == ZombieKind::Screamer) keepAway(c, z, velocity);
                else chase(c, z, velocity);
                break;
            case ZombieState::Attack: attack(c, z, velocity); break;
            }

            // Pressing into a closed door on the way somewhere: batter it.
            // Paths lead through closed doors, so this is where they stop.
            // A Crawler cannot reach a door handle or a window.
            if (tick.blows != nullptr && ai.doorDamage > 0.0f &&
                (ai.state == ZombieState::Chase || ai.state == ZombieState::Investigate))
            {
                const glm::vec2 heading(velocity.dx, velocity.dy);
                const float speed2 = glm::dot(heading, heading);
                if (speed2 > 1.0f)
                {
                    const glm::vec2 ahead = z.center + heading / std::sqrt(speed2) * (c.tileSize * 0.75f);
                    const glm::ivec2 aheadTile = worldToTile(tileMap, ahead);
                    const bool barrier = tileMap.isClosedDoor(aheadTile.x, aheadTile.y) ||
                                         tileMap.hasWindow(aheadTile.x, aheadTile.y) ||
                                         tileMap.doorwayBlocked(aheadTile.x, aheadTile.y);
                    if (barrier && glm::length(tileCenterWorld(tileMap, aheadTile) - z.center) < c.tileSize * 1.3f)
                    {
                        tick.blows->push_back(BarrierBlow{aheadTile, zombieEntity});
                    }
                }
            }

            // Where it walks is where it looks.
            const float moving2 = velocity.dx * velocity.dx + velocity.dy * velocity.dy;
            if (moving2 > 4.0f)
            {
                ai.facing = glm::vec2(velocity.dx, velocity.dy) / std::sqrt(moving2);
            }
            if (!z.sleeping) separate(c, z, velocity);
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
    bool throughWindow = false;

    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const glm::vec2 p = worldStart + delta * t;

        const int tileX = static_cast<int>(std::floor(p.x / static_cast<float>(tileMap.tileWidth())));
        const int tileY = static_cast<int>(std::floor(p.y / static_cast<float>(tileMap.tileHeight())));

        if (tileMap.blocksView(tileX, tileY))
        {
            return false;
        }
        throughWindow = throughWindow || tileMap.hasWindow(tileX, tileY);
    }

    // Through glass, or an empty frame, sight reaches only so far.
    return !throughWindow || distance <= kWindowSightTiles * static_cast<float>(tileMap.tileWidth());
}
