#include "systems/CombatSystem.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <map>

#include "components/Animation.h"
#include "components/Bleeding.h"
#include "components/Combat.h"
#include "components/Facing.h"
#include "components/Health.h"
#include "components/Inventory.h"
#include "components/MentalState.h"
#include "components/PlayerInput.h"
#include "components/Stamina.h"
#include "components/SurvivorAI.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"
#include "systems/InventorySystem.h"
#include "world/CombatRules.h"
#include "world/MindRules.h"
#include "world/SkillRules.h"

namespace
{
// The build a blow meets: a Brute is heavy, a Crawler is already low.
combatrules::Body bodyOf(const ZombieAI& ai)
{
    switch (ai.kind)
    {
    case ZombieKind::Brute: return combatrules::Body::Brute;
    case ZombieKind::Crawler: return combatrules::Body::Crawler;
    default: return combatrules::Body::Ordinary;
    }
}

glm::vec2 entityCenter(const Transform& transform)
{
    return glm::vec2(transform.x + 16.0f, transform.y + 16.0f);
}

float angleDelta(float a, float b)
{
    return std::atan2(std::sin(a - b), std::cos(a - b));
}

glm::vec2 unitOr(const glm::vec2& v, const glm::vec2& fallback)
{
    const float len2 = glm::dot(v, v);
    return len2 > 1e-6f ? v / std::sqrt(len2) : fallback;
}

bool armed(const Combat& combat)
{
    return combat.weapon.itemId >= 0 && combat.weapon.category != WeaponCategory::Unarmed;
}

// The blow lands: the zombie is thrown and reels, and its velocity carries it.
void throwBack(ZombieAI& ai, Velocity& vel, const glm::vec2& direction, float pixels)
{
    ZombieBlow::land(ai, direction, pixels);
    vel.dx = ai.knock.x;
    vel.dy = ai.knock.y;
}
} // namespace

CombatSystem::CombatSystem()
    : mRng(0xC0B7u) // reseeded from the run seed at each run start
    , mDist(0.0f, 1.0f)
{
}

glm::vec2 CombatSystem::zombieFacing(World& world, Entity zombie)
{
    if (world.hasComponent<Animation>(zombie))
    {
        return combatrules::dirToWorld(world.getComponent<Animation>(zombie).dir);
    }
    if (world.hasComponent<Velocity>(zombie))
    {
        const Velocity& v = world.getComponent<Velocity>(zombie);
        return unitOr(glm::vec2(v.dx, v.dy), glm::vec2(0.0f));
    }
    return glm::vec2(0.0f);
}

int CombatSystem::carriedRounds(const Combat& combat, const Inventory& inventory)
{
    if (combat.weapon.ammoItemId < 0) return 0;
    return InventorySystem::countItem(inventory, combat.weapon.ammoItemId);
}

bool CombatSystem::startReload(Combat& combat, const Inventory& inventory)
{
    if (combat.weapon.category != WeaponCategory::Ranged || combat.reloadTimer > 0.0f)
    {
        return false;
    }
    if (combat.jammed)
    {
        combat.reloadTimer = combatrules::kJamClearSeconds;
        return true;
    }
    const int missing = combat.weapon.magazine - combat.weapon.ammo;
    const int carried = carriedRounds(combat, inventory);
    if (missing <= 0 || carried <= 0)
    {
        return false;
    }
    // The pistol fills its magazine in one go; the shotgun takes a shell at a
    // time (updateReload carries on to the next).
    combat.reloadTimer = combatrules::reloadSeconds(combat.weapon.pellets, 1);
    return true;
}

bool CombatSystem::twoHanded(const WeaponStats& weapon)
{
    // The heavy blunt weapons and the shotgun take both hands.
    return weapon.itemId >= 0 && ((weapon.category == WeaponCategory::Blunt && weapon.knocksDown) ||
                                  (weapon.category == WeaponCategory::Ranged && weapon.pellets > 1));
}

