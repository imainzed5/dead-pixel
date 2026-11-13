// The blows of the dead: the telegraphed lunge, the hold and its bites, and
// what each kind's blow does (phase 4: the Brute puts you down, the Crawler
// bites your legs, a sprinting Runner always gets hold).
#include "systems/ZombieAISystem.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

#include "components/Health.h"
#include "components/Transform.h"
#include "components/Wounds.h"
#include "core/Tuning.h"
#include "world/CombatRules.h"

namespace
{
glm::ivec2 worldToTile(const TileMap& tileMap, const glm::vec2& world)
{
    return glm::ivec2(
        static_cast<int>(std::floor(world.x / static_cast<float>(tileMap.tileWidth()))),
        static_cast<int>(std::floor(world.y / static_cast<float>(tileMap.tileHeight()))));
}

float windupSeconds(ZombieKind kind)
{
    return kind == ZombieKind::Runner ? zombiekinds::kRunnerWindupSeconds : ZombieAISystem::kZombieAttackWindupSeconds;
}
} // namespace

bool ZombieAISystem::backCovered(const Ctx& c) const
{
    if (glm::dot(c.playerFacing, c.playerFacing) < 0.5f) return false;
    for (float t = 10.0f; t <= c.tileSize; t += 6.0f)
    {
        const glm::ivec2 behind = worldToTile(*c.tick->tileMap, c.playerCenter - c.playerFacing * t);
        if (c.tick->tileMap->isSolid(behind.x, behind.y)) return true;
    }
    return false;
}

void ZombieAISystem::hold(const Ctx& c, One& z, Velocity& v)
{
    ZombieAI& ai = *z.ai;
    World& world = *c.world;
    const ZombieTick& t = *c.tick;
    v.dx = 0.0f;
    v.dy = 0.0f;
    if (!c.playerValid || glm::length(c.playerCenter - z.center) > kZombieAttackRange * 1.8f)
    {
        ai.holding = false;
        return;
    }
    Health& playerHealth = world.getComponent<Health>(t.player);
    ai.state = ZombieState::Attack;
    ai.targetWorld = c.playerCenter;
    ai.biteTimer -= z.dt;
    if (ai.biteTimer > 0.0f) return;
    ai.biteTimer = combatrules::kBiteIntervalSeconds;
    ai.strikeTimer = kZombieStrikeSeconds;
    ai.strikeDir = glm::length(c.playerCenter - z.center) > 0.01f ? glm::normalize(c.playerCenter - z.center) : glm::vec2(0.0f);
    const int age = t.day - ai.spawnDay;
    const float stageDamage = age <= 3 ? 1.0f : age <= 10 ? 0.9f : age <= 20 ? 0.6f : 0.35f;
    playerHealth.current = std::max(0.0f, playerHealth.current - combatrules::kBiteDamage * stageDamage);
    if (t.playerBlows != nullptr)
    {
        PlayerBlow bite;
        bite.direction = ai.strikeDir;
        bite.kind = ai.kind;
        bite.follower = ai.follower;
        t.playerBlows->push_back(bite);
    }
    if (world.hasComponent<Wounds>(t.player) && nextRandom(ai, z.entity) < combatrules::kBiteWoundChance)
    {
        Wound w{};
        w.type = WoundType::Bite;
        w.contaminated = nextRandom(ai, z.entity) < kBiteContaminationChance;
        w.severity = 0.5f + nextRandom(ai, z.entity) * 0.5f;
        world.getComponent<Wounds>(t.player).active.push_back(w);
    }
}

