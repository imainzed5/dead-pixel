#pragma once

#include <string>

enum class PositiveTrait
{
    None,
    Tough,         // +20 max HP
    Quick,         // +15% move speed
    LightSleeper   // wake range doubled
};

enum class NegativeTrait
{
    None,
    Clumsy,        // +1 noise tier on footsteps
    Hungry,        // +50% hunger decay rate
    Nervous        // +30% base panic gain
};

enum class BloodlineTrait
{
    None,
    Hardened        // slower early depression accumulation (earned by retirement)
};

struct Traits
{
    PositiveTrait positive = PositiveTrait::None;
    NegativeTrait negative = NegativeTrait::None;
    BloodlineTrait bloodline = BloodlineTrait::None;

    [[nodiscard]] const char* positiveName() const
    {
        switch (positive)
        {
        case PositiveTrait::Tough:        return "Tough";
        case PositiveTrait::Quick:        return "Quick";
        case PositiveTrait::LightSleeper: return "Light Sleeper";
        default:                          return "";
        }
    }

    [[nodiscard]] const char* negativeName() const
    {
        switch (negative)
        {
        case NegativeTrait::Clumsy:  return "Clumsy";
        case NegativeTrait::Hungry:  return "Hungry";
        case NegativeTrait::Nervous: return "Nervous";
        default:                     return "";
        }
    }
};