void CombatSystem::update(World& world, const Input& input, const TileMap& tileMap, NoiseModel& noiseModel, float dtSeconds)
{
    mShotLines.clear();
    mSwung = false;
    mShoveSaves = 0;
    mRefusedTwoHanded = false;
    mFumbledReload = false;

    world.forEach<PlayerInput, Transform, Facing, Combat, Stamina>(
        [&](Entity playerEntity, PlayerInput&, Transform& transform, Facing& facing, Combat& combat, Stamina& stamina)
        {
            combat.stuckEvent = false;
            combat.shotEvent = 0;
            combat.wallEvent = false;
            combat.shoveEvent = false;
            combat.jamEvent = false;
            combat.brokeFree = false;

            if (world.hasComponent<Health>(playerEntity) && world.getComponent<Health>(playerEntity).current <= 0.0f)
            {
                return;
            }

            combat.phaseTimer = std::max(0.0f, combat.phaseTimer - dtSeconds);
            combat.recoveryDelayTimer = std::max(0.0f, combat.recoveryDelayTimer - dtSeconds);
            combat.grabImmunity = std::max(0.0f, combat.grabImmunity - dtSeconds);

            float actionStaminaMultiplier = 1.0f;
            if (world.hasComponent<MentalState>(playerEntity) && world.getComponent<MentalState>(playerEntity).inCrisis)
            {
                actionStaminaMultiplier = 1.5f;
            }
            actionStaminaMultiplier *= mBody.staminaScale;
            const bool isExhausted = combatrules::exhausted(stamina.current, stamina.maximum);
            const glm::vec2 centre = entityCenter(transform);

            // --- Held by the dead: Space struggles; nothing else, bar a blade.
            int holders = 0;
            world.forEach<ZombieAI, Health>(
                [&](Entity, ZombieAI& ai, Health& h)
                {
                    if (ai.holding && h.current > 0.0f) ++holders;
                });
            combat.holders = holders;
            if (holders > 0)
            {
                combat.phase = AttackPhase::Idle;
                combat.shoving = false;
                combat.reloadTimer = 0.0f;
                combat.grabState = GrabState::None;
                updateHeld(world, input, playerEntity, combat, stamina.current, dtSeconds);
                return;
            }
            combat.struggle = 0.0f;
            combat.heldStabTimer = 0.0f;

            if (combat.reloadTimer > 0.0f)
            {
                updateReload(world, playerEntity, combat, facing.running, dtSeconds);
            }

            // --- The take-down (right click) runs to its end once started.
            if (combat.grabState != GrabState::None)
            {
                updateTakeDown(world, input, noiseModel, playerEntity, centre, combat, dtSeconds);
                return;
            }

            if (combat.phase == AttackPhase::Idle)
            {
                if (combat.recoveryDelayTimer <= 0.0f)
                {
                    stamina.current = std::min(stamina.maximum,
                                               stamina.current + stamina.recoveryPerSecond * mBody.regenScale * dtSeconds);
                }

                // Right click: a take-down, on one turned away or on the floor.
                const float grabCost = kGrabStaminaCost * actionStaminaMultiplier;
                if (input.wasMouseButtonPressed(SDL_BUTTON_RIGHT) && stamina.current >= grabCost)
                {
                    Entity closest = kInvalidEntity;
                    float closestDist = kGrabRange;
                    world.forEach<ZombieAI, Transform, Health>(
                        [&](Entity ze, ZombieAI& ai, Transform& zt, Health& zh)
                        {
                            if (zh.current <= 0.0f) return;
                            const glm::vec2 zc = entityCenter(zt);
                            const float d = glm::length(zc - centre);
                            if (d >= closestDist) return;
                            const bool open = ZombieBlow::isDown(ai) ||
                                              combatrules::fromBehind(zombieFacing(world, ze), zc - centre);
                            if (!open) return;
                            closestDist = d;
                            closest = ze;
                        });
                    if (closest != kInvalidEntity)
                    {
                        stamina.current = std::max(0.0f, stamina.current - grabCost);
                        combat.grabState = GrabState::Grabbing;
                        combat.grabTimer = kGrabLungeSeconds;
                        combat.grabTarget = closest;
                        return;
                    }
                }

                // Space: the shove.
                const float shoveCost = combatrules::shoveStamina(armed(combat)) * actionStaminaMultiplier;
                if (input.wasKeyPressed(SDL_SCANCODE_SPACE) && stamina.current >= shoveCost * 0.5f)
                {
                    stamina.current = std::max(0.0f, stamina.current - shoveCost);
                    combat.recoveryDelayTimer = kRecoveryDelayAfterAttackSeconds;
                    combat.phase = AttackPhase::Windup;
                    combat.shoving = true;
                    combat.windupTotal = combatrules::kShoveWindupSeconds;
                    combat.recoveryTotal = combatrules::kShoveRecoverySeconds;
                    combat.phaseTimer = combat.windupTotal;
                    combat.hitProcessed = false;
                    combat.swingWillMiss = false;
                    combat.stompTarget = kInvalidEntity;
                    combat.reloadTimer = 0.0f;
                    return;
                }

                if (!input.wasMouseButtonPressed(SDL_BUTTON_LEFT))
                {
                    return;
                }

                const bool ranged = combat.weapon.category == WeaponCategory::Ranged;
                // A broken arm, not yet set, cannot take a two-handed weapon.
                if (mBody.noTwoHanded && twoHanded(combat.weapon))
                {
                    mRefusedTwoHanded = true;
                    return;
                }
                if (ranged)
                {
                    // A shotgun can stop between shells to fire; the pistol's
                    // magazine is all or nothing.
                    if (combat.reloadTimer > 0.0f)
                    {
                        if (combat.weapon.pellets <= 1 || combat.weapon.ammo <= 0 || combat.jammed) return;
                        combat.reloadTimer = 0.0f;
                    }
                    if (combat.jammed || combat.weapon.ammo <= 0)
                    {
                        combat.jamEvent = combat.jammed;
                        return; // a dry click
                    }
                }

                // A swing at one lying at your feet goes down at it: a stomp.
                combat.stompTarget = kInvalidEntity;
                if (!ranged)
                {
                    float best = combatrules::kStompReachPixels;
                    const float halfArc = glm::radians(std::max(combat.weapon.arcDegrees * 0.5f, 50.0f));
                    world.forEach<ZombieAI, Transform, Health>(
                        [&](Entity ze, ZombieAI& ai, Transform& zt, Health& zh)
                        {
                            if (zh.current <= 0.0f || !ZombieBlow::isDown(ai)) return;
                            const glm::vec2 to = entityCenter(zt) - centre;
                            const float d = glm::length(to);
                            if (d > best) return;
                            if (d > 4.0f && std::fabs(angleDelta(std::atan2(to.y, to.x), facing.angleRadians)) > halfArc) return;
                            best = d;
                            combat.stompTarget = ze;
                        });
                }

                const float swingScale = ranged ? 1.0f : mBody.swingStaminaScale;
                float attackCost = combat.weapon.staminaCost * actionStaminaMultiplier * swingScale;
                if (combat.stompTarget != kInvalidEntity)
                {
                    attackCost = combatrules::stompStamina(combat.weapon, combat.weapon.staminaCost) *
                                 actionStaminaMultiplier * swingScale;
                }
                if (stamina.current < attackCost * 0.5f)
                {
                    return;
                }
                stamina.current = std::max(0.0f, stamina.current - attackCost);
                combat.recoveryDelayTimer = kRecoveryDelayAfterAttackSeconds;
                combat.phase = AttackPhase::Windup;
                combat.shoving = false;
                combat.windupTotal = combatrules::windupSeconds(combat.weapon, isExhausted && !ranged) * mBody.windupScale;
                // Numb: now and then the blow comes late.
                if (mBody.hesitationChance > 0.0f && mDist(mRng) < mBody.hesitationChance)
                {
                    combat.windupTotal += mindrules::kHesitationSeconds;
                }
                mSwingDamageScale = (!ranged && mBody.doubleBlowChance > 0.0f && mDist(mRng) < mBody.doubleBlowChance) ? 2.0f : 1.0f;
                mSwung = !ranged;
                combat.recoveryTotal = combat.weapon.recoverySeconds;
                combat.phaseTimer = combat.windupTotal;
                combat.hitProcessed = false;
                combat.lastHitCount = 0;
                // Out of breath, a swing may go wide. (A gun's spread covers it.)
                combat.swingWillMiss = !ranged && isExhausted && mDist(mRng) < combatrules::kExhaustedMissChance;
                return;
            }

            if (combat.phase == AttackPhase::Windup)
            {
                // Panic shakes a swing (a gun's spread takes panic itself).
                if (!combat.shoving && combat.weapon.category != WeaponCategory::Ranged &&
                    world.hasComponent<MentalState>(playerEntity))
                {
                    const MentalState& mental = world.getComponent<MentalState>(playerEntity);
                    if (mental.panic >= 25.0f)
                    {
                        const float wobbleScale = std::min(mental.panic, 100.0f) / 100.0f;
                        const float maxWobbleRadians = glm::radians(15.0f) * wobbleScale;
                        std::uniform_real_distribution<float> wobbleDist(-maxWobbleRadians, maxWobbleRadians);
                        facing.angleRadians += wobbleDist(mRng);
                    }
                }

                if (combat.phaseTimer <= 0.0f)
                {
                    combat.phase = AttackPhase::Active;
                    combat.phaseTimer = kActiveSeconds;
                    combat.hitProcessed = false;
                }
                return;
            }

            if (combat.phase == AttackPhase::Active)
            {
                if (!combat.hitProcessed)
                {
                    if (combat.shoving)
                    {
                        resolveShove(world, tileMap, noiseModel, playerEntity, centre, facing.angleRadians, combat, isExhausted);
                    }
                    else if (combat.weapon.category == WeaponCategory::Ranged)
                    {
                        resolveShot(world, tileMap, noiseModel, playerEntity, centre, facing.angleRadians, combat);
                    }
                    else
                    {
                        resolveSwing(world, tileMap, noiseModel, playerEntity, centre, facing.angleRadians, combat,
                                     stamina.current);
                    }
                    combat.hitProcessed = true;
                }

                if (combat.phaseTimer <= 0.0f)
                {
                    combat.phase = AttackPhase::Recovery;
                    combat.phaseTimer = combat.recoveryTotal;
                }
                return;
            }

            if (combat.phase == AttackPhase::Recovery && combat.phaseTimer <= 0.0f)
            {
                combat.phase = AttackPhase::Idle;
                combat.shoving = false;
            }
        });
}

