// Fire on a grid (overhaul phase 7): fuel by tile, spread by wind, rain
// putting it out.
#include "TestFramework.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

#include "systems/FireSystem.h"

using namespace firerules;

namespace
{
TileFacts on(Ground g)
{
    TileFacts f;
    f.ground = g;
    return f;
}

// A field of grass, with what burns decided by `fuel`.
FireSystem::Env field(float fuel, glm::vec2 windTo = {1.0f, 0.0f}, float windStrength = 0.0f)
{
    FireSystem::Env env;
    env.fuelAt = [fuel](int x, int y) { return x >= -60 && x <= 60 && y >= -60 && y <= 60 ? fuel : 0.0f; };
    env.outdoors = [](int, int) { return true; };
    env.grass = [](int, int) { return true; };
    env.windTo = windTo;
    env.windStrength = windStrength;
    env.seed = 99u;
    return env;
}
} // namespace

DP_TEST(fire_fuel_by_tile)
{
    DP_CHECK_NEAR(fuelOf(on(Ground::Grass)), 0.5f, 1e-6f);
    TileFacts dry = on(Ground::Grass);
    dry.heatwave = true;
    DP_CHECK_NEAR(fuelOf(dry), 0.8f, 1e-6f);
    TileFacts wet = on(Ground::Grass);
    wet.wet = true;
    wet.heatwave = true; // wet wins
    DP_CHECK_NEAR(fuelOf(wet), 0.2f, 1e-6f);
    TileFacts snow = on(Ground::Grass);
    snow.snow = true;
    DP_CHECK_NEAR(fuelOf(snow), 0.0f, 1e-6f);
    DP_CHECK_NEAR(fuelOf(on(Ground::WoodFloor)), 0.6f, 1e-6f);
    DP_CHECK_NEAR(fuelOf(on(Ground::Carpet)), 0.7f, 1e-6f);
    DP_CHECK_NEAR(fuelOf(on(Ground::WoodWall)), 0.4f, 1e-6f);
    DP_CHECK_NEAR(fuelOf(on(Ground::Tree)), 0.9f, 1e-6f);
    DP_CHECK_NEAR(fuelOf(on(Ground::Bare)), 0.0f, 1e-6f);
    TileFacts table = on(Ground::WoodFloor);
    table.woodFurniture = true;
    DP_CHECK_NEAR(fuelOf(table), 0.9f, 1e-6f);
    TileFacts fridge = on(Ground::Bare);
    fridge.metalFurniture = true;
    DP_CHECK_NEAR(fuelOf(fridge), 0.05f, 1e-6f);
    TileFacts boarded = on(Ground::WoodWall);
    boarded.boards = true;
    DP_CHECK_NEAR(fuelOf(boarded), 0.6f, 1e-6f);
    TileFacts full = on(Ground::Carpet);
    full.woodFurniture = true;
    full.boards = true;
    DP_CHECK_NEAR(fuelOf(full), 1.0f, 1e-6f); // never more than 1
}

DP_TEST(fire_spread_chance_by_wind)
{
    const glm::vec2 east(1.0f, 0.0f);
    // Calm: every way alike.
    DP_CHECK_NEAR(windFactor({1, 0}, east, 0.1f), 1.0f, 1e-6f);
    DP_CHECK_NEAR(windFactor({-1, 0}, east, 0.1f), 1.0f, 1e-6f);
    // A wind: x1.6 downwind, x0.6 upwind, across unchanged.
    DP_CHECK_NEAR(windFactor({1, 0}, east, 0.4f), kDownwind, 1e-6f);
    DP_CHECK_NEAR(windFactor({-1, 0}, east, 0.4f), kUpwind, 1e-6f);
    DP_CHECK_NEAR(windFactor({0, 1}, east, 0.4f), 1.0f, 1e-6f);
    // chance = fuel x (0.25 + 0.3 x heat) x weather x wind
    DP_CHECK_NEAR(spreadChance(0.5f, 1.0f, 1.0f, 1.0f), 0.5f * 0.55f, 1e-6f);
    DP_CHECK_NEAR(spreadChance(0.5f, 0.0f, 1.0f, 1.0f), 0.5f * 0.25f, 1e-6f);
    DP_CHECK_NEAR(spreadChance(0.5f, 1.0f, 0.35f, 1.6f), 0.5f * 0.55f * 0.35f * 1.6f, 1e-6f);
    DP_CHECK_NEAR(spreadChance(0.0f, 1.0f, 1.0f, 1.6f), 0.0f, 1e-6f);
    DP_CHECK(spreadChance(1.0f, 1.0f, 2.0f, 1.6f) <= 1.0f);
    // Four neighbours; a strong wind adds the two downwind diagonals.
    DP_CHECK(spreadDirections(east, 0.3f).size() == 4);
    const std::vector<glm::ivec2> strong = spreadDirections(east, 0.8f);
    DP_CHECK(strong.size() == 6);
    DP_CHECK(strong[4].x == 1 && strong[5].x == 1);
}

