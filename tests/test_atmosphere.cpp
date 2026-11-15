// Weather you can see and things that move: rain, lightning and wind follow
// the weather, trees lean with the wind, undergrowth shakes when pushed
// through, and litter is kicked along and stopped by walls.
#include "TestFramework.h"

#include <cmath>
#include <vector>

#include "components/Weather.h"
#include "map/TileIds.h"
#include "map/TileMap.h"
#include "rendering/Camera.h"
#include "rendering/SceneryView.h"
#include "systems/Atmosphere.h"
#include "systems/ParticleSystem.h"
#include "systems/WeatherSystem.h"

namespace
{
// 12x12 of grass with a wall down x = 8.
TileMap field()
{
    TileMap map;
    map.resize(12, 12, 32, 32);
    std::vector<int> ground(144, TileGid::kGrass);
    std::vector<int> collision(144, 0);
    for (int y = 0; y < 12; ++y)
    {
        ground[static_cast<std::size_t>(y * 12 + 8)] = TileGid::kWall;
        collision[static_cast<std::size_t>(y * 12 + 8)] = 1;
    }
    map.setGroundLayer(ground);
    map.setCollisionLayer(collision);
    return map;
}

Camera cameraOver(const glm::vec2& world)
{
    Camera camera;
    camera.setViewportSize(1280.0f, 720.0f);
    camera.setZoom(1.0f);
    camera.snapTo(world);
    return camera;
}
} // namespace

DP_TEST(rain_comes_on_gradually_and_splashes_on_open_ground)
{
    const TileMap map = field();
    const Camera camera = cameraOver(glm::vec2(96.0f, 192.0f));
    ParticleSystem particles;
    Atmosphere atmosphere;
    WeatherState weather{};
    WeatherSystem system;
    system.hold(weather, WeatherCondition::HeavyRain, 1000.0f);
    system.update(weather, 0.016f, 12.0, 1);
    DP_CHECK(weather.rainIntensity > 0.5f);

    atmosphere.update(0.1f, weather, camera, map, particles);
    const float early = atmosphere.rain();
    DP_CHECK(early > 0.0f && early < weather.rainIntensity);
    for (int i = 0; i < 100; ++i)
    {
        atmosphere.update(0.1f, weather, camera, map, particles);
    }
    DP_CHECK_NEAR(atmosphere.rain(), weather.rainIntensity, 1e-4f);
    DP_CHECK(atmosphere.skyDim() > 0.1f);
    DP_CHECK(particles.activeCount() > 0); // splashes

    // And it clears the same way.
    system.hold(weather, WeatherCondition::Clear, 1000.0f);
    system.update(weather, 0.016f, 12.0, 1);
    for (int i = 0; i < 100; ++i)
    {
        atmosphere.update(0.1f, weather, camera, map, particles);
    }
    DP_CHECK_NEAR(atmosphere.rain(), 0.0f, 1e-4f);
    DP_CHECK_NEAR(atmosphere.skyDim(), 0.0f, 1e-4f);
}

DP_TEST(lightning_only_strikes_in_a_storm_and_dies_away)
{
    const TileMap map = field();
    const Camera camera = cameraOver(glm::vec2(96.0f, 192.0f));
    ParticleSystem particles;
    WeatherState weather{};
    WeatherSystem system;

    Atmosphere calm;
    system.hold(weather, WeatherCondition::Rain, 1000.0f);
    system.update(weather, 0.016f, 12.0, 1);
    for (int i = 0; i < 600; ++i)
    {
        calm.update(0.1f, weather, camera, map, particles);
        DP_CHECK(!calm.takeThunder());
    }

    Atmosphere storm;
    system.hold(weather, WeatherCondition::Thunderstorm, 1000.0f);
    system.update(weather, 0.016f, 12.0, 1);
    int strikes = 0;
    float brightest = 0.0f;
    for (int i = 0; i < 3600; ++i)
    {
        storm.update(1.0f / 60.0f, weather, camera, map, particles);
        brightest = std::max(brightest, storm.flash());
        if (storm.takeThunder())
        {
            ++strikes;
        }
    }
    DP_CHECK(strikes >= 2 && strikes <= 12); // a minute of storm
    DP_CHECK(brightest > 0.7f);
    DP_CHECK(storm.gust() > calm.gust());

    // Dimmer weather the worse it gets.
    DP_CHECK(Atmosphere::skyDimFor(WeatherCondition::Clear) < Atmosphere::skyDimFor(WeatherCondition::Rain));
    DP_CHECK(Atmosphere::skyDimFor(WeatherCondition::Rain) < Atmosphere::skyDimFor(WeatherCondition::Thunderstorm));
}