void CombatSystem::updateHeld(World& world, const Input& input, Entity player, Combat& combat, float& stamina, float dtSeconds)
{
    combatrules::Struggle struggle{combat.struggle, combat.struggleSinceTap};
    combatrules::struggleDrain(struggle, dtSeconds);
    if (input.wasKeyPressed(SDL_SCANCODE_SPACE))
    {
        combatrules::struggleTap(struggle, stamina);
    }
    combat.struggle = struggle.meter;
    combat.struggleSinceTap = struggle.sinceTap;

    const glm::vec2 centre = world.hasComponent<Transform>(player) ? entityCenter(world.getComponent<Transform>(player))
                                                                   : glm::vec2(0.0f);
    if (combatrules::struggleFree(struggle, combat.holders))
    {
        // Free: every one holding on is thrown off, and for a moment none
        // can take hold again.
        world.forEach<ZombieAI, Transform, Velocity, Health>(
            [&](Entity ze, ZombieAI& ai, Transform& zt, Velocity& vel, Health& h)
            {
                if (!ai.holding || h.current <= 0.0f) return;
                const glm::vec2 zc = entityCenter(zt);
                const combatrules::ShoveOutcome outcome = combatrules::shove(
                    bodyOf(ai), h.maximum > 0.0f ? h.current / h.maximum : 1.0f,
                    combatrules::fromBehind(zombieFacing(world, ze), zc - centre), false);
                throwBack(ai, vel, unitOr(zc - centre, glm::vec2(1.0f, 0.0f)), outcome.distancePixels);
                ai.stagger = std::max(ai.stagger, outcome.staggerSeconds);
                ai.lastBlow = -1;
                ai.hitFlash = 0.0f; // a shove draws no blood
                if (outcome.falls) ZombieBlow::knockDown(ai, combatrules::downSeconds(bodyOf(ai)));
            });
        combat.struggle = 0.0f;
        combat.holders = 0;
        combat.grabImmunity = combatrules::kFreeImmunitySeconds;
        combat.brokeFree = true;
        return;
    }

    // A short blade works while held: hold the button and stab the one
    // holding on.
    const bool shortBlade = combat.weapon.itemId >= 0 && combat.weapon.category == WeaponCategory::Bladed &&
                            combat.weapon.rangePixels <= 34.0f;
    if (!shortBlade || !input.isMouseButtonDown(SDL_BUTTON_LEFT))
    {
        combat.heldStabTimer = 0.0f;
        return;
    }
    combat.heldStabTimer -= dtSeconds;
    if (combat.heldStabTimer > 0.0f)
    {
        return;
    }
    combat.heldStabTimer = combatrules::kHeldStabIntervalSeconds;
    Entity target = kInvalidEntity;
    float best = 1e9f;
    world.forEach<ZombieAI, Transform, Health>(
        [&](Entity ze, ZombieAI& ai, Transform& zt, Health& h)
        {
            if (!ai.holding || h.current <= 0.0f) return;
            const float d = glm::length(entityCenter(zt) - centre);
            if (d < best)
            {
                best = d;
                target = ze;
            }
        });
    if (target == kInvalidEntity) return;
    Health& h = world.getComponent<Health>(target);
    ZombieAI& ai = world.getComponent<ZombieAI>(target);
    h.current = std::max(0.0f, h.current - effectiveDamage(combat) * combatrules::kHeldStabDamageScale);
    ai.lastDamagedBy = player;
    ai.lastBlow = static_cast<int>(combat.weapon.category);
    ai.lastBlowHeavy = false;
    ai.hitFlash = ZombieBlow::kFreshHit;
    degradeWeapon(combat, combatrules::wearForSwing(combatrules::SwingContact::Body));
}

