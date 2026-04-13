#pragma once

#include "ecs/Types.h"

enum class AttackPhase
{
    Idle,
    Windup,
    Active,
    Recovery
};

enum class WeaponCondition
{
    Good,
    Worn,
    Damaged,
    Broken
};

enum class WeaponCategory
{
    Improvised = 0,
    Bladed = 1,
    Blunt = 2,
    Ranged = 3,
    Unarmed = 4
};

enum class GrabState
{
    None,
    Grabbing,    // Lunge toward zombie
    Holding,     // Pinning zombie
    Executing    // Execution attack
};

struct WeaponStats
{
    float arcDegrees = 90.0f;
    float rangePixels = 48.0f;
    float damage = 26.0f;
    float staminaCost = 15.0f;
    float knockbackPixels = 24.0f;
    int durability = 8;
    int maxDurability = 8;
    WeaponCondition condition = WeaponCondition::Good;
    WeaponCategory category = WeaponCategory::Improvised;
    bool causesBleeding = false;
    int ammo = 0;
};

inline WeaponStats makeWeaponStats(WeaponCategory cat)
{
    WeaponStats w{};
    w.category = cat;
    switch (cat)
    {
    case WeaponCategory::Improvised:
        w.arcDegrees = 90.0f;  w.rangePixels = 48.0f; w.damage = 26.0f;
        w.staminaCost = 15.0f; w.knockbackPixels = 24.0f;
        w.durability = 8; w.maxDurability = 8;
        break;
    case WeaponCategory::Bladed:
        w.arcDegrees = 60.0f;  w.rangePixels = 32.0f; w.damage = 42.0f;
        w.staminaCost = 12.0f; w.knockbackPixels = 12.0f;
        w.durability = 12; w.maxDurability = 12;
        w.causesBleeding = true;
        break;
    case WeaponCategory::Blunt:
        w.arcDegrees = 120.0f; w.rangePixels = 48.0f; w.damage = 30.0f;
        w.staminaCost = 20.0f; w.knockbackPixels = 48.0f;
        w.durability = 14; w.maxDurability = 14;
        break;
    case WeaponCategory::Ranged:
        w.arcDegrees = 15.0f;  w.rangePixels = 192.0f; w.damage = 55.0f;
        w.staminaCost = 5.0f;  w.knockbackPixels = 8.0f;
        w.durability = 999; w.maxDurability = 999;
        w.ammo = 6;
        break;
    case WeaponCategory::Unarmed:
        w.arcDegrees = 45.0f;  w.rangePixels = 26.0f; w.damage = 8.0f;
        w.staminaCost = 8.0f;  w.knockbackPixels = 16.0f;
        w.durability = 999; w.maxDurability = 999; // no degradation
        break;
    }
    return w;
}

struct Combat
{
    AttackPhase phase = AttackPhase::Idle;
    float phaseTimer = 0.0f;
    float recoveryDelayTimer = 0.0f;
    bool hitProcessed = false;
    bool swingWillMiss = false;
    int lastHitCount = 0;
    WeaponStats weapon{};

    // Grab mechanic
    GrabState grabState = GrabState::None;
    float grabTimer = 0.0f;
    Entity grabTarget = 0xFFFFFFFF; // kInvalidEntity
};
