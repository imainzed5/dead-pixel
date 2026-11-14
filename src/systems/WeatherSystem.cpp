#include "systems/WeatherSystem.h"
#include "world/WeatherRules.h"

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
    // Which way the wind blows is the run's; it turns slowly over the day.
    mWindBase = static_cast<float>(seed % 628u) * 0.01f;
    mWindHeld = false;
    mAirHeld = std::numeric_limits<float>::quiet_NaN();
}

void WeatherSystem::holdWind(WeatherState& weather, float x, float y)
{
    const float length = std::sqrt(x * x + y * y);
    mWindHeld = length > 0.0001f;
    if (!mWindHeld) return;
    mHeldWindX = x / length;
    mHeldWindY = y / length;
    mHeldWindStrength = std::min(1.0f, length);
    weather.windX = mHeldWindX;
    weather.windY = mHeldWindY;
    weather.windStrength = mHeldWindStrength;
}

void WeatherSystem::refreshNext(WeatherState& weather) const
{
    std::mt19937 rng = mRng;
    weather.nextCondition = weatherrules::inSeason(rollCondition(weather.season, rng), weather.season);
    weather.hoursToChange = mHoursLeft;
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
    refreshNext(weather);
    return true;
}

std::vector<ForecastStep> WeatherSystem::forecast(const WeatherState& weather, float hours) const
{
    std::vector<ForecastStep> steps;
    steps.push_back(ForecastStep{0.0f, weather.condition});
    std::mt19937 rng = mRng;
    float at = std::max(0.0f, mHoursLeft);
    while (at < hours)
    {
        const WeatherCondition next = weatherrules::inSeason(rollCondition(weather.season, rng), weather.season);
        steps.push_back(ForecastStep{at, next});
        at += rollHours(rng);
    }
    return steps;
}

namespace
{
bool wet(WeatherCondition c)
{
    return c == WeatherCondition::Rain || c == WeatherCondition::HeavyRain || c == WeatherCondition::Thunderstorm ||
           c == WeatherCondition::Blizzard || c == WeatherCondition::Snow;
}

const char* wetName(WeatherCondition c)
{
    switch (c)
    {
    case WeatherCondition::HeavyRain: return "Heavy rain";
    case WeatherCondition::Thunderstorm: return "Storms";
    case WeatherCondition::Blizzard: return "A blizzard";
    case WeatherCondition::Snow: return "Snow";
    default: return "Rain";
    }
}

const char* partOfDay(double hour)
{
    const double h = std::fmod(std::fmod(hour, 24.0) + 24.0, 24.0);
    if (h >= 5.0 && h < 11.0) return "morning";
    if (h >= 11.0 && h < 17.0) return "afternoon";
    if (h >= 17.0 && h < 21.0) return "evening";
    return "nightfall";
}
} // namespace

std::string WeatherSystem::forecastLine(const std::vector<ForecastStep>& steps, double hourOfDay)
{
    std::string line;
    const bool wetNow = !steps.empty() && wet(steps.front().condition);
    if (wetNow)
    {
        line = std::string(wetName(steps.front().condition)) + " all day.";
        for (const ForecastStep& step : steps)
        {
            if (step.startsIn > 0.0f && !wet(step.condition))
            {
                line = std::string("Dry by ") + partOfDay(hourOfDay + step.startsIn) + ".";
                break;
            }
        }
    }
    else
    {
        line = "Dry for now.";
        for (const ForecastStep& step : steps)
        {
            if (step.startsIn > 0.0f && wet(step.condition))
            {
                line = std::string(wetName(step.condition)) + " by " + partOfDay(hourOfDay + step.startsIn) + ".";
                break;
            }
            if (step.startsIn > 0.0f && step.condition == WeatherCondition::Fog)
            {
                line = std::string("Fog by ") + partOfDay(hourOfDay + step.startsIn) + ".";
                break;
            }
        }
    }
    const double h = std::fmod(hourOfDay, 24.0);
    line += h >= 10.0 && h < 22.0 ? " Colder tonight." : " Warmer by midday.";
    return line;
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
    weather.airTemperature = base + dayNightFactor * kDayNightSwing;
    if (!std::isnan(mAirHeld)) weather.airTemperature = mAirHeld;

    // --- Weather condition state machine (counted in game hours) ---
    const float hoursPassed = dtSeconds * kGameHoursPerRealSecond;
    mHoursLeft -= hoursPassed;
    if (mHoursLeft <= 0.0f)
    {
        weather.condition = weatherrules::inSeason(rollCondition(weather.season, mRng), weather.season);
        mHoursLeft = rollHours(mRng);
        weather.conditionDuration = mHoursLeft;
        refreshNext(weather);
    }
    weather.hoursToChange = mHoursLeft;

    // --- What the condition does (world/WeatherRules) ---
    weather.temperature = weather.airTemperature + weatherrules::bodyShift(weather.condition);
    weather.visibilityMultiplier = weatherrules::deadSightScale(weather.condition);
    weather.snowCover = weatherrules::stepSnowCover(weather.snowCover, weather.condition, weather.airTemperature, hoursPassed);
    switch (weather.condition)
    {
    case WeatherCondition::Clear: weather.rainIntensity = 0.0f; weather.windStrength = 0.0f; break;
    case WeatherCondition::Cloudy: weather.rainIntensity = 0.0f; weather.windStrength = 0.2f; break;
    case WeatherCondition::Rain: weather.rainIntensity = 0.4f; weather.windStrength = 0.3f; break;
    case WeatherCondition::HeavyRain: weather.rainIntensity = 0.8f; weather.windStrength = 0.5f; break;
    case WeatherCondition::Thunderstorm: weather.rainIntensity = 1.0f; weather.windStrength = 0.8f; break;
    case WeatherCondition::Fog: weather.rainIntensity = 0.0f; weather.windStrength = 0.05f; break;
    case WeatherCondition::Blizzard: weather.rainIntensity = 0.9f; weather.windStrength = 0.9f; break;
    case WeatherCondition::Heatwave: weather.rainIntensity = 0.0f; weather.windStrength = 0.1f; break;
    case WeatherCondition::Snow: weather.rainIntensity = 0.45f; weather.windStrength = 0.25f; break;
    }

    // The wind's way: the run's, turning slowly over the day, unless held.
    if (mWindHeld)
    {
        weather.windX = mHeldWindX;
        weather.windY = mHeldWindY;
        weather.windStrength = mHeldWindStrength;
    }
    else
    {
        const float angle = mWindBase + 0.6f * std::sin(static_cast<float>(gameHours) * 0.26f + static_cast<float>(currentDay));
        weather.windX = std::cos(angle);
        weather.windY = std::sin(angle);
    }
}
