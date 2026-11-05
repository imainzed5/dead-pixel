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
