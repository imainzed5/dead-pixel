#include "systems/SurvivorBrain.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/geometric.hpp>

#include "ai/Pathfinder.h"
#include "components/Health.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"
#include "systems/ZombieAISystem.h"

using survivorrules::Option;

namespace
{
constexpr float kHalf = 16.0f;

glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + kHalf, t.y + kHalf);
}

glm::ivec2 tileOf(const TileMap& map, const glm::vec2& world)
{
    const float tw = static_cast<float>(std::max(1, map.tileWidth()));
    const float th = static_cast<float>(std::max(1, map.tileHeight()));
    return glm::ivec2(static_cast<int>(std::floor(world.x / tw)), static_cast<int>(std::floor(world.y / th)));
}

glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

SurvivorState stateFor(Option option)
{
    switch (option)
    {
    case Option::Idle: return SurvivorState::Idle;
    case Option::Flee: return SurvivorState::Flee;
    case Option::Fight: return SurvivorState::Fight;
    case Option::Eat: return SurvivorState::Eat;
    case Option::Drink: return SurvivorState::Drink;
    case Option::Sleep: return SurvivorState::Sleep;
    case Option::Scavenge: return SurvivorState::Scavenge;
    case Option::Fortify: return SurvivorState::Fortify;
    case Option::Tend: return SurvivorState::Tend;
    case Option::Watch: return SurvivorState::Watch;
    case Option::Count: break;
    }
    return SurvivorState::Idle;
}

void stop(glm::vec2& velocity)
{
    velocity = glm::vec2(0.0f);
}
} // namespace

bool SurvivorBrain::isOverride(SurvivorState state)
{
    return state == SurvivorState::Follow || state == SurvivorState::Wait || state == SurvivorState::Rob ||
           state == SurvivorState::Attack || state == SurvivorState::GoHome;
}

Entity SurvivorBrain::nearestDead(const SurvivorTick& tick, const glm::vec2& centre, float tiles, int* count,
                                  float* distanceTiles)
{
    Entity best = kInvalidEntity;
    const float tileSize = static_cast<float>(std::max(1, tick.map->tileWidth()));
    float bestDistance = tiles * tileSize;
    int n = 0;
    if (tick.grid != nullptr)
    {
        tick.grid->query(centre, tiles * tileSize, SpatialGrid::Zombie,
                         [&](const SpatialGrid::Item& item)
                         {
                             const float d = glm::length(item.position - centre);
                             ++n;
                             if (d <= bestDistance)
                             {
                                 bestDistance = d;
                                 best = item.entity;
                             }
                         });
    }
    if (count != nullptr) *count = n;
    if (distanceTiles != nullptr) *distanceTiles = best != kInvalidEntity ? bestDistance / tileSize : 99.0f;
    return best;
}

bool SurvivorBrain::walkTo(SurvivorAI& ai, const glm::vec2& centre, glm::vec2& velocity, const glm::ivec2& goal,
                           float speed, const SurvivorTick& tick, SurvivorHooksBase& hooks, Entity self)
{
    const TileMap& map = *tick.map;
    const glm::ivec2 here = tileOf(map, centre);
    const glm::vec2 goalCentre = tileCentre(map, goal);
    if (here == goal && glm::length(goalCentre - centre) < 6.0f)
    {
        stop(velocity);
        ai.path.clear();
        return true;
    }

    ai.repathTimer -= tick.dt;
    if (goal != ai.goal || ai.repathTimer <= 0.0f || ai.stuckTimer > 1.5f || (ai.path.empty() && !ai.noPath))
    {
        ai.goal = goal;
        ai.path = Pathfinder::findPath(map, here, goal, Pathfinder::Mover::Living);
        ai.pathStep = 1;
        ai.repathTimer = 3.0f + static_cast<float>(ai.id % 5) * 0.2f;
        ai.stuckTimer = 0.0f;
        ai.noPath = ai.path.empty();
    }
    if (ai.path.empty())
    {
        // No way there: wait for the next try (the caller may give up).
        stop(velocity);
        return false;
    }

    // Skip steps already reached.
    while (ai.pathStep < ai.path.size() && ai.path[ai.pathStep] == here &&
           glm::length(tileCentre(map, here) - centre) < 10.0f)
    {
        ++ai.pathStep;
    }
    if (ai.pathStep >= ai.path.size())
    {
        // The last step: straight to the goal's centre.
        const glm::vec2 to = goalCentre - centre;
        const float d = glm::length(to);
        if (d < 3.0f)
        {
            stop(velocity);
            return true;
        }
        velocity = to / d * std::min(speed, d * 8.0f);
        return false;
    }

    const glm::ivec2 next = ai.path[ai.pathStep];
    // A closed door: open it from here (and remember to shut it).
    if (map.isClosedDoor(next.x, next.y))
    {
        const float d = glm::length(tileCentre(map, next) - centre);
        if (d < static_cast<float>(map.tileWidth()) * 1.4f)
        {
            if (!hooks.openDoor(self, next))
            {
                // Locked or boarded: no way through here (the caller may give up).
                ai.path.clear();
                ai.noPath = true;
                ai.repathTimer = 3.0f;
                stop(velocity);
                return false;
            }
        }
    }
    if (hooks.tileBurning(next))
    {
        stop(velocity);
        return false;
    }
    const glm::vec2 to = tileCentre(map, next) - centre;
    const float d = glm::length(to);
    if (d < 4.0f)
    {
        ++ai.pathStep;
        return false;
    }
    velocity = to / d * speed;
    return false;
}

