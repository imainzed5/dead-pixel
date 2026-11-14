// Weather and seasons (overhaul phase 7): every column of the table in
// docs/overhaul/phases/P07_weather_and_fire.md, and the hours of the seasons.
#include "TestFramework.h"

#include <cmath>
#include <string>

#include "systems/NeedsSystem.h"
#include "systems/WeatherSystem.h"
#include "world/WeatherRules.h"

using namespace weatherrules;
using WC = WeatherCondition;

DP_TEST(weather_table_player_sight)
{
    DP_CHECK_NEAR(playerViewScale(WC::Clear), 1.0f, 1e-6f);
    DP_CHECK_NEAR(playerViewScale(WC::Cloudy), 1.0f, 1e-6f);
    DP_CHECK_NEAR(playerViewScale(WC::Rain), 0.9f, 1e-6f);
    DP_CHECK_NEAR(playerViewScale(WC::HeavyRain), 0.75f, 1e-6f);
    DP_CHECK_NEAR(playerViewScale(WC::Thunderstorm), 0.7f, 1e-6f);
    DP_CHECK_NEAR(playerViewScale(WC::Snow), 0.85f, 1e-6f);
    DP_CHECK_NEAR(playerViewTiles(WC::Fog), 5.0f, 1e-6f);
    DP_CHECK_NEAR(playerViewTiles(WC::Blizzard), 4.0f, 1e-6f);
    DP_CHECK_NEAR(playerViewTiles(WC::Clear), 0.0f, 1e-6f);
    // The distance fog: none on a clear day; fog fades from 5 tiles; rain
    // pulls a clear day's view in by its share.
    DP_CHECK(playerViewFade(WC::Clear).endTiles <= 0.0f);
    DP_CHECK(playerViewFade(WC::Heatwave).endTiles <= 0.0f);
    DP_CHECK_NEAR(playerViewFade(WC::Fog).startTiles, 5.0f, 1e-6f);
    DP_CHECK(playerViewFade(WC::Blizzard).endTiles > 4.0f && playerViewFade(WC::Blizzard).startTiles < 4.0f);
    DP_CHECK_NEAR(playerViewFade(WC::Rain).endTiles, kClearViewTiles * 0.9f, 1e-4f);
    DP_CHECK(playerViewFade(WC::Thunderstorm).endTiles < playerViewFade(WC::HeavyRain).endTiles);
}

DP_TEST(weather_table_dead_sight_and_noise)
{
    DP_CHECK_NEAR(deadSightScale(WC::Clear), 1.0f, 1e-6f);
    DP_CHECK_NEAR(deadSightScale(WC::Cloudy), 1.0f, 1e-6f);
    DP_CHECK_NEAR(deadSightScale(WC::Rain), 0.85f, 1e-6f);
    DP_CHECK_NEAR(deadSightScale(WC::HeavyRain), 0.7f, 1e-6f);
    DP_CHECK_NEAR(deadSightScale(WC::Thunderstorm), 0.6f, 1e-6f);
    DP_CHECK_NEAR(deadSightScale(WC::Thunderstorm, true), 1.5f, 1e-6f); // a flash
    DP_CHECK_NEAR(deadSightScale(WC::Fog), 0.45f, 1e-6f);
    DP_CHECK_NEAR(deadSightScale(WC::Heatwave), 1.0f, 1e-6f);
    DP_CHECK_NEAR(deadSightScale(WC::Snow), 0.8f, 1e-6f);
    DP_CHECK_NEAR(deadSightScale(WC::Blizzard), 0.3f, 1e-6f);

    DP_CHECK_NEAR(noiseScale(WC::Clear), 1.0f, 1e-6f);
    DP_CHECK_NEAR(noiseScale(WC::Fog), 1.0f, 1e-6f);
    DP_CHECK_NEAR(noiseScale(WC::Heatwave), 1.0f, 1e-6f);
    DP_CHECK_NEAR(noiseScale(WC::Rain), 0.7f, 1e-6f);
    DP_CHECK_NEAR(noiseScale(WC::HeavyRain), 0.55f, 1e-6f);
    DP_CHECK_NEAR(noiseScale(WC::Thunderstorm), 0.45f, 1e-6f);
    DP_CHECK_NEAR(noiseScale(WC::Snow), 0.75f, 1e-6f);
    DP_CHECK_NEAR(noiseScale(WC::Blizzard), 0.4f, 1e-6f);
    DP_CHECK(footstepTierShift(WC::Snow) == -1);
    DP_CHECK(footstepTierShift(WC::Rain) == 0);
}