void CombatSystem::updateReload(World& world, Entity player, Combat& combat, bool running, float dtSeconds)
{
    if (combat.weapon.category != WeaponCategory::Ranged || running)
    {
        combat.reloadTimer = 0.0f; // running spills the reload
        return;
    }
    combat.reloadTimer -= dtSeconds / std::max(0.1f, mBody.reloadScale);
    if (combat.reloadTimer > 0.0f)
    {
        return;
    }
    combat.reloadTimer = 0.0f;
    // Panicking hands drop a round: start again.
    if (mBody.fumbleChance > 0.0f && mDist(mRng) < mBody.fumbleChance)
    {
        mFumbledReload = true;
        combat.reloadTimer = combat.jammed ? combatrules::kJamClearSeconds
                                           : combatrules::reloadSeconds(combat.weapon.pellets, 1);
        return;
    }
    if (combat.jammed)
    {
        combat.jammed = false;
        return;
    }
    if (!world.hasComponent<Inventory>(player))
    {
        return;
    }
    Inventory& inv = world.getComponent<Inventory>(player);
    const int carried = carriedRounds(combat, inv);
    const int missing = combat.weapon.magazine - combat.weapon.ammo;
    const int load = combat.weapon.pellets > 1 ? std::min(1, std::min(missing, carried)) : std::min(missing, carried);
    if (load <= 0)
    {
        return;
    }
    InventorySystem::removeItem(inv, combat.weapon.ammoItemId, load);
    combat.weapon.ammo += load;
    // The shotgun takes the next shell, if there is room and one to take.
    if (combat.weapon.pellets > 1 && combat.weapon.ammo < combat.weapon.magazine && carried - load > 0)
    {
        combat.reloadTimer = combatrules::kShellReloadSeconds;
    }
}

void CombatSystem::updateTakeDown(World& world, const Input& input, NoiseModel& noiseModel, Entity player,
                                  const glm::vec2& centre, Combat& combat, float dtSeconds)
{
    combat.grabTimer -= dtSeconds;
    const bool targetAlive = combat.grabTarget != kInvalidEntity && world.hasComponent<Transform>(combat.grabTarget) &&
                             world.hasComponent<Health>(combat.grabTarget) &&
                             world.getComponent<Health>(combat.grabTarget).current > 0.0f;
    if (!targetAlive)
    {
        combat.grabState = GrabState::None;
        combat.grabTarget = kInvalidEntity;
        return;
    }
    if (world.hasComponent<Velocity>(combat.grabTarget) && combat.grabState != GrabState::Grabbing)
    {
        Velocity& zv = world.getComponent<Velocity>(combat.grabTarget);
        zv.dx = 0.0f;
        zv.dy = 0.0f;
    }

    if (combat.grabState == GrabState::Grabbing)
    {
        if (combat.grabTimer > 0.0f) return;
        const float dist = glm::length(entityCenter(world.getComponent<Transform>(combat.grabTarget)) - centre);
        if (dist < kGrabRange * 1.5f)
        {
            combat.grabState = GrabState::Holding;
            combat.grabTimer = kGrabHoldSeconds;
            if (world.hasComponent<ZombieAI>(combat.grabTarget))
            {
                ZombieAI& ai = world.getComponent<ZombieAI>(combat.grabTarget);
                ai.attackWindup = 0.0f;
                ai.stagger = std::max(ai.stagger, kGrabHoldSeconds + kGrabExecuteSeconds);
            }
        }
        else
        {
            combat.grabState = GrabState::None;
        }
        return;
    }
    if (combat.grabState == GrabState::Holding)
    {
        if (input.wasMouseButtonPressed(SDL_BUTTON_LEFT))
        {
            combat.grabState = GrabState::Executing;
            combat.grabTimer = kGrabExecuteSeconds;
        }
        else if (combat.grabTimer <= 0.0f)
        {
            combat.grabState = GrabState::None;
        }
        return;
    }
    if (combat.grabState == GrabState::Executing && combat.grabTimer <= 0.0f)
    {
        Health& zh = world.getComponent<Health>(combat.grabTarget);
        zh.current = std::max(0.0f, zh.current - kGrabExecuteDamage);
        if (world.hasComponent<ZombieAI>(combat.grabTarget))
        {
            ZombieAI& ai = world.getComponent<ZombieAI>(combat.grabTarget);
            ai.lastDamagedBy = player;
            ai.lastBlow = static_cast<int>(combat.weapon.category);
        }
        // With a blade it is done quietly.
        const bool blade = combat.weapon.itemId >= 0 && combat.weapon.category == WeaponCategory::Bladed;
        noiseModel.emitNoise(centre, blade ? NoiseTier::Soft : NoiseTier::Loud, blade ? 0.35f : 0.8f, player, NoiseSource::World,
                             1.0f, blade ? SoundId::HitBlade : SoundId::Stomp);
        combat.grabState = GrabState::None;
        combat.grabTarget = kInvalidEntity;
    }
}

