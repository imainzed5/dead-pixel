#include "systems/CombatSystem.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

#include "components/Bleeding.h"
#include "components/Combat.h"
#include "components/Facing.h"
#include "components/Health.h"
#include "components/MentalState.h"
#include "components/PlayerInput.h"
#include "components/Projectile.h"
#include "components/Sprite.h"
#include "components/Stamina.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"

namespace
{
glm::vec2 entityCenter(const Transform& transform)
{
    return glm::vec2(transform.x + 16.0f, transform.y + 16.0f);
}

float angleDelta(float a, float b)
{
    return std::atan2(std::sin(a - b), std::cos(a - b));
}
}

CombatSystem::CombatSystem()
    : mRng(std::random_device{}())
    , mDist(0.0f, 1.0f)
{
}

void CombatSystem::update(World& world, const Input& input, const TileMap& tileMap, NoiseModel& noiseModel, float dtSeconds)
{
    std::vector<Entity> destroyedEntities;

    world.forEach<PlayerInput, Transform, Facing, Combat, Stamina>(
        [&](Entity playerEntity, PlayerInput&, Transform& transform, Facing& facing, Combat& combat, Stamina& stamina)
        {
            if (world.hasComponent<Health>(playerEntity))
            {
                const Health& health = world.getComponent<Health>(playerEntity);
                if (health.current <= 0.0f)
                {
                    return;
                }
            }

            combat.phaseTimer = std::max(0.0f, combat.phaseTimer - dtSeconds);
            combat.recoveryDelayTimer = std::max(0.0f, combat.recoveryDelayTimer - dtSeconds);

            // --- Grab state machine ---
            if (combat.grabState != GrabState::None)
            {
                combat.grabTimer -= dtSeconds;
                const glm::vec2 pCenter = entityCenter(transform);

                if (combat.grabState == GrabState::Grabbing)
                {
                    if (combat.grabTimer <= 0.0f)
                    {
                        // Check if grab target still alive and in range
                        if (combat.grabTarget != kInvalidEntity &&
                            world.hasComponent<Transform>(combat.grabTarget) &&
                            world.hasComponent<Health>(combat.grabTarget))
                        {
                            const Transform& zt = world.getComponent<Transform>(combat.grabTarget);
                            const float dist = glm::length(entityCenter(zt) - pCenter);
                            if (dist < kGrabRange * 1.5f && world.getComponent<Health>(combat.grabTarget).current > 0.0f)
                            {
                                combat.grabState = GrabState::Holding;
                                combat.grabTimer = kGrabHoldSeconds;
                                // Pin zombie velocity
                                if (world.hasComponent<Velocity>(combat.grabTarget))
                                {
                                    Velocity& zv = world.getComponent<Velocity>(combat.grabTarget);
                                    zv.dx = 0.0f; zv.dy = 0.0f;
                                }
                            }
                            else
                            {
                                combat.grabState = GrabState::None;
                            }
                        }
                        else
                        {
                            combat.grabState = GrabState::None;
                        }
                    }
                }
                else if (combat.grabState == GrabState::Holding)
                {
                    // Pin zombie
                    if (combat.grabTarget != kInvalidEntity && world.hasComponent<Velocity>(combat.grabTarget))
                    {
                        Velocity& zv = world.getComponent<Velocity>(combat.grabTarget);
                        zv.dx = 0.0f; zv.dy = 0.0f;
                    }

                    // Left click during hold → execute
                    if (input.wasMouseButtonPressed(SDL_BUTTON_LEFT))
                    {
                        combat.grabState = GrabState::Executing;
                        combat.grabTimer = kGrabExecuteSeconds;
                    }
                    else if (combat.grabTimer <= 0.0f)
                    {
                        // Hold expired
                        combat.grabState = GrabState::None;
                    }
                }
                else if (combat.grabState == GrabState::Executing)
                {
                    if (combat.grabTimer <= 0.0f)
                    {
                        // Execute the zombie
                        if (combat.grabTarget != kInvalidEntity && world.hasComponent<Health>(combat.grabTarget))
                        {
                            Health& zh = world.getComponent<Health>(combat.grabTarget);
                            zh.current = std::max(0.0f, zh.current - kGrabExecuteDamage);
                            if (zh.current <= 0.0f)
                            {
                                destroyedEntities.push_back(combat.grabTarget);
                            }
                            // Loud noise from execution
                            noiseModel.emitNoise(pCenter, NoiseTier::Loud, 0.8f, playerEntity);
                        }
                        combat.grabState = GrabState::None;
                        combat.grabTarget = kInvalidEntity;
                    }
                }
                return; // Skip normal attack while grabbing
            }

            // --- Right-click: grab attempt ---
            if (input.wasMouseButtonPressed(SDL_BUTTON_RIGHT) &&
                combat.phase == AttackPhase::Idle &&
                stamina.current >= kGrabStaminaCost)
            {
                const glm::vec2 pCenter = entityCenter(transform);
                Entity closest = kInvalidEntity;
                float closestDist = kGrabRange;

                world.forEach<ZombieAI, Transform, Health>(
                    [&](Entity ze, ZombieAI&, Transform& zt, Health& zh)
                    {
                        if (zh.current <= 0.0f) return;
                        const float d = glm::length(entityCenter(zt) - pCenter);
                        if (d < closestDist)
                        {
                            closestDist = d;
                            closest = ze;
                        }
                    });

                if (closest != kInvalidEntity)
                {
                    stamina.current = std::max(0.0f, stamina.current - kGrabStaminaCost);
                    combat.grabState = GrabState::Grabbing;
                    combat.grabTimer = kGrabLungeSeconds;
                    combat.grabTarget = closest;
                }
            }

            // --- Normal melee attack ---
            if (combat.phase == AttackPhase::Idle)
            {
                if (combat.recoveryDelayTimer <= 0.0f)
                {
                    stamina.current = std::min(stamina.maximum, stamina.current + stamina.recoveryPerSecond * dtSeconds);
                }

                // Ranged: check ammo
                const bool canFire = combat.weapon.category != WeaponCategory::Ranged || combat.weapon.ammo > 0;

                if (input.wasMouseButtonPressed(SDL_BUTTON_LEFT) &&
                    stamina.current >= combat.weapon.staminaCost * 0.5f &&
                    canFire)
                {
                    stamina.current = std::max(0.0f, stamina.current - combat.weapon.staminaCost);
                    combat.recoveryDelayTimer = kRecoveryDelayAfterAttackSeconds;
                    combat.phase = AttackPhase::Windup;
                    combat.phaseTimer = kWindupSeconds;
                    combat.hitProcessed = false;
                    combat.lastHitCount = 0;

                    if (combat.weapon.category == WeaponCategory::Ranged)
                    {
                        combat.weapon.ammo = std::max(0, combat.weapon.ammo - 1);
                    }

                    const bool exhausted = stamina.maximum > 0.0f && stamina.current <= stamina.maximum * kExhaustedThreshold;
                    combat.swingWillMiss = exhausted && mDist(mRng) < kExhaustedMissChance;
                }

                return;
            }

            if (combat.phase == AttackPhase::Windup)
            {
                // Panic aim wobble: offset facing when panicking
                if (world.hasComponent<MentalState>(playerEntity))
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
                    resolveSwing(
                        world,
                        tileMap,
                        noiseModel,
                        playerEntity,
                        entityCenter(transform),
                        facing.angleRadians,
                        combat,
                        destroyedEntities);
                    combat.hitProcessed = true;
                }

                if (combat.phaseTimer <= 0.0f)
                {
                    combat.phase = AttackPhase::Recovery;
                    combat.phaseTimer = kRecoverySeconds;
                }
                return;
            }

            if (combat.phase == AttackPhase::Recovery)
            {
                if (combat.phaseTimer <= 0.0f)
                {
                    combat.phase = AttackPhase::Idle;
                }
            }
        });

    for (Entity entity : destroyedEntities)
    {
        if (world.hasComponent<ZombieAI>(entity))
        {
            world.destroyEntity(entity);
        }
    }
}

