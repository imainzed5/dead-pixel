// Wave 1a survival pressure: needs pacing, body temperature, panic sources,
// depression pacing and the threat director.
#include "TestFramework.h"

#include <cmath>
#include <vector>

#include <glm/geometric.hpp>

#include "components/Bleeding.h"
#include "components/Facing.h"
#include "components/Health.h"
#include "components/MentalState.h"
#include "components/Needs.h"
#include "components/Sleeping.h"
#include "components/Stamina.h"
#include "components/SurvivorAI.h"
#include "components/Traits.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/Wounds.h"
#include "components/ZombieAI.h"
#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"
#include "systems/MentalStateSystem.h"
#include "systems/NeedsSystem.h"
#include "systems/ThreatDirectorSystem.h"
#include "systems/WoundSystem.h"
#include "systems/ZombieAISystem.h"

namespace
{
constexpr float kTile = 32.0f;

struct SurvivalWorld
{
    World world;
    NoiseModel noise;
    TileMap map;
    Entity player = kInvalidEntity;

    explicit SurvivalWorld(int width = 40, int height = 40)
    {
        world.registerComponent<Transform>();
        world.registerComponent<Health>();
        world.registerComponent<Needs>();
        world.registerComponent<MentalState>();
        world.registerComponent<Traits>();
        world.registerComponent<ZombieAI>();
        world.registerComponent<SurvivorAI>();
        world.registerComponent<Sleeping>();
        world.registerComponent<Velocity>();
        world.registerComponent<Facing>();
        world.registerComponent<Stamina>();
        world.registerComponent<Wounds>();
        world.registerComponent<Bleeding>();

        map.resize(width, height, 32, 32);

        player = world.createEntity();
        world.addComponent(player, tileTransform(width / 2, height / 2));
        world.addComponent(player, Health{});
        world.addComponent(player, Needs{});
        world.addComponent(player, MentalState{});
        world.addComponent(player, Traits{});
        world.addComponent(player, Wounds{});
    }

    static Transform tileTransform(int tx, int ty)
    {
        Transform t{};
        t.x = static_cast<float>(tx) * kTile;
        t.y = static_cast<float>(ty) * kTile;
        t.prevX = t.x;
        t.prevY = t.y;
        return t;
    }

    glm::ivec2 playerTile() const
    {
        const Transform& t = world.getComponent<Transform>(player);
        return {static_cast<int>(t.x / kTile), static_cast<int>(t.y / kTile)};
    }

    Entity addZombie(int tx, int ty, ZombieState state)
    {
        const Entity z = world.createEntity();
        world.addComponent(z, tileTransform(tx, ty));
        ZombieAI ai{};
        ai.state = state;
        world.addComponent(z, ai);
        world.addComponent(z, Health{50.0f, 50.0f});
        world.addComponent(z, Velocity{});
        return z;
    }

    void setWalls(const std::vector<glm::ivec2>& tiles)
    {
        std::vector<int> collision(static_cast<std::size_t>(map.width() * map.height()), 0);
        for (const auto& t : tiles)
        {
            collision[static_cast<std::size_t>(t.y * map.width() + t.x)] = 1;
        }
        map.setCollisionLayer(collision);
    }

    // Marks a rectangle as the inside of building 1.
    void setBuilding(int x0, int y0, int x1, int y1)
    {
        std::vector<int> layer(static_cast<std::size_t>(map.width() * map.height()), 0);
        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                layer[static_cast<std::size_t>(y * map.width() + x)] = 1;
            }
        }
        map.setBuildingLayer(layer);
    }

    MentalState& mental() { return world.getComponent<MentalState>(player); }
    Needs& needs() { return world.getComponent<Needs>(player); }
};
} // namespace

// ----------------------------------------------------------- Body temperature

