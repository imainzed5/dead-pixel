#pragma once

#include "components/Weather.h"

class WeatherSystem
{
public:
    void update(WeatherState& weather, float dtSeconds, double gameHours, int currentDay);

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

    float mConditionTimer = 0.0f;
};