void SurvivorBrain::update(World& world, const SurvivorTick& tick, SurvivorHooksBase& hooks)
{
    if (tick.map == nullptr) return;
    world.forEach<SurvivorAI, Transform, Velocity, Health>(
        [&](Entity e, SurvivorAI& ai, Transform& t, Velocity& v, Health& h)
        {
            if (!ai.alive || ai.state == SurvivorState::Dead)
            {
                v.dx = v.dy = 0.0f;
                return;
            }
            if (h.current <= 0.0f)
            {
                v.dx = v.dy = 0.0f;
                return; // the game lays them down
            }
            const glm::vec2 centre = centreOf(t);
            const float tileSize = static_cast<float>(std::max(1, tick.map->tileWidth()));
            const bool farNow = !tick.playerValid ||
                                glm::length(centre - tick.playerCentre) > survivorrules::kNearTiles * tileSize;
            if (farNow && !ai.companion)
            {
                if (!ai.far)
                {
                    ai.far = true;
                    ai.path.clear();
                }
                v.dx = v.dy = 0.0f;
                far(world, e, ai, tick, hooks);
                return;
            }
            ai.far = false;
            near(world, e, ai, tick, hooks);
        });
}

void SurvivorBrain::far(World& world, Entity e, SurvivorAI& ai, const SurvivorTick& tick, SurvivorHooksBase& hooks)
{
    // An hour at a time: needs, a meal from stock, an hour out for supplies.
    ai.farHourClock += tick.dt;
    if (ai.farHourClock < 60.0f) return;
    ai.farHourClock -= 60.0f;
    const float perHour = 60.0f * survivorrules::kNeedsScale;
    ai.hunger = std::max(0.0f, ai.hunger - 0.025f * perHour);
    ai.thirst = std::max(0.0f, ai.thirst - 0.045f * perHour);
    ai.sleepDebt = tick.night ? std::max(0.0f, ai.sleepDebt - 12.0f) : std::min(100.0f, ai.sleepDebt + 0.058f * perHour);
    if (ai.hunger < survivorrules::kHungryBelow) hooks.consume(e, false);
    if (ai.thirst < survivorrules::kHungryBelow) hooks.consume(e, true);
    Health& h = world.getComponent<Health>(e);
    if (ai.hunger <= 0.0f || ai.thirst <= 0.0f) h.current -= 6.0f;
    else if (ai.hunger > 45.0f && ai.thirst > 45.0f) h.current = std::min(h.maximum, h.current + kMendAsleepPerHour);

    // Home, out of sight.
    Transform& t = world.getComponent<Transform>(e);
    const glm::ivec2 here = tileOf(*tick.map, centreOf(t));
    if (ai.homeBuilding > 0 && tick.map->getBuildingId(here.x, here.y) != ai.homeBuilding && ai.state != SurvivorState::Rob &&
        !tick.map->isSolid(ai.homeTile.x, ai.homeTile.y))
    {
        const glm::vec2 c = tileCentre(*tick.map, ai.homeTile);
        t.x = t.prevX = c.x - kHalf;
        t.y = t.prevY = c.y - kHalf;
    }
    if (isOverride(ai.state)) ai.state = SurvivorState::Idle;

    const SurvivorHooksBase::Stock stock = hooks.stock(e);
    const int absoluteHour = tick.day * 24 + static_cast<int>(tick.hours);
    const survivorrules::FarHour hour =
        survivorrules::farHour(!tick.night, stock.days, ai.outDay == tick.day || ai.stayer, ai.danger,
                               tick.seed, ai.id, absoluteHour);
    if (hour.scavenge)
    {
        ai.outDay = tick.day;
        hooks.farScavenge(e);
        if (hour.wound)
        {
            ++ai.wounds;
            h.current -= 15.0f;
        }
        if (hour.dies) h.current = 0.0f;
    }
}

