// The town ages between lives (phase 10): built things a stage worse, boards
// a quarter weaker, unkept crates picked over, gardens, bones, infection, the
// growing dead, weathered graves; and the same seed ages the same way.
#include "TestFramework.h"

#include <cmath>
#include <vector>

#include "legacy/WorldAging.h"

namespace
{
aging::Built crate(float hp, int building)
{
    aging::Built b;
    b.record.type = 4;
    b.record.hpRatio = hp;
    b.building = building;
    b.wears = true;
    b.crate = true;
    return b;
}
} // namespace

DP_TEST(aging_a_crate_goes_intact_damaged_breaking_gone)
{
    aging::WorldState world;
    world.built.push_back(crate(1.0f, 0));
    aging::ageWorld(world, 3, 1u);
    DP_CHECK_EQ(world.built.size(), std::size_t{1});
    DP_CHECK(aging::stageOf(world.built[0].record.hpRatio) == aging::Stage::Damaged);
    aging::ageWorld(world, 3, 2u);
    DP_CHECK(aging::stageOf(world.built[0].record.hpRatio) == aging::Stage::Breaking);
    DP_CHECK_EQ(world.built[0].record.condition, 2);
    const aging::Report last = aging::ageWorld(world, 3, 3u);
    DP_CHECK(world.built.empty());
    DP_CHECK_EQ(last.lost, 1);
    // A crate already worn in its life starts its next a stage below where it was.
    aging::WorldState worn;
    worn.built.push_back(crate(0.5f, 0));
    aging::ageWorld(worn, 1, 4u);
    DP_CHECK(aging::stageOf(worn.built[0].record.hpRatio) == aging::Stage::Breaking);
}

DP_TEST(aging_boards_lose_a_quarter_a_life_and_go_at_nothing)
{
    DP_CHECK_NEAR(aging::wornBoards(450.0f, 450.0f), 337.5f, 0.01f);
    DP_CHECK_NEAR(aging::wornBoards(900.0f, 450.0f), 675.0f, 0.01f); // two boards, a quarter each
    DP_CHECK_NEAR(aging::wornBoards(112.5f, 450.0f), 0.0f, 0.01f);
    aging::WorldState world;
    world.doors.push_back(BoardedDoorRecord{3, 4, 450.0f, 1.0f});
    WindowRecord window;
    window.boardHp = 300.0f;
    world.windows.push_back(window);
    for (int life = 0; life < 3; ++life) aging::ageWorld(world, 2, static_cast<std::uint32_t>(life));
    DP_CHECK_EQ(world.doors.size(), std::size_t{1});
    DP_CHECK_NEAR(world.doors[0].boardHp, 112.5f, 0.01f);
    const aging::Report report = aging::ageWorld(world, 2, 9u);
    DP_CHECK(world.doors.empty());
    DP_CHECK(world.windows.empty()); // nothing left to remember of it
    DP_CHECK_EQ(report.boardsGone, 2);
}

DP_TEST(aging_unkept_crates_lose_food_and_kept_ones_do_not)
{
    aging::WorldState world;
    world.foodItems = {1, 2};
    aging::Built unkept = crate(1.0f, 5);
    unkept.record.items = {{1, 2}, {2, 1}, {1, 1}, {2, 3}, {1, 1}, {9, 1}}; // five food stacks, one nail
    aging::Built kept = unkept;
    kept.building = 7;
    world.built = {unkept, kept};
    world.keptBuildings = {7};
    // A seed that does not strip the crate.
    std::uint32_t seed = 0;
    for (; seed < 100; ++seed)
    {
        aging::WorldState trial = world;
        if (aging::ageWorld(trial, 4, seed).stripped == 0) break;
    }
    const aging::Report report = aging::ageWorld(world, 4, seed);
    DP_CHECK_EQ(report.foodLost, 1); // a fifth of five
    DP_CHECK_EQ(world.built[0].record.items.size(), std::size_t{5});
    DP_CHECK_EQ(world.built[1].record.items.size(), std::size_t{6});
}

