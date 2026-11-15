#pragma once

#include <string>
#include <vector>

#include "components/Wounds.h"

// Wounds as rules (phase 5): where they land, what each does to the body,
// how they heal, what treats them, how they fester, and what the HUD and the
// BODY tab call them. WoundSystem, the game and the tests ask these.
namespace woundrules
{
// --- Where a wound lands -----------------------------------------------------
[[nodiscard]] bool isArm(BodyPart part);
[[nodiscard]] bool isLeg(BodyPart part);
[[nodiscard]] const char* partName(BodyPart part); // "left arm"
// Claws: arms 50 %, torso 30 %, head 10 %, legs 10 %.
[[nodiscard]] BodyPart clawPart(float roll);
// Bites while held: arms 40 %, torso 30 %, neck and head 30 %.
[[nodiscard]] BodyPart heldBitePart(float roll);
// A Crawler bites the legs.
[[nodiscard]] BodyPart crawlerPart(float roll);
// A break lands on an arm or a leg.
[[nodiscard]] BodyPart limbPart(float roll);
[[nodiscard]] BodyPart legPart(float roll);

// Half of the claws and bites that wound go deep (as the old severity above
// 0.75 did).
constexpr float kDeepChance = 0.5f;
// Fractures: a Brute's blow, a failed window climb, a fall under too much.
constexpr float kBruteFractureChance = 0.25f;
constexpr float kFailedClimbFractureChance = 0.05f;
constexpr float kOverloadedFallFractureChance = 0.10f;

[[nodiscard]] Wound make(WoundType type, BodyPart part, WoundDepth depth = WoundDepth::Shallow,
                         bool contaminated = false);

// --- What wounds do ------------------------------------------------------------
// Per kind, the worst wound counts; different kinds stack.
struct BodyEffects
{
    float swingStaminaScale = 1.0f; // an undressed arm wound: +15 %
    float moveScale = 1.0f;         // legs: -8 % undressed, -45 % broken, -20 % splinted
    float maxStaminaPenalty = 0.0f; // an undressed torso wound: -10
    int sightPenaltyTiles = 0;      // an undressed head wound: -1 tile
    bool headVignette = false;
    bool noTwoHanded = false;       // a broken arm, unsplinted
    float missWearScale = 1.0f;     // a broken arm: a missed swing wears x2
    float windupScale = 1.0f;       // a splinted arm: +20 %
    bool canRun = true;             // a broken leg, unsplinted
    bool runAtWalkSpeed = false;    // a splinted leg
    bool rebleedOnSwing = false;    // a deep arm wound, not stitched
    bool rebleedOnRun = false;      // a deep leg wound, not stitched
    float slowBleedPerHour = 0.0f;  // a deep torso or head wound, not stitched
    bool limp = false;              // any hurt leg
    bool armHeldIn = false;         // a broken arm
};
constexpr float kRebleedOnSwingChance = 0.30f;
constexpr float kSlowBleedPerHour = 2.0f;        // health a game hour, undressed
constexpr float kSlowBleedDressedPerHour = 1.0f; // dressed but not stitched
[[nodiscard]] BodyEffects effects(const std::vector<Wound>& wounds);

// Max health: 5 off for each wound, 10 for a fracture, never below 20.
[[nodiscard]] float maxHealthPenalty(const std::vector<Wound>& wounds);
[[nodiscard]] float maxHealth(float baseMaximum, const std::vector<Wound>& wounds);

// --- Healing ------------------------------------------------------------------------
// Game hours to close: bruise 2, shallow cut 6, deep cut 30 (12 stitched),
// bite 18, fracture 96 splinted. Negative: never (an unsplinted fracture, an
// infection, a turning).
[[nodiscard]] float healHours(const Wound& wound);
// How fast healing runs: bed rest x1.5, Medicine +8 % a level, Slow Healer x1/1.5.
constexpr float kBedRestHealScale = 1.5f;
[[nodiscard]] float healRate(bool bedRest, int medicineLevel, float healTimeScale);

// --- Dirt and infection ---------------------------------------------------------------
constexpr float kDressingDirtyHours = 12.0f;
constexpr float kBiteInfectionHours = 3.0f;
constexpr float kOpenCutInfectionPerHour = 0.04f;
constexpr float kInfectionIllnessPerSecond = 0.05f;
// Below this, illness does not climb on its own unless a wound is infected.
constexpr float kIllnessCeilingWithoutInfection = 60.0f;
// Can this wound take infection this hour, and by which road.
[[nodiscard]] bool festers(const Wound& wound);        // a dirty bite: infects after 3 h
[[nodiscard]] bool mayInfect(const Wound& wound);      // an open deep or dirty cut: 4 % an hour
// Fever: +0.3 C for every 20 illness while a wound is infected.
[[nodiscard]] float feverDegrees(float illness);
// Hopeless only: an infected bite untreated a day turns; death in 12 to 24 h.
constexpr float kTurningAfterHours = 24.0f;
[[nodiscard]] float turnDeathHours(float roll);

// --- Treatment ----------------------------------------------------------------------------
enum class Treatment
{
    None,
    Dress,         // Bandage, First Aid Kit: dresses, stops bleeding
    DressClean,    // Disinfected Bandage: dresses and removes dirt
    Stitch,        // Suture Kit: sews a deep wound
    Splint,        // Splint: sets a fracture
    CureInfection, // Antibiotics
    Disinfect      // Bottle of Spirits on a wound (hurts)
};
[[nodiscard]] Treatment treatmentFromName(const std::string& name);
[[nodiscard]] bool canTreat(Treatment treatment, const Wound& wound);
// The wound a treatment goes to: the worst it can help, or -1.
[[nodiscard]] int worstFor(Treatment treatment, const std::vector<Wound>& wounds);
void apply(Treatment treatment, Wound& wound);
// Held time (seconds) at a Medicine level; 0 is at once.
[[nodiscard]] float treatSeconds(Treatment treatment, int medicineLevel);
constexpr float kStitchSeconds = 8.0f;
constexpr float kSplintSeconds = 10.0f;
constexpr float kStitchPanic = 6.0f;        // without Painkillers in the last 2 h
constexpr float kPainkillerWindowHours = 2.0f;
constexpr float kDisinfectPanic = 8.0f;
constexpr float kSpiritsUsesPerBottle = 4.0f;
[[nodiscard]] const char* treatmentVerb(Treatment treatment); // "dress", "stitch"

// --- Names --------------------------------------------------------------------------------
// How bad, for ordering: higher is worse.
[[nodiscard]] int urgency(const Wound& wound);
// "Deep cut, left arm"
[[nodiscard]] std::string label(const Wound& wound);
// "needs stitches", "dressed", "infected: antibiotics". seesInfection: the
// Medicine-2 eye that sees a wound will fester before it does.
[[nodiscard]] std::string need(const Wound& wound, bool seesInfection);
// HUD names, worst first: "Bleeding", "Turning", "Infected bite", "Broken
// leg", "Deep cut", ... then "Fever". No duplicates.
[[nodiscard]] std::vector<std::string> hudLabels(const std::vector<Wound>& wounds, bool bleeding, float illness);
} // namespace woundrules