void SurvivorBrain::near(World& world, Entity e, SurvivorAI& ai, const SurvivorTick& tick, SurvivorHooksBase& hooks)
{
    Transform& t = world.getComponent<Transform>(e);
    Velocity& v = world.getComponent<Velocity>(e);
    Health& h = world.getComponent<Health>(e);
    const glm::vec2 centre = centreOf(t);

    // Needs at 0.8 of the player's.
    const float scale = survivorrules::kNeedsScale * tick.dt;
    ai.hunger = std::max(0.0f, ai.hunger - 0.025f * scale);
    ai.thirst = std::max(0.0f, ai.thirst - 0.045f * scale);
    if (ai.state == SurvivorState::Sleep && ai.actionTimer > 0.0f)
    {
        ai.sleepDebt = std::max(0.0f, ai.sleepDebt - 0.2f * tick.dt);
    }
    else
    {
        ai.sleepDebt = std::min(100.0f, ai.sleepDebt + 0.058f * scale);
    }
    if (ai.hunger <= 0.0f || ai.thirst <= 0.0f) h.current -= 0.1f * tick.dt;
    // Fed, not bleeding: they mend (faster asleep), as the player does.
    if (ai.hunger > 45.0f && ai.thirst > 45.0f && ai.bleedSeconds <= 0.0f && h.current < h.maximum)
    {
        const bool asleep = ai.state == SurvivorState::Sleep && ai.actionTimer > 0.0f;
        h.current = std::min(h.maximum, h.current + (asleep ? kMendAsleepPerHour : kMendPerHour) * tick.dt / 60.0f);
    }
    if (ai.bleedSeconds > 0.0f)
    {
        ai.bleedSeconds -= tick.dt;
        h.current -= 0.4f * tick.dt;
        if (static_cast<int>(ai.bleedSeconds * 2.0f) != static_cast<int>((ai.bleedSeconds + tick.dt) * 2.0f))
        {
            hooks.bleed(e, centre);
        }
    }
    ai.attackCooldown = std::max(0.0f, ai.attackCooldown - tick.dt);
    ai.swingTimer = std::max(0.0f, ai.swingTimer - tick.dt);
    ai.hurtFlash = std::max(0.0f, ai.hurtFlash - tick.dt);

    // The dead coming close cut the thinking short.
    ai.dangerClock -= tick.dt;
    if (ai.dangerClock <= 0.0f)
    {
        ai.dangerClock = 0.3f;
        if (ai.state != SurvivorState::Flee && ai.state != SurvivorState::Fight && !isOverride(ai.state))
        {
            int n = 0;
            (void)nearestDead(tick, centre, std::max(0.5f, survivorrules::fleeTiles(ai.p.aggression)), &n, nullptr);
            if (n > 0) ai.thinkTimer = 0.0f;
        }
    }
    ai.thinkTimer -= tick.dt;
    if (ai.thinkTimer <= 0.0f)
    {
        ai.thinkTimer = survivorrules::kThinkSeconds;
        think(world, e, ai, centre, tick, hooks);
    }

    glm::vec2 velocity(0.0f);
    act(world, e, ai, centre, velocity, tick, hooks);
    v.dx = velocity.x;
    v.dy = velocity.y;

    // Progress check: walking but going nowhere.
    const float speed = glm::length(velocity);
    const glm::vec2 moved(t.x - t.prevX, t.y - t.prevY);
    if (speed > 1.0f && glm::length(moved) < speed * tick.dt * 0.25f) ai.stuckTimer += tick.dt;
    else ai.stuckTimer = std::max(0.0f, ai.stuckTimer - tick.dt);

    // Feet make noise, as the player's do.
    if (speed > 1.0f && tick.noise != nullptr)
    {
        ai.noiseClock -= tick.dt;
        if (ai.noiseClock <= 0.0f)
        {
            ai.noiseClock = 0.5f;
            const bool running = speed > kWalkSpeed * 1.2f;
            tick.noise->emitNoise(centre, running ? NoiseTier::Medium : NoiseTier::Soft, 0.4f, e);
        }
    }

    // A door they opened, shut behind them once through.
    if (ai.openedDoor.x >= 0 && !ai.companion && ai.state != SurvivorState::Rob && ai.state != SurvivorState::Attack)
    {
        const float d = glm::length(tileCentre(*tick.map, ai.openedDoor) - centre);
        const bool stillOnPath = std::find(ai.path.begin() + static_cast<std::ptrdiff_t>(std::min(ai.pathStep, ai.path.size())),
                                           ai.path.end(), ai.openedDoor) != ai.path.end();
        if (d > static_cast<float>(tick.map->tileWidth()) * 1.6f && !stillOnPath)
        {
            hooks.closeDoor(ai.openedDoor);
            ai.openedDoor = glm::ivec2(-1);
        }
    }
}

