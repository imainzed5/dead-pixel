#pragma once

#include "components/Weather.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

// One step of the radio's forecast (phase 6): a condition starting this many
// game hours from now.
struct ForecastStep
{
    float startsIn = 0.0f;
    WeatherCondition condition = WeatherCondition::Clear;
};

class WeatherSystem
{
public:
    // A new run: the weather follows its own generator, seeded from the run.
    void reset(std::uint32_t seed);
    void update(WeatherState& weather, float dtSeconds, double gameHours, int currentDay);
    // Set the weather and keep it for `gameHours` (dev harness and tests).
    void hold(WeatherState& weather, WeatherCondition condition, float gameHours)
    {
        weather.condition = condition;
        mHoursLeft = gameHours;
        weather.conditionDuration = gameHours;
    }

    // The next condition for `season`, and how long it lasts (2-6 game hours).
    static WeatherCondition rollCondition(Season season, std::mt19937& rng);
    static float rollHours(std::mt19937& rng);

    // The coming `hours` of weather, from a copy of the generator (the real
    // weather will follow it): the current condition first.
    [[nodiscard]] std::vector<ForecastStep> forecast(const WeatherState& weather, float hours) const;
    // The radio's one line for a forecast heard at `hourOfDay`, e.g.
    // "Rain by evening. Colder tonight."
    [[nodiscard]] static std::string forecastLine(const std::vector<ForecastStep>& steps, double hourOfDay);

    // For the checkpoint: hours left on this condition and the generator.
    [[nodiscard]] float hoursLeft() const { return mHoursLeft; }
    [[nodiscard]] std::string rngState() const;
    // Back to a saved state. False (and nothing changed) if `rngState` is unreadable.
    bool restore(WeatherState& weather, WeatherCondition condition, float hoursLeft, const std::string& rngState);

private:
    // Season base temperatures (Celsius)
    static constexpr float kSpringBaseTemp = 14.0f;
    static constexpr float kSummerBaseTemp = 28.0f;
    static constexpr float kAutumnBaseTemp = 12.0f;
    static constexpr float kWinterBaseTemp = -2.0f;

    // Day/night temperature swing
    static constexpr float kDayNightSwing = 6.0f;

    // Weather condition effects
    static constexpr float kRainTempDrop = 3.0f;
    static constexpr float kFogVisibility = 0.4f;
    static constexpr float kStormVisibility = 0.6f;

    float mHoursLeft = 0.0f; // game hours until the next condition
    std::mt19937 mRng{0x5EA7u};
};
