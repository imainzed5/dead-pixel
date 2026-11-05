#include "TestFramework.h"

#include "map/MapGenerator.h"

DP_TEST(mapgen_same_seed_gives_identical_layout)
{
    MapGenerator a;
    MapGenerator b;
    const LayoutData la = a.generateLayout(1234u);
    const LayoutData lb = b.generateLayout(1234u);

    DP_CHECK(la.width > 0 && la.height > 0);
    DP_CHECK_EQ(la.width, lb.width);
    DP_CHECK_EQ(la.height, lb.height);
    DP_CHECK(la.ground == lb.ground);
    DP_CHECK(la.collision == lb.collision);
    DP_CHECK(la.surface == lb.surface);
    DP_CHECK(la.district == lb.district);
    DP_CHECK_EQ(la.buildings.size(), lb.buildings.size());
}

DP_TEST(mapgen_different_seeds_differ)
{
    MapGenerator a;
    MapGenerator b;
    const LayoutData la = a.generateLayout(1u);
    const LayoutData lb = b.generateLayout(2u);
    DP_CHECK(la.ground != lb.ground || la.collision != lb.collision);
}

DP_TEST(mapgen_has_buildings_and_districts)
{
    MapGenerator gen;
    const LayoutData layout = gen.generateLayout(77u);
    DP_CHECK(!layout.buildings.empty());
    DP_CHECK(!layout.doors.empty());

    bool hasDistrict = false;
    for (int id : layout.district)
    {
        if (id != 0)
        {
            hasDistrict = true;
            break;
        }
    }
    DP_CHECK(hasDistrict);
}

DP_TEST(mapgen_building_layer_marks_interiors_only)
{
    MapGenerator gen;
    const LayoutData layout = gen.generateLayout(1234u);
    TileMap map;
    gen.applyToTileMap(map);

    int indoorTiles = 0;
    for (int y = 0; y < map.height(); ++y)
    {
        for (int x = 0; x < map.width(); ++x)
        {
            if (map.isIndoors(x, y)) ++indoorTiles;
        }
    }
    DP_CHECK(indoorTiles > 0);

    // Every building's center is indoors and belongs to that building.
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        const auto& b = layout.buildings[i];
        const int cx = b.x + b.w / 2;
        const int cy = b.y + b.h / 2;
        DP_CHECK_EQ(map.getBuildingId(cx, cy), static_cast<int>(i) + 1);
    }

    // Map edges and out-of-bounds are never indoors.
    for (int x = 0; x < map.width(); ++x)
    {
        DP_CHECK(!map.isIndoors(x, 0));
        DP_CHECK(!map.isIndoors(x, map.height() - 1));
    }
    DP_CHECK(!map.isIndoors(-1, -1));
    DP_CHECK(!map.isIndoorsAtWorld(-50.0f, -50.0f));
}