void SurvivorBrain::think(World& world, Entity e, SurvivorAI& ai, const glm::vec2& centre, const SurvivorTick& tick,
                          SurvivorHooksBase& hooks)
{
    const Health& h = world.getComponent<Health>(e);
    const SurvivorHooksBase::Stock stock = hooks.stock(e);
    ai.desperation = survivorrules::desperation(ai.hunger, ai.thirst, stock.days);
    ai.attitude = survivorrules::attitude(ai.p, ai.desperation, ai.trust, ai.angryDay == tick.day);
    if (ai.companion) ai.attitude = survivorrules::Attitude::Open;

    if (isOverride(ai.state)) return; // the game's

    survivorrules::Situation s;
    s.p = ai.p;
    s.hunger = ai.hunger;
    s.thirst = ai.thirst;
    s.sleepDebt = ai.sleepDebt;
    s.health = h.maximum > 0.0f ? h.current / h.maximum : 1.0f;
    s.wounds = ai.wounds;
    s.stockDays = stock.days;
    s.hasFood = stock.food;
    s.hasDrink = stock.drink;
    s.hasMedicine = stock.medicine;
    s.armed = stock.weapon || stock.gun;
    s.hasBoards = stock.boards;
    glm::ivec2 door(-1);
    glm::ivec2 stand(-1);
    s.homeOpen = ai.homeBuilding > 0 && stock.boards && hooks.openWayIn(ai.homeBuilding, door, stand);
    int inFlee = 0;
    float nearest = 99.0f;
    (void)nearestDead(tick, centre, std::max(0.5f, survivorrules::fleeTiles(ai.p.aggression)), &inFlee, &nearest);
    int inFight = 0;
    (void)nearestDead(tick, centre, survivorrules::kFightTiles, &inFight, nullptr);
    float anyNear = 99.0f;
    (void)nearestDead(tick, centre, 6.0f, nullptr, &anyNear);
    s.deadInFleeRange = inFlee;
    s.deadInFightRange = inFight;
    s.nearestDeadTiles = anyNear;
    s.night = tick.night;
    s.stayer = ai.stayer;
    // One trip out a day, as when nobody is looking (far): the town is not
    // stripped around the player faster than away from them.
    if (stock.days < survivorrules::kLowStockDays && !tick.night && !ai.stayer && !ai.carrying &&
        (ai.outDay != tick.day || ai.state == SurvivorState::Scavenge))
    {
        glm::ivec2 at(-1);
        s.containerKnown = hooks.findContainer(e, at) != kInvalidEntity;
    }
    const float tileSize = static_cast<float>(std::max(1, tick.map->tileWidth()));
    s.playerWatched = tick.playerValid && ai.attitude == survivorrules::Attitude::Wary &&
                      glm::length(tick.playerCentre - centre) < survivorrules::kWatchTiles * tileSize;
    s.current = ai.option;

    Option chosen = survivorrules::choose(s);
    // Home with what they found before anything but trouble.
    if (ai.carrying && chosen != Option::Flee && chosen != Option::Fight && chosen != Option::Eat && chosen != Option::Drink)
    {
        chosen = Option::Scavenge;
    }
    // A trip cut short by the dead is the day's going out: home and stay in.
    if (chosen == Option::Flee && ai.option == Option::Scavenge) ai.outDay = tick.day;
    if (chosen != ai.option)
    {
        ai.option = chosen;
        ai.actionTimer = 0.0f;
        ai.target = kInvalidEntity;
        ai.path.clear();
    }
    ai.state = stateFor(chosen);
}

