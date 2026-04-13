#include "systems/WeatherSystem.h"

#include <algorithm>
#include <cmath>

namespace
{
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

// Deterministic pseudo-random from seed
int pseudoRandom(int seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed = seed + (seed << 3);
    seed = seed ^ (seed >> 4);
    seed = seed * 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed & 0x7FFFFFFF;
}
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

    // --- Weather condition state machine ---
    mConditionTimer -= dtSeconds;
    if (mConditionTimer <= 0.0f)
    {
        // Pick new condition based on season and pseudo-random
        const int seed = currentDay * 7919 + static_cast<int>(gameHours * 100.0);
        const int roll = pseudoRandom(seed) % 100;

        switch (weather.season)
        {
        case Season::Spring:
            if (roll < 30) weather.condition = WeatherCondition::Clear;
            else if (roll < 55) weather.condition = WeatherCondition::Cloudy;
            else if (roll < 80) weather.condition = WeatherCondition::Rain;
            else if (roll < 90) weather.condition = WeatherCondition::Fog;
            else weather.condition = WeatherCondition::Thunderstorm;
            break;
        case Season::Summer:
            if (roll < 45) weather.condition = WeatherCondition::Clear;
            else if (roll < 65) weather.condition = WeatherCondition::Cloudy;
            else if (roll < 80) weather.condition = WeatherCondition::Heatwave;
            else if (roll < 90) weather.condition = WeatherCondition::Rain;
            else weather.condition = WeatherCondition::Thunderstorm;
            break;
        case Season::Autumn:
            if (roll < 20) weather.condition = WeatherCondition::Clear;
            else if (roll < 45) weather.condition = WeatherCondition::Cloudy;
            else if (roll < 70) weather.condition = WeatherCondition::Rain;
            else if (roll < 85) weather.condition = WeatherCondition::HeavyRain;
            else if (roll < 95) weather.condition = WeatherCondition::Fog;
            else weather.condition = WeatherCondition::Thunderstorm;
            break;
        case Season::Winter:
            if (roll < 25) weather.condition = WeatherCondition::Clear;
            else if (roll < 50) weather.condition = WeatherCondition::Cloudy;
            else if (roll < 70) weather.condition = WeatherCondition::Fog;
            else if (roll < 85) weather.condition = WeatherCondition::Rain;
            else weather.condition = WeatherCondition::Blizzard;
            break;
        }

        // Duration: 2-6 game hours
        mConditionTimer = static_cast<float>(2.0f + (pseudoRandom(seed + 1) % 40) * 0.1f);
        weather.conditionDuration = mConditionTimer;
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
