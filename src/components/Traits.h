#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

// Chosen on the character screen (phase 5): up to three of each, paid for
// with points (see world/TraitRules.h for costs and effects). The first three
// of each keep their old numbers so checkpoints from before phase 5, which
// did not save traits, can rebuild the seed's old pick.
enum class PositiveTrait
{
    None,
    Tough,        // +20 max health
    Quick,        // +15 % move speed
    LightSleeper, // wakes at 1.5x range
    Handy,        // Carpentry starts at 2
    Brawler,      // Fighting starts at 2
    Nurse,        // Medicine starts at 2
    Outdoorsy,    // Living off the land starts at 2; rain chills half as much
    Steady,       // panic gain -25 %
    StrongBack,   // carry limit +6 kg
    NightOwl      // sees further in the dark; sleep debt builds slower
};

enum class NegativeTrait
{
    None,
    Clumsy,       // footsteps one tier louder
    Hungry,       // hunger +50 %
    Nervous,      // panic gain +30 %
    Frail,        // -20 max health
    HeavySleeper, // wakes at 0.6x range
    ThinSkinned,  // cold and heat bite 30 % harder
    Squeamish,    // each kill +2 panic; numbs at half speed
    SlowHealer,   // wounds take 50 % longer
    Loner,        // company gives half the relief
    WeakStomach   // spoiled or raw food makes twice the illness
};

enum class BloodlineTrait
{
    None,
    Hardened,       // slower early depression accumulation (earned by retirement)
    Scavenger,      // starts with slightly better salvage baseline
    Firewise,       // history of fire-related deaths/survival
    Vigilant,       // heightened awareness from repeated close calls
    Wayfinder       // phase 9: someone of the line got out; every street is known
};

[[nodiscard]] inline const char* traitName(PositiveTrait trait)
{
    switch (trait)
    {
    case PositiveTrait::Tough:        return "Tough";
    case PositiveTrait::Quick:        return "Quick";
    case PositiveTrait::LightSleeper: return "Light Sleeper";
    case PositiveTrait::Handy:        return "Handy";
    case PositiveTrait::Brawler:      return "Brawler";
    case PositiveTrait::Nurse:        return "Nurse";
    case PositiveTrait::Outdoorsy:    return "Outdoorsy";
    case PositiveTrait::Steady:       return "Steady";
    case PositiveTrait::StrongBack:   return "Strong Back";
    case PositiveTrait::NightOwl:     return "Night Owl";
    case PositiveTrait::None:         break;
    }
    return "";
}

[[nodiscard]] inline const char* traitName(NegativeTrait trait)
{
    switch (trait)
    {
    case NegativeTrait::Clumsy:       return "Clumsy";
    case NegativeTrait::Hungry:       return "Hungry";
    case NegativeTrait::Nervous:      return "Nervous";
    case NegativeTrait::Frail:        return "Frail";
    case NegativeTrait::HeavySleeper: return "Heavy Sleeper";
    case NegativeTrait::ThinSkinned:  return "Thin-Skinned";
    case NegativeTrait::Squeamish:    return "Squeamish";
    case NegativeTrait::SlowHealer:   return "Slow Healer";
    case NegativeTrait::Loner:        return "Loner";
    case NegativeTrait::WeakStomach:  return "Weak Stomach";
    case NegativeTrait::None:         break;
    }
    return "";
}

struct Traits
{
    static constexpr std::size_t kMaxEach = 3;

    std::vector<PositiveTrait> positives;
    std::vector<NegativeTrait> negatives;
    BloodlineTrait bloodline = BloodlineTrait::None;

    [[nodiscard]] bool has(PositiveTrait trait) const
    {
        return std::find(positives.begin(), positives.end(), trait) != positives.end();
    }
    [[nodiscard]] bool has(NegativeTrait trait) const
    {
        return std::find(negatives.begin(), negatives.end(), trait) != negatives.end();
    }

    [[nodiscard]] const char* bloodlineName() const
    {
        switch (bloodline)
        {
        case BloodlineTrait::Hardened:  return "Hardened";
        case BloodlineTrait::Scavenger: return "Scavenger";
        case BloodlineTrait::Firewise:  return "Firewise";
        case BloodlineTrait::Vigilant:  return "Vigilant";
        case BloodlineTrait::Wayfinder: return "Wayfinder";
        default:                        return "";
        }
    }
};