DP_TEST(trees_lean_from_the_trunk_up_and_further_in_a_gale)
{
    // The foot of a tree never moves; the crown does.
    for (float clock = 0.0f; clock < 10.0f; clock += 0.37f)
    {
        DP_CHECK_EQ(SceneryView::lean(clock, 1.0f, 1.0f, 0.2f, 0.3f), 0.0f);
    }
    float still = 0.0f;
    float gale = 0.0f;
    for (float clock = 0.0f; clock < 20.0f; clock += 0.1f)
    {
        still = std::max(still, std::abs(SceneryView::lean(clock, 0.5f, 0.1f, 1.0f, 0.3f)));
        gale = std::max(gale, std::abs(SceneryView::lean(clock, 0.5f, 1.0f, 1.0f, 0.3f)));
    }
    DP_CHECK(still > 0.3f && still < 2.0f);
    DP_CHECK(gale > 3.0f && gale < 8.0f);
}

DP_TEST(a_kicked_can_rolls_to_a_stop_and_bounces_off_a_wall)
{
    const TileMap map = field();
    SceneryView view;
    // A can on the ground at tile (6, 5), two tiles short of the wall.
    view.build({SceneryPiece{glm::vec2(208.0f, 176.0f), "scn_can", true, false, false}});
    DP_CHECK_EQ(view.looseCount(), std::size_t{1});

    // Someone standing on it does nothing; someone running through kicks it.
    std::vector<SceneryMover> movers{SceneryMover{glm::vec2(206.0f, 176.0f), glm::vec2(0.0f)}};
    view.update(1.0f / 60.0f, glm::vec2(0.0f), 0.0f, movers, map);
    DP_CHECK(view.loosePosition(0) == glm::vec2(208.0f, 176.0f));

    movers[0].velocity = glm::vec2(140.0f, 0.0f);
    view.update(1.0f / 60.0f, glm::vec2(0.0f), 0.0f, movers, map);
    movers.clear();
    float furthest = 0.0f;
    for (int i = 0; i < 600; ++i)
    {
        view.update(1.0f / 60.0f, glm::vec2(0.0f), 0.0f, movers, map);
        furthest = std::max(furthest, view.loosePosition(0).x);
        DP_CHECK(view.loosePosition(0).x < 256.0f); // never into the wall at x = 8
    }
    DP_CHECK(furthest > 240.0f); // it reached the wall
    const glm::vec2 rest = view.loosePosition(0);
    DP_CHECK(rest.x < furthest); // and came back off it
    view.update(1.0f, glm::vec2(0.0f), 0.0f, movers, map);
    DP_CHECK(view.loosePosition(0) == rest); // at rest
}

DP_TEST(undergrowth_shakes_once_when_someone_pushes_through)
{
    const TileMap map = field();
    SceneryView view;
    view.build({SceneryPiece{glm::vec2(100.0f, 100.0f), "scn_bush_a", false, false, false},
                SceneryPiece{glm::vec2(300.0f, 300.0f), "scn_bush_b", false, false, false}});
    std::vector<glm::vec2> rustled;
    std::vector<SceneryMover> movers{SceneryMover{glm::vec2(104.0f, 98.0f), glm::vec2(80.0f, 0.0f)}};
    view.update(1.0f / 60.0f, glm::vec2(0.0f), 0.0f, movers, map, &rustled);
    DP_CHECK_EQ(rustled.size(), std::size_t{1});
    DP_CHECK(rustled[0] == glm::vec2(100.0f, 100.0f));

    // Still in it: no second report until it has settled.
    rustled.clear();
    view.update(1.0f / 60.0f, glm::vec2(0.0f), 0.0f, movers, map, &rustled);
    DP_CHECK(rustled.empty());
    movers.clear();
    view.update(1.0f, glm::vec2(0.0f), 0.0f, movers, map, &rustled);
    movers.push_back(SceneryMover{glm::vec2(104.0f, 98.0f), glm::vec2(80.0f, 0.0f)});
    view.update(1.0f / 60.0f, glm::vec2(0.0f), 0.0f, movers, map, &rustled);
    DP_CHECK_EQ(rustled.size(), std::size_t{1});
}