void ZombieAISystem::attack(const Ctx& c, One& z, Velocity& v)
{
    ZombieAI& ai = *z.ai;
    World& world = *c.world;
    const ZombieTick& t = *c.tick;
    v.dx = 0.0f;
    v.dy = 0.0f;
    // The first half of a strike throws the body forward.
    if (ai.strikeTimer > kZombieStrikeSeconds * 0.5f)
    {
        v.dx = ai.strikeDir.x * kZombieStrikeSpeed;
        v.dy = ai.strikeDir.y * kZombieStrikeSpeed;
    }

    // Set on the attack with no prey of its own (a harness, an old save):
    // the player.
    if (ai.prey == kInvalidEntity && c.playerValid) ai.prey = t.player;
    const bool preyIsPlayer = ai.prey == t.player && t.player != kInvalidEntity;
    bool preyValid = ai.prey != kInvalidEntity && world.hasComponent<Transform>(ai.prey) &&
                     world.hasComponent<Health>(ai.prey) && world.getComponent<Health>(ai.prey).current > 0.0f;
    if (preyIsPlayer) preyValid = preyValid && c.playerValid;
    const glm::vec2 preyCenter = preyValid
        ? glm::vec2(world.getComponent<Transform>(ai.prey).x + 16.0f, world.getComponent<Transform>(ai.prey).y + 16.0f)
        : ai.targetWorld;
    const float distance = glm::length(preyCenter - z.center);
    const bool inRange = preyValid && distance <= kZombieAttackRange;

    // Mid-lunge it commits; otherwise it follows one stepping back.
    if (ai.attackWindup <= 0.0f && !inRange)
    {
        ai.state = preyValid ? ZombieState::Chase : ZombieState::Wander;
        ai.pathRepathTimer = kChaseRepathSeconds;
        ai.stateTimer = 0.0f;
        return;
    }
    if (ai.attackWindup <= 0.0f)
    {
        if (ai.attackCooldown <= 0.0f) ai.attackWindup = windupSeconds(ai.kind);
        return;
    }
    ai.attackWindup -= z.dt;
    if (ai.attackWindup > 0.0f) return;

    // Resolve the lunge.
    ai.attackWindup = 0.0f;
    ai.strikeTimer = kZombieStrikeSeconds;
    {
        const glm::vec2 toPrey = preyCenter - z.center;
        const float d = glm::length(toPrey);
        ai.strikeDir = d > 0.01f ? toPrey / d : glm::vec2(0.0f);
    }
    ai.attackCooldown = kZombieAttackCooldownSeconds * (0.85f + nextRandom(ai, z.entity) * 0.3f);
    const bool stillInReach = preyValid && distance <= kZombieAttackRange * kZombieAttackReachScale;
    const bool sprinting = ai.kind == ZombieKind::Runner && ai.sprintTimer > 0.0f;
    if (ai.kind == ZombieKind::Runner)
    {
        // The lunge ends the sprint; it has to start again.
        ai.sprintTimer = 0.0f;
        ai.windedTimer = zombiekinds::kRunnerWindedSeconds;
    }
    if (!stillInReach) return; // dodged: feet, a shove or a blow did it

    const float baseDamage = ai.clawDamage * Tuning::get("zombie.attack.damage", 7.0f) / 7.0f;
    if (!preyIsPlayer)
    {
        // Another of the living: a plain blow.
        Health& h = world.getComponent<Health>(ai.prey);
        h.current = std::max(0.0f, h.current - baseDamage * z.damageMultiplier);
        return;
    }

    Health& playerHealth = world.getComponent<Health>(t.player);
    const bool fromBehindUncovered =
        combatrules::fromBehind(c.playerFacing, c.playerCenter - z.center) && !backCovered(c);
    PlayerBlow blow;
    blow.direction = ai.strikeDir;
    blow.kind = ai.kind;
    blow.follower = ai.follower;

    if (ai.kind == ZombieKind::Crawler)
    {
        // Low: it bites the legs, and the leg drags for a while.
        playerHealth.current = std::max(0.0f, playerHealth.current - baseDamage * z.damageMultiplier);
        blow.hobbleSeconds = zombiekinds::kCrawlerHobbleSeconds;
        if (t.playerBlows != nullptr) t.playerBlows->push_back(blow);
        if (world.hasComponent<Wounds>(t.player) && nextRandom(ai, z.entity) < kZombieWoundChance)
        {
            Wound w{};
            w.type = WoundType::Bite;
            w.contaminated = nextRandom(ai, z.entity) < kBiteContaminationChance;
            w.severity = 0.4f + nextRandom(ai, z.entity) * 0.3f;
            world.getComponent<Wounds>(t.player).active.push_back(w);
        }
        return;
    }
    if (ai.kind == ZombieKind::Brute)
    {
        // Its claw is a shove: down you go.
        playerHealth.current = std::max(0.0f, playerHealth.current -
            combatrules::clawDamage(baseDamage, fromBehindUncovered) * z.damageMultiplier);
        blow.knockdownSeconds = zombiekinds::kBruteKnockdownSeconds;
        if (t.playerBlows != nullptr) t.playerBlows->push_back(blow);
        return;
    }

    // A lunge that reaches lands: a claw, or a grab that holds on. From
    // behind (back not to a wall), out of breath, already held, on the
    // floor, or caught by a sprinting Runner, it grabs.
    combatrules::LungeResult result = combatrules::lungeResult(
        nextRandom(ai, z.entity), fromBehindUncovered, c.playerExhausted, c.playerHeld);
    if (sprinting || t.playerDown) result = combatrules::LungeResult::Grab;
    if (c.playerImmune) result = combatrules::LungeResult::Claw; // just shaken free: no hold yet
    if (result == combatrules::LungeResult::Grab)
    {
        ai.holding = true;
        ai.biteTimer = combatrules::kBiteIntervalSeconds;
        return;
    }

    playerHealth.current = std::max(0.0f, playerHealth.current -
        combatrules::clawDamage(baseDamage, fromBehindUncovered) * z.damageMultiplier);
    if (t.playerBlows != nullptr) t.playerBlows->push_back(blow);

    if (world.hasComponent<Wounds>(t.player) && nextRandom(ai, z.entity) < kZombieWoundChance)
    {
        Wounds& wounds = world.getComponent<Wounds>(t.player);
        Wound w{};
        const float roll = nextRandom(ai, z.entity);
        if (roll < kZombieBiteShare)
        {
            w.type = WoundType::Bite;
            w.contaminated = nextRandom(ai, z.entity) < kBiteContaminationChance;
        }
        else if (roll < kZombieBiteShare + 0.4f)
        {
            w.type = WoundType::Cut;
        }
        else
        {
            w.type = WoundType::Bruise;
        }
        w.severity = 0.5f + nextRandom(ai, z.entity) * 0.5f;
        wounds.active.push_back(w);
    }
}
