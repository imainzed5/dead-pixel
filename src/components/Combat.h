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
    int ammo = 0;    // guns: rounds loaded now
    int itemId = -1; // the inventory item in hand; -1 for none

    // Phase 3: what makes one weapon differ from another of its class.
    float windupSeconds = 0.20f;
    float recoverySeconds = 0.30f;
    bool canStick = false;   // a heavy blow may lodge it (Axe, Machete, Spear)
    bool knocksDown = false; // any hit puts them on the floor (Sledgehammer)
    bool reach = false;      // keeps one off you: hits before a lunge lands (Spear)
    int noiseTier = 3;       // NoiseTier of a hit (Medium); a frying pan is Loud
    int magazine = 0;        // guns: rounds it holds
    int ammoItemId = -1;     // guns: what it loads
    int pellets = 1;         // guns: shot per trigger pull
};

// Condition bands for durability left.
inline WeaponCondition weaponConditionFor(int durability, int maxDurability)
{
    if (durability <= 0)
    {
        return WeaponCondition::Broken;
    }
    const float ratio = static_cast<float>(durability) / static_cast<float>(maxDurability > 0 ? maxDurability : 1);
    if (ratio > 0.66f) return WeaponCondition::Good;
    if (ratio > 0.33f) return WeaponCondition::Worn;
    return WeaponCondition::Damaged;
}

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
        w.windupSeconds = 0.14f; w.recoverySeconds = 0.22f;
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

    // Take down (right click, from behind or on the floor)
    GrabState grabState = GrabState::None;
    float grabTimer = 0.0f;
    Entity grabTarget = 0xFFFFFFFF; // kInvalidEntity

    // The swing under way: its own wind-up and recovery (seconds).
    float windupTotal = 0.20f;
    float recoveryTotal = 0.30f;
    bool shoving = false;       // this "swing" is a shove (Space)
    Entity stompTarget = 0xFFFFFFFF; // a swing aimed down at one on the floor

    // Held by the dead (phase 3): how many hold on, the struggle meter, and
    // a moment after breaking free when none can take hold again.
    int holders = 0;
    float struggle = 0.0f;
    float struggleSinceTap = 0.0f;
    float grabImmunity = 0.0f;
    float heldStabTimer = 0.0f;
    bool brokeFree = false; // set the tick the player breaks free

    // Guns: a reload or a jam being cleared (seconds left), and whether the
    // gun is jammed.
    float reloadTimer = 0.0f;
    bool jammed = false;

    // What happened this tick, for the game to show and sound: a blade
    // lodged, a gun fired (pellet count), a swing hit a wall, a shove landed.
    bool stuckEvent = false;
    int shotEvent = 0;
    bool wallEvent = false;
    bool shoveEvent = false;
    bool jamEvent = false;
};