DP_TEST(needs_temperature_follows_shelter_fire_and_season)
{
    auto target = [](float outdoor, bool indoors, float rain, float wind, bool fire, float clothing)
    {
        return NeedsSystem::targetBodyTemperature(
            NeedsSystem::effectiveAmbient(outdoor, indoors, rain, wind, fire, clothing), 0.0f);
    };

    // Spring night outdoors: chilly, not dangerous.
    const float springNightOut = target(8.0f, false, 0.0f, 0.0f, false, 0.0f);
    DP_CHECK(springNightOut < NeedsSystem::kChillyBelow);
    DP_CHECK(springNightOut >= NeedsSystem::kColdBelow);

    // Rainy windy autumn night outdoors: cold.
    DP_CHECK(target(3.0f, false, 1.0f, 0.6f, false, 0.0f) < NeedsSystem::kColdBelow);

    // Winter night outdoors: hypothermic. A jacket buys time but is not shelter.
    DP_CHECK(target(-8.0f, false, 0.0f, 0.0f, false, 0.0f) < NeedsSystem::kHypothermicBelow);
    const float jacketWinter = target(-8.0f, false, 0.0f, 0.0f, false, 5.0f);
    DP_CHECK(jacketWinter >= NeedsSystem::kHypothermicBelow);
    DP_CHECK(jacketWinter < NeedsSystem::kColdBelow);

    // Shelter: a spring night indoors is comfortable; winter indoors needs a fire.
    DP_CHECK_NEAR(target(8.0f, true, 1.0f, 1.0f, false, 0.0f), 37.0f, 0.001f);
    DP_CHECK(target(-8.0f, true, 0.0f, 0.0f, false, 0.0f) < NeedsSystem::kChillyBelow);
    DP_CHECK_NEAR(target(-8.0f, true, 0.0f, 0.0f, true, 0.0f), 37.0f, 0.001f);
}

DP_TEST(needs_body_temperature_drifts_and_shelter_protects)
{
    SurvivalWorld outside;
    SurvivalWorld inside;
    inside.setBuilding(15, 15, 25, 25);

    NeedsEnvironment winterNight;
    winterNight.outdoorTemperature = -8.0f;
    NeedsSystem needs;

    for (int s = 0; s < 240; ++s) // 4 game hours
    {
        winterNight.tileMap = &outside.map;
        needs.update(outside.world, 1.0f, winterNight);
        winterNight.tileMap = &inside.map;
        needs.update(inside.world, 1.0f, winterNight);
    }
    DP_CHECK(outside.needs().bodyTemperature < NeedsSystem::kHypothermicBelow);
    DP_CHECK(inside.needs().bodyTemperature > outside.needs().bodyTemperature + 1.0f);
    DP_CHECK(outside.needs().illness > inside.needs().illness);
}

// ---------------------------------------------------------------- Needs pacing

DP_TEST(needs_dehydration_is_a_countdown_not_a_cliff)
{
    SurvivalWorld w;
    NeedsEnvironment mild; // 18 C, no map: outdoors but comfortable
    NeedsSystem needs;

    int secondsToEmpty = -1;
    float hpWhenEmpty = 0.0f;
    int secondsToDeath = -1;
    for (int s = 0; s < 6000; ++s)
    {
        needs.update(w.world, 1.0f, mild);
        if (secondsToEmpty < 0 && w.needs().thirst <= 0.0f)
        {
            secondsToEmpty = s;
            hpWhenEmpty = w.world.getComponent<Health>(w.player).current;
        }
        if (w.world.getComponent<Health>(w.player).current <= 0.0f)
        {
            secondsToDeath = s;
            break;
        }
    }

    // Thirst lasts about a day and a half of game time (36 h = 2160 s).
    DP_CHECK(secondsToEmpty > 30 * 60 && secondsToEmpty < 42 * 60);
    DP_CHECK(hpWhenEmpty > 99.0f); // untouched until the bar empties
    // Then hours, not seconds, to fix it.
    DP_CHECK(secondsToDeath - secondsToEmpty > 6 * 60);
}

