// Windows: placed on the outside walls of houses and shops by one rule (so
// drawing and play agree), glass the dead see through and break, boards
// that take the blows first, curtains, and empty frames to climb through.
#include "TestFramework.h"

#include <algorithm>

#include "ai/Pathfinder.h"
#include "map/MapGenerator.h"
#include "map/TileMap.h"
#include "systems/ZombieAISystem.h"
#include "world/Windows.h"

namespace
{
struct Town
{
    MapGenerator generator;
    LayoutData layout;
    TileMap map;
    std::vector<windows::Spot> spots;

    explicit Town(std::uint32_t seed)
    {
        layout = generator.generateLayout(seed);
        generator.applyToTileMap(map);
        spots = windows::place(map, layout);
    }
};

glm::vec2 centre(const glm::ivec2& tile)
{
    return (glm::vec2(tile) + glm::vec2(0.5f)) * 32.0f;
}
} // namespace

DP_TEST(windows_are_placed_on_outside_walls_by_seed)
{
    Town a(1234u);
    Town b(1234u);
    DP_CHECK(a.spots.size() > 20);
    DP_CHECK_EQ(a.spots.size(), b.spots.size());
    for (std::size_t i = 0; i < a.spots.size() && i < b.spots.size(); ++i)
    {
        DP_CHECK(a.spots[i].tile == b.spots[i].tile);
    }
    for (const windows::Spot& spot : a.spots)
    {
        // A wall of a building, with that building's floor inside and the street outside.
        DP_CHECK(a.map.isSolid(spot.tile.x, spot.tile.y));
        DP_CHECK(spot.building > 0);
        DP_CHECK_EQ(a.map.getBuildingId(spot.tile.x, spot.tile.y), spot.building);
        DP_CHECK(!a.map.isSolid(spot.inside.x, spot.inside.y));
        DP_CHECK(!a.map.isSolid(spot.outside.x, spot.outside.y));
        DP_CHECK_EQ(a.map.getBuildingId(spot.inside.x, spot.inside.y), spot.building);
        DP_CHECK(a.map.getBuildingId(spot.outside.x, spot.outside.y) <= 0);
        // Inside and outside face each other across the wall.
        DP_CHECK(spot.inside + spot.outside == spot.tile * 2);
    }
}

DP_TEST(windows_never_sit_beside_a_doorway)
{
    Town town(77u);
    for (const windows::Spot& spot : town.spots)
    {
        for (const auto& door : town.layout.doors)
        {
            const int d = std::abs(door.x - spot.tile.x) + std::abs(door.y - spot.tile.y);
            DP_CHECK(d > 1);
        }
    }
}

DP_TEST(windows_glass_breaks_and_boards_take_blows_first)
{
    Town town(1234u);
    Windows windows;
    windows.reset(town.map, town.spots);
    DP_CHECK(!town.spots.empty());
    const glm::ivec2 tile = town.spots.front().tile;
    DP_CHECK(town.map.windowState(tile.x, tile.y) == WindowState::Intact);
    DP_CHECK(town.map.isSolid(tile.x, tile.y));

    // Glass: 60 health.
    DP_CHECK(windows.batter(town.map, tile, 30.0f) == Windows::Blow::Bang);
    DP_CHECK(windows.batter(town.map, tile, 29.0f) == Windows::Blow::Thud);
    DP_CHECK(windows.batter(town.map, tile, 2.0f) == Windows::Blow::Shatters);
    DP_CHECK(town.map.windowState(tile.x, tile.y) == WindowState::Broken);
    DP_CHECK(town.map.windowOpen(tile.x, tile.y));
    // An empty frame is climbed, not battered; it stays solid to walk into.
    DP_CHECK(windows.batter(town.map, tile, 10.0f) == Windows::Blow::None);
    DP_CHECK(town.map.isSolid(tile.x, tile.y));

    // Two boards at most, 300 each; they go before anything else.
    DP_CHECK(windows.addBoard(town.map, tile));
    DP_CHECK(windows.addBoard(town.map, tile));
    DP_CHECK(!windows.addBoard(town.map, tile));
    DP_CHECK_EQ(windows.boards(tile), 2);
    DP_CHECK(!town.map.windowOpen(tile.x, tile.y));
    Windows::Blow blow = Windows::Blow::None;
    float dealt = 0.0f;
    while (blow != Windows::Blow::Splinters && dealt < 1000.0f)
    {
        windows.tick(1.0f);
        blow = windows.batter(town.map, tile, 7.0f);
        dealt += 7.0f;
    }
    DP_CHECK(blow == Windows::Blow::Splinters);
    DP_CHECK(dealt >= 300.0f - 0.01f);
    DP_CHECK_EQ(windows.boards(tile), 1);
    DP_CHECK(windows.removeBoard(town.map, tile));
    DP_CHECK_EQ(windows.boards(tile), 0);
    DP_CHECK(town.map.windowOpen(tile.x, tile.y));
}

