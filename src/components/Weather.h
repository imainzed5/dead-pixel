#pragma once

enum class Season
{
    Spring = 0,
    Summer = 1,
    Autumn = 2,
    Winter = 3
};

// Saved as a number in checkpoints: append, never renumber.
enum class WeatherCondition
{
    Clear,
    Cloudy,
    Rain,
    HeavyRain,
    Thunderstorm,
    Fog,
    Blizzard,
    Heatwave,
    Snow // phase 7: winter's rain
};

struct WeatherState
{
    Season season = Season::Spring;
    WeatherCondition condition = WeatherCondition::Clear;

    // Celsius. `airTemperature` is the season and the hour; `temperature`
    // is what the weather makes of it outdoors (the HUD's number).
    float airTemperature = 20.0f;
    float temperature = 20.0f;
    float windStrength = 0.0f;       // 0-1 scale
    float windX = 1.0f;              // the way the wind blows toward (unit, tile axes)
    float windY = 0.0f;
    float rainIntensity = 0.0f;      // 0-1 (0=dry, 1=downpour); snow counts
    float visibilityMultiplier = 1.0f; // the dead's sight (WeatherRules)

    // Phase 7: snow lying on the ground (0..1), and what comes next.
    float snowCover = 0.0f;
    WeatherCondition nextCondition = WeatherCondition::Clear;
    float hoursToChange = 0.0f;

    // Timers
    float conditionDuration = 0.0f;  // hours remaining for current condition
    float seasonProgress = 0.0f;     // 0-1 progress through current season
    int seasonDay = 0;               // days into current season

    static constexpr int kDaysPerSeason = 15;
    static constexpr float kSeasonTransitionDays = 3.0f;
};
