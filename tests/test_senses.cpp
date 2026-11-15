// How the dead sense the living (overhaul phase 4): sight range and cone,
// the awareness meter, noise through walls, weather and wind.
#include "TestFramework.h"

#include "world/Senses.h"

using namespace senses;

DP_TEST(sight_range_scales_with_dark_light_weather_and_crouching)
{
    SightInput in;
    in.baseTiles = 8.0f;
    DP_CHECK_NEAR(sightRangeTiles(in), 8.0f, 1e-4f);
    in.dark = true;
    DP_CHECK_NEAR(sightRangeTiles(in), 4.8f, 1e-4f);
    in.playerLit = true;
    DP_CHECK_NEAR(sightRangeTiles(in), 8.0f * 0.6f * 1.3f, 1e-4f);
    in = SightInput{};
    in.weatherVisibility = 0.5f;
    DP_CHECK_NEAR(sightRangeTiles(in), 4.0f, 1e-4f);
    in = SightInput{};
    in.crouched = true;
    DP_CHECK_NEAR(sightRangeTiles(in), 4.0f, 1e-4f);
    in.moving = true;
    DP_CHECK_NEAR(sightRangeTiles(in), 6.0f, 1e-4f);
}

DP_TEST(a_crouched_still_player_five_tiles_off_in_the_dark_is_out_of_sight)
{
    SightInput in;
    in.dark = true;
    in.crouched = true;
    DP_CHECK(sightRangeTiles(in) < 5.0f); // 8 x 0.6 x 0.5 = 2.4
}

DP_TEST(the_cone_sees_in_front_not_behind)
{
    const glm::vec2 facing(1.0f, 0.0f);
    DP_CHECK(inCone(facing, glm::vec2(1.0f, 0.2f)));
    DP_CHECK(inCone(facing, glm::vec2(0.2f, 1.0f)));   // 79 degrees off: inside 80
    DP_CHECK(!inCone(facing, glm::vec2(-1.0f, 0.0f)));
    DP_CHECK(!inCone(facing, glm::vec2(-0.3f, 1.0f)));
    DP_CHECK(inCone(glm::vec2(0.0f), glm::vec2(-1.0f, 0.0f))); // alerted: all round
}

DP_TEST(awareness_fills_faster_close_and_twice_as_fast_for_a_runner)
{
    DP_CHECK_NEAR(awarenessFillPerSecond(0.0f, 8.0f, false), 1.6f, 1e-4f);
    DP_CHECK_NEAR(awarenessFillPerSecond(8.0f, 8.0f, false), 0.6f, 1e-4f);
    DP_CHECK_NEAR(awarenessFillPerSecond(4.0f, 8.0f, true), 2.0f * 1.1f, 1e-4f);
    float a = 0.0f;
    for (int i = 0; i < 60; ++i) a = stepAwareness(a, true, 8.0f, 8.0f, false, true, 1.0f / 60.0f);
    DP_CHECK_NEAR(a, 0.6f, 1e-3f);
}

DP_TEST(awareness_drains_out_of_sight_except_for_the_stalker)
{
    float a = 0.8f;
    a = stepAwareness(a, false, 0.0f, 8.0f, false, true, 1.0f);
    DP_CHECK_NEAR(a, 0.3f, 1e-4f);
    a = stepAwareness(a, false, 0.0f, 8.0f, false, true, 1.0f);
    DP_CHECK_NEAR(a, 0.0f, 1e-4f);
    float s = 0.8f;
    s = stepAwareness(s, false, 0.0f, 8.0f, false, false, 5.0f);
    DP_CHECK_NEAR(s, 0.8f, 1e-4f);
    DP_CHECK(stepAwareness(0.95f, true, 0.0f, 8.0f, false, true, 1.0f) <= 1.0f);
}

DP_TEST(noise_through_a_sealed_building_drops_a_tier_and_an_open_one_does_not)
{
    const Shell outside{};
    const Shell sealedHouse{3, true};
    const Shell openHouse{3, false};
    DP_CHECK(tierHeard(NoiseTier::Medium, sealedHouse, outside) == NoiseTier::Soft);
    DP_CHECK(tierHeard(NoiseTier::Medium, openHouse, outside) == NoiseTier::Medium);
    DP_CHECK(tierHeard(NoiseTier::Medium, sealedHouse, sealedHouse) == NoiseTier::Medium); // same building
    DP_CHECK(tierHeard(NoiseTier::Loud, sealedHouse, Shell{5, true}) == NoiseTier::Soft);  // two shells
    DP_CHECK(tierHeard(NoiseTier::Whisper, sealedHouse, outside) == NoiseTier::Whisper);
    // A swing indoors (Medium, 15 tiles): not heard 10 tiles out with the
    // house shut, heard with a door open.
    DP_CHECK(heardRadiusTiles(NoiseTier::Medium, sealedHouse, outside, WeatherCondition::Clear) < 10.0f);
    DP_CHECK(heardRadiusTiles(NoiseTier::Medium, openHouse, outside, WeatherCondition::Clear) > 10.0f);
}

DP_TEST(rain_muffles_and_wind_carries_loud_noise)
{
    DP_CHECK_NEAR(weatherHearingScale(WeatherCondition::Clear), 1.0f, 1e-6f);
    DP_CHECK_NEAR(weatherHearingScale(WeatherCondition::Rain), 0.7f, 1e-6f);
    DP_CHECK_NEAR(weatherHearingScale(WeatherCondition::HeavyRain), 0.55f, 1e-6f);
    DP_CHECK_NEAR(weatherHearingScale(WeatherCondition::Thunderstorm), 0.45f, 1e-6f);
    // Walking (Soft, 7 tiles) in heavy rain is not heard at 5 tiles.
    DP_CHECK(heardRadiusTiles(NoiseTier::Soft, Shell{}, Shell{}, WeatherCondition::HeavyRain) < 5.0f);
    const glm::vec2 east(1.0f, 0.0f);
    DP_CHECK_NEAR(windScale(NoiseTier::Loud, east, 1.0f, east), 1.2f, 1e-4f);
    DP_CHECK_NEAR(windScale(NoiseTier::Loud, east, 1.0f, -east), 0.8f, 1e-4f);
    DP_CHECK_NEAR(windScale(NoiseTier::Medium, east, 1.0f, east), 1.0f, 1e-4f);
}