void CombatSystem::degradeWeapon(Combat& combat)
{
    combat.weapon.durability = std::max(0, combat.weapon.durability - 1);

    if (combat.weapon.durability <= 0)
    {
        combat.weapon.condition = WeaponCondition::Broken;
        return;
    }

    const float ratio = static_cast<float>(combat.weapon.durability) / static_cast<float>(combat.weapon.maxDurability);
    if (ratio > 0.66f)
    {
        combat.weapon.condition = WeaponCondition::Good;
    }
    else if (ratio > 0.33f)
    {
        combat.weapon.condition = WeaponCondition::Worn;
    }
    else
    {
        combat.weapon.condition = WeaponCondition::Damaged;
    }
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

void CombatSystem::resolveSwing(
    World& world,
    const TileMap& tileMap,
    NoiseModel& noiseModel,
    Entity playerEntity,
    const glm::vec2& playerCenter,
    float facingRadians,
    Combat& combat,
    std::vector<Entity>& destroyedEntities)
{
    degradeWeapon(combat);

    noiseModel.emitNoise(playerCenter, NoiseTier::Medium, 0.45f, playerEntity);

    combat.lastHitCount = 0;
    if (combat.swingWillMiss)
    {
        return;
    }

    // Ranged weapons: spawn a projectile entity instead of arc-sweep
    if (combat.weapon.category == WeaponCategory::Ranged)
    {
        Entity projEntity = world.createEntity();

        Transform projTransform{};
        projTransform.x = playerCenter.x;
        projTransform.y = playerCenter.y;

        Projectile proj{};
        proj.vx = std::cos(facingRadians);
        proj.vy = std::sin(facingRadians);
        proj.damage = effectiveDamage(combat);
        proj.rangeRemaining = effectiveRangePixels(combat);
        proj.source = playerEntity;

        Sprite projSprite{};
        projSprite.color = glm::vec4(1.0f, 0.9f, 0.3f, 1.0f);
        projSprite.layer = 11;

        world.addComponent<Transform>(projEntity, projTransform);
        world.addComponent<Projectile>(projEntity, proj);
        world.addComponent<Sprite>(projEntity, projSprite);

        noiseModel.emitNoise(playerCenter, NoiseTier::Loud, 0.8f, playerEntity);
        return;
    }

    const float rangePixels = effectiveRangePixels(combat);
    const float arcDegrees = effectiveArcDegrees(combat);
    const float damage = effectiveDamage(combat);

    const float knockback = combat.weapon.knockbackPixels;

    world.forEach<ZombieAI, Transform, Velocity, Health>(
        [&](Entity zombieEntity, ZombieAI&, Transform& zombieTransform, Velocity& zombieVelocity, Health& zombieHealth)
        {
            const glm::vec2 zombieCenter = entityCenter(zombieTransform);
            if (!isPointInsideArc(playerCenter, facingRadians, arcDegrees, rangePixels, zombieCenter))
            {
                return;
            }

            // Backstab check: derive zombie facing from velocity, then check
            // if player attack comes from behind the zombie's ±90° vision cone.
            float finalDamage = damage;
            const float zombieSpeed2 = zombieVelocity.dx * zombieVelocity.dx + zombieVelocity.dy * zombieVelocity.dy;
            if (zombieSpeed2 > 1.0f)
            {
                const float zombieFacing = std::atan2(zombieVelocity.dy, zombieVelocity.dx);
                const glm::vec2 attackDir = zombieCenter - playerCenter;
                const float attackAngle = std::atan2(attackDir.y, attackDir.x);
                const float delta = std::fabs(angleDelta(attackAngle, zombieFacing));
                // Attack coming from behind if angle difference > 90 degrees
                if (delta > glm::radians(90.0f))
                {
                    finalDamage *= kBackstabMultiplier;
                }
            }

            zombieHealth.current = std::max(0.0f, zombieHealth.current - finalDamage);
            ++combat.lastHitCount;

            // Apply bleeding from bladed weapons to surviving zombies
            if (combat.weapon.causesBleeding && zombieHealth.current > 0.0f)
            {
                if (!world.hasComponent<Bleeding>(zombieEntity))
                {
                    world.addComponent<Bleeding>(zombieEntity, Bleeding{2.0f, 8.0f});
                }
                else
                {
                    // Refresh bleeding duration
                    world.getComponent<Bleeding>(zombieEntity).remainingSeconds = 8.0f;
                }
            }

            glm::vec2 pushDirection = zombieCenter - playerCenter;
            if (glm::dot(pushDirection, pushDirection) > 0.001f)
            {
                pushDirection = glm::normalize(pushDirection);
            }
            else
            {
                pushDirection = glm::vec2(1.0f, 0.0f);
            }

            const glm::vec2 pushedCenter = zombieCenter + pushDirection * knockback;
            const float proposedX = pushedCenter.x - kSpriteSize * 0.5f;
            const float proposedY = pushedCenter.y - kSpriteSize * 0.5f;

            const float collisionCenterX = proposedX + kSpriteSize * 0.5f;
            const float collisionCenterY = proposedY + kCollisionCenterYOffset;

            const float left = collisionCenterX - kCollisionWidth * 0.5f;
            const float right = collisionCenterX + kCollisionWidth * 0.5f;
            const float top = collisionCenterY - kCollisionHeight * 0.5f;
            const float bottom = collisionCenterY + kCollisionHeight * 0.5f;

            if (!tileMap.isSolidAtWorldRect(left, top, right, bottom))
            {
                zombieTransform.x = proposedX;
                zombieTransform.y = proposedY;
            }

            zombieVelocity.dx = pushDirection.x * 120.0f;
            zombieVelocity.dy = pushDirection.y * 120.0f;

            if (zombieHealth.current <= 0.0f)
            {
                destroyedEntities.push_back(zombieEntity);
            }
        });
}
