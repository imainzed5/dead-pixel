#pragma once

#include <glm/vec2.hpp>

#include <cstdint>
#include <random>
#include <vector>

#include "components/Combat.h"
#include "components/Inventory.h"
#include "core/Input.h"
#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"

// What the body and mind bring to a fight (phase 5): skills, wounds, sleep
// and nerves, worked out by the game each tick (world/SkillRules,
// WoundRules, SleepRules, MindRules).
struct CombatBody
{
    float staminaScale = 1.0f;      // every action: panic (Uneasy +10 %)
    float swingStaminaScale = 1.0f; // swings: Fighting, an undressed arm
    float windupScale = 1.0f;       // Fighting, a splinted arm, tiredness
    float hesitationChance = 0.0f;  // Numb: a swing starts 0.6 s late
    float wearScale = 1.0f;         // Fighting 3: wear -20 %
    float missWear = 0.0f;          // a broken arm: a missed swing still wears the weapon
    int fightingLevel = 0;          // Fighting 3 halves the chance a blade sticks (CombatRules)
    float doubleBlowChance = 0.0f;  // Fighting 5: one blow in ten does double
    bool shoveFellsWeakened = false; // Fighting 2
    float extraSpreadDegrees = 0.0f; // tiredness
    float reloadScale = 1.0f;       // shaking hands: Shaken +30 %
    float fumbleChance = 0.0f;      // Panicking: a reload slips and starts again
    float regenScale = 1.0f;        // Drowsy -15 %, Rested +10 %
    bool noTwoHanded = false;       // a broken arm, unsplinted
};

// The player's side of a fight: swings, the shove (Space), the stomp, the
// take-down (right click), shots, and getting free of the dead that hold on.
// The rules themselves live in world/CombatRules.
class CombatSystem
{
public:
    CombatSystem();

    // Deterministic combat rolls for harness/test runs.
    void reseed(std::uint32_t seed) { mRng.seed(seed); }

    void update(World& world, const Input& input, const TileMap& tileMap, NoiseModel& noiseModel, float dtSeconds);

    // Set before update each tick (phase 5).
    void setBody(const CombatBody& body) { mBody = body; }
    // What happened this tick, for skills and wounds: a melee swing began, a
    // shove met one already lunging, a two-handed weapon was refused, a
    // reload slipped.
    [[nodiscard]] bool swungThisTick() const { return mSwung; }
    [[nodiscard]] int shoveSavesThisTick() const { return mShoveSaves; }
    [[nodiscard]] bool refusedTwoHandedThisTick() const { return mRefusedTwoHanded; }
    [[nodiscard]] bool fumbledReloadThisTick() const { return mFumbledReload; }
    [[nodiscard]] static bool twoHanded(const WeaponStats& weapon);

    // The active part of every swing (wind-up and recovery are the weapon's).
    static constexpr float kActiveSeconds = 0.10f;

    // What the weapon in hand really reaches and hits for, after wear.
    static float effectiveRangePixels(const Combat& combat);
    static float effectiveDamage(const Combat& combat);

    // Start a reload (or clear a jam) of the gun in hand from the pack.
    // Returns false (and does nothing) when there is nothing to do.
    static bool startReload(Combat& combat, const Inventory& inventory);
    // How many rounds for the gun in hand the pack holds.
    static int carriedRounds(const Combat& combat, const Inventory& inventory);

    // The lines the last shots took this tick (start, end), for drawing.
    [[nodiscard]] const std::vector<std::pair<glm::vec2, glm::vec2>>& shotLines() const { return mShotLines; }

    // A unit world vector for where a zombie faces: its drawn facing when it
    // has one, else where it moves.
    static glm::vec2 zombieFacing(World& world, Entity zombie);

private:
    static constexpr float kRecoveryDelayAfterAttackSeconds = 1.00f;
    static constexpr float kBackstabMultiplier = 1.5f;

    static constexpr float kGrabStaminaCost = 35.0f;
    static constexpr float kGrabLungeSeconds = 0.15f;
    static constexpr float kGrabHoldSeconds = 1.0f;
    static constexpr float kGrabExecuteSeconds = 0.6f;
    static constexpr float kGrabExecuteDamage = 200.0f;
    static constexpr float kGrabRange = 36.0f;

    static void degradeWeapon(Combat& combat, int wear);
    static float effectiveArcDegrees(const Combat& combat);
    static bool isPointInsideArc(const glm::vec2& origin, float facingRadians, float arcDegrees, float rangePixels, const glm::vec2& point);
    // A wall or closed door between two points (swings and shoves stop there).
    static bool wallBetween(const TileMap& tileMap, const glm::vec2& from, const glm::vec2& to);

    void updateHeld(World& world, const Input& input, Entity player, Combat& combat, float& stamina, float dtSeconds);
    void updateReload(World& world, Entity player, Combat& combat, bool running, float dtSeconds);
    void updateTakeDown(World& world, const Input& input, NoiseModel& noiseModel, Entity player, const glm::vec2& centre,
                        Combat& combat, float dtSeconds);

    void resolveSwing(World& world, const TileMap& tileMap, NoiseModel& noiseModel, Entity playerEntity,
                      const glm::vec2& playerCenter, float facingRadians, Combat& combat, float& stamina);
    void resolveShove(World& world, const TileMap& tileMap, NoiseModel& noiseModel, Entity playerEntity,
                      const glm::vec2& playerCenter, float facingRadians, Combat& combat, bool exhausted);
    void resolveShot(World& world, const TileMap& tileMap, NoiseModel& noiseModel, Entity playerEntity,
                     const glm::vec2& playerCenter, float facingRadians, Combat& combat);

    CombatBody mBody;
    bool mSwung = false;
    int mShoveSaves = 0;
    bool mRefusedTwoHanded = false;
    bool mFumbledReload = false;
    float mSwingDamageScale = 1.0f;

    std::mt19937 mRng;
    std::uniform_real_distribution<float> mDist;
    std::vector<std::pair<glm::vec2, glm::vec2>> mShotLines;
};
