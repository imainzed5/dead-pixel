#include "world/WeatherRules.h"

#include <algorithm>
#include <cmath>

namespace weatherrules
{
namespace
{
constexpr int kDaysPerYear = WeatherState::kDaysPerSeason * 4;
constexpr double kDawn[4] = {6.0, 5.0, 7.0, 8.0};
constexpr double kDusk[4] = {20.0, 21.5, 18.5, 17.0};

int index(Season s)
{
    return static_cast<int>(s) & 3;
}

// How far into the change to the next season this day is: 0 for most of a
// season, then 1/3 and 2/3 over its last days (as the temperature blends).
double transition(int day)
{
    const int into = seasonDay(day);
    const int from = WeatherState::kDaysPerSeason - static_cast<int>(WeatherState::kSeasonTransitionDays);
    if (into < from) return 0.0;
    return static_cast<double>(into - from) / static_cast<double>(WeatherState::kSeasonTransitionDays);
}

double blendHour(const double (&table)[4], int day)
{
    const int s = index(seasonOf(day));
    const double t = transition(day);
    return table[s] * (1.0 - t) + table[(s + 1) % 4] * t;
}
} // namespace

float playerViewScale(WeatherCondition c)
{
    switch (c)
    {
    case WeatherCondition::Rain: return 0.9f;
    case WeatherCondition::HeavyRain: return 0.75f;
    case WeatherCondition::Thunderstorm: return 0.7f;
    case WeatherCondition::Snow: return 0.85f;
    default: return 1.0f;
    }
}

float playerViewTiles(WeatherCondition c)
{
    switch (c)
    {
    case WeatherCondition::Fog: return 5.0f;
    case WeatherCondition::Blizzard: return 4.0f;
    default: return 0.0f;
    }
}

ViewFade playerViewFade(WeatherCondition c)
{
    // Fog fades from 5 tiles and a blizzard shows about 4; otherwise rain
    // and snow pull a clear day's 20 tiles in.
    if (c == WeatherCondition::Fog) return ViewFade{5.0f, 9.0f};
    if (c == WeatherCondition::Blizzard) return ViewFade{3.0f, 5.5f};
    const float scale = playerViewScale(c);
    if (scale >= 1.0f) return ViewFade{};
    const float end = kClearViewTiles * scale;
    return ViewFade{end * 0.55f, end};
}

float deadSightScale(WeatherCondition c, bool flash)
{
    switch (c)
    {
    case WeatherCondition::Rain: return 0.85f;
    case WeatherCondition::HeavyRain: return 0.7f;
    case WeatherCondition::Thunderstorm: return flash ? 1.5f : 0.6f;
    case WeatherCondition::Fog: return 0.45f;
    case WeatherCondition::Snow: return 0.8f;
    case WeatherCondition::Blizzard: return 0.3f;
    default: return 1.0f;
    }
}

float noiseScale(WeatherCondition c)
{
    switch (c)
    {
    case WeatherCondition::Rain: return 0.7f;
    case WeatherCondition::HeavyRain: return 0.55f;
    case WeatherCondition::Thunderstorm: return 0.45f;
    case WeatherCondition::Snow: return 0.75f;
    case WeatherCondition::Blizzard: return 0.4f;
    default: return 1.0f;
    }
}

int footstepTierShift(WeatherCondition c)
{
    return c == WeatherCondition::Snow ? -1 : 0;
}

float bodyShift(WeatherCondition c, bool raincoat)
{
    switch (c)
    {
    case WeatherCondition::Rain: return raincoat ? 0.0f : -4.0f;
    case WeatherCondition::HeavyRain: return raincoat ? 0.0f : -4.5f;
    // A storm's wind still bites through a raincoat.
    case WeatherCondition::Thunderstorm: return raincoat ? -2.0f : -6.0f;
    case WeatherCondition::Heatwave: return 8.0f;
    case WeatherCondition::Snow: return -3.0f;
    case WeatherCondition::Blizzard: return -8.0f;
    default: return 0.0f;
    }
}

float coldDriftScale(WeatherCondition c)
{
    return c == WeatherCondition::Blizzard ? 2.0f : 1.0f;
}

float fireSpreadScale(WeatherCondition c, bool grass)
{
    switch (c)
    {
    case WeatherCondition::Rain: return 0.6f;
    case WeatherCondition::HeavyRain: return 0.35f;
    case WeatherCondition::Thunderstorm: return 0.2f;
    case WeatherCondition::Heatwave: return grass ? 1.5f : 1.0f;
    case WeatherCondition::Snow: return 0.5f;
    case WeatherCondition::Blizzard: return 0.2f;
    default: return 1.0f;
    }
}

float outdoorFireSeconds(WeatherCondition c)
{
    switch (c)
    {
    case WeatherCondition::Rain: return 120.0f;        // 2 game hours
    case WeatherCondition::HeavyRain: return 40.0f;    // 40 game minutes
    case WeatherCondition::Thunderstorm: return 40.0f; // rains as hard
    case WeatherCondition::Blizzard: return 2.0f;      // fires in the open die
    default: return 0.0f;
    }
}

float nightDarkScale(WeatherCondition c)
{
    return c == WeatherCondition::Cloudy ? 0.9f : 1.0f;
}

float collectorRate(WeatherCondition c)
{
    switch (c)
    {
    case WeatherCondition::Rain: return 1.0f;
    case WeatherCondition::HeavyRain: return 2.0f;
    case WeatherCondition::Thunderstorm: return 2.0f;
    case WeatherCondition::Snow: return 0.5f;
    case WeatherCondition::Blizzard: return 0.5f;
    default: return 0.0f;
    }
}

float bloodOutdoorSeconds(WeatherCondition c)
{
    if (rainy(c)) return 3.0f;
    if (snowy(c)) return 30.0f;
    return 10.0f;
}

bool rainy(WeatherCondition c)
{
    return c == WeatherCondition::Rain || c == WeatherCondition::HeavyRain || c == WeatherCondition::Thunderstorm;
}

bool snowy(WeatherCondition c)
{
    return c == WeatherCondition::Snow || c == WeatherCondition::Blizzard;
}

bool heatwave(WeatherCondition c)
{
    return c == WeatherCondition::Heatwave;
}

bool precipitation(WeatherCondition c)
{
    return rainy(c) || snowy(c);
}

float sleepQualityShift(WeatherCondition c)
{
    return c == WeatherCondition::Thunderstorm ? -0.1f : 0.0f;
}

float deadDrawTiles(WeatherCondition c)
{
    return c == WeatherCondition::Fog ? 6.0f : 0.0f;
}

float minimapTiles(WeatherCondition c)
{
    return c == WeatherCondition::Fog ? 6.0f : 0.0f;
}

float thirstScale(WeatherCondition c, bool outdoors, bool daytime)
{
    return c == WeatherCondition::Heatwave && outdoors && daytime ? 1.6f : 1.0f;
}

float spoilScale(WeatherCondition c, Season season)
{
    return (c == WeatherCondition::Heatwave ? 1.5f : 1.0f) * seasonSpoilScale(season);
}

float deadMoveScale(WeatherCondition c)
{
    return c == WeatherCondition::Blizzard ? 0.6f : 1.0f;
}

float playerMoveScale(WeatherCondition c, bool outdoors)
{
    return c == WeatherCondition::Blizzard && outdoors ? 0.75f : 1.0f;
}

const char* label(WeatherCondition c)
{
    switch (c)
    {
    case WeatherCondition::Clear: return "Clear";
    case WeatherCondition::Cloudy: return "Cloudy";
    case WeatherCondition::Rain: return "Rain";
    case WeatherCondition::HeavyRain: return "Heavy rain";
    case WeatherCondition::Thunderstorm: return "Storm";
    case WeatherCondition::Fog: return "Fog";
    case WeatherCondition::Blizzard: return "Blizzard";
    case WeatherCondition::Heatwave: return "Heatwave";
    case WeatherCondition::Snow: return "Snow";
    }
    return "Clear";
}

const char* changeWord(WeatherCondition next)
{
    switch (next)
    {
    case WeatherCondition::Clear: return "clearing";
    case WeatherCondition::Cloudy: return "clouding";
    case WeatherCondition::Rain:
    case WeatherCondition::HeavyRain: return "rain near";
    case WeatherCondition::Thunderstorm: return "storm near";
    case WeatherCondition::Fog: return "fog near";
    case WeatherCondition::Blizzard: return "blizzard near";
    case WeatherCondition::Heatwave: return "warming";
    case WeatherCondition::Snow: return "snow near";
    }
    return "";
}

float clothingInsulation(float bestWarmth, bool rags)
{
    return std::max(0.0f, bestWarmth) + (rags ? kRagsWrap : 0.0f);
}

Season seasonOf(int day)
{
    const int d = ((day % kDaysPerYear) + kDaysPerYear) % kDaysPerYear;
    return static_cast<Season>(d / WeatherState::kDaysPerSeason);
}

int seasonDay(int day)
{
    return ((day % WeatherState::kDaysPerSeason) + WeatherState::kDaysPerSeason) % WeatherState::kDaysPerSeason;
}

const char* seasonName(Season s)
{
    switch (s)
    {
    case Season::Spring: return "Spring";
    case Season::Summer: return "Summer";
    case Season::Autumn: return "Autumn";
    case Season::Winter: return "Winter";
    }
    return "Spring";
}

int firstDayOf(Season s)
{
    return s == Season::Spring ? 1 : WeatherState::kDaysPerSeason * index(s);
}

double dawnHour(int day)
{
    return blendHour(kDawn, day);
}

double duskHour(int day)
{
    return blendHour(kDusk, day);
}

bool isNight(int day, double hours)
{
    return hours >= duskHour(day) || hours < dawnHour(day);
}

float nightFactor(int day, double hours)
{
    const double dusk = duskHour(day);
    const double dawn = dawnHour(day);
    if (hours >= dawn && hours < dusk - 2.0) return 0.0f;
    if (hours >= dusk - 2.0 && hours < dusk) return static_cast<float>((hours - (dusk - 2.0)) / 2.0);
    if (hours >= dawn - 1.0 && hours < dawn) return 1.0f - static_cast<float>(hours - (dawn - 1.0));
    return 1.0f;
}

float seasonSpoilScale(Season s)
{
    return s == Season::Summer ? 1.25f : 1.0f;
}

float deadNightScale(Season s)
{
    return s == Season::Summer ? 1.1f : 1.0f;
}

bool bushesFruit(int day)
{
    const Season s = seasonOf(day);
    if (s == Season::Winter) return false;
    const int ofYear = ((day % kDaysPerYear) + kDaysPerYear) % kDaysPerYear;
    return !(s == Season::Autumn && ofYear >= kBushesStopDay);
}

bool mushroomsCommon(int day)
{
    return seasonOf(day) == Season::Autumn;
}

bool frostMorning(int day)
{
    return ((day % kDaysPerYear) + kDaysPerYear) % kDaysPerYear == kFrostDay;
}

bool pondsFrozen(int day)
{
    return seasonOf(day) == Season::Winter;
}

bool gardensGrow(int day)
{
    return seasonOf(day) != Season::Winter;
}

bool collectorsFrozen(float airTemperature)
{
    return airTemperature < 0.0f;
}

float stepSnowCover(float cover, WeatherCondition c, float airTemperature, float gameHours)
{
    if (c == WeatherCondition::Snow) cover += gameHours / kSnowBuildHours;
    else if (c == WeatherCondition::Blizzard) cover += gameHours * 2.0f / kSnowBuildHours;
    else if (airTemperature > kSnowMeltAbove) cover -= gameHours / kSnowMeltHours;
    return std::clamp(cover, 0.0f, 1.0f);
}

WeatherCondition inSeason(WeatherCondition c, Season s)
{
    if (s == Season::Winter && (c == WeatherCondition::Rain || c == WeatherCondition::HeavyRain))
    {
        return WeatherCondition::Snow;
    }
    if (s != Season::Winter && c == WeatherCondition::Snow) return WeatherCondition::Rain;
    if (s != Season::Winter && c == WeatherCondition::Blizzard) return WeatherCondition::HeavyRain;
    return c;
}
} // namespace weatherrules
