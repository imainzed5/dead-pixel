#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "ecs/World.h"

class TileMap;

// Everything outside the body that the needs simulation reacts to.
struct NeedsEnvironment
{
    double gameHours = 12.0;
    float outdoorTemperature = 18.0f; // Celsius: the season's air at this hour, before the weather
    const TileMap* tileMap = nullptr; // indoor lookup; null = everyone is outdoors
    std::vector<glm::vec2> heatSources; // world-space centers of fires and campfires

    // Phase 7: the weather as world/WeatherRules reads it.
    float weatherShift = 0.0f;         // degrees outdoors (weatherrules::bodyShift)
    float weatherShiftRaincoat = 0.0f; // the same in a raincoat
    bool rainy = false;                // rain soaks whoever is out in it
    float coldDrift = 1.0f;            // a blizzard: the cold comes on twice as fast
    float thirstOutdoors = 1.0f;       // a heatwave by day
    bool heatwave = false;             // the sun is the danger: shade and a coat matter
    std::vector<glm::vec2> shade;      // world-space trunks of trees that give shade
};

// Hunger, thirst, fatigue, sleep debt, body temperature and illness.
//
// Design rules (see docs/revival_plan.md, Wave 1a):
//  - Needs drain on a daily rhythm: roughly two drinks, one or two meals and one
//    sleep per in-game day (24 real minutes).
//  - Penalties ramp in gradually (stamina first, then illness, then health), so
//    an empty bar is a countdown to fix, not an instant death.
//  - Body temperature follows the environment: shelter, fire, clothing and
//    movement keep you warm; night, rain, wind and winter push you toward
//    hypothermia.
class NeedsSystem
{
public:
    void update(World& world, float dtSeconds, const NeedsEnvironment& environment) const;

    // Air temperature the body actually feels.
    // `weatherShift` counts only outdoors; clothing keeps you warm but
    // never makes the air feel hotter than kHotAmbient.
    [[nodiscard]] static float effectiveAmbient(float outdoorTemperature, bool indoors, float weatherShift,
                                                bool nearHeat, float clothingInsulation);
    // Body temperature the body drifts toward in that air, given activity heat.
    [[nodiscard]] static float targetBodyTemperature(float effectiveAmbientTemperature, float activityHeat);

    // Body temperature thresholds.
    static constexpr float kChillyBelow = 36.0f;
    static constexpr float kColdBelow = 35.0f;
    static constexpr float kHypothermicBelow = 34.0f;
    static constexpr float kSevereHypothermiaBelow = 32.5f;
    static constexpr float kGravelyIllAbove = 80.0f; // illness that takes health
    static constexpr float kOverheatedAbove = 38.3f;
    static constexpr float kHotAmbient = 30.0f;
    static constexpr float kShadeRadius = 40.0f; // under a tree canopy

    // Needs thresholds (0-100 bars; higher is better for hunger and thirst).
    static constexpr float kHungryBelow = 25.0f;
    static constexpr float kStarvingBelow = 10.0f;
    static constexpr float kThirstyBelow = 25.0f;
    static constexpr float kParchedBelow = 10.0f;
    static constexpr float kTiredAbove = 70.0f; // sleep debt

    static constexpr float kHeatSourceRadius = 96.0f; // 3 tiles
    static constexpr float kTemperatureDriftPerSecond = 0.012f;
    static constexpr float kSleepingDrainScale = 0.5f;
};
