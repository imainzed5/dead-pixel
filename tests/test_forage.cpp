// Living off the land and food (phase 6): what a bush or a tree gives and
// when, snares at dawn, garden beds, and food going stale.
#include "TestFramework.h"

#include "world/Forage.h"
#include "world/Garden.h"

DP_TEST(forage_is_a_hash_of_tile_and_day)
{
    const glm::ivec2 tile(40, 52);
    const auto a = forage::search(forage::Spot::Bush, tile, 3, Season::Summer, false, 0, 99u);
    const auto b = forage::search(forage::Spot::Bush, tile, 3, Season::Summer, false, 0, 99u);
    DP_CHECK(a.find == b.find);
    DP_CHECK_EQ(a.count, b.count);

    // About 60 % of searches find nothing; bushes give berries, tufts herbs.
    int nothing = 0;
    int total = 0;
    for (int x = 0; x < 40; ++x)
    {
        for (int day = 1; day <= 10; ++day)
        {
            const auto r = forage::search(forage::Spot::Bush, glm::ivec2(x, 7), day, Season::Summer, false, 0, 5u);
            ++total;
            if (r.find == forage::Find::Nothing) ++nothing;
            else
            {
                DP_CHECK(r.find == forage::Find::Berries);
                DP_CHECK(r.count >= 1 && r.count <= 4);
            }
            const auto h = forage::search(forage::Spot::Tuft, glm::ivec2(x, 7), day, Season::Winter, false, 0, 5u);
            DP_CHECK(h.find == forage::Find::Nothing || h.find == forage::Find::Herbs);
        }
    }
    const double share = static_cast<double>(nothing) / static_cast<double>(total);
    DP_CHECK(share > 0.5 && share < 0.7);

    // No berries in winter; no mushrooms without rain.
    for (int x = 0; x < 60; ++x)
    {
        DP_CHECK(forage::search(forage::Spot::Bush, glm::ivec2(x, 1), 2, Season::Winter, true, 0, 5u).find == forage::Find::Nothing);
        DP_CHECK(forage::search(forage::Spot::TreeBase, glm::ivec2(x, 1), 2, Season::Autumn, false, 0, 5u).find == forage::Find::Nothing);
    }
    // A spot gives again after three days.
    DP_CHECK(!forage::regrown(4, 6));
    DP_CHECK(forage::regrown(4, 7));
}

DP_TEST(forage_mushrooms_and_snares)
{
    // One mushroom in four is bad until Living off the land 2.
    DP_CHECK(forage::badMushroom(0, 0.1f));
    DP_CHECK(!forage::badMushroom(0, 0.3f));
    DP_CHECK(!forage::badMushroom(2, 0.1f));

    // Snares: about 25 % caught at dawn (50 % at skill 3), 10 % sprung empty.
    const auto rate = [](int skill, forage::SnareDawn want)
    {
        int hits = 0;
        int total = 0;
        for (int x = 0; x < 50; ++x)
        {
            for (int day = 1; day <= 20; ++day)
            {
                ++total;
                if (forage::snareAtDawn(glm::ivec2(x, 9), day, skill, 1234u) == want) ++hits;
            }
        }
        return static_cast<double>(hits) / static_cast<double>(total);
    };
    DP_CHECK_NEAR(rate(0, forage::SnareDawn::Caught), 0.25, 0.05);
    DP_CHECK_NEAR(rate(3, forage::SnareDawn::Caught), 0.5, 0.05);
    DP_CHECK_NEAR(rate(0, forage::SnareDawn::Sprung), 0.10, 0.04);
}

DP_TEST(food_states_and_what_they_do)
{
    DP_CHECK(food::stateOf(0.9f) == food::State::Fresh);
    DP_CHECK(food::stateOf(0.5f) == food::State::Stale);
    DP_CHECK(food::stateOf(0.2f) == food::State::Spoiled);
    DP_CHECK(std::string(food::stateName(food::State::Stale)) == "stale");
    DP_CHECK_NEAR(food::valueScale(0.9f), 1.0f, 0.0001);
    DP_CHECK_NEAR(food::valueScale(0.5f), 0.75f, 0.0001);
    DP_CHECK_NEAR(food::valueScale(0.1f), 0.5f, 0.0001);
    // Fresh cooked food: no illness. Raw meat: +20. Weak Stomach doubles.
    DP_CHECK_NEAR(food::illnessFrom(1.0f, 0.0f, 1.0f), 0.0f, 0.0001);
    DP_CHECK_NEAR(food::illnessFrom(1.0f, 20.0f, 1.0f), 20.0f, 0.0001);
    DP_CHECK_NEAR(food::illnessFrom(1.0f, 20.0f, 2.0f), 40.0f, 0.0001);
    DP_CHECK(food::illnessFrom(0.1f, 0.0f, 1.0f) > 0.0f);
    DP_CHECK_NEAR(food::kDirtyWaterIllness, 12.0f, 0.0001);
}

DP_TEST(garden_grows_dies_and_regrows)
{
    // Watered every day: ripe after 6 days, 4 vegetables.
    garden::Bed bed;
    for (int day = 0; day < 6; ++day)
    {
        garden::water(bed);
        for (int h = 0; h < 24; ++h) garden::tick(bed, 1.0f, false, 0);
    }
    DP_CHECK(garden::ripe(bed));
    DP_CHECK_EQ(garden::stage(bed), garden::kStages - 1);
    DP_CHECK_EQ(garden::harvest(bed), garden::kYield);
    DP_CHECK_EQ(garden::kYield, 4);
    DP_CHECK(!garden::ripe(bed));
    // It grows again from its own seed: 5 days.
    for (int day = 0; day < 5; ++day)
    {
        garden::water(bed);
        for (int h = 0; h < 24; ++h) garden::tick(bed, 1.0f, false, 0);
    }
    DP_CHECK(garden::ripe(bed));

    // Rain waters it.
    garden::Bed rained;
    for (int day = 0; day < 6; ++day)
    {
        for (int h = 0; h < 24; ++h) garden::tick(rained, 1.0f, h == 0, 0);
    }
    DP_CHECK(garden::ripe(rained));

    // Dry for 3 days: dead, and dead stays dead.
    garden::Bed dry;
    for (int h = 0; h < 73; ++h) garden::tick(dry, 1.0f, false, 0);
    DP_CHECK(dry.dead);
    garden::water(dry);
    DP_CHECK(dry.dead);
    DP_CHECK(!garden::ripe(dry));

    // Trampled: back to seedlings, half the time.
    garden::Bed walked;
    walked.growHours = 100.0f;
    garden::trample(walked, 0.7f);
    DP_CHECK_NEAR(walked.growHours, 100.0f, 0.0001);
    garden::trample(walked, 0.2f);
    DP_CHECK_NEAR(walked.growHours, 0.0f, 0.0001);
    DP_CHECK_EQ(garden::stage(walked), 0);

    // The next life finds seedlings.
    garden::Bed kept;
    kept.growHours = 130.0f;
    kept.regrowing = true;
    garden::carryOver(kept);
    DP_CHECK_NEAR(kept.growHours, 0.0f, 0.0001);
    DP_CHECK(!kept.regrowing);
}
