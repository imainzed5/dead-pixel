#pragma once

// A mind that shows (phase 5): panic and depression bands and what each does,
// the new things that frighten and comfort, numbness fading, and when the
// character starts seeing things. MentalStateSystem and the game ask these.
namespace mindrules
{
// --- Panic -----------------------------------------------------------------
enum class PanicBand
{
    Calm,
    Uneasy,    // 25-50
    Shaken,    // 50-75
    Panicking, // 75-90
    Breaking   // 90+
};
[[nodiscard]] PanicBand panicBand(float panic);
[[nodiscard]] const char* panicName(PanicBand band); // "" when calm

// Effects (each band carries the ones below it).
[[nodiscard]] float staminaCostScale(PanicBand band); // Uneasy: +10 %
[[nodiscard]] float heldActionScale(PanicBand band);  // Shaken: hands shake, +30 %
[[nodiscard]] float vignette(PanicBand band);         // Shaken: the view narrows 15 %
// Shaken: footsteps a tier louder half the time (every other step).
[[nodiscard]] bool louderStep(PanicBand band, int stepIndex);
constexpr float kFumbleChance = 0.10f;
[[nodiscard]] float fumbleChance(PanicBand band);     // Panicking: a reload or treatment slips
[[nodiscard]] bool canSleep(PanicBand band);          // not while Panicking
// Breaking, and grabbed or under 30 % health: 2 s running blind, once a minute.
constexpr float kLostControlSeconds = 2.0f;
constexpr float kLostControlCooldownSeconds = 60.0f;
[[nodiscard]] bool losesControl(PanicBand band, bool grabbed, float healthShare, float secondsSinceLast);

// New frights (instant, panic points) and steady ones (per second).
constexpr float kGrabbedPanic = 12.0f;
constexpr float kScreamerPanic = 8.0f;
constexpr float kScreamerPanicTiles = 10.0f;
constexpr float kWokenByZombiePanic = 30.0f;
constexpr float kWokenByZombieTiles = 5.0f;
constexpr float kHordePanicPerSecond = 0.8f;
constexpr float kHordePanicTiles = 15.0f;
constexpr float kFirstKillPanic = 10.0f;
constexpr float kDarkIndoorsPanicPerSecond = 1.0f;
constexpr float kDarkIndoorsPanicCeiling = 40.0f;
constexpr float kDarkIndoorsHeardTiles = 10.0f;

// Recovery: x2 crouched still out of sight; x1.5 near a friend.
[[nodiscard]] float panicRecoveryScale(bool crouchedStillHidden, bool friendNear);
constexpr float kPainkillerPanicRelief = 15.0f;
constexpr float kChocolatePanicRelief = 6.0f;
constexpr float kSpiritsPanicRelief = 20.0f;
constexpr float kHerbalTeaPanicRelief = 12.0f;
// A deep breath: hold C, still and out of sight, for 3 s: -10, once a minute.
constexpr float kDeepBreathSeconds = 3.0f;
constexpr float kDeepBreathRelief = 10.0f;
constexpr float kDeepBreathCooldownSeconds = 60.0f;

// Numb to it: past 60 desensitisation there is no first-kill fright and one
// zombie alone no longer makes the character uneasy.
constexpr float kHardenedDesensitisation = 60.0f;
[[nodiscard]] bool firstKillFrightens(float desensitisation);
// The most panic one zombie in view can raise (100 when not hardened).
[[nodiscard]] float singleZombiePanicCap(float desensitisation);

// --- Depression ---------------------------------------------------------------
enum class DepressionBand
{
    Fine,
    Low,       // 30-50
    WornDown,  // 50-70
    Numb,      // 70-90
    GivingUp   // 90+
};
[[nodiscard]] DepressionBand depressionBand(float depression);
[[nodiscard]] const char* depressionName(DepressionBand band); // "" when fine

[[nodiscard]] float restScale(DepressionBand band);          // Low: 0.9 of a night's rest
[[nodiscard]] float staminaCapScale(DepressionBand band);    // Worn down: -10 %
[[nodiscard]] float comfortScale(DepressionBand band);       // Worn down: comfort gives half
constexpr float kHesitationChance = 0.15f;
constexpr float kHesitationSeconds = 0.6f;
[[nodiscard]] float hesitationChance(DepressionBand band);   // Numb: 15 % start 0.6 s late
[[nodiscard]] float moveScale(DepressionBand band);          // Numb: -5 %
[[nodiscard]] bool canBuild(DepressionBand band);            // not when Giving up
[[nodiscard]] bool canLootBodies(DepressionBand band);       // not when Giving up

// New weights and reliefs (depression points).
constexpr float kOwnBloodlineGrave = 4.0f;
constexpr float kLayRisenToRest = -10.0f;
constexpr float kDeadJournal = 3.0f;
constexpr float kCompanionDied = 20.0f;
constexpr float kBaseBrokenInto = 6.0f;
constexpr int kKillsBeforeWeight = 20;
// Each kill past 20 in a run weighs 0.3 x (desensitisation / 100).
[[nodiscard]] float killWeight(int killsThisRun, float desensitisation);
constexpr float kBookRelief = 10.0f;
constexpr float kBookGameMinutes = 20.0f;
constexpr float kHotMealRelief = 3.0f;
constexpr float kStewRelief = 8.0f;
constexpr float kKeepsakeRelief = 25.0f;
constexpr float kWeekAliveRelief = 10.0f;

// Desensitisation fades by 1 a game day without a kill.
constexpr float kDesensitisationFadePerDay = 1.0f;
[[nodiscard]] float desensitisationFade(float hoursSinceKill, float hoursPassed);

// --- Seeing things ----------------------------------------------------------------
constexpr float kSeeingDebt = 85.0f;
constexpr float kSeeingPanic = 75.0f;
constexpr float kSeeingPanicSeconds = 60.0f;
constexpr float kSeeingFever = 39.0f;
[[nodiscard]] bool seesThings(float shownDebt, float panicHighSeconds, float bodyTemperature);
} // namespace mindrules