DP_TEST(fire_burns_its_fuel_and_heats_up)
{
    FireSystem fire;
    FireSystem::Env env = field(0.5f);
    env.fuelAt = [](int x, int y) { return x == 0 && y == 0 ? 0.5f : 0.0f; };
    DP_CHECK(fire.ignite({0, 0}, 0.2f, true, env));
    DP_CHECK(!fire.ignite({5, 5}, 0.2f, true, env)); // nothing to burn there
    FireSystem::Events events;
    fire.update(10.0f, env, events);
    DP_CHECK(fire.at({0, 0}) != nullptr);
    DP_CHECK_NEAR(fire.at({0, 0})->heat, 0.7f, 1e-4f); // 0.2 + 10/20
    DP_CHECK(fire.at({0, 0})->byPlayer);
    // 0.5 fuel at 0.02/s: out after 25 s, burned.
    for (int i = 0; i < 20 && fire.count() > 0; ++i) fire.update(1.0f, env, events);
    DP_CHECK(fire.count() == 0);
    DP_CHECK(events.burnedOut.size() == 1);
}

DP_TEST(fire_runs_downwind)
{
    FireSystem fire;
    const FireSystem::Env env = field(0.8f, {1.0f, 0.0f}, 1.0f);
    fire.ignite({0, 0}, kMolotovHeat, true, env);
    fire.ignite({0, 1}, kMolotovHeat, true, env);
    fire.ignite({0, -1}, kMolotovHeat, true, env);
    std::set<std::pair<int, int>> touched;
    FireSystem::Events events;
    for (int step = 0; step < 60 * 60; ++step)
    {
        fire.update(1.0f / 60.0f, env, events);
        for (const FireSystem::Burning& b : fire.all()) touched.insert({b.tile.x, b.tile.y});
    }
    int east = 0;
    int west = 0;
    for (const auto& [x, y] : touched)
    {
        if (x > 0) east = std::max(east, x);
        if (x < 0) west = std::max(west, -x);
    }
    DP_CHECK(east > west);
}

DP_TEST(fire_in_the_rain_goes_out)
{
    FireSystem fire;
    FireSystem::Env env = field(0.2f); // wet grass round it
    env.outdoorDieSeconds = 40.0f;     // heavy rain
    env.spreadScale = 0.35f;
    // A heap that would burn for 50 s: the rain has it out first.
    env.fuelAt = [](int x, int y) { return x == 0 && y == 0 ? 1.0f : (std::abs(x) <= 60 && std::abs(y) <= 60 ? 0.2f : 0.0f); };
    fire.ignite({0, 0}, 1.0f, true, env);
    FireSystem::Events events;
    for (int step = 0; step < 41 * 60 && fire.count() > 0; ++step) fire.update(1.0f / 60.0f, env, events);
    DP_CHECK(fire.count() == 0);
    DP_CHECK(std::find(events.wentOut.begin(), events.wentOut.end(), glm::ivec2(0, 0)) != events.wentOut.end());
    // Water from a bottle: -0.9 heat.
    FireSystem::Env dry = field(0.5f);
    fire.ignite({3, 3}, 1.0f, true, dry);
    DP_CHECK(fire.douse({3, 3}, kDouseHeat));
    DP_CHECK(fire.at({3, 3}) != nullptr && fire.at({3, 3})->heat < 0.2f);
    DP_CHECK(fire.douse({3, 3}, kDouseHeat));
    DP_CHECK(fire.at({3, 3}) == nullptr);
}

DP_TEST(fire_grid_answers_by_tile)
{
    FireSystem fire;
    const FireSystem::Env env = field(0.5f);
    fire.ignite({10, 10}, 0.5f, false, env);
    fire.ignite({12, 10}, 0.5f, false, env);
    fire.ignite({30, 30}, 0.5f, false, env);
    DP_CHECK(fire.burning({10, 10}) && !fire.burning({11, 10}));
    DP_CHECK(fire.burningNear({11, 10}, 2).size() == 2);
    DP_CHECK(fire.burningNear({0, 0}, 3).empty());
    DP_CHECK(FireSystem::tileOf(FireSystem::key({-3, 7})) == glm::ivec2(-3, 7));
    fire.restore({5, 5}, 0.4f, 0.6f, true);
    DP_CHECK(fire.at({5, 5}) != nullptr && fire.at({5, 5})->byPlayer);
}
