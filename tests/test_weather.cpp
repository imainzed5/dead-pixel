#include "TestFramework.h"

#include <random>
#include <vector>

#include "components/Weather.h"
#include "systems/WeatherSystem.h"

namespace
{
// The conditions over `days`, one entry per new roll (one real second = one game minute).
std::vector<int> weatherLog(std::uint32_t seed, int days, std::vector<float>* lengths = nullptr)
{
    WeatherSystem system;
    WeatherState state;
    system.reset(seed);
    std::vector<int> log;
    float lastDuration = -1.0f;
    double hours = 6.0;
    int day = 1;
    for (int minute = 0; minute < days * 24 * 60; ++minute)
    {
        system.update(state, 1.0f, hours, day);
        if (state.conditionDuration != lastDuration)
        {
            lastDuration = state.conditionDuration;
            log.push_back(static_cast<int>(state.condition));
            if (lengths != nullptr) lengths->push_back(state.conditionDuration);
        }
        hours += 1.0 / 60.0;
        if (hours >= 24.0)
        {
            hours -= 24.0;
            ++day;
        }
    }
    return log;
}
} // namespace

DP_TEST(weather_lasts_two_to_six_game_hours)
{
    std::mt19937 rng(3u);
    for (int i = 0; i < 5000; ++i)
    {
        const float hours = WeatherSystem::rollHours(rng);
        DP_CHECK(hours >= 2.0f && hours < 6.0f);
    }

    std::vector<float> lengths;
    const std::vector<int> log = weatherLog(1234u, 7, &lengths);
    // A week of weather is 28 to 84 conditions, not one a minute.
    DP_CHECK(log.size() >= 28 && log.size() <= 85);
    for (float length : lengths)
    {
        DP_CHECK(length >= 2.0f && length < 6.0f);
    }
}

DP_TEST(weather_repeats_for_a_seed)
{
    DP_CHECK(weatherLog(77u, 3) == weatherLog(77u, 3));
    DP_CHECK(weatherLog(77u, 3) != weatherLog(78u, 3));
}

DP_TEST(weather_resumes_from_its_saved_state)
{
    WeatherSystem a;
    WeatherState stateA;
    a.reset(99u);
    for (int i = 0; i < 600; ++i) a.update(stateA, 1.0f, 12.0, 1);

    // What a checkpoint keeps, into a fresh system.
    WeatherSystem b;
    WeatherState stateB;
    b.reset(5u);
    DP_CHECK(b.restore(stateB, stateA.condition, a.hoursLeft(), a.rngState()));
    for (int i = 0; i < 3000; ++i)
    {
        a.update(stateA, 1.0f, 12.0, 1);
        b.update(stateB, 1.0f, 12.0, 1);
        DP_CHECK(stateA.condition == stateB.condition);
    }
    DP_CHECK(!b.restore(stateB, WeatherCondition::Clear, 1.0f, "not a generator"));
}