DP_TEST(weather_table_body)
{
    DP_CHECK_NEAR(bodyShift(WC::Clear), 0.0f, 1e-6f);
    DP_CHECK_NEAR(bodyShift(WC::Rain), -4.0f, 1e-6f);
    DP_CHECK_NEAR(bodyShift(WC::Rain, true), 0.0f, 1e-6f); // a Raincoat
    DP_CHECK_NEAR(bodyShift(WC::HeavyRain), -4.5f, 1e-6f);
    DP_CHECK_NEAR(bodyShift(WC::HeavyRain, true), 0.0f, 1e-6f);
    DP_CHECK_NEAR(bodyShift(WC::Thunderstorm), -6.0f, 1e-6f);
    DP_CHECK_NEAR(bodyShift(WC::Heatwave), 8.0f, 1e-6f);
    DP_CHECK_NEAR(bodyShift(WC::Snow), -3.0f, 1e-6f);
    DP_CHECK_NEAR(bodyShift(WC::Blizzard), -8.0f, 1e-6f);
    DP_CHECK_NEAR(coldDriftScale(WC::Blizzard), 2.0f, 1e-6f);
    DP_CHECK_NEAR(coldDriftScale(WC::Snow), 1.0f, 1e-6f);
    // Heat: thirst x1.6 outdoors by day; overheated and heatstroke limits.
    DP_CHECK_NEAR(thirstScale(WC::Heatwave, true, true), 1.6f, 1e-6f);
    DP_CHECK_NEAR(thirstScale(WC::Heatwave, false, true), 1.0f, 1e-6f);
    DP_CHECK_NEAR(thirstScale(WC::Heatwave, true, false), 1.0f, 1e-6f);
    DP_CHECK(kHeatstrokeAbove > kOverheatedAbove);
    DP_CHECK_NEAR(kOverheatedAbove, NeedsSystem::kOverheatedAbove, 1e-6f);
    // Clothing: the best warm thing carried, and 2 more wrapped in rags.
    DP_CHECK_NEAR(clothingInsulation(11.0f, false), 11.0f, 1e-6f);
    DP_CHECK_NEAR(clothingInsulation(5.0f, true), 7.0f, 1e-6f);
    DP_CHECK_NEAR(clothingInsulation(0.0f, false), 0.0f, 1e-6f);
}

DP_TEST(weather_table_fire)
{
    DP_CHECK_NEAR(fireSpreadScale(WC::Clear), 1.0f, 1e-6f);
    DP_CHECK_NEAR(fireSpreadScale(WC::Fog), 1.0f, 1e-6f);
    DP_CHECK_NEAR(fireSpreadScale(WC::Rain), 0.6f, 1e-6f);
    DP_CHECK_NEAR(fireSpreadScale(WC::HeavyRain), 0.35f, 1e-6f);
    DP_CHECK_NEAR(fireSpreadScale(WC::Thunderstorm), 0.2f, 1e-6f);
    DP_CHECK_NEAR(fireSpreadScale(WC::Heatwave, true), 1.5f, 1e-6f);  // grass
    DP_CHECK_NEAR(fireSpreadScale(WC::Heatwave, false), 1.0f, 1e-6f);
    DP_CHECK_NEAR(fireSpreadScale(WC::Snow), 0.5f, 1e-6f);
    DP_CHECK_NEAR(fireSpreadScale(WC::Blizzard), 0.2f, 1e-6f);
    // Rain puts out fires in the open: in 2 hours, heavy rain in 40 minutes,
    // a blizzard at once; dry weather never does.
    DP_CHECK_NEAR(outdoorFireSeconds(WC::Rain), 120.0f, 1e-6f);
    DP_CHECK_NEAR(outdoorFireSeconds(WC::HeavyRain), 40.0f, 1e-6f);
    DP_CHECK(outdoorFireSeconds(WC::Blizzard) > 0.0f && outdoorFireSeconds(WC::Blizzard) < 5.0f);
    DP_CHECK_NEAR(outdoorFireSeconds(WC::Clear), 0.0f, 1e-6f);
    DP_CHECK_NEAR(outdoorFireSeconds(WC::Heatwave), 0.0f, 1e-6f);
}