void SurvivorBrain::fight(World& world, Entity e, SurvivorAI& ai, const glm::vec2& centre, glm::vec2& velocity,
                          Entity target, const SurvivorTick& tick, SurvivorHooksBase& hooks)
{
    if (target == kInvalidEntity || !world.hasComponent<Transform>(target)) return;
    const glm::vec2 at = centreOf(world.getComponent<Transform>(target));
    const glm::vec2 to = at - centre;
    const float d = glm::length(to);
    ai.faceTo = to;
    const float tileSize = static_cast<float>(std::max(1, tick.map->tileWidth()));
    const SurvivorHooksBase::Stock stock = hooks.stock(e);
    if (stock.gun && d > kReach * 1.5f && d < kGunTiles * tileSize && ZombieAISystem::hasLineOfSight(*tick.map, centre, at))
    {
        stop(velocity);
        if (ai.attackCooldown <= 0.0f)
        {
            hooks.shootAt(e, target);
            ai.attackCooldown = survivorrules::blowSeconds(ai.p.competence) + 0.4f;
            ai.swingTimer = 0.3f;
            ai.shooting = true;
        }
        return;
    }
    if (d > kReach)
    {
        velocity = to / std::max(1.0f, d) * kWalkSpeed * 1.1f;
        return;
    }
    stop(velocity);
    if (ai.attackCooldown <= 0.0f)
    {
        hooks.blowAt(e, target);
        ai.attackCooldown = survivorrules::blowSeconds(ai.p.competence);
        ai.swingTimer = 0.4f;
        ai.shooting = false;
    }
}