DP_TEST(needs_sleep_pauses_debt_and_slows_drain)
{
    SurvivalWorld awake;
    SurvivalWorld asleep;
    asleep.world.addComponent(asleep.player, Sleeping{});
    NeedsEnvironment mild;
    NeedsSystem needs;

    for (int s = 0; s < 600; ++s)
    {
        needs.update(awake.world, 1.0f, mild);
        needs.update(asleep.world, 1.0f, mild);
    }
    DP_CHECK(awake.needs().sleepDebt > 30.0f);
    DP_CHECK_NEAR(asleep.needs().sleepDebt, 0.0f, 0.001f);
    const float awakeThirstUsed = 100.0f - awake.needs().thirst;
    const float asleepThirstUsed = 100.0f - asleep.needs().thirst;
    DP_CHECK_NEAR(asleepThirstUsed, awakeThirstUsed * NeedsSystem::kSleepingDrainScale, 0.01f);
}

DP_TEST(needs_low_needs_shave_stamina_gradually)
{
    SurvivalWorld w;
    w.world.addComponent(w.player, Stamina{});
    NeedsEnvironment mild;
    NeedsSystem needs;

    needs.update(w.world, 0.01f, mild);
    DP_CHECK_NEAR(w.world.getComponent<Stamina>(w.player).maximum, 100.0f, 0.001f);

    w.needs().thirst = 20.0f; // thirsty
    needs.update(w.world, 0.01f, mild);
    const float thirstyCap = w.world.getComponent<Stamina>(w.player).maximum;
    DP_CHECK(thirstyCap < 100.0f && thirstyCap > 70.0f);

    w.needs().thirst = 5.0f; // parched
    needs.update(w.world, 0.01f, mild);
    DP_CHECK(w.world.getComponent<Stamina>(w.player).maximum < thirstyCap);
}

// ----------------------------------------------------------------------- Panic

DP_TEST(panic_needs_line_of_sight)
{
    SurvivalWorld w;
    MentalStateSystem mental;
    const glm::ivec2 p = w.playerTile();
    w.addZombie(p.x + 4, p.y, ZombieState::Chase);

    // A wall between them: no panic.
    std::vector<glm::ivec2> wall;
    for (int y = p.y - 3; y <= p.y + 3; ++y) wall.push_back({p.x + 2, y});
    w.setWalls(wall);
    for (int s = 0; s < 10; ++s) mental.update(w.world, w.noise, w.player, 1.0f, 12.0, 1, &w.map);
    DP_CHECK_NEAR(w.mental().panic, 0.0f, 0.001f);

    // Wall gone: a hunting zombie in plain view is frightening.
    w.setWalls({});
    for (int s = 0; s < 10; ++s) mental.update(w.world, w.noise, w.player, 1.0f, 12.0, 1, &w.map);
    DP_CHECK(w.mental().panic > 20.0f);
}

DP_TEST(panic_scream_only_with_a_hunter_in_view)
{
    SurvivalWorld w;
    MentalStateSystem mental;
    const glm::ivec2 p = w.playerTile();
    const Entity z = w.addZombie(p.x + 3, p.y, ZombieState::Wander);

    w.mental().panic = 100.0f;
    mental.update(w.world, w.noise, w.player, 0.1f, 12.0, 1, &w.map);
    DP_CHECK(w.noise.activeEvents().empty());

    w.world.getComponent<ZombieAI>(z).state = ZombieState::Chase;
    w.mental().panic = 100.0f;
    mental.update(w.world, w.noise, w.player, 0.1f, 12.0, 1, &w.map);
    DP_CHECK(!w.noise.activeEvents().empty());
}

DP_TEST(panic_darkness_is_capped_and_indoors_is_calm)
{
    SurvivalWorld outside;
    SurvivalWorld inside;
    inside.setBuilding(15, 15, 25, 25);
    MentalStateSystem mental;

    for (int s = 0; s < 120; ++s)
    {
        mental.update(outside.world, outside.noise, outside.player, 1.0f, 23.0, 1, &outside.map);
        mental.update(inside.world, inside.noise, inside.player, 1.0f, 23.0, 1, &inside.map);
    }
    DP_CHECK(outside.mental().panic > 5.0f);
    DP_CHECK(outside.mental().panic < MentalStateSystem::kPanicScreamThreshold);
    DP_CHECK_NEAR(inside.mental().panic, 0.0f, 0.001f);
}

// ------------------------------------------------------------------ Depression

