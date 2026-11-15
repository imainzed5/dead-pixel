#pragma once

// Sleep debt with teeth (phase 5): the bands and what each does, coffee that
// hides tiredness for a while, what spoils a night, and the "Rested" that a
// full night gives. NeedsSystem, CombatSystem and the game ask these.
namespace sleeprules
{
enum class Band
{
    Fine,
    Drowsy,     // 50-70
    Tired,      // 70-85
    Exhausted,  // 85-95
    DeadOnFeet  // 95-100
};

constexpr float kDrowsyAt = 50.0f;
constexpr float kTiredAt = 70.0f;
constexpr float kExhaustedAt = 85.0f;
constexpr float kDeadOnFeetAt = 95.0f;

[[nodiscard]] Band band(float shownDebt);
[[nodiscard]] const char* name(Band band); // "" when fine

// Each band carries the ones below it.
[[nodiscard]] float staminaRegenScale(Band band, bool rested); // Drowsy -15 %, Rested +10 %
[[nodiscard]] float aimSpreadDegrees(Band band);              // Tired: +3
[[nodiscard]] float windupScale(Band band);                   // Tired: +10 %
[[nodiscard]] float viewSoften(Band band);                    // Tired: the edges of the view soften
[[nodiscard]] float staminaCapScale(Band band);               // Exhausted: -25 %
[[nodiscard]] bool seesThings(Band band);                     // Exhausted
[[nodiscard]] bool microsleeps(Band band);                    // Dead on your feet

// Microsleeps: every 40 to 90 s the eyes close for 1.2 s and input is lost.
constexpr float kMicrosleepSeconds = 1.2f;
[[nodiscard]] float microsleepInterval(float roll);

// Coffee: hides 20 debt for 4 game hours (the real debt keeps rising), then
// +5 when it wears off. A second within 6 hours: +10 panic and the shakes.
constexpr float kCoffeeMask = 20.0f;
constexpr float kCoffeeHours = 4.0f;
constexpr float kCoffeeCrash = 5.0f;
constexpr float kCoffeeDoubleHours = 6.0f;
constexpr float kCoffeeDoublePanic = 10.0f;
constexpr float kCoffeeJitterHours = 2.0f;
[[nodiscard]] float shownDebt(float debt, float coffeeHoursLeft);
[[nodiscard]] bool doubleCoffee(double nowHour, double lastCoffeeHour);

// Painkillers or Spirits in the two hours before lying down: -0.15 quality.
constexpr float kSedativeQualityPenalty = 0.15f;
constexpr double kSedativeWindowHours = 2.0;
[[nodiscard]] float sleepQuality(float roomQuality, bool sedatedRecently);

// A full night (6+ hours at quality 0.8+) leaves you Rested for 6 hours.
constexpr float kFullSleepHours = 6.0f;
constexpr float kFullSleepQuality = 0.8f;
constexpr float kRestedHours = 6.0f;
constexpr float kRestedDepressionRelief = 8.0f;
[[nodiscard]] bool fullSleep(float hoursSlept, float quality);
} // namespace sleeprules
