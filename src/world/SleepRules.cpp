#include "world/SleepRules.h"

#include <algorithm>

namespace sleeprules
{
Band band(float shownDebt)
{
    if (shownDebt >= kDeadOnFeetAt) return Band::DeadOnFeet;
    if (shownDebt >= kExhaustedAt) return Band::Exhausted;
    if (shownDebt >= kTiredAt) return Band::Tired;
    if (shownDebt >= kDrowsyAt) return Band::Drowsy;
    return Band::Fine;
}

const char* name(Band band)
{
    switch (band)
    {
    case Band::Drowsy: return "Drowsy";
    case Band::Tired: return "Tired";
    case Band::Exhausted: return "Exhausted";
    case Band::DeadOnFeet: return "Dead on your feet";
    case Band::Fine: break;
    }
    return "";
}

float staminaRegenScale(Band band, bool rested)
{
    float scale = band >= Band::Drowsy ? 0.85f : 1.0f;
    if (rested) scale *= 1.10f;
    return scale;
}

float aimSpreadDegrees(Band band) { return band >= Band::Tired ? 3.0f : 0.0f; }
float windupScale(Band band) { return band >= Band::Tired ? 1.10f : 1.0f; }

float viewSoften(Band band)
{
    if (band >= Band::Exhausted) return 0.30f;
    if (band >= Band::Tired) return 0.18f;
    return 0.0f;
}

float staminaCapScale(Band band) { return band >= Band::Exhausted ? 0.75f : 1.0f; }
bool seesThings(Band band) { return band >= Band::Exhausted; }
bool microsleeps(Band band) { return band >= Band::DeadOnFeet; }

float microsleepInterval(float roll) { return 40.0f + 50.0f * std::clamp(roll, 0.0f, 1.0f); }

float shownDebt(float debt, float coffeeHoursLeft)
{
    return coffeeHoursLeft > 0.0f ? std::max(0.0f, debt - kCoffeeMask) : debt;
}

bool doubleCoffee(double nowHour, double lastCoffeeHour)
{
    return nowHour - lastCoffeeHour < static_cast<double>(kCoffeeDoubleHours);
}

float sleepQuality(float roomQuality, bool sedatedRecently)
{
    return std::max(0.1f, roomQuality - (sedatedRecently ? kSedativeQualityPenalty : 0.0f));
}

bool fullSleep(float hoursSlept, float quality)
{
    return hoursSlept >= kFullSleepHours - 0.01f && quality >= kFullSleepQuality - 0.001f;
}
} // namespace sleeprules