DP_TEST(depression_builds_over_days_not_minutes)
{
    SurvivalWorld outside;
    SurvivalWorld sheltered;
    sheltered.setBuilding(15, 15, 25, 25);
    SurvivalWorld neglected;
    MentalStateSystem mentalA;
    MentalStateSystem mentalB;
    MentalStateSystem mentalC;

    double hours = 8.0;
    int day = 1;
    for (int s = 0; s < 24 * 60; ++s) // one game day
    {
        neglected.needs().hunger = 10.0f;
        neglected.needs().thirst = 10.0f;
        mentalA.update(outside.world, outside.noise, outside.player, 1.0f, hours, day, &outside.map);
        mentalB.update(sheltered.world, sheltered.noise, sheltered.player, 1.0f, hours, day, &sheltered.map);
        mentalC.update(neglected.world, neglected.noise, neglected.player, 1.0f, hours, day, &neglected.map);
        hours += 1.0 / 60.0;
        if (hours >= 24.0) { hours -= 24.0; ++day; }
    }

    // Alone outdoors with needs met: a slow slide, days from a crisis.
    DP_CHECK(outside.mental().depression > 5.0f);
    DP_CHECK(outside.mental().depression < 25.0f);
    // Fed, rested and under a roof: holding steady.
    DP_CHECK(sheltered.mental().depression < 3.0f);
    // Starving and parched: a fast spiral.
    DP_CHECK(neglected.mental().depression > 50.0f);
}

// --------------------------------------------------------------- Threat director

DP_TEST(director_draws_far_zombies_toward_an_idle_player)
{
    SurvivalWorld w(100, 100);
    const glm::ivec2 p = w.playerTile();
    for (int i = 0; i < 20; ++i)
    {
        const float angle = static_cast<float>(i) * 0.314f;
        w.addZombie(p.x + static_cast<int>(std::cos(angle) * 30.0f), p.y + static_cast<int>(std::sin(angle) * 30.0f),
                    ZombieState::Wander);
    }

    ThreatDirectorSystem director;
    director.reset(42u, ThreatDirectorSystem::Tuning{});
    std::vector<ThreatDirectorSystem::SpawnRequest> spawns;

    // Grace period: nobody is sent.
    float t = 0.0f;
    for (; t < ThreatDirectorSystem::kGracePeriodSeconds - 1.0f; t += 0.5f)
    {
        director.update(w.world, w.map, w.player, 0.5f, 22.0, 1, nullptr, spawns);
    }
    DP_CHECK_EQ(director.migrationsSent(), 0);

    // After it, night waves start drawing zombies in.
    for (; t < 300.0f; t += 0.5f)
    {
        director.update(w.world, w.map, w.player, 0.5f, 22.0, 1, nullptr, spawns);
    }
    DP_CHECK(director.migrationsSent() > 0);

    int headedOurWay = 0;
    const glm::vec2 playerCenter((static_cast<float>(p.x) + 0.5f) * kTile, (static_cast<float>(p.y) + 0.5f) * kTile);
    w.world.forEach<ZombieAI>(
        [&](Entity, ZombieAI& ai)
        {
            if (ai.migrating && glm::length(ai.targetWorld - playerCenter) <= ThreatDirectorSystem::kTargetScatterMaxTiles * kTile + kTile)
            {
                ++headedOurWay;
            }
        });
    DP_CHECK(headedOurWay > 0);
    // Day one keeps its starting population.
    DP_CHECK(spawns.empty());
}

DP_TEST(director_grows_population_far_from_the_player)
{
    SurvivalWorld w(100, 100);
    const glm::ivec2 p = w.playerTile();
    for (int i = 0; i < 20; ++i)
    {
        w.addZombie(5 + i, 5, ZombieState::Wander);
    }

    ThreatDirectorSystem director;
    director.reset(7u, ThreatDirectorSystem::Tuning{});
    std::vector<ThreatDirectorSystem::SpawnRequest> spawns;
    for (float t = 0.0f; t < 400.0f; t += 0.5f)
    {
        director.update(w.world, w.map, w.player, 0.5f, 12.0, 4, nullptr, spawns);
    }

    DP_CHECK(director.populationTarget() > 20);
    DP_CHECK(!spawns.empty());
    const glm::vec2 playerCenter((static_cast<float>(p.x) + 0.5f) * kTile, (static_cast<float>(p.y) + 0.5f) * kTile);
    for (const auto& spawn : spawns)
    {
        const glm::vec2 center = spawn.topLeftWorld + glm::vec2(kTile * 0.5f);
        DP_CHECK(glm::length(center - playerCenter) >= ThreatDirectorSystem::kSpawnMinDistanceTiles * kTile - 1.0f);
    }
}