void CombatSystem::degradeWeapon(Combat& combat, int wear)
{
    if (combat.weapon.category == WeaponCategory::Unarmed || combat.weapon.itemId < 0 || wear <= 0)
    {
        return;
    }
    combat.weapon.durability = std::max(0, combat.weapon.durability - wear);
    combat.weapon.condition = weaponConditionFor(combat.weapon.durability, combat.weapon.maxDurability);
}

float CombatSystem::effectiveRangePixels(const Combat& combat)
{
    if (combat.weapon.condition == WeaponCondition::Broken)
    {
        return combat.weapon.rangePixels * 0.6f;
    }
    if (combat.weapon.condition == WeaponCondition::Damaged)
    {
        return combat.weapon.rangePixels * 0.85f;
    }
    return combat.weapon.rangePixels;
}

float CombatSystem::effectiveArcDegrees(const Combat& combat)
{
    if (combat.weapon.condition == WeaponCondition::Broken)
    {
        return combat.weapon.arcDegrees * 0.7f;
    }
    return combat.weapon.arcDegrees;
}

float CombatSystem::effectiveDamage(const Combat& combat)
{
    if (combat.weapon.condition == WeaponCondition::Broken)
    {
        return combat.weapon.damage * 0.5f;
    }
    if (combat.weapon.condition == WeaponCondition::Damaged)
    {
        return combat.weapon.damage * 0.75f;
    }
    return combat.weapon.damage;
}

bool CombatSystem::isPointInsideArc(
    const glm::vec2& origin,
    float facingRadians,
    float arcDegrees,
    float rangePixels,
    const glm::vec2& point)
{
    const glm::vec2 toPoint = point - origin;
    const float distance = glm::length(toPoint);
    if (distance > rangePixels || distance < 0.001f)
    {
        return false;
    }

    const float angleToPoint = std::atan2(toPoint.y, toPoint.x);
    const float halfArc = glm::radians(arcDegrees * 0.5f);
    const float delta = std::fabs(angleDelta(angleToPoint, facingRadians));
    return delta <= halfArc;
}

bool CombatSystem::wallBetween(const TileMap& tileMap, const glm::vec2& from, const glm::vec2& to)
{
    const glm::vec2 delta = to - from;
    const float distance = glm::length(delta);
    const float tile = static_cast<float>(std::max(1, tileMap.tileWidth()));
    const int steps = std::max(1, static_cast<int>(distance / 6.0f));
    for (int i = 1; i < steps; ++i)
    {
        const glm::vec2 p = from + delta * (static_cast<float>(i) / static_cast<float>(steps));
        if (tileMap.blocksSight(static_cast<int>(std::floor(p.x / tile)), static_cast<int>(std::floor(p.y / tile))))
        {
            return true;
        }
    }
    return false;
}

