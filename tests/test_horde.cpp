// Hordes (overhaul phase 4): which nights, how many, and the way across.
#include "TestFramework.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "world/Horde.h"
#include "world/ZombieKinds.h"

DP_TEST(hordes_come_on_night_seven_then_every_four_to_six)
{
    const zombiekinds::Table table;
    const zombiekinds::ModeRules& stalker = table.mode(1);
    for (std::uint32_t seed = 1; seed < 40; ++seed)
    {
        const std::vector<int> nights = horde::hordeNights(60, seed, stalker);
        DP_CHECK(!nights.empty());
        if (nights.empty()) continue;
        DP_CHECK_EQ(nights.front(), 7);
        for (std::size_t i = 1; i < nights.size(); ++i)
        {
            const int gap = nights[i] - nights[i - 1];
            DP_CHECK(gap >= 4 && gap <= 6);
        }
        DP_CHECK(horde::isHordeNight(7, seed, stalker));
        DP_CHECK(!horde::isHordeNight(6, seed, stalker));
        DP_CHECK(horde::hordeNights(60, seed, stalker) == nights); // the seed decides
    }
}

DP_TEST(swarm_hordes_come_from_night_five_every_three_to_four)
{
    const zombiekinds::Table table;
    for (int modeIndex : {0, 5})
    {
        const std::vector<int> nights = horde::hordeNights(40, 1234u, table.mode(modeIndex));
        DP_CHECK(!nights.empty());
        if (nights.empty()) continue;
        DP_CHECK_EQ(nights.front(), 5);
        for (std::size_t i = 1; i < nights.size(); ++i)
        {
            const int gap = nights[i] - nights[i - 1];
            DP_CHECK(gap >= 3 && gap <= 4);
        }
    }
}

DP_TEST(horde_size_grows_by_two_a_day_from_fourteen_to_forty)
{
    DP_CHECK_EQ(horde::hordeSize(5), 14);
    DP_CHECK_EQ(horde::hordeSize(7), 14);
    DP_CHECK_EQ(horde::hordeSize(10), 20);
    DP_CHECK_EQ(horde::hordeSize(40), 40);
}

DP_TEST(a_horde_route_crosses_the_map_and_passes_near_home)
{
    horde::RouteInput in;
    in.width = 120;
    in.height = 100;
    in.home = glm::ivec2(60, 50);
    in.seed = 99u;
    in.walkable = [](int, int) { return true; };
    in.isRoad = [](int x, int y) { return x % 10 == 0 || y % 10 == 0; };
    const std::vector<glm::ivec2> route = horde::route(in);
    DP_CHECK(route.size() >= 3);
    if (route.size() < 3) return;
    const auto nearEdge = [&](const glm::ivec2& p)
    { return p.x <= 6 || p.y <= 6 || p.x >= in.width - 7 || p.y >= in.height - 7; };
    DP_CHECK(nearEdge(route.front()));
    DP_CHECK(nearEdge(route.back()));
    float nearest = 1.0e9f;
    for (const glm::ivec2& p : route)
    {
        nearest = std::min(nearest, glm::length(glm::vec2(p - in.home)));
    }
    DP_CHECK(nearest >= 5.0f && nearest <= 11.5f);
}