DP_TEST(windows_let_sight_through_until_boarded_or_curtained)
{
    Town town(1234u);
    Windows windows;
    windows.reset(town.map, town.spots);
    const windows::Spot spot = town.spots.front();
    const glm::ivec2 tile = spot.tile;
    DP_CHECK(town.map.blocksSight(tile.x, tile.y)); // weather and walls still treat it as wall
    DP_CHECK(!town.map.blocksView(tile.x, tile.y)); // but eyes see through glass
    DP_CHECK(ZombieAISystem::hasLineOfSight(town.map, centre(spot.outside), centre(spot.inside)));

    town.map.setWindowCovered(tile.x, tile.y, true);
    DP_CHECK(town.map.blocksView(tile.x, tile.y));
    DP_CHECK(!ZombieAISystem::hasLineOfSight(town.map, centre(spot.outside), centre(spot.inside)));
    town.map.setWindowCovered(tile.x, tile.y, false);

    windows.addBoard(town.map, tile);
    DP_CHECK(town.map.blocksView(tile.x, tile.y));
    DP_CHECK(!ZombieAISystem::hasLineOfSight(town.map, centre(spot.outside), centre(spot.inside)));
}

DP_TEST(windows_sight_through_glass_reaches_six_tiles)
{
    Town town(1234u);
    Windows windows;
    windows.reset(town.map, town.spots);
    // A window with a clear line out into the street.
    for (const windows::Spot& spot : town.spots)
    {
        const glm::ivec2 out = spot.outside - spot.tile;
        glm::ivec2 far = spot.tile;
        int clear = 0;
        while (clear < 9)
        {
            const glm::ivec2 next = far + out;
            if (!town.map.inBounds(next.x, next.y) || town.map.blocksSight(next.x, next.y)) break;
            far = next;
            ++clear;
        }
        if (clear < 9) continue;
        DP_CHECK(ZombieAISystem::hasLineOfSight(town.map, centre(spot.inside), centre(spot.tile + out * 4)));
        DP_CHECK(!ZombieAISystem::hasLineOfSight(town.map, centre(spot.inside), centre(spot.tile + out * 9)));
        return;
    }
    DP_CHECK(false); // no window with a long view: the town changed
}

DP_TEST(windows_are_a_way_in_for_the_dead_but_not_the_living)
{
    Town town(1234u);
    Windows windows;
    windows.reset(town.map, town.spots);
    const glm::ivec2 tile = town.spots.front().tile;
    DP_CHECK_NEAR(Pathfinder::stepCost(town.map, tile, Pathfinder::Mover::Dead), Pathfinder::kWindowCost, 0.001);
    DP_CHECK(Pathfinder::stepCost(town.map, tile, Pathfinder::Mover::Living) < 0.0f);
    windows.breakGlass(town.map, tile);
    DP_CHECK_NEAR(Pathfinder::stepCost(town.map, tile, Pathfinder::Mover::Dead), Pathfinder::kOpenWindowCost, 0.001);
    windows.addBoard(town.map, tile);
    DP_CHECK_NEAR(Pathfinder::stepCost(town.map, tile, Pathfinder::Mover::Dead),
                  Pathfinder::kWindowCost + Pathfinder::kBoardCost, 0.001);
    windows.addBoard(town.map, tile);
    DP_CHECK_NEAR(Pathfinder::stepCost(town.map, tile, Pathfinder::Mover::Dead),
                  Pathfinder::kWindowCost + 2.0f * Pathfinder::kBoardCost, 0.001);
}

DP_TEST(windows_restore_what_was_saved)
{
    Town town(1234u);
    Windows windows;
    windows.reset(town.map, town.spots);
    const glm::ivec2 a = town.spots[0].tile;
    const glm::ivec2 b = town.spots[1].tile;
    windows.restore(town.map, a, true, 450.0f, false);
    windows.restore(town.map, b, false, 0.0f, true);
    DP_CHECK(town.map.windowState(a.x, a.y) == WindowState::Broken);
    DP_CHECK_EQ(windows.boards(a), 2);
    DP_CHECK_NEAR(windows.find(a)->boardHp, 450.0f, 0.01);
    DP_CHECK(town.map.windowCovered(b.x, b.y));
    // A fresh run: whole again until memory says otherwise.
    windows.reset(town.map, town.spots);
    DP_CHECK(town.map.windowState(a.x, a.y) == WindowState::Intact);
    DP_CHECK_EQ(windows.boards(a), 0);
    DP_CHECK(!town.map.windowCovered(b.x, b.y));
}
