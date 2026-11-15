#pragma once

#include "components/Weather.h"

// What the weather and the seasons do to the rules (overhaul phase 7), as
// plain functions: one per column of the table in
// docs/overhaul/phases/P07_weather_and_fire.md. Every system asks these;
// none reads a WeatherCondition for itself.
namespace weatherrules
{
// --- The table, one function per column ---

// Player sight: the view range as a share of a clear day's, and a hard
// limit in tiles where one applies (fog fades from 5 tiles, a blizzard
// shows 4). 0 means no limit.
inline constexpr float kClearViewTiles = 20.0f;
[[nodiscard]] float playerViewScale(WeatherCondition c);
[[nodiscard]] float playerViewTiles(WeatherCondition c);
// Where the distance haze starts and where nothing is left, tiles.
// Both 0 when nothing is hidden.
struct ViewFade
{
    float startTiles = 0.0f;
    float endTiles = 0.0f;
};
[[nodiscard]] ViewFade playerViewFade(WeatherCondition c);

// The dead's sight, scaled. During a lightning flash a storm shows
// everything (x1.5 instead of x0.6).
[[nodiscard]] float deadSightScale(WeatherCondition c, bool flash = false);

// How far a noise carries (every tier).
[[nodiscard]] float noiseScale(WeatherCondition c);
// Footsteps in snow are one tier quieter.
[[nodiscard]] int footstepTierShift(WeatherCondition c);

// Body: degrees added to the air the body feels outdoors. A Raincoat takes
// the rain's chill away (rain, heavy rain, storm). A heatwave counts only in
// the sun (not indoors, not under a tree).
[[nodiscard]] float bodyShift(WeatherCondition c, bool raincoat = false);
// Cold comes on this much faster outdoors (a blizzard: x2).
[[nodiscard]] float coldDriftScale(WeatherCondition c);

// Fire: how readily it spreads (grass in a heatwave x1.5), and how many
// seconds (game minutes) the rain takes to put out a fire in the open.
// 0: the weather does not put fires out.
[[nodiscard]] float fireSpreadScale(WeatherCondition c, bool grass = false);
[[nodiscard]] float outdoorFireSeconds(WeatherCondition c);

// Other: the night is darker under cloud; rain collectors fill (units of
// water a game hour); how long blood shows outdoors (game minutes); mud or
// snow for prints; sleep quality; fog hides the dead and the minimap past
// a few tiles; heat makes you thirsty and food spoil; a blizzard slows
// everyone in it.
[[nodiscard]] float nightDarkScale(WeatherCondition c);
[[nodiscard]] float collectorRate(WeatherCondition c);
[[nodiscard]] float bloodOutdoorSeconds(WeatherCondition c);
[[nodiscard]] bool rainy(WeatherCondition c);  // rain, heavy rain, storm
[[nodiscard]] bool snowy(WeatherCondition c);     // snow, blizzard
[[nodiscard]] bool heatwave(WeatherCondition c);
[[nodiscard]] bool precipitation(WeatherCondition c);
[[nodiscard]] float sleepQualityShift(WeatherCondition c);
[[nodiscard]] float deadDrawTiles(WeatherCondition c);  // 0: no limit
[[nodiscard]] float minimapTiles(WeatherCondition c);   // 0: no limit
[[nodiscard]] float thirstScale(WeatherCondition c, bool outdoors, bool daytime);
[[nodiscard]] float spoilScale(WeatherCondition c, Season season);
[[nodiscard]] float deadMoveScale(WeatherCondition c);
[[nodiscard]] float playerMoveScale(WeatherCondition c, bool outdoors);
// A lightning strike sets a tree alight this often.
inline constexpr float kLightningTreeChance = 0.05f;
// A lightning flash lights the town this long, seconds.
inline constexpr float kFlashSeconds = 0.2f;

// The HUD's word for the weather, and the second word when it is about to
// change ("Clear, clouding").
[[nodiscard]] const char* label(WeatherCondition c);
[[nodiscard]] const char* changeWord(WeatherCondition next);
// The sky warns this far ahead (game hours).
inline constexpr float kWarningHours = 1.5f;

// --- Wet and hot (the body) ---
inline constexpr float kWetAfterSeconds = 20.0f;  // 20 game minutes in the rain
inline constexpr float kWetHours = 1.0f;          // then wet for an hour after shelter
inline constexpr float kWetColdDrift = 1.5f;
inline constexpr float kFireDryPerHour = 6.0f;    // a fire dries you in 10 minutes
inline constexpr float kOverheatedAbove = 38.3f;
inline constexpr float kHeatstrokeAbove = 39.5f;
inline constexpr float kHeatstrokeDamagePerSecond = 0.05f;
inline constexpr float kOverheatedRegenScale = 0.7f; // stamina regen -30 %
inline constexpr float kOverheatedThirst = 1.5f;
inline constexpr float kWinterCoatHeat = 3.0f;       // carried in a heatwave
inline constexpr float kDrinkCooling = 0.6f;         // a drink takes this off when hot

// Clothing: the warmest thing carried counts, and Rags wrapped round it
// add 2 more.
inline constexpr float kRagsWrap = 2.0f;
[[nodiscard]] float clothingInsulation(float bestWarmth, bool rags);

// --- Smoke ---
inline constexpr int kSmokyTiles = 3;              // burning tiles in a building
inline constexpr float kCoughAfterSeconds = 20.0f;
inline constexpr float kSmokeHurtAfterSeconds = 40.0f;
inline constexpr float kSmokeDamagePerSecond = 1.0f;
inline constexpr float kCoughingRegenScale = 0.5f;

// --- Seasons ---
[[nodiscard]] Season seasonOf(int day);
[[nodiscard]] int seasonDay(int day);
[[nodiscard]] const char* seasonName(Season s);
// The first day of a season (for the harness and the day card).
[[nodiscard]] int firstDayOf(Season s);
// Dawn and dusk by the day: Spring 06:00-20:00, Summer 05:00-21:30, Autumn
// 07:00-18:30, Winter 08:00-17:00, blended over the last 3 days of each.
[[nodiscard]] double dawnHour(int day);
[[nodiscard]] double duskHour(int day);
[[nodiscard]] bool isNight(int day, double hours);
// 0 by day, 1 by night: two hours of dusk before dark, one of dawn before light.
[[nodiscard]] float nightFactor(int day, double hours);

// Summer: food spoils x1.25, the dead are x1.1 more active by night.
[[nodiscard]] float seasonSpoilScale(Season s);
[[nodiscard]] float deadNightScale(Season s);
// Autumn: bushes stop fruiting on day 40; mushrooms are common.
inline constexpr int kBushesStopDay = 40;
inline constexpr int kFrostDay = 43;
[[nodiscard]] bool bushesFruit(int day);
[[nodiscard]] bool mushroomsCommon(int day);
// The morning of the first frost (day 43 of the year).
[[nodiscard]] bool frostMorning(int day);
// Winter: ponds freeze, gardens rest, collectors freeze below 0 C, the dead
// wade slower through snow on the ground.
[[nodiscard]] bool pondsFrozen(int day);
[[nodiscard]] bool gardensGrow(int day);
[[nodiscard]] bool collectorsFrozen(float airTemperature);
inline constexpr float kSnowDeadScale = 0.8f;
inline constexpr float kSnowDeepEnough = 0.3f; // cover that slows the dead and takes prints
inline constexpr float kIceAccelerationScale = 0.5f;

// Snow on the ground, 0..1: it builds over 6 game hours of snow (3 in a
// blizzard) and melts over a clear day above 2 C.
inline constexpr float kSnowBuildHours = 6.0f;
inline constexpr float kSnowMeltHours = 12.0f;
inline constexpr float kSnowMeltAbove = 2.0f;
[[nodiscard]] float stepSnowCover(float cover, WeatherCondition c, float airTemperature, float gameHours);

// What one season's weather rolls: winter's rain falls as snow.
[[nodiscard]] WeatherCondition inSeason(WeatherCondition c, Season s);
} // namespace weatherrules
