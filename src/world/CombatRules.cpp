#include "world/CombatRules.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace combatrules
{
bool exhausted(float stamina, float staminaMax)
{
    return staminaMax > 0.0f && stamina <= staminaMax * kExhaustedShare;
}

float windupSeconds(const WeaponStats& weapon, bool isExhausted)
{
    float seconds = weapon.windupSeconds;
    if (weapon.reach)
    {
        seconds = std::max(0.02f, seconds - kReachHeadStartSeconds);
    }
    return isExhausted ? seconds * kExhaustedWindupScale : seconds;
}

int wearForSwing(SwingContact contact)
{
    switch (contact)
    {
    case SwingContact::Miss: return 0;
    case SwingContact::Body: return 1;
    case SwingContact::Wall: return 2;
    }
    return 0;
}

float shoveStamina(bool armed)
{
    return armed ? kShoveStamina : kShoveStaminaUnarmed;
}

ShoveOutcome shove(Body body, float healthShare, bool isFromBehind, bool shoverExhausted)
{
    ShoveOutcome out;
    if (body == Body::Crawler)
    {
        return out; // low on the ground: nothing to push
    }
    if (body == Body::Brute)
    {
        out.distancePixels = kBruteShoveDistancePixels;
    }
    else
    {
        out.distancePixels = kShoveDistancePixels;
        out.staggerSeconds = kShoveStaggerSeconds;
        out.falls = healthShare < kShoveFallHealthShare || isFromBehind;
    }
    if (shoverExhausted)
    {
        out.distancePixels *= 0.5f;
    }
    return out;
}

bool fromBehind(const glm::vec2& targetFacing, const glm::vec2& attackerToTarget)
{
    const float lenF = glm::length(targetFacing);
    const float lenA = glm::length(attackerToTarget);
    if (lenF < 1e-4f || lenA < 1e-4f)
    {
        return false;
    }
    // The attacker stands behind when the blow travels the way the target
    // faces: the target's front points away from the attacker.
    return glm::dot(targetFacing / lenF, attackerToTarget / lenA) > 0.0f;
}

LungeResult lungeResult(float roll, bool fromBehindUncovered, bool playerExhausted, bool alreadyHeld)
{
    if (fromBehindUncovered || playerExhausted || alreadyHeld)
    {
        return LungeResult::Grab;
    }
    return roll < kClawShare ? LungeResult::Claw : LungeResult::Grab;
}

float clawDamage(float base, bool fromBehindUncovered)
{
    return fromBehindUncovered ? base * kBackClawScale : base;
}

float struggleTap(Struggle& struggle, float& stamina)
{
    const bool winded = stamina < kStruggleTapStamina;
    const float spent = std::min(stamina, kStruggleTapStamina);
    stamina -= spent;
    struggle.meter += winded ? 0.5f : 1.0f;
    struggle.sinceTap = 0.0f;
    return spent;
}

void struggleDrain(Struggle& struggle, float dtSeconds)
{
    const float before = struggle.sinceTap;
    struggle.sinceTap += dtSeconds;
    // Only the part of this step past the pause drains.
    const float draining = std::max(0.0f, struggle.sinceTap - std::max(before, kStruggleDrainDelaySeconds));
    struggle.meter = std::max(0.0f, struggle.meter - kStruggleDrainPerSecond * draining);
}

float struggleNeeded(int holders)
{
    return kStrugglePerHolder * static_cast<float>(std::max(1, holders));
}

bool struggleFree(const Struggle& struggle, int holders)
{
    return struggle.meter >= struggleNeeded(holders) - 1e-4f;
}

float downSeconds(Body body)
{
    return body == Body::Brute ? kBruteDownSeconds : kDownSeconds;
}

bool blowKnocksDown(const WeaponStats& weapon, float damageDealt)
{
    if (weapon.knocksDown)
    {
        return true;
    }
    return weapon.category == WeaponCategory::Blunt && damageDealt >= kHeavyBluntDownDamage;
}

float stompDamage(const WeaponStats& weapon, float weaponDamage)
{
    if (weapon.category == WeaponCategory::Unarmed || weapon.itemId < 0)
    {
        return kUnarmedStompDamage;
    }
    return weaponDamage * kStompScale;
}

float stompStamina(const WeaponStats& weapon, float swingStamina)
{
    if (weapon.category == WeaponCategory::Unarmed || weapon.itemId < 0)
    {
        return kUnarmedStompStamina;
    }
    return swingStamina;
}

float stickChance(const WeaponStats& weapon, float damageDealt, bool killed, int fightingLevel)
{
    if (!weapon.canStick || (!killed && damageDealt < kStickDamage))
    {
        return 0.0f;
    }
    float chance = 0.12f;
    if (weapon.condition == WeaponCondition::Worn) chance = 0.20f;
    else if (weapon.condition == WeaponCondition::Damaged || weapon.condition == WeaponCondition::Broken) chance = 0.30f;
    if (fightingLevel >= 3)
    {
        chance *= 0.5f;
    }
    return chance;
}

float reachKnockback(const WeaponStats& weapon, float distancePixels, float knockback, bool targetWindingUp)
{
    if (!weapon.reach || !targetWindingUp)
    {
        return knockback;
    }
    return std::max(knockback, weapon.rangePixels - distancePixels);
}

float spreadDegrees(const SpreadInputs& in)
{
    float spread = in.baseDegrees;
    if (in.running) spread += 10.0f;
    else if (in.walking) spread += 4.0f;
    if (in.panic > 25.0f) spread += (in.panic - 25.0f) * 0.12f;
    if (in.exhausted) spread += 6.0f;
    if (in.crouchedStill) spread *= 0.6f;
    return spread;
}

float bulletDamage(float damage, float distancePixels, float tilePixels)
{
    return distancePixels <= tilePixels ? damage * kPointBlankScale : damage;
}

bool pelletsFell(int pelletsTaken, float distancePixels, float tilePixels)
{
    return static_cast<float>(pelletsTaken) >= kShotgunFallPellets && distancePixels <= kShotgunFallTiles * tilePixels;
}

float jamChance(float conditionShare)
{
    return conditionShare < kJamConditionShare ? kJamChance : 0.0f;
}

float reloadSeconds(int pellets, int roundsToLoad)
{
    if (roundsToLoad <= 0)
    {
        return 0.0f;
    }
    // Shotguns (more than one pellet) take one shell at a time.
    return pellets > 1 ? kShellReloadSeconds * static_cast<float>(roundsToLoad) : kPistolReloadSeconds;
}

float shotPanic(float desensitisation)
{
    return desensitisation < kShotPanicUnderDesensitisation ? kShotPanic : 0.0f;
}

float speedRating(const WeaponStats& weapon)
{
    // The quickest (bare hands, 0.36 s) to the slowest (the Sledgehammer, 1.02 s).
    const float total = weapon.windupSeconds + weapon.recoverySeconds;
    return std::clamp(1.0f - (total - 0.30f) / 0.80f, 0.05f, 1.0f);
}

std::string specialLine(const WeaponStats& weapon)
{
    std::string line;
    const auto add = [&](const char* part)
    {
        if (!line.empty()) line += ' ';
        line += part;
    };
    if (weapon.category == WeaponCategory::Ranged)
    {
        add(weapon.noiseTier >= 5 ? "Heard across town." : "Loud.");
        add(weapon.pellets > 1 ? "A spread of shot." : "One round at a time.");
        return line;
    }
    if (weapon.causesBleeding) add("Bleeds.");
    if (weapon.canStick) add("Can stick.");
    if (weapon.knocksDown) add("Knocks down.");
    if (weapon.reach) add("Keeps one at bay.");
    if (weapon.noiseTier >= 4) add("Loud.");
    return line;
}

glm::vec2 throwLanding(const glm::vec2& from, const glm::vec2& target, float rangePixels)
{
    const glm::vec2 delta = target - from;
    const float length = glm::length(delta);
    if (length <= rangePixels || length < 1e-4f)
    {
        return target;
    }
    return from + delta / length * rangePixels;
}

float throwArcHeight(float progress, float distancePixels)
{
    const float t = std::clamp(progress, 0.0f, 1.0f);
    const float peak = std::clamp(distancePixels * 0.18f, 6.0f, 48.0f);
    return 4.0f * peak * t * (1.0f - t);
}

bool alarmSurvives(float roll)
{
    return roll < 1.0f / 3.0f;
}

float flareSeconds(bool raining)
{
    return raining ? kFlareSeconds * 0.5f : kFlareSeconds;
}

glm::vec2 dirToWorld(int dir)
{
    // Screen sector angle a, on the ground plane (gx, gy) = (x - y, x + y).
    const float a = static_cast<float>(dir & 7) * 0.78539816f;
    const float gx = std::cos(a);
    const float gy = std::sin(a);
    const glm::vec2 world((gx + gy) * 0.5f, (gy - gx) * 0.5f);
    return glm::normalize(world);
}
} // namespace combatrules