DP_TEST(aging_one_unkept_crate_in_ten_is_stripped_into_a_survivors_stock)
{
    int stripped = 0;
    for (std::uint32_t seed = 0; seed < 1000; ++seed)
    {
        aging::WorldState world;
        aging::Built b = crate(1.0f, 3);
        b.record.items = {{9, 4}};
        world.built.push_back(b);
        const aging::Report report = aging::ageWorld(world, 2, seed);
        if (report.stripped > 0)
        {
            ++stripped;
            DP_CHECK(world.built[0].record.items.empty());
            DP_CHECK_EQ(report.strippedItems.size(), std::size_t{1});
            DP_CHECK_EQ(report.strippedItems[0].count, 4);
        }
    }
    DP_CHECK(stripped > 60 && stripped < 140);
}

DP_TEST(aging_gardens_go_to_seed_when_watered_late_and_die_when_not)
{
    aging::WorldState world;
    aging::Built watered;
    watered.garden = true;
    watered.record.type = 10;
    watered.record.growHours = 50.0f;
    watered.record.sinceWaterHours = 20.0f;
    aging::Built dry = watered;
    dry.record.sinceWaterHours = 90.0f;
    world.built = {watered, dry};
    aging::ageWorld(world, 5, 1u);
    DP_CHECK_EQ(world.built.size(), std::size_t{1});
    DP_CHECK_NEAR(world.built[0].record.growHours, 0.0f, 0.001f);
}

DP_TEST(aging_infection_creeps_up_from_where_the_life_began)
{
    // +0.5 a day, -10 a life, never below the start.
    DP_CHECK_NEAR(aging::agedInfection(30.0f, 20.0f, 10), 25.0f, 0.001f);
    DP_CHECK_NEAR(aging::agedInfection(22.0f, 20.0f, 2), 20.0f, 0.001f);
    DP_CHECK_NEAR(aging::agedInfection(60.0f, 10.0f, 40), 70.0f, 0.001f);
    aging::WorldState world;
    world.infection = {{1, 30.0f}, {2, 5.0f}};
    world.lifeStart = {{1, 20.0f}, {2, 8.0f}};
    aging::ageWorld(world, 10, 1u);
    DP_CHECK_NEAR(world.infection[0].second, 25.0f, 0.001f);
    DP_CHECK_NEAR(world.infection[1].second, 8.0f, 0.001f);
}

DP_TEST(aging_bodies_become_bones_and_only_forty_are_kept)
{
    aging::WorldState world;
    for (int i = 0; i < 30; ++i) world.bones.emplace_back(i, 0);
    for (int i = 0; i < 15; ++i) world.bodies.emplace_back(i, 1);
    aging::ageWorld(world, 1, 1u);
    DP_CHECK_EQ(world.bones.size(), aging::kMaxBones);
    DP_CHECK(world.bones.front() == std::make_pair(5, 0)); // the oldest went
    DP_CHECK(world.bodies.empty());
}

DP_TEST(aging_the_dead_grow_four_percent_a_life_to_sixty)
{
    DP_CHECK_NEAR(aging::populationScale(0), 1.0f, 0.0001f);
    DP_CHECK_NEAR(aging::populationScale(5), 1.2f, 0.0001f);
    DP_CHECK_NEAR(aging::populationScale(40), 1.6f, 0.0001f);
}

DP_TEST(aging_graves_weather_after_five_lives_unless_tended)
{
    DP_CHECK(!aging::weathered(1, 0, 6));
    DP_CHECK(aging::weathered(1, 0, 7));
    DP_CHECK(!aging::weathered(1, 5, 7)); // tended in life 5
}

DP_TEST(aging_the_same_seed_ages_the_same_way)
{
    aging::WorldState world;
    world.foodItems = {1};
    for (int i = 0; i < 12; ++i)
    {
        aging::Built b = crate(1.0f, i + 1);
        b.record.items = {{1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7}};
        world.built.push_back(b);
    }
    aging::WorldState again = world;
    const aging::Report a = aging::ageWorld(world, 6, 4242u);
    const aging::Report b = aging::ageWorld(again, 6, 4242u);
    DP_CHECK_EQ(a.foodLost, b.foodLost);
    DP_CHECK_EQ(a.stripped, b.stripped);
    for (std::size_t i = 0; i < world.built.size(); ++i)
    {
        DP_CHECK(world.built[i].record.items == again.built[i].record.items);
    }
}