DP_TEST(weather_table_other)
{
    DP_CHECK_NEAR(nightDarkScale(WC::Cloudy), 0.9f, 1e-6f);
    DP_CHECK_NEAR(nightDarkScale(WC::Clear), 1.0f, 1e-6f);
    DP_CHECK_NEAR(collectorRate(WC::Clear), 0.0f, 1e-6f);
    DP_CHECK_NEAR(collectorRate(WC::Rain), 1.0f, 1e-6f);
    DP_CHECK_NEAR(collectorRate(WC::HeavyRain), 2.0f, 1e-6f);
    DP_CHECK_NEAR(collectorRate(WC::Snow), 0.5f, 1e-6f);
    DP_CHECK_NEAR(bloodOutdoorSeconds(WC::Rain), 3.0f, 1e-6f);
    DP_CHECK_NEAR(bloodOutdoorSeconds(WC::Snow), 30.0f, 1e-6f);
    DP_CHECK_NEAR(bloodOutdoorSeconds(WC::Clear), 10.0f, 1e-6f);
    DP_CHECK_NEAR(sleepQualityShift(WC::Thunderstorm), -0.1f, 1e-6f);
    DP_CHECK_NEAR(sleepQualityShift(WC::Rain), 0.0f, 1e-6f);
    DP_CHECK_NEAR(deadDrawTiles(WC::Fog), 6.0f, 1e-6f);
    DP_CHECK_NEAR(minimapTiles(WC::Fog), 6.0f, 1e-6f);
    DP_CHECK_NEAR(deadDrawTiles(WC::Clear), 0.0f, 1e-6f);
    DP_CHECK_NEAR(spoilScale(WC::Heatwave, Season::Spring), 1.5f, 1e-6f);
    DP_CHECK_NEAR(spoilScale(WC::Clear, Season::Summer), 1.25f, 1e-6f);
    DP_CHECK_NEAR(deadMoveScale(WC::Blizzard), 0.6f, 1e-6f);
    DP_CHECK_NEAR(playerMoveScale(WC::Blizzard, true), 0.75f, 1e-6f);
    DP_CHECK_NEAR(playerMoveScale(WC::Blizzard, false), 1.0f, 1e-6f);
    DP_CHECK(rainy(WC::Thunderstorm) && !rainy(WC::Snow));
    DP_CHECK(snowy(WC::Blizzard) && precipitation(WC::Snow) && !precipitation(WC::Fog));
}

DP_TEST(weather_words_for_the_hud)
{
    DP_CHECK(std::string(label(WC::Snow)) == "Snow");
    DP_CHECK(std::string(label(WC::Thunderstorm)) == "Storm");
    DP_CHECK(std::string(changeWord(WC::Cloudy)) == "clouding");
    DP_CHECK(std::string(changeWord(WC::Rain)) == "rain near");
    DP_CHECK(std::string(changeWord(WC::Clear)) == "clearing");
    DP_CHECK_NEAR(kWarningHours, 1.5f, 1e-6f);
}

