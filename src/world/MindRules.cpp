#include "world/MindRules.h"

#include <algorithm>

namespace mindrules
{
PanicBand panicBand(float panic)
{
    if (panic >= 90.0f) return PanicBand::Breaking;
    if (panic >= 75.0f) return PanicBand::Panicking;
    if (panic >= 50.0f) return PanicBand::Shaken;
    if (panic >= 25.0f) return PanicBand::Uneasy;
    return PanicBand::Calm;
}

const char* panicName(PanicBand band)
{
    switch (band)
    {
    case PanicBand::Uneasy: return "Uneasy";
    case PanicBand::Shaken: return "Shaken";
    case PanicBand::Panicking: return "Panicking";
    case PanicBand::Breaking: return "Breaking";
    case PanicBand::Calm: break;
    }
    return "";
}

float staminaCostScale(PanicBand band) { return band >= PanicBand::Uneasy ? 1.10f : 1.0f; }
float heldActionScale(PanicBand band) { return band >= PanicBand::Shaken ? 1.30f : 1.0f; }
float vignette(PanicBand band) { return band >= PanicBand::Shaken ? 0.15f : 0.0f; }
bool louderStep(PanicBand band, int stepIndex) { return band >= PanicBand::Shaken && (stepIndex % 2) == 1; }
float fumbleChance(PanicBand band) { return band >= PanicBand::Panicking ? kFumbleChance : 0.0f; }
bool canSleep(PanicBand band) { return band < PanicBand::Panicking; }

bool losesControl(PanicBand band, bool grabbed, float healthShare, float secondsSinceLast)
{
    return band >= PanicBand::Breaking && (grabbed || healthShare < 0.3f) &&
           secondsSinceLast >= kLostControlCooldownSeconds;
}

float panicRecoveryScale(bool crouchedStillHidden, bool friendNear)
{
    return (crouchedStillHidden ? 2.0f : 1.0f) * (friendNear ? 1.5f : 1.0f);
}

bool firstKillFrightens(float desensitisation) { return desensitisation <= kHardenedDesensitisation; }

float singleZombiePanicCap(float desensitisation)
{
    return desensitisation > kHardenedDesensitisation ? 24.9f : 100.0f;
}

DepressionBand depressionBand(float depression)
{
    if (depression >= 90.0f) return DepressionBand::GivingUp;
    if (depression >= 70.0f) return DepressionBand::Numb;
    if (depression >= 50.0f) return DepressionBand::WornDown;
    if (depression >= 30.0f) return DepressionBand::Low;
    return DepressionBand::Fine;
}

const char* depressionName(DepressionBand band)
{
    switch (band)
    {
    case DepressionBand::Low: return "Low";
    case DepressionBand::WornDown: return "Worn down";
    case DepressionBand::Numb: return "Numb";
    case DepressionBand::GivingUp: return "Giving up";
    case DepressionBand::Fine: break;
    }
    return "";
}

float restScale(DepressionBand band) { return band >= DepressionBand::Low ? 0.9f : 1.0f; }
float staminaCapScale(DepressionBand band) { return band >= DepressionBand::WornDown ? 0.9f : 1.0f; }
float comfortScale(DepressionBand band) { return band >= DepressionBand::WornDown ? 0.5f : 1.0f; }
float hesitationChance(DepressionBand band) { return band >= DepressionBand::Numb ? kHesitationChance : 0.0f; }
float moveScale(DepressionBand band) { return band >= DepressionBand::Numb ? 0.95f : 1.0f; }
bool canBuild(DepressionBand band) { return band < DepressionBand::GivingUp; }
bool canLootBodies(DepressionBand band) { return band < DepressionBand::GivingUp; }

float killWeight(int killsThisRun, float desensitisation)
{
    if (killsThisRun <= kKillsBeforeWeight) return 0.0f;
    return 0.3f * std::clamp(desensitisation, 0.0f, 100.0f) / 100.0f;
}

float desensitisationFade(float hoursSinceKill, float hoursPassed)
{
    return hoursSinceKill >= 24.0f ? kDesensitisationFadePerDay * hoursPassed / 24.0f : 0.0f;
}

bool seesThings(float shownDebt, float panicHighSeconds, float bodyTemperature)
{
    return shownDebt >= kSeeingDebt || panicHighSeconds >= kSeeingPanicSeconds || bodyTemperature > kSeeingFever;
}
} // namespace mindrules
