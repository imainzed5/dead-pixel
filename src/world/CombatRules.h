#pragma once

#include <glm/vec2.hpp>

#include <string>

#include "components/Combat.h"

// Fighting, as rules with no game state of their own (phase 3): the shove,
// what a lunge that reaches you does, the struggle to get free, knockdowns
// and stomps, blades that stick, gun spread and wear. CombatSystem,
// ZombieAISystem and the game ask these; the tests pin them down.
namespace combatrules
{
// --- Swings -------------------------------------------------------------

// Wind-up and recovery when an item gives none (seconds). The active part of
// a swing is the same for every weapon.
constexpr float kDefaultWindupSeconds = 0.20f;
constexpr float kActiveSeconds = 0.10f;
constexpr float kDefaultRecoverySeconds = 0.30f;

// Out of breath: stamina under this share of the maximum.
constexpr float kExhaustedShare = 0.20f;
constexpr float kExhaustedMissChance = 0.35f;
constexpr float kExhaustedWindupScale = 1.30f;
constexpr float kExhaustedMoveScale = 0.85f;

// A reach weapon's blow lands this much sooner (seconds off the wind-up).
constexpr float kReachHeadStartSeconds = 0.06f;

[[nodiscard]] bool exhausted(float stamina, float staminaMax);
// The wind-up a swing really takes.
[[nodiscard]] float windupSeconds(const WeaponStats& weapon, bool exhausted);

// What a swing costs the weapon: a hit 1, a miss nothing, a wall or a piece
// of furniture 2.
enum class SwingContact
{
    Miss,
    Body,
    Wall
};
[[nodiscard]] int wearForSwing(SwingContact contact);

// --- The shove ----------------------------------------------------------

constexpr float kShoveWindupSeconds = 0.10f;
constexpr float kShoveRecoverySeconds = 0.25f;
constexpr float kShoveStamina = 10.0f;
constexpr float kShoveStaminaUnarmed = 6.0f;
constexpr float kShoveArcDegrees = 100.0f;
constexpr float kShoveRangePixels = 36.0f;
constexpr float kShoveDistancePixels = 56.0f;
constexpr float kShoveStaggerSeconds = 0.6f;
constexpr float kShoveKnockOnStaggerSeconds = 0.3f; // one thrown into another
constexpr float kShoveWallDamage = 5.0f;
constexpr float kShoveWallStaggerSeconds = 0.9f;
constexpr float kShoveFallHealthShare = 0.35f;
constexpr float kBruteShoveDistancePixels = 16.0f;

// Which kind of the dead takes the shove (phase 4 adds the kinds).
enum class Body
{
    Ordinary,
    Brute,
    Crawler
};

struct ShoveOutcome
{
    float distancePixels = 0.0f; // how far it is thrown
    float staggerSeconds = 0.0f; // 0: it does not reel
    bool falls = false;          // goes down (see Down below)
};

[[nodiscard]] float shoveStamina(bool armed);
// A shove on one of the dead: weakened or hit in the back, it falls.
[[nodiscard]] ShoveOutcome shove(Body body, float healthShare, bool fromBehind, bool shoverExhausted);

// "From behind": the blow comes from more than 90 degrees off where the
// target faces. `facing` and the direction from attacker to target are unit
// world vectors.
[[nodiscard]] bool fromBehind(const glm::vec2& targetFacing, const glm::vec2& attackerToTarget);

// --- The dead grab ------------------------------------------------------

constexpr float kClawShare = 0.55f;         // of lunges that reach
constexpr float kClawDamage = 7.0f;
constexpr float kBackClawScale = 1.5f;      // a claw from outside your front half
constexpr float kBiteIntervalSeconds = 1.4f; // each holder, while you are held
constexpr float kBiteDamage = 9.0f;
constexpr float kBiteWoundChance = 0.60f;
constexpr float kStruggleTapStamina = 7.0f;
constexpr float kStrugglePerHolder = 4.0f;
constexpr float kStruggleDrainPerSecond = 1.5f;
// The meter holds this long after a tap before it drains, so four quick taps
// always free you; it is a pause that lets the hold close again.
constexpr float kStruggleDrainDelaySeconds = 0.5f;
constexpr float kFreeImmunitySeconds = 1.0f;
constexpr float kHeldStabIntervalSeconds = 0.7f;
constexpr float kHeldStabDamageScale = 0.6f;

enum class LungeResult
{
    Claw,
    Grab
};

// A lunge that reached you. `roll` is uniform [0, 1). A lunge from behind
// (and not against a wall), at a player out of breath or already held, grabs.
[[nodiscard]] LungeResult lungeResult(float roll, bool fromBehindUncovered, bool playerExhausted, bool alreadyHeld);
// A claw's damage: half again from behind, unless your back is to a wall.
[[nodiscard]] float clawDamage(float base, bool fromBehindUncovered);

// The struggle meter: each tap of Space adds 1 (half with no stamina left),
// it drains once you stop tapping (after a short pause), and it needs 4 for
// each one holding on.
struct Struggle
{
    float meter = 0.0f;
    float sinceTap = 0.0f; // seconds since the last tap
};
// One tap: what it adds and what it costs. Returns the stamina spent.
float struggleTap(Struggle& struggle, float& stamina);
void struggleDrain(Struggle& struggle, float dtSeconds);
[[nodiscard]] float struggleNeeded(int holders);
[[nodiscard]] bool struggleFree(const Struggle& struggle, int holders);

// --- Knockdown and the floor -------------------------------------------

constexpr float kDownSeconds = 2.5f;
constexpr float kBruteDownSeconds = 1.5f;
constexpr float kRiseSeconds = 1.2f;
constexpr float kStompReachPixels = 30.0f;
constexpr float kStompScale = 2.0f;
constexpr float kUnarmedStompDamage = 30.0f;
constexpr float kUnarmedStompStamina = 14.0f;
constexpr float kHeavyBluntDownDamage = 45.0f;

[[nodiscard]] float downSeconds(Body body);
// A blow that puts one on the floor: any hit from a weapon that knocks down
// (the Sledgehammer), or a blunt blow of 45 or more.
[[nodiscard]] bool blowKnocksDown(const WeaponStats& weapon, float damageDealt);
// A swing at one lying down.
[[nodiscard]] float stompDamage(const WeaponStats& weapon, float weaponDamage);
[[nodiscard]] float stompStamina(const WeaponStats& weapon, float swingStamina);

// --- Blades stick -------------------------------------------------------

constexpr float kStickDamage = 40.0f;
constexpr float kStickRecoverySeconds = 0.5f;
constexpr float kStickStamina = 8.0f;

// The chance a blow lodges the blade: only a killing or 40+ blow with a
// weapon that can stick, by its condition. Fighting level 3 halves it (P05).
[[nodiscard]] float stickChance(const WeaponStats& weapon, float damageDealt, bool killed, int fightingLevel = 0);

// A reach weapon's blow on one winding up a lunge pushes it out to the
// weapon's range: the knockback (px) that does that.
[[nodiscard]] float reachKnockback(const WeaponStats& weapon, float distancePixels, float knockback, bool targetWindingUp);

// --- Guns ---------------------------------------------------------------

constexpr float kPointBlankScale = 2.5f;
constexpr float kShotgunFallPellets = 3.0f;
constexpr float kShotgunFallTiles = 2.0f;
constexpr float kShotPanic = 4.0f;
constexpr float kShotPanicUnderDesensitisation = 30.0f;
constexpr float kJamConditionShare = 0.33f;
constexpr float kJamChance = 0.10f;
constexpr float kJamClearSeconds = 1.0f;
constexpr float kPistolReloadSeconds = 1.6f;   // the whole magazine
constexpr float kShellReloadSeconds = 0.9f;    // each shell

struct SpreadInputs
{
    float baseDegrees = 1.5f;
    bool walking = false;
    bool running = false;
    float panic = 0.0f;
    bool exhausted = false;
    bool crouchedStill = false;
};
// How far (degrees, either side) a shot may leave the aim.
[[nodiscard]] float spreadDegrees(const SpreadInputs& in);
// A bullet's damage at a distance (px): far more within one tile.
[[nodiscard]] float bulletDamage(float damage, float distancePixels, float tilePixels);
// The shotgun puts down anyone close who takes 3 or more pellets.
[[nodiscard]] bool pelletsFell(int pelletsTaken, float distancePixels, float tilePixels);
[[nodiscard]] float jamChance(float conditionShare);
// Seconds for a reload of what is missing: the pistol fills its magazine in
// one go, the shotgun one shell at a time.
[[nodiscard]] float reloadSeconds(int pellets, int roundsToLoad);
[[nodiscard]] float shotPanic(float desensitisation);

// --- What the pack panel says ------------------------------------------

// 0..1: how quick a weapon is, from wind-up and recovery.
[[nodiscard]] float speedRating(const WeaponStats& weapon);
// One line of what is special: "Bleeds. Can stick." / "Knocks down." / "Loud."
[[nodiscard]] std::string specialLine(const WeaponStats& weapon);

// --- Throwing -----------------------------------------------------------

constexpr float kThrowSpeedPixelsPerSecond = 420.0f;
constexpr float kThrowHitDamage = 6.0f;
constexpr float kLureSeconds = 20.0f;
constexpr float kAlarmDelaySeconds = 8.0f;
constexpr float kAlarmRingSeconds = 20.0f;
constexpr float kFlareSeconds = 90.0f;
constexpr float kFlareLightTiles = 6.0f;
constexpr float kFlarePullTiles = 15.0f;
constexpr float kFlareNoiseEverySeconds = 2.0f;

// Where a throw aimed at `target` comes down: no further than the range.
[[nodiscard]] glm::vec2 throwLanding(const glm::vec2& from, const glm::vec2& target, float rangePixels);
// How high (px) the thing flies at `progress` (0..1) of a flight `distance` long.
[[nodiscard]] float throwArcHeight(float progress, float distancePixels);
// An alarm clock lives through one landing in three: `roll` uniform [0, 1).
[[nodiscard]] bool alarmSurvives(float roll);
// A flare burns half as long in rain.
[[nodiscard]] float flareSeconds(bool raining);

// --- The facing a character's drawing shows ----------------------------

// A unit world vector for Animation::dir (0 E on screen, clockwise).
[[nodiscard]] glm::vec2 dirToWorld(int dir);
} // namespace combatrules