DP_TEST(season_hours_dawn_and_dusk)
{
    // Spring 06:00-20:00, Summer 05:00-21:30, Autumn 07:00-18:30, Winter 08:00-17:00.
    DP_CHECK(seasonOf(1) == Season::Spring);
    DP_CHECK(seasonOf(15) == Season::Summer);
    DP_CHECK(seasonOf(30) == Season::Autumn);
    DP_CHECK(seasonOf(45) == Season::Winter);
    DP_CHECK(seasonOf(60) == Season::Spring);
    DP_CHECK_NEAR(dawnHour(5), 6.0, 1e-9);
    DP_CHECK_NEAR(duskHour(5), 20.0, 1e-9);
    DP_CHECK_NEAR(dawnHour(20), 5.0, 1e-9);
    DP_CHECK_NEAR(duskHour(20), 21.5, 1e-9);
    DP_CHECK_NEAR(dawnHour(35), 7.0, 1e-9);
    DP_CHECK_NEAR(duskHour(35), 18.5, 1e-9);
    DP_CHECK_NEAR(dawnHour(50), 8.0, 1e-9);
    DP_CHECK_NEAR(duskHour(50), 17.0, 1e-9);
    // Blended over the last three days of a season.
    DP_CHECK(duskHour(12) == 20.0);
    DP_CHECK(duskHour(13) > 20.0 && duskHour(13) < 21.5);
    DP_CHECK(duskHour(14) > duskHour(13) && duskHour(14) < 21.5);
    DP_CHECK(duskHour(43) < 18.5 && duskHour(43) > 17.0);
    // Night by the season's hours.
    DP_CHECK(isNight(50, 17.5));
    DP_CHECK(!isNight(20, 21.0));
    DP_CHECK(isNight(5, 5.5) && !isNight(20, 5.5));
    DP_CHECK_NEAR(nightFactor(5, 12.0), 0.0f, 1e-6f);
    DP_CHECK_NEAR(nightFactor(5, 23.0), 1.0f, 1e-6f);
    // Names, first days, the year's events.
    DP_CHECK(std::string(seasonName(Season::Summer)) == "Summer");
    DP_CHECK(firstDayOf(Season::Summer) == 15 && firstDayOf(Season::Spring) == 1);
    DP_CHECK(bushesFruit(20) && bushesFruit(39) && !bushesFruit(40) && !bushesFruit(50));
    DP_CHECK(mushroomsCommon(35) && !mushroomsCommon(20));
    DP_CHECK(frostMorning(43) && !frostMorning(42));
    DP_CHECK(pondsFrozen(50) && !pondsFrozen(35));
    DP_CHECK(!gardensGrow(50) && gardensGrow(20));
    DP_CHECK(collectorsFrozen(-1.0f) && !collectorsFrozen(1.0f));
    DP_CHECK_NEAR(seasonSpoilScale(Season::Summer), 1.25f, 1e-6f);
    DP_CHECK_NEAR(deadNightScale(Season::Summer), 1.1f, 1e-6f);
}

DP_TEST(snow_builds_over_six_hours_and_melts_on_a_warm_day)
{
    float cover = 0.0f;
    for (int i = 0; i < 6; ++i) cover = stepSnowCover(cover, WC::Snow, -2.0f, 1.0f);
    DP_CHECK_NEAR(cover, 1.0f, 1e-5f);
    // Cold and clear: it stays.
    DP_CHECK_NEAR(stepSnowCover(cover, WC::Clear, -1.0f, 5.0f), 1.0f, 1e-6f);
    // Above 2 C it melts over 12 hours.
    DP_CHECK_NEAR(stepSnowCover(cover, WC::Clear, 5.0f, 6.0f), 0.5f, 1e-5f);
    DP_CHECK(stepSnowCover(0.0f, WC::Blizzard, -5.0f, 1.0f) > stepSnowCover(0.0f, WC::Snow, -5.0f, 1.0f));
    // Winter turns rain to snow; snow outside winter is rain.
    DP_CHECK(inSeason(WC::Rain, Season::Winter) == WC::Snow);
    DP_CHECK(inSeason(WC::Snow, Season::Summer) == WC::Rain);
    DP_CHECK(inSeason(WC::Blizzard, Season::Autumn) == WC::HeavyRain);
}

DP_TEST(weather_system_reports_wind_and_what_comes_next)
{
    WeatherSystem system;
    WeatherState weather{};
    system.reset(1234u);
    system.hold(weather, WC::Rain, 0.5f);
    system.update(weather, 1.0f / 60.0f, 12.0, 5);
    DP_CHECK(weather.condition == WC::Rain);
    DP_CHECK(weather.hoursToChange <= 0.5f + 1e-3f);
    const float wind = std::sqrt(weather.windX * weather.windX + weather.windY * weather.windY);
    DP_CHECK_NEAR(wind, 1.0f, 1e-3f);
    // The HUD temperature is the air and the weather's bite.
    DP_CHECK_NEAR(weather.temperature, weather.airTemperature + bodyShift(WC::Rain), 1e-3f);
    // A held wind holds.
    system.holdWind(weather, 1.0f, 0.0f);
    system.update(weather, 1.0f / 60.0f, 12.0, 5);
    DP_CHECK_NEAR(weather.windX, 1.0f, 1e-4f);
    DP_CHECK_NEAR(weather.windY, 0.0f, 1e-4f);
}
