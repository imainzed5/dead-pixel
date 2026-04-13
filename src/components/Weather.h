#pragma once

enum class Season
{
    Spring = 0,
    Summer = 1,
    Autumn = 2,
    Winter = 3
};

enum class WeatherCondition
{
    Clear,
    Cloudy,
    Rain,
    HeavyRain,
    Thunderstorm,
    Fog,
    Blizzard,
    Heatwave
};

struct WeatherState
{
    Season season = Season::Spring;
    WeatherCondition condition = WeatherCondition::Clear;

    float temperature = 20.0f;       // Celsius, outdoor ambient
    float windStrength = 0.0f;       // 0-1 scale
    float rainIntensity = 0.0f;      // 0-1 (0=dry, 1=downpour)
    float visibilityMultiplier = 1.0f; // 1=full, 0.5=fog

    // Timers
    float conditionDuration = 0.0f;  // hours remaining for current condition
    float seasonProgress = 0.0f;     // 0-1 progress through current season
    int seasonDay = 0;               // days into current season

    static constexpr int kDaysPerSeason = 15;
    static constexpr float kSeasonTransitionDays = 3.0f;
};
