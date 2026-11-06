// Walls, doorways, roofs and cut-away (StructureView), and the map generator's
// guarantees that make them look right: closed buildings, doors that lead
// inside, one floor per room, sidewalks along roads.
#include "TestFramework.h"

#include <cstdlib>
#include <vector>

#include "map/MapGenerator.h"
#include "map/TileIds.h"
#include "map/TileMap.h"
#include "rendering/Camera.h"
#include "rendering/StructureView.h"

namespace
{
// 16x14 map: one building at x 2..7, y 2..6 with a door at (4,6), and a
// free-standing wall line (no building) at y = 10, x 6..14.
struct TinyTown
{
    TileMap map;
    LayoutData layout;

    TinyTown()
    {
        const int w = 16;
        const int h = 14;
        map.resize(w, h, 32, 32);
        std::vector<int> ground(static_cast<std::size_t>(w * h), TileGid::kGrass);
        std::vector<int> collision(static_cast<std::size_t>(w * h), 0);
        std::vector<int> building(static_cast<std::size_t>(w * h), 0);
        auto at = [&](int x, int y) { return static_cast<std::size_t>(y * w + x); };

        for (int y = 2; y <= 6; ++y)
        {
            for (int x = 2; x <= 7; ++x)
            {
                const bool edge = x == 2 || x == 7 || y == 2 || y == 6;
                ground[at(x, y)] = edge ? TileGid::kWall : TileGid::kWood;
                collision[at(x, y)] = edge ? 1 : 0;
                building[at(x, y)] = 1;
            }
        }
        ground[at(4, 6)] = TileGid::kFloor;
        collision[at(4, 6)] = 0;

        for (int x = 6; x <= 14; ++x)
        {
            ground[at(x, 10)] = TileGid::kWall;
            collision[at(x, 10)] = 1;
        }

        map.setGroundLayer(ground);
        map.setCollisionLayer(collision);
        map.setBuildingLayer(building);

        layout.width = w;
        layout.height = h;
        layout.buildings.push_back({2, 2, 6, 5});
        layout.doors.push_back({4, 6});
    }
};

int countSprites(const std::vector<StaticSprite>& sprites, float height)
{
    int n = 0;
    for (const auto& s : sprites)
    {
        if (std::abs(s.size.y - height) < 0.01f)
        {
            ++n;
        }
    }
    return n;
}
} // namespace

DP_TEST(structures_classify_walls_doorways_and_runs)
{
    TinyTown town;
    StructureView view;
    view.build(town.map, town.layout);

    DP_CHECK(view.tile(2, 2).kind == StructureKind::Wall);
    DP_CHECK(view.tile(2, 2).run == StructureRun::Corner);
    DP_CHECK(view.tile(5, 2).run == StructureRun::AlongX);
    DP_CHECK(view.tile(2, 4).run == StructureRun::AlongY);
    DP_CHECK(view.tile(4, 6).kind == StructureKind::Doorway);
    DP_CHECK(view.tile(4, 6).run == StructureRun::AlongX);
    DP_CHECK(view.tile(4, 6).perimeter);
    DP_CHECK(view.tile(4, 4).kind == StructureKind::None);
    DP_CHECK_EQ(view.tile(4, 4).building, 1);
    DP_CHECK(view.tile(8, 10).kind == StructureKind::Wall);
    DP_CHECK(view.tile(8, 10).perimeter); // walls outside buildings always show
    DP_CHECK(view.coversGround(2, 2));
    DP_CHECK(!view.coversGround(4, 6));
}

DP_TEST(structures_roof_hides_interior_until_player_enters)
{
    TinyTown town;
    StructureView view;
    view.build(town.map, town.layout);

    view.snapRoofs({12, 4}); // outside, away from the door
    DP_CHECK_NEAR(view.roofAlpha(1), 1.0f, 1e-6f);
    DP_CHECK(view.isHiddenAt(4, 4));
    DP_CHECK(!view.isHiddenAt(4, 6)); // the doorway itself is seen from the street
    DP_CHECK(!view.isHiddenAt(12, 4));

    view.snapRoofs({4, 4}); // inside
    DP_CHECK_NEAR(view.roofAlpha(1), 0.0f, 1e-6f);
    DP_CHECK(!view.isHiddenAt(5, 3));

    DP_CHECK_EQ(view.openBuilding({4, 7}), 1); // standing at the door looks in
    DP_CHECK_EQ(view.openBuilding({5, 7}), 0); // next to a wall does not
}

DP_TEST(structures_roof_fades_and_interior_stays_visible_while_it_does)
{
    TinyTown town;
    StructureView view;
    view.build(town.map, town.layout);
    view.snapRoofs({4, 4});

    view.update(0.1f, {12, 4}); // walked out
    DP_CHECK(view.roofAlpha(1) > 0.0f && view.roofAlpha(1) < 1.0f);
    DP_CHECK(!view.isHiddenAt(4, 4));
    for (int i = 0; i < 20; ++i)
    {
        view.update(0.1f, {12, 4});
    }
    DP_CHECK(view.isHiddenAt(4, 4));
}

