#include "systems/WeatherSystem.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
// One real second is one game minute (core/GameShared.h).
constexpr float kGameHoursPerRealSecond = 1.0f / 60.0f;

float baseTemperature(Season season)
{
    switch (season)
    {
    case Season::Spring: return 14.0f;
    case Season::Summer: return 28.0f;
    case Season::Autumn: return 12.0f;
    case Season::Winter: return -2.0f;
    }
    return 14.0f;
}

float seasonTransitionTemp(Season from, Season to, float t)
{
    return baseTemperature(from) * (1.0f - t) + baseTemperature(to) * t;
}

Season nextSeason(Season s)
{
    return static_cast<Season>((static_cast<int>(s) + 1) % 4);
}

// A float in [0, 1), the same on every compiler.
float unit(std::mt19937& rng)
{
    return static_cast<float>(rng() >> 8) * (1.0f / 16777216.0f);
}
}

void WeatherSystem::reset(std::uint32_t seed)
{
    mRng.seed(seed);
    mHoursLeft = 0.0f;
}

WeatherCondition WeatherSystem::rollCondition(Season season, std::mt19937& rng)
{
    const int roll = static_cast<int>(unit(rng) * 100.0f);
    switch (season)
    {
    case Season::Spring:
        if (roll < 30) return WeatherCondition::Clear;
        if (roll < 55) return WeatherCondition::Cloudy;
        if (roll < 80) return WeatherCondition::Rain;
        if (roll < 90) return WeatherCondition::Fog;
        return WeatherCondition::Thunderstorm;
    case Season::Summer:
        if (roll < 45) return WeatherCondition::Clear;
        if (roll < 65) return WeatherCondition::Cloudy;
        if (roll < 80) return WeatherCondition::Heatwave;
        if (roll < 90) return WeatherCondition::Rain;
        return WeatherCondition::Thunderstorm;
    case Season::Autumn:
        if (roll < 20) return WeatherCondition::Clear;
        if (roll < 45) return WeatherCondition::Cloudy;
        if (roll < 70) return WeatherCondition::Rain;
        if (roll < 85) return WeatherCondition::HeavyRain;
        if (roll < 95) return WeatherCondition::Fog;
        return WeatherCondition::Thunderstorm;
    case Season::Winter:
        if (roll < 25) return WeatherCondition::Clear;
        if (roll < 50) return WeatherCondition::Cloudy;
        if (roll < 70) return WeatherCondition::Fog;
        if (roll < 85) return WeatherCondition::Rain;
        return WeatherCondition::Blizzard;
    }
    return WeatherCondition::Clear;
}

float WeatherSystem::rollHours(std::mt19937& rng)
{
    return 2.0f + unit(rng) * 4.0f;
}

std::string WeatherSystem::rngState() const
{
    std::ostringstream out;
    out << mRng;
    return out.str();
}

bool WeatherSystem::restore(WeatherState& weather, WeatherCondition condition, float hoursLeft,
                            const std::string& rngState)
{
    std::mt19937 rng;
    std::istringstream in(rngState);
    in >> rng;
    if (in.fail())
    {
        return false;
    }
    mRng = rng;
    mHoursLeft = hoursLeft;
    weather.condition = condition;
    weather.conditionDuration = hoursLeft;
    return true;
}

void WeatherSystem::update(WeatherState& weather, float dtSeconds, double gameHours, int currentDay)
{
    // --- Season progression ---
    weather.seasonDay = currentDay % WeatherState::kDaysPerSeason;
    const int seasonIndex = (currentDay / WeatherState::kDaysPerSeason) % 4;
    weather.season = static_cast<Season>(seasonIndex);
    weather.seasonProgress = static_cast<float>(weather.seasonDay) / static_cast<float>(WeatherState::kDaysPerSeason);

    // --- Temperature calculation ---
    const bool inTransition = weather.seasonDay >= (WeatherState::kDaysPerSeason - static_cast<int>(WeatherState::kSeasonTransitionDays));
    float base;
    if (inTransition)
    {
        const float transitionT = static_cast<float>(weather.seasonDay - (WeatherState::kDaysPerSeason - static_cast<int>(WeatherState::kSeasonTransitionDays))) / WeatherState::kSeasonTransitionDays;
        base = seasonTransitionTemp(weather.season, nextSeason(weather.season), transitionT);
    }
    else
    {
        base = baseTemperature(weather.season);
    }

    // Day/night temperature swing: warmest at 14:00, coldest at 04:00
    const float hourOffset = static_cast<float>(gameHours) - 14.0f;
    const float dayNightFactor = std::cos(hourOffset * 3.14159f / 12.0f);
    weather.temperature = base + dayNightFactor * kDayNightSwing;

    // --- Weather condition state machine (counted in game hours) ---
    (void)gameHours;
    mHoursLeft -= dtSeconds * kGameHoursPerRealSecond;
    if (mHoursLeft <= 0.0f)
    {
        weather.condition = rollCondition(weather.season, mRng);
        mHoursLeft = rollHours(mRng);
        weather.conditionDuration = mHoursLeft;
    }

    // --- Apply condition effects ---
    weather.windStrength = 0.0f;
    weather.rainIntensity = 0.0f;
    weather.visibilityMultiplier = 1.0f;

    switch (weather.condition)
    {
    case WeatherCondition::Clear:
        break;
    case WeatherCondition::Cloudy:
        weather.windStrength = 0.2f;
        break;
    case WeatherCondition::Rain:
        weather.rainIntensity = 0.4f;
        weather.windStrength = 0.3f;
        weather.temperature -= kRainTempDrop;
        break;
    case WeatherCondition::HeavyRain:
        weather.rainIntensity = 0.8f;
        weather.windStrength = 0.5f;
        weather.visibilityMultiplier = 0.7f;
        weather.temperature -= kRainTempDrop * 1.5f;
        break;
    case WeatherCondition::Thunderstorm:
        weather.rainIntensity = 1.0f;
        weather.windStrength = 0.8f;
        weather.visibilityMultiplier = kStormVisibility;
        weather.temperature -= kRainTempDrop * 2.0f;
        break;
    case WeatherCondition::Fog:
        weather.visibilityMultiplier = kFogVisibility;
        weather.windStrength = 0.05f;
        break;
    case WeatherCondition::Blizzard:
        weather.rainIntensity = 0.9f; // snow treated as rain intensity
        weather.windStrength = 0.9f;
        weather.visibilityMultiplier = 0.3f;
        weather.temperature -= 8.0f;
        break;
    case WeatherCondition::Heatwave:
        weather.temperature += 8.0f;
        weather.windStrength = 0.1f;
        break;
    }
}