void CombatSystem::resolveShove(World& world, const TileMap& tileMap, NoiseModel& noiseModel, Entity playerEntity,
                                const glm::vec2& playerCenter, float facingRadians, Combat& combat, bool exhausted)
{
    noiseModel.emitNoise(playerCenter, NoiseTier::Soft, 0.35f, playerEntity, NoiseSource::World, 1.0f, SoundId::Shove);
    combat.shoveEvent = true;
    combat.lastHitCount = 0;
    const float tile = static_cast<float>(std::max(1, tileMap.tileWidth()));

    struct Pushed
    {
        Entity entity;
        glm::vec2 from;
        glm::vec2 to;
    };
    std::vector<Pushed> pushed;
    world.forEach<ZombieAI, Transform, Velocity, Health>(
        [&](Entity ze, ZombieAI& ai, Transform& zt, Velocity& vel, Health& h)
        {
            if (h.current <= 0.0f || ZombieBlow::isDown(ai)) return;
            const glm::vec2 zc = entityCenter(zt);
            if (!isPointInsideArc(playerCenter, facingRadians, combatrules::kShoveArcDegrees, combatrules::kShoveRangePixels, zc) ||
                wallBetween(tileMap, playerCenter, zc))
            {
                return;
            }
            const glm::vec2 dir = unitOr(zc - playerCenter, glm::vec2(std::cos(facingRadians), std::sin(facingRadians)));
            float share = h.maximum > 0.0f ? h.current / h.maximum : 1.0f;
            if (mBody.shoveFellsWeakened && share < skillrules::kWeakenedShoveShare)
            {
                share = std::min(share, combatrules::kShoveFallHealthShare * 0.99f);
            }
            const combatrules::ShoveOutcome outcome = combatrules::shove(
                bodyOf(ai), share, combatrules::fromBehind(zombieFacing(world, ze), zc - playerCenter), exhausted);
            if (outcome.distancePixels <= 0.0f) return;
            ++combat.lastHitCount;
            if (ai.attackWindup > 0.0f || ai.state == ZombieState::Attack) ++mShoveSaves;

            // Where it would end up: a wall on the way stops it and hurts it.
            glm::vec2 to = zc + dir * outcome.distancePixels;
            bool wall = false;
            for (float t = 6.0f; t <= outcome.distancePixels; t += 6.0f)
            {
                const glm::vec2 p = zc + dir * (t + 8.0f);
                if (tileMap.isSolid(static_cast<int>(std::floor(p.x / tile)), static_cast<int>(std::floor(p.y / tile))))
                {
                    wall = true;
                    to = zc + dir * t;
                    break;
                }
            }

            throwBack(ai, vel, dir, outcome.distancePixels);
            ai.hitFlash = 0.0f; // a shove draws no blood
            ai.lastBlow = -1;
            ai.stagger = outcome.staggerSeconds;
            if (wall)
            {
                h.current = std::max(0.0f, h.current - combatrules::kShoveWallDamage);
                ai.lastDamagedBy = playerEntity;
                ai.stagger = std::max(ai.stagger, combatrules::kShoveWallStaggerSeconds);
            }
            if (outcome.falls)
            {
                ZombieBlow::knockDown(ai, combatrules::downSeconds(bodyOf(ai)));
            }
            pushed.push_back(Pushed{ze, zc, to});
        });

    // One thrown into another staggers that one too.
    for (const Pushed& p : pushed)
    {
        const glm::vec2 seg = p.to - p.from;
        const float segLen2 = std::max(1e-4f, glm::dot(seg, seg));
        world.forEach<ZombieAI, Transform, Velocity, Health>(
            [&](Entity ze, ZombieAI& ai, Transform& zt, Velocity& vel, Health& h)
            {
                if (h.current <= 0.0f || ZombieBlow::isDown(ai)) return;
                if (std::any_of(pushed.begin(), pushed.end(), [&](const Pushed& q) { return q.entity == ze; })) return;
                const glm::vec2 zc = entityCenter(zt);
                const float t = std::clamp(glm::dot(zc - p.from, seg) / segLen2, 0.0f, 1.0f);
                if (glm::length(p.from + seg * t - zc) > 18.0f) return;
                throwBack(ai, vel, seg / std::sqrt(segLen2), 12.0f);
                ai.hitFlash = 0.0f;
                ai.stagger = std::max(ai.stagger, combatrules::kShoveKnockOnStaggerSeconds);
            });
    }
}