DP_TEST(structures_cut_away_front_walls)
{
    TinyTown town;
    StructureView view;
    view.build(town.map, town.layout);

    // Inside: the walls of your building in front of you drop, the back ones stay.
    DP_CHECK(view.isCutAway({5, 6}, {4, 4}));
    DP_CHECK(view.isCutAway({7, 4}, {4, 4}));
    DP_CHECK(!view.isCutAway({4, 2}, {4, 4}));
    DP_CHECK(!view.isCutAway({2, 4}, {4, 4}));

    // Outside: only walls in a narrow band right in front of you drop.
    DP_CHECK(view.isCutAway({11, 10}, {10, 8}));
    DP_CHECK(!view.isCutAway({14, 10}, {10, 8})); // too far in front
    DP_CHECK(!view.isCutAway({9, 10}, {12, 6}));  // off to the side
    DP_CHECK(!view.isCutAway({6, 10}, {10, 8}));  // behind
}

DP_TEST(structures_collect_emits_walls_and_roofs)
{
    TinyTown town;
    StructureView view;
    view.build(town.map, town.layout);
    Camera camera;
    camera.setViewportSize(4000.0f, 4000.0f);
    camera.setZoom(1.0f);
    camera.snapTo(glm::vec2(8.0f * 32.0f, 7.0f * 32.0f));
    const StructureSprites sprites{};

    std::vector<StaticSprite> out;
    view.snapRoofs({12, 4});
    view.collect(camera, {12, 4}, sprites, out);
    const float full = 32.0f + StructureView::kWallHeight;
    DP_CHECK_EQ(countSprites(out, 32.0f), 12);        // roof over the 4x3 interior
    DP_CHECK_EQ(countSprites(out, full), 18 + 9);     // 17 walls + doorway, 9 free-standing

    out.clear();
    view.snapRoofs({4, 4});
    view.collect(camera, {4, 4}, sprites, out);
    DP_CHECK_EQ(countSprites(out, 32.0f), 0); // roof open
    // Front walls (south and east, minus the shared corner and the doorway) are stubs.
    DP_CHECK(countSprites(out, 32.0f + StructureView::kWallLowHeight) >= 8);
}

DP_TEST(mapgen_buildings_are_closed_and_doors_lead_inside)
{
    for (std::uint32_t seed : {1234u, 42u, 777u})
    {
        MapGenerator gen;
        const LayoutData layout = gen.generateLayout(seed);
        TileMap map;
        gen.applyToTileMap(map);

        auto isDoor = [&](int x, int y)
        {
            for (const auto& d : layout.doors)
            {
                if (d.x == x && d.y == y) return true;
            }
            return false;
        };

        constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int y = 1; y < map.height() - 1; ++y)
        {
            for (int x = 1; x < map.width() - 1; ++x)
            {
                if (map.getBuildingId(x, y) == 0 || map.isSolid(x, y) || isDoor(x, y)) continue;
                // An interior floor tile never opens straight onto the outside.
                for (const auto& d : kDirs)
                {
                    const int nx = x + d[0];
                    const int ny = y + d[1];
                    const bool outside = map.getBuildingId(nx, ny) == 0 && !map.isSolid(nx, ny);
                    DP_CHECK(!outside);
                }
            }
        }

        for (const auto& d : layout.doors)
        {
            bool leadsToFloor = false;
            for (const auto& dir : kDirs)
            {
                const int nx = d.x + dir[0];
                const int ny = d.y + dir[1];
                if (map.getBuildingId(nx, ny) != 0 && !map.isSolid(nx, ny) && !isDoor(nx, ny))
                {
                    leadsToFloor = true;
                }
            }
            DP_CHECK(leadsToFloor);
        }
    }
}

DP_TEST(mapgen_rooms_have_one_floor_and_roads_have_sidewalks)
{
    MapGenerator gen;
    const LayoutData layout = gen.generateLayout(1234u);
    TileMap map;
    gen.applyToTileMap(map);

    auto isDoor = [&](int x, int y)
    {
        for (const auto& d : layout.doors)
        {
            if (d.x == x && d.y == y) return true;
        }
        return false;
    };

    // Neighbouring floor tiles in the same room share a floor type.
    for (int y = 1; y < map.height() - 1; ++y)
    {
        for (int x = 1; x < map.width() - 1; ++x)
        {
            const int b = map.getBuildingId(x, y);
            if (b == 0 || map.isSolid(x, y) || isDoor(x, y)) continue;
            for (const auto& d : {glm::ivec2(1, 0), glm::ivec2(0, 1)})
            {
                const int nx = x + d.x;
                const int ny = y + d.y;
                if (map.getBuildingId(nx, ny) != b || map.isSolid(nx, ny) || isDoor(nx, ny)) continue;
                DP_CHECK_EQ(map.groundGid(x, y), map.groundGid(nx, ny));
            }
        }
    }

    int asphalt = 0;
    int sidewalk = 0;
    for (int y = 1; y < map.height() - 1; ++y)
    {
        for (int x = 1; x < map.width() - 1; ++x)
        {
            const int gid = map.groundGid(x, y);
            if (gid == TileGid::kSidewalk) ++sidewalk;
            if (gid != TileGid::kAsphalt) continue;
            ++asphalt;
            for (const auto& d : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
            {
                DP_CHECK(!TileGid::isGrass(map.groundGid(x + d.x, y + d.y)));
            }
        }
    }
    DP_CHECK(asphalt > 0);
    DP_CHECK(sidewalk > 0);
}