// --------------------------------------------------------------- Zombie attacks

DP_TEST(zombie_attacks_are_survivable_lunges)
{
    SurvivalWorld w;
    ZombieAISystem ai;
    const glm::ivec2 p = w.playerTile();
    // Same tile: well inside attack range.
    const Entity z = w.addZombie(p.x, p.y, ZombieState::Attack);
    w.world.getComponent<Transform>(z).x += 10.0f;

    for (int i = 0; i < 600; ++i) // 10 s at 60 Hz
    {
        ai.update(w.world, w.map, w.noise, w.player, 1.0f / 60.0f, 12.0, 1, nullptr);
        w.world.getComponent<Transform>(z).x = w.world.getComponent<Transform>(w.player).x + 10.0f; // pin in place
        w.world.getComponent<Transform>(z).y = w.world.getComponent<Transform>(w.player).y;
    }
    const float hp = w.world.getComponent<Health>(w.player).current;
    // A lone zombie hurts but takes well over 10 seconds to kill a standing player.
    DP_CHECK(hp < 100.0f);
    DP_CHECK(hp > 55.0f);
    // Not every hit leaves a wound.
    DP_CHECK(w.world.getComponent<Wounds>(w.player).active.size() <= 4u);
}

DP_TEST(zombie_lunge_can_be_dodged)
{
    SurvivalWorld w;
    ZombieAISystem ai;
    const glm::ivec2 p = w.playerTile();
    const Entity z = w.addZombie(p.x, p.y, ZombieState::Attack);
    Transform& zt = w.world.getComponent<Transform>(z);
    Transform& pt = w.world.getComponent<Transform>(w.player);
    zt.x = pt.x + 10.0f;
    zt.y = pt.y;

    // Let the lunge start, then step well out of reach before it lands.
    ai.update(w.world, w.map, w.noise, w.player, 1.0f / 60.0f, 12.0, 1, nullptr);
    DP_CHECK(w.world.getComponent<ZombieAI>(z).attackWindup > 0.0f);
    pt.x -= 3.0f * kTile;
    pt.prevX = pt.x;
    for (int i = 0; i < 40; ++i)
    {
        ai.update(w.world, w.map, w.noise, w.player, 1.0f / 60.0f, 12.0, 1, nullptr);
        zt.x = pt.x + 3.0f * kTile + 10.0f; // the zombie stays where it lunged
        zt.y = pt.y;
    }
    DP_CHECK_NEAR(w.world.getComponent<Health>(w.player).current, 100.0f, 0.001f);
}

// ---------------------------------------------------------------------- Wounds

DP_TEST(wounds_only_dirty_bites_fester)
{
    SurvivalWorld w;
    WoundSystem wounds;
    auto& list = w.world.getComponent<Wounds>(w.player).active;
    Wound clean{};
    clean.type = WoundType::Bite;
    clean.contaminated = false;
    Wound dirty = clean;
    dirty.contaminated = true;
    Wound cut{};
    cut.type = WoundType::Cut;
    list = {clean, dirty, cut};

    for (int s = 0; s < 4 * 60; ++s) // 4 game hours
    {
        wounds.update(w.world, w.player, 1.0f, 12.0);
    }
    int infected = 0;
    for (const Wound& wound : list) infected += wound.infected ? 1 : 0;
    DP_CHECK_EQ(infected, 1);
    DP_CHECK(w.needs().illness > 0.0f);

    for (int s = 0; s < 20 * 60; ++s) // a day later
    {
        wounds.update(w.world, w.player, 1.0f, 12.0);
    }
    // The clean bite and the cut closed; the infection stays until treated.
    DP_CHECK_EQ(list.size(), 1u);
    DP_CHECK(list.front().infected);
}
