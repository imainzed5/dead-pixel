#include "audio/Ambience.h"

#include <algorithm>

namespace ambience
{
namespace
{
float& at(Mix& m, LayerId layer)
{
    return m[static_cast<std::size_t>(layer)];
}

bool rainy(WeatherCondition c)
{
    return c == WeatherCondition::Rain || c == WeatherCondition::HeavyRain || c == WeatherCondition::Thunderstorm;
}
} // namespace

bool isAmbience(LayerId layer)
{
    switch (layer)
    {
    case LayerId::Wind:
    case LayerId::WindGust:
    case LayerId::WindTrees:
    case LayerId::RainLight:
    case LayerId::RainHeavy:
    case LayerId::RainRoof:
    case LayerId::Snow:
    case LayerId::Blizzard:
    case LayerId::FogDrip:
    case LayerId::Birds:
    case LayerId::Insects:
    case LayerId::Crows:
    case LayerId::Hum:
    case LayerId::Room:
    case LayerId::Crowd:
    case LayerId::FireSmall:
    case LayerId::FireLarge:
        return true;
    default:
        return false;
    }
}

float duck(float tinnitusSeconds)
{
    if (tinnitusSeconds <= 0.0f) return 1.0f;
    // Full duck for most of it, easing back over the last half second.
    const float easing = std::clamp(tinnitusSeconds / 0.5f, 0.0f, 1.0f);
    return 1.0f - kDuck * easing;
}

float outdoorShare(const Facts& f)
{
    if (!f.indoors) return 1.0f;
    if (f.closedIn) return 0.0f;
    return f.openToOutside ? 0.55f : 0.2f;
}

Mix mix(const Facts& f)
{
    Mix m{};
    if (!f.playing) return m;
    if (f.out)
    {
        // Getting out: wind, and nothing else.
        at(m, LayerId::Wind) = 0.8f;
        return m;
    }

    const float outside = outdoorShare(f);
    const float wind = std::clamp(f.windStrength, 0.0f, 1.0f);
    const WeatherCondition c = f.condition;

    // Wind: always a breath of it; x0.4 indoors.
    const float windIndoor = f.indoors ? (f.closedIn ? 0.0f : 0.4f) : 1.0f;
    at(m, LayerId::Wind) = (0.15f + 0.6f * wind) * windIndoor;
    if (wind > 0.55f) at(m, LayerId::WindGust) = (wind - 0.55f) / 0.45f * windIndoor;
    if (f.treesNear && !f.indoors && wind > 0.2f) at(m, LayerId::WindTrees) = 0.3f + 0.7f * wind;

    // Rain, snow, storm. Indoors it is on the roof.
    if (rainy(c) && !f.closedIn)
    {
        const bool heavy = c != WeatherCondition::Rain;
        if (f.indoors)
        {
            at(m, LayerId::RainRoof) = heavy ? 0.9f : 0.6f;
            if (f.openToOutside) at(m, heavy ? LayerId::RainHeavy : LayerId::RainLight) = 0.35f;
        }
        else
        {
            at(m, heavy ? LayerId::RainHeavy : LayerId::RainLight) = heavy ? 0.9f : 0.7f;
        }
    }
    if (c == WeatherCondition::Snow) at(m, LayerId::Snow) = 0.6f * outside;
    if (c == WeatherCondition::Blizzard) at(m, LayerId::Blizzard) = 0.9f * (f.indoors ? (f.closedIn ? 0.0f : 0.4f) : 1.0f);
    if (c == WeatherCondition::Fog) at(m, LayerId::FogDrip) = 0.7f * outside;

    // The tells: birds by day and insects by night stop when the dead are near.
    const bool quiet = f.deadNear == 0;
    const bool fair = c == WeatherCondition::Clear || c == WeatherCondition::Cloudy || c == WeatherCondition::Heatwave;
    if (f.day && fair && quiet && f.infection < kBirdsBelowInfection) at(m, LayerId::Birds) = 0.8f * outside;
    if (!f.day && quiet && f.temperature > kInsectsAboveCelsius && !rainy(c) && !(c == WeatherCondition::Snow) &&
        c != WeatherCondition::Blizzard)
    {
        at(m, LayerId::Insects) = 0.7f * outside;
    }
    if (f.day && f.ruin >= kCrowsFromRuin && !rainy(c)) at(m, LayerId::Crows) = 0.6f * outside;

    // The town's hum while the power lasts; a room's closed-in air.
    if (f.hum) at(m, LayerId::Hum) = 0.6f;
    if (f.indoors) at(m, LayerId::Room) = f.openToOutside ? 0.35f : 0.7f;

    // The crowd: the number of the dead within 30 tiles.
    if (f.deadAround >= 3) at(m, LayerId::Crowd) = std::clamp(static_cast<float>(f.deadAround - 2) / 18.0f, 0.0f, 1.0f);

    // Fire.
    if (f.burningNear > 0)
    {
        if (f.burningNear >= 4) at(m, LayerId::FireLarge) = std::clamp(0.4f + 0.05f * static_cast<float>(f.burningNear), 0.0f, 1.0f);
        else at(m, LayerId::FireSmall) = 0.4f + 0.2f * static_cast<float>(f.burningNear);
    }

    // The ringing itself (the duck of everything else is the mixer's, at once).
    if (f.tinnitusSeconds > 0.0f) at(m, LayerId::Tinnitus) = std::clamp(f.tinnitusSeconds / kTinnitusSeconds, 0.0f, 1.0f);
    return m;
}
} // namespace ambience

namespace music
{
bool stingsAllowed(Tone tone)
{
    return tone != Tone::Hopeless;
}

void Director::reset()
{
    *this = Director{};
}

void Director::update(const Facts& f)
{
    if (!f.playing || f.tone == Tone::Hopeless)
    {
        // Hopeless: no music at all.
        mPadOn = false;
        mPulseOn = false;
        mCalmSince = f.calm ? (mCalmSince < 0.0 ? f.hours : mCalmSince) : -1.0;
        return;
    }

    const bool brutal = f.tone == Tone::BrutalArcade;
    if (f.calm)
    {
        if (mCalmSince < 0.0) mCalmSince = f.hours;
    }
    else
    {
        mCalmSince = -1.0;
    }

    // Danger stops the pad at once.
    if (mPadOn && (!f.calm || f.hours >= mPadEnds)) mPadOn = false;

    const double calmFor = brutal ? kBrutalCalmHours : kGrimCalmHours;
    const double gap = brutal ? kBrutalGapHours : kGrimGapHours;
    if (!mPadOn && f.calm && mCalmSince >= 0.0 && f.hours - mCalmSince >= calmFor && f.hours - mLastStart >= gap &&
        (f.indoors || f.dawnOrDusk))
    {
        static constexpr LayerId kPads[] = {LayerId::PadA, LayerId::PadB, LayerId::PadC};
        mPad = kPads[mNext % 3];
        ++mNext;
        mPadOn = true;
        mLastStart = f.hours;
        mPadEnds = f.hours + kPadHours;
        ++mStarts;
    }

    // Brutal Arcade: a low pulse under a fight of four or more.
    mPulseOn = brutal && f.fight >= kFightForPulse;
}
} // namespace music