void CombatSystem::resolveSwing(World& world, const TileMap& tileMap, NoiseModel& noiseModel, Entity playerEntity,
                                const glm::vec2& playerCenter, float facingRadians, Combat& combat, float& stamina)
{
    const bool heavySwing = combat.weapon.knocksDown || combat.weapon.windupSeconds >= 0.3f;
    noiseModel.emitNoise(playerCenter, NoiseTier::Medium, 0.45f, playerEntity, NoiseSource::World, 1.0f,
                         heavySwing ? SoundId::SwingHeavy : SoundId::SwingLight);

    combat.lastHitCount = 0;
    const float rangePixels = effectiveRangePixels(combat);
    const float arcDegrees = effectiveArcDegrees(combat);
    const float damage = effectiveDamage(combat) * mSwingDamageScale;
    const float knockback = combat.weapon.knockbackPixels;
    const int blowKind = combat.weapon.itemId >= 0 ? static_cast<int>(combat.weapon.category)
                                                   : static_cast<int>(WeaponCategory::Unarmed);
    bool stuck = false;

    if (!combat.swingWillMiss)
    {
        world.forEach<ZombieAI, Transform, Velocity, Health>(
            [&](Entity zombieEntity, ZombieAI& zombieAI, Transform& zombieTransform, Velocity& zombieVelocity, Health& zombieHealth)
            {
                if (zombieHealth.current <= 0.0f)
                {
                    return; // already down; the game clears the dead after counting kills
                }
                const glm::vec2 zombieCenter = entityCenter(zombieTransform);
                const bool stomp = combat.stompTarget != kInvalidEntity;
                if (stomp && zombieEntity != combat.stompTarget)
                {
                    return; // a stomp comes down on the one on the floor
                }
                if (!stomp && !isPointInsideArc(playerCenter, facingRadians, arcDegrees, rangePixels, zombieCenter))
                {
                    return;
                }
                if (wallBetween(tileMap, playerCenter, zombieCenter))
                {
                    return; // swings stop at walls
                }

                float finalDamage = stomp ? combatrules::stompDamage(combat.weapon, damage) : damage;
                // From behind (by where it moves), a blow does more.
                if (!stomp)
                {
                    const float zombieSpeed2 = zombieVelocity.dx * zombieVelocity.dx + zombieVelocity.dy * zombieVelocity.dy;
                    if (zombieSpeed2 > 1.0f)
                    {
                        const float zombieFacing = std::atan2(zombieVelocity.dy, zombieVelocity.dx);
                        const glm::vec2 attackDir = zombieCenter - playerCenter;
                        const float delta = std::fabs(angleDelta(std::atan2(attackDir.y, attackDir.x), zombieFacing));
                        if (delta > glm::radians(90.0f))
                        {
                            finalDamage *= kBackstabMultiplier;
                        }
                    }
                }

                const float before = zombieHealth.current;
                zombieHealth.current = std::max(0.0f, zombieHealth.current - finalDamage);
                const float dealt = before - zombieHealth.current;
                const bool killed = zombieHealth.current <= 0.0f;
                zombieAI.lastDamagedBy = playerEntity;
                zombieAI.lastBlow = blowKind;
                zombieAI.lastBlowHeavy = combat.weapon.knocksDown;
                ++combat.lastHitCount;

                if (combat.weapon.causesBleeding && !killed)
                {
                    if (!world.hasComponent<Bleeding>(zombieEntity))
                    {
                        world.addComponent<Bleeding>(zombieEntity, Bleeding{2.0f, 8.0f});
                    }
                    else
                    {
                        world.getComponent<Bleeding>(zombieEntity).remainingSeconds = 8.0f;
                    }
                }

                const glm::vec2 pushDirection = unitOr(zombieCenter - playerCenter, glm::vec2(1.0f, 0.0f));
                const float distance = glm::length(zombieCenter - playerCenter);
                const float push = stomp ? 4.0f
                                         : combatrules::reachKnockback(combat.weapon, distance, knockback,
                                                                       zombieAI.attackWindup > 0.0f);
                const bool wasDown = ZombieBlow::isDown(zombieAI);
                ZombieBlow::land(zombieAI, pushDirection, push);
                zombieVelocity.dx = zombieAI.knock.x;
                zombieVelocity.dy = zombieAI.knock.y;
                if (!killed && (wasDown || combatrules::blowKnocksDown(combat.weapon, dealt)))
                {
                    ZombieBlow::knockDown(zombieAI, wasDown ? std::max(zombieAI.downTimer, 0.6f)
                                                            : combatrules::downSeconds(bodyOf(zombieAI)));
                }

                if (!stuck && mDist(mRng) < combatrules::stickChance(combat.weapon, dealt, killed, mBody.fightingLevel))
                {
                    stuck = true;
                }
            });

        // People (phase 8): a swing meets them as it meets the dead, all but
        // the one travelling with you. The game decides what it means.
        if (combat.stompTarget == kInvalidEntity)
        {
            world.forEach<SurvivorAI, Transform, Velocity, Health>(
                [&](Entity, SurvivorAI& ai, Transform& st, Velocity& sv, Health& sh)
                {
                    if (!ai.alive || ai.companion || sh.current <= 0.0f) return;
                    const glm::vec2 centre = entityCenter(st);
                    if (!isPointInsideArc(playerCenter, facingRadians, arcDegrees, rangePixels, centre)) return;
                    if (wallBetween(tileMap, playerCenter, centre)) return;
                    sh.current = std::max(0.0f, sh.current - damage);
                    ai.playerDamage += damage;
                    const glm::vec2 push = unitOr(centre - playerCenter, glm::vec2(1.0f, 0.0f)) * knockback * 4.0f;
                    sv.dx = push.x;
                    sv.dy = push.y;
                    ++combat.lastHitCount;
                });
        }
    }

    // What the swing met decides the wear: flesh 1, air nothing, a wall 2.
    combatrules::SwingContact contact = combat.lastHitCount > 0 ? combatrules::SwingContact::Body
                                                                : combatrules::SwingContact::Miss;
    if (contact == combatrules::SwingContact::Miss && combat.weapon.itemId >= 0)
    {
        const float tile = static_cast<float>(std::max(1, tileMap.tileWidth()));
        const glm::vec2 aim(std::cos(facingRadians), std::sin(facingRadians));
        for (float t = 12.0f; t <= rangePixels; t += 6.0f)
        {
            const glm::vec2 p = playerCenter + aim * t;
            if (tileMap.isSolid(static_cast<int>(std::floor(p.x / tile)), static_cast<int>(std::floor(p.y / tile))))
            {
                contact = combatrules::SwingContact::Wall;
                combat.wallEvent = true;
                break;
            }
        }
    }
    int wear = combatrules::wearForSwing(contact);
    // A broken arm: a missed swing wrenches the weapon too.
    if (contact == combatrules::SwingContact::Miss && combat.weapon.itemId >= 0) wear += static_cast<int>(mBody.missWear);
    // A skilled fighter spares the weapon: each point of wear may be saved.
    if (mBody.wearScale < 1.0f)
    {
        int kept = 0;
        for (int i = 0; i < wear; ++i)
        {
            if (mDist(mRng) < mBody.wearScale) ++kept;
        }
        wear = kept;
    }
    degradeWeapon(combat, wear);

    // A loud weapon (the frying pan) rings out when it lands.
    if (combat.lastHitCount > 0 && combat.weapon.noiseTier >= static_cast<int>(NoiseTier::Loud))
    {
        noiseModel.emitNoise(playerCenter, static_cast<NoiseTier>(combat.weapon.noiseTier), 0.6f, playerEntity,
                             NoiseSource::World, 1.0f, SoundId::HitPan);
    }

    if (stuck)
    {
        combat.stuckEvent = true;
        combat.recoveryTotal += combatrules::kStickRecoverySeconds;
        stamina = std::max(0.0f, stamina - combatrules::kStickStamina);
    }
    combat.stompTarget = kInvalidEntity;
}