void SurvivorBrain::act(World& world, Entity e, SurvivorAI& ai, const glm::vec2& centre, glm::vec2& velocity,
                        const SurvivorTick& tick, SurvivorHooksBase& hooks)
{
    const TileMap& map = *tick.map;
    const float tileSize = static_cast<float>(std::max(1, map.tileWidth()));
    const glm::ivec2 here = tileOf(map, centre);
    const bool atHome = ai.homeBuilding > 0 && map.getBuildingId(here.x, here.y) == ai.homeBuilding;
    const auto goHome = [&](float speed) { return walkTo(ai, centre, velocity, ai.homeTile, speed, tick, hooks, e); };

    switch (ai.state)
    {
    case SurvivorState::Idle:
    {
        if (!atHome && ai.homeBuilding > 0)
        {
            goHome(kWalkSpeed);
            break;
        }
        // Moving about the house now and then.
        ai.actionTimer -= tick.dt;
        if (ai.actionTimer <= 0.0f)
        {
            ai.actionTimer = 8.0f + static_cast<float>((ai.id * 7 + tick.day) % 6);
            const std::uint32_t roll = static_cast<std::uint32_t>(ai.id * 2654435761u) ^
                                       static_cast<std::uint32_t>(tick.hours * 60.0);
            ai.targetTile = ai.homeBuilding > 0 ? hooks.roomTile(ai.homeBuilding, roll) : here;
        }
        if (ai.targetTile.x >= 0 && !tick.night) walkTo(ai, centre, velocity, ai.targetTile, kWalkSpeed * 0.6f, tick, hooks, e);
        break;
    }
    case SurvivorState::Flee:
    {
        if (ai.homeBuilding <= 0 || atHome)
        {
            // In: shut the doors and keep away from the dead.
            if (atHome) hooks.shutHome(ai.homeBuilding);
            int n = 0;
            float d = 99.0f;
            const Entity dead = nearestDead(tick, centre, 4.0f, &n, &d);
            if (dead != kInvalidEntity && world.hasComponent<Transform>(dead))
            {
                const glm::vec2 away = centre - centreOf(world.getComponent<Transform>(dead));
                const float len = glm::length(away);
                if (len > 0.1f) velocity = away / len * kRunSpeed * 0.6f;
            }
            break;
        }
        goHome(kRunSpeed);
        break;
    }
    case SurvivorState::Fight:
    {
        const Entity target = nearestDead(tick, centre, survivorrules::kFightTiles + 1.0f, nullptr, nullptr);
        fight(world, e, ai, centre, velocity, target, tick, hooks);
        break;
    }
    case SurvivorState::Eat:
    case SurvivorState::Drink:
    {
        stop(velocity);
        ai.actionTimer += tick.dt;
        if (ai.actionTimer >= kEatSeconds)
        {
            hooks.consume(e, ai.state == SurvivorState::Drink);
            ai.actionTimer = 0.0f;
            ai.option = Option::Idle;
            ai.state = SurvivorState::Idle;
        }
        break;
    }
    case SurvivorState::Sleep:
    {
        if (ai.homeBuilding > 0 && ai.actionTimer <= 0.0f)
        {
            if (ai.targetTile.x < 0 || map.getBuildingId(ai.targetTile.x, ai.targetTile.y) != ai.homeBuilding)
            {
                ai.targetTile = hooks.bedTile(ai.homeBuilding, here);
            }
            if (!walkTo(ai, centre, velocity, ai.targetTile, kWalkSpeed, tick, hooks, e)) break;
        }
        stop(velocity);
        ai.actionTimer = std::max(ai.actionTimer, 0.01f); // lying down
        if (ai.sleepDebt < 5.0f && !tick.night)
        {
            ai.actionTimer = 0.0f;
            ai.option = Option::Idle;
            ai.state = SurvivorState::Idle;
        }
        break;
    }
    case SurvivorState::Scavenge:
    {
        if (ai.carrying)
        {
            if (goHome(kWalkSpeed))
            {
                hooks.store(e);
                ai.carrying = false;
                ai.option = Option::Idle;
                ai.state = SurvivorState::Idle;
            }
            break;
        }
        if (ai.target == kInvalidEntity || !world.hasComponent<Transform>(ai.target))
        {
            glm::ivec2 stand(-1);
            ai.target = hooks.findContainer(e, stand);
            ai.targetTile = stand;
            ai.actionTimer = 0.0f;
            if (ai.target == kInvalidEntity)
            {
                ai.option = Option::Idle;
                ai.state = SurvivorState::Idle;
                break;
            }
        }
        if (!walkTo(ai, centre, velocity, ai.targetTile, kWalkSpeed, tick, hooks, e))
        {
            if (ai.noPath)
            {
                // Cannot get there: another one, another time.
                std::cout << "[Survivors] " << ai.name << " cannot get to (" << ai.targetTile.x << "," << ai.targetTile.y << ")\n";
                ai.skipped.push_back(ai.targetTile);
                ai.target = kInvalidEntity;
                ai.noPath = false;
            }
            break;
        }
        ai.faceTo = centreOf(world.getComponent<Transform>(ai.target)) - centre;
        ai.actionTimer += tick.dt;
        if (ai.actionTimer >= kSearchSeconds)
        {
            hooks.search(e, ai.target);
            ai.target = kInvalidEntity;
            ai.actionTimer = 0.0f;
            ai.carrying = true;
        }
        break;
    }
    case SurvivorState::Fortify:
    {
        glm::ivec2 door(-1);
        glm::ivec2 stand(-1);
        if (ai.homeBuilding <= 0 || !hooks.openWayIn(ai.homeBuilding, door, stand))
        {
            ai.option = Option::Idle;
            ai.state = SurvivorState::Idle;
            break;
        }
        if (!walkTo(ai, centre, velocity, stand, kWalkSpeed, tick, hooks, e)) break;
        ai.faceTo = glm::vec2(door - here);
        ai.actionTimer += tick.dt;
        if (ai.actionTimer >= kBoardSeconds)
        {
            hooks.board(e, door);
            ai.actionTimer = 0.0f;
            ai.option = Option::Idle;
            ai.state = SurvivorState::Idle;
        }
        break;
    }
    case SurvivorState::Tend:
    {
        stop(velocity);
        ai.actionTimer += tick.dt;
        if (ai.actionTimer >= 3.0f)
        {
            hooks.tend(e);
            ai.actionTimer = 0.0f;
            ai.option = Option::Idle;
            ai.state = SurvivorState::Idle;
        }
        break;
    }
    case SurvivorState::Watch:
    {
        stop(velocity);
        if (tick.playerValid) ai.faceTo = tick.playerCentre - centre;
        break;
    }
    case SurvivorState::Follow:
    {
        int n = 0;
        Entity dead = nearestDead(tick, centre, survivorrules::kFightTiles, &n, nullptr);
        const float d = tick.playerValid ? glm::length(tick.playerCentre - centre) / tileSize : 99.0f;
        // One at you, close by: they come and fight it beside you.
        if (dead == kInvalidEntity && d < 8.0f)
        {
            dead = nearestDead(tick, tick.playerCentre, survivorrules::kFightTiles, &n, nullptr);
        }
        if (dead != kInvalidEntity && n <= 2)
        {
            fight(world, e, ai, centre, velocity, dead, tick, hooks);
            break;
        }
        if (!tick.playerValid) break;
        if (tick.playerCrouched && d < 6.0f)
        {
            ai.faceTo = tick.playerCentre - centre;
            break; // still, with you
        }
        if (d > 4.0f || (d > 3.0f && ai.path.size() > 0 && ai.pathStep < ai.path.size()))
        {
            walkTo(ai, centre, velocity, tileOf(map, tick.playerCentre), d > 8.0f ? kRunSpeed : kWalkSpeed * 1.4f, tick,
                   hooks, e);
            if (ai.goal != tileOf(map, tick.playerCentre)) ai.repathTimer = 0.0f;
            ai.repathTimer = std::min(ai.repathTimer, 0.8f);
        }
        else
        {
            ai.path.clear();
            ai.faceTo = tick.playerCentre - centre;
        }
        break;
    }
    case SurvivorState::Wait:
    {
        stop(velocity);
        const Entity dead = nearestDead(tick, centre, 1.5f, nullptr, nullptr);
        if (dead != kInvalidEntity) fight(world, e, ai, centre, velocity, dead, tick, hooks);
        break;
    }
    case SurvivorState::Rob:
    {
        if (!tick.playerValid) break;
        const float d = glm::length(tick.playerCentre - centre) / tileSize;
        if (d > (tick.playerAsleep ? 0.8f : 9.0f))
        {
            walkTo(ai, centre, velocity, tileOf(map, tick.playerCentre), kWalkSpeed, tick, hooks, e);
            ai.repathTimer = std::min(ai.repathTimer, 1.0f);
        }
        else
        {
            ai.path.clear();
            ai.faceTo = tick.playerCentre - centre;
        }
        break;
    }
    case SurvivorState::Attack:
    {
        if (!tick.playerValid) break;
        const glm::vec2 to = tick.playerCentre - centre;
        const float d = glm::length(to);
        const SurvivorHooksBase::Stock stock = hooks.stock(e);
        ai.faceTo = to;
        // A shot only along a clear line; otherwise they come round to you.
        if (stock.gun && d < kGunTiles * tileSize && d > kReach * 1.5f &&
            ZombieAISystem::hasLineOfSight(*tick.map, centre, tick.playerCentre))
        {
            fight(world, e, ai, centre, velocity, tick.player, tick, hooks);
        }
        else if (d > kReach * 1.5f)
        {
            walkTo(ai, centre, velocity, tileOf(map, tick.playerCentre), kWalkSpeed * 1.3f, tick, hooks, e);
            ai.repathTimer = std::min(ai.repathTimer, 0.6f);
            if (d < 3.0f * tileSize) velocity = to / std::max(1.0f, d) * kWalkSpeed * 1.3f;
        }
        else
        {
            fight(world, e, ai, centre, velocity, tick.player, tick, hooks);
        }
        break;
    }
    case SurvivorState::GoHome:
    {
        if (ai.homeBuilding <= 0 || goHome(kWalkSpeed))
        {
            ai.state = SurvivorState::Idle;
            ai.option = Option::Idle;
        }
        break;
    }
    case SurvivorState::Dead:
        break;
    }
    if (glm::length(velocity) > 1.0f) ai.faceTo = velocity;
}