void CombatSystem::resolveShot(World& world, const TileMap& tileMap, NoiseModel& noiseModel, Entity playerEntity,
                               const glm::vec2& playerCenter, float facingRadians, Combat& combat)
{
    // A worn gun jams now and then: nothing fires until R clears it.
    const float conditionShare = static_cast<float>(combat.weapon.durability) /
                                 static_cast<float>(std::max(1, combat.weapon.maxDurability));
    if (mDist(mRng) < combatrules::jamChance(conditionShare))
    {
        combat.jammed = true;
        combat.jamEvent = true;
        return;
    }
    combat.weapon.ammo = std::max(0, combat.weapon.ammo - 1);
    degradeWeapon(combat, 1);
    combat.shotEvent = std::max(1, combat.weapon.pellets);

    const float tile = static_cast<float>(std::max(1, tileMap.tileWidth()));
    combatrules::SpreadInputs spread;
    if (world.hasComponent<Velocity>(playerEntity))
    {
        const Velocity& v = world.getComponent<Velocity>(playerEntity);
        spread.walking = v.dx * v.dx + v.dy * v.dy > 16.0f;
    }
    if (world.hasComponent<Facing>(playerEntity))
    {
        const Facing& f = world.getComponent<Facing>(playerEntity);
        spread.running = f.running && spread.walking;
        spread.crouchedStill = f.crouching && !spread.walking;
    }
    if (world.hasComponent<MentalState>(playerEntity))
    {
        MentalState& mental = world.getComponent<MentalState>(playerEntity);
        spread.panic = mental.panic;
        // The shot frightens the one who fires it, until they are used to it.
        mental.panic = std::min(100.0f, mental.panic + combatrules::shotPanic(mental.desensitisation));
    }
    if (world.hasComponent<Stamina>(playerEntity))
    {
        const Stamina& s = world.getComponent<Stamina>(playerEntity);
        spread.exhausted = combatrules::exhausted(s.current, s.maximum);
    }
    const float spreadRadians = glm::radians(combatrules::spreadDegrees(spread) + mBody.extraSpreadDegrees);
    const float aim = facingRadians + (mDist(mRng) * 2.0f - 1.0f) * spreadRadians;

    const int pellets = std::max(1, combat.weapon.pellets);
    const float cone = pellets > 1 ? glm::radians(combat.weapon.arcDegrees) : 0.0f;
    const float range = effectiveRangePixels(combat);
    std::map<Entity, int> pelletsTaken;
    for (int p = 0; p < pellets; ++p)
    {
        float angle = aim;
        if (pellets > 1)
        {
            angle += cone * (static_cast<float>(p) / static_cast<float>(pellets - 1) - 0.5f);
            angle += (mDist(mRng) - 0.5f) * cone / static_cast<float>(pellets * 2);
        }
        const glm::vec2 dir(std::cos(angle), std::sin(angle));
        glm::vec2 end = playerCenter + dir * range;
        Entity hit = kInvalidEntity;
        for (float t = 8.0f; t <= range; t += 4.0f)
        {
            const glm::vec2 point = playerCenter + dir * t;
            if (tileMap.blocksSight(static_cast<int>(std::floor(point.x / tile)), static_cast<int>(std::floor(point.y / tile))))
            {
                end = point;
                break;
            }
            float best = 11.0f;
            world.forEach<ZombieAI, Transform, Health>(
                [&](Entity ze, ZombieAI&, Transform& zt, Health& h)
                {
                    if (h.current <= 0.0f) return;
                    const float d = glm::length(entityCenter(zt) - point);
                    if (d < best)
                    {
                        best = d;
                        hit = ze;
                    }
                });
            if (hit != kInvalidEntity)
            {
                end = point;
                break;
            }
            // People (phase 8), all but the one travelling with you.
            Entity person = kInvalidEntity;
            world.forEach<SurvivorAI, Transform, Health>(
                [&](Entity se, SurvivorAI& ai, Transform& st, Health& h)
                {
                    if (!ai.alive || ai.companion || h.current <= 0.0f) return;
                    const float d = glm::length(entityCenter(st) - point);
                    if (d < best)
                    {
                        best = d;
                        person = se;
                    }
                });
            if (person != kInvalidEntity)
            {
                end = point;
                const float distance = glm::length(point - playerCenter);
                const float damage = pellets > 1 ? effectiveDamage(combat)
                                                 : combatrules::bulletDamage(effectiveDamage(combat), distance, tile);
                Health& h = world.getComponent<Health>(person);
                h.current = std::max(0.0f, h.current - damage);
                world.getComponent<SurvivorAI>(person).playerDamage += damage;
                ++combat.lastHitCount;
                break;
            }
        }
        mShotLines.emplace_back(playerCenter + dir * 14.0f, end);
        if (hit == kInvalidEntity) continue;

        ZombieAI& ai = world.getComponent<ZombieAI>(hit);
        Health& h = world.getComponent<Health>(hit);
        const glm::vec2 zc = entityCenter(world.getComponent<Transform>(hit));
        const float distance = glm::length(zc - playerCenter);
        const float damage = pellets > 1 ? effectiveDamage(combat)
                                         : combatrules::bulletDamage(effectiveDamage(combat), distance, tile);
        h.current = std::max(0.0f, h.current - damage);
        ai.lastDamagedBy = playerEntity;
        ai.lastBlow = static_cast<int>(WeaponCategory::Ranged);
        ai.lastBlowHeavy = false;
        ++pelletsTaken[hit];
        if (world.hasComponent<Velocity>(hit))
        {
            throwBack(ai, world.getComponent<Velocity>(hit), dir, combat.weapon.knockbackPixels);
        }
    }
    for (const auto& [zombie, taken] : pelletsTaken)
    {
        ++combat.lastHitCount;
        if (pellets <= 1) continue;
        ZombieAI& ai = world.getComponent<ZombieAI>(zombie);
        const float distance = glm::length(entityCenter(world.getComponent<Transform>(zombie)) - playerCenter);
        if (world.getComponent<Health>(zombie).current > 0.0f && combatrules::pelletsFell(taken, distance, tile))
        {
            ZombieBlow::knockDown(ai, combatrules::downSeconds(bodyOf(ai)));
        }
    }

    const NoiseTier tier = static_cast<NoiseTier>(std::clamp(combat.weapon.noiseTier, 1, 5));
    noiseModel.emitNoise(playerCenter, tier, tier == NoiseTier::Explosive ? 0.9f : 0.8f, playerEntity, NoiseSource::World,
                         1.0f, pellets > 1 ? SoundId::Shotgun : SoundId::Pistol);
}
