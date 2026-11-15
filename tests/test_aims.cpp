#include "TestFramework.h"

#include "world/Aims.h"
#include "world/DayLength.h"

#include <nlohmann/json.hpp>

namespace
{
// A day-4 house with everything done: only "Hold on" holds.
aims::Facts settled()
{
    aims::Facts f;
    f.day = 12;
    f.season = "Spring";
    f.rainCollector = true;
    f.cleanBottles = 6;
    f.ways = 4;
    f.waysClosed = 4;
    f.alarm = true;
    f.powerFailed = true;
    f.food = 30;
    f.crate = true;
    f.workbench = true;
    return f;
}
} // namespace

DP_TEST(no_collector_and_few_bottles_is_the_water_aim)
{
    aims::Facts f = settled();
    f.rainCollector = false;
    f.cleanBottles = 2;
    DP_CHECK(aims::choose(f) == aims::Kind::Water);
    const aims::Aim aim = aims::build(aims::Kind::Water, f);
    DP_CHECK_EQ(aim.title, std::string("Water that refills"));
    DP_CHECK_EQ(aim.steps.size(), std::size_t{2});
    DP_CHECK(!aims::complete(aim));
    // A collector built: the steps are done, and the next dawn picks another aim.
    f.rainCollector = true;
    DP_CHECK(aims::complete(aims::build(aims::Kind::Water, f)));
    DP_CHECK(aims::choose(f) != aims::Kind::Water);
}

DP_TEST(boiling_three_bottles_also_fills_the_water_aim)
{
    aims::Facts f = settled();
    f.rainCollector = false;
    f.cleanBottles = 0;
    f.boiled = 2;
    DP_CHECK(!aims::complete(aims::build(aims::Kind::Water, f)));
    f.boiled = 3;
    DP_CHECK(aims::complete(aims::build(aims::Kind::Water, f)));
}

DP_TEST(aims_are_tried_in_the_tables_order)
{
    aims::Facts f = settled();
    f.waysClosed = 2;   // the house
    f.hordeTonight = true;
    DP_CHECK(aims::choose(f) == aims::Kind::House);
    f.waysClosed = 4;
    DP_CHECK(aims::choose(f) == aims::Kind::Tonight);
    f.hordeTonight = false;
    f.powerFailed = false;
    DP_CHECK(aims::choose(f) == aims::Kind::Power);
    f.powerFailed = true;
    f.food = 10;
    DP_CHECK(aims::choose(f) == aims::Kind::Food);
    f.food = 30;
    f.workbench = false;
    DP_CHECK(aims::choose(f) == aims::Kind::Work);
    f.workbench = true;
    DP_CHECK(aims::choose(f) == aims::Kind::HoldOn);
}

DP_TEST(the_cold_aim_waits_for_late_autumn)
{
    aims::Facts f = settled();
    f.autumn = true;
    f.day = 30;
    DP_CHECK(aims::choose(f) == aims::Kind::HoldOn);
    f.day = 40;
    DP_CHECK(aims::choose(f) == aims::Kind::Cold);
    const aims::Aim aim = aims::build(aims::Kind::Cold, f);
    DP_CHECK_EQ(aim.steps[1].text, std::string("Store wood (0/20)"));
    f.coat = true;
    f.planks = 25;
    DP_CHECK(aims::choose(f) == aims::Kind::HoldOn);
}

DP_TEST(the_way_out_and_a_lead_come_before_holding_on)
{
    aims::Facts f = settled();
    f.leadName = "Ada's grave";
    f.leadWhere = "north-east, 40 tiles";
    DP_CHECK(aims::choose(f) == aims::Kind::Someone);
    const aims::Aim lead = aims::build(aims::Kind::Someone, f);
    DP_CHECK_EQ(lead.steps[0].text, std::string("Go to Ada's grave"));
    DP_CHECK_EQ(lead.detail, std::string("north-east, 40 tiles"));
    f.wayOutMode = true;
    f.wayOutStep = "Find the fuse";
    DP_CHECK(aims::choose(f) == aims::Kind::WayOut);
}

DP_TEST(hold_on_names_the_day_and_season_with_no_steps)
{
    const aims::Aim aim = aims::build(aims::Kind::HoldOn, settled());
    DP_CHECK(aim.steps.empty());
    DP_CHECK_EQ(aim.detail, std::string("Day 12. Spring."));
    DP_CHECK(!aims::complete(aim));
}

DP_TEST(aim_keys_round_trip)
{
    for (int i = 0; i < aims::kKindCount; ++i)
    {
        const aims::Kind kind = static_cast<aims::Kind>(i);
        DP_CHECK(aims::fromKey(aims::key(kind)) == kind);
    }
}

DP_TEST(compass_reads_the_screen)
{
    DP_CHECK_EQ(aims::compass(1.0f, 0.0f), std::string("east"));
    DP_CHECK_EQ(aims::compass(0.0f, -1.0f), std::string("north"));
    DP_CHECK_EQ(aims::compass(1.0f, -1.0f), std::string("north-east"));
    DP_CHECK_EQ(aims::compass(-1.0f, 1.0f), std::string("south-west"));
}

DP_TEST(day_cards_pick_the_most_pressing_line)
{
    const aims::DayCards cards = aims::defaultCards();
    aims::CardFacts f;
    f.day = 4;
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("Nobody is coming."));
    f.day = 7;
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("A week."));
    f.day = 20;
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("20 days."));
    f.day = 13;
    DP_CHECK(aims::cardLine(cards, f).empty());
    f.firstDayWithoutPower = true;
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("The lights are not coming back."));
    f.seasonFirstDay = true;
    f.season = "Summer";
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("Summer."));
    f.hordeTonight = true;
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("Something is moving on the far side of town."));
}

DP_TEST(day_cards_read_from_the_data_file)
{
    const nlohmann::json json = {{"days", {{"4", "Still nobody."}, {"12", "Twelve."}}}, {"everyTenthDay", "Day {day}."}};
    const aims::DayCards cards = aims::cardsFromJson(json);
    aims::CardFacts f;
    f.day = 4;
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("Still nobody."));
    f.day = 12;
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("Twelve."));
    f.day = 30;
    DP_CHECK_EQ(aims::cardLine(cards, f), std::string("Day 30."));
    DP_CHECK_EQ(aims::fill(cards.demoEnd, "name", "Ada"),
                std::string("This is the end of the demo's week. The town will remember Ada."));
}

DP_TEST(death_hints_follow_the_cause)
{
    aims::DeathFacts f;
    DP_CHECK(aims::deathHintIds(f).empty());
    f.thirst = true;
    const std::vector<std::string> thirst = aims::deathHintIds(f);
    DP_CHECK_EQ(thirst.front(), std::string("thirst"));
    f.grabbedNoShoves = true;
    const std::vector<std::string> grabbed = aims::deathHintIds(f);
    DP_CHECK_EQ(grabbed.front(), std::string("grab_no_shove"));
    const auto hints = aims::deathHintsFromJson(nlohmann::json{{"thirst", "Drink."}});
    DP_CHECK_EQ(hints.front().line, std::string("Drink."));
}

DP_TEST(day_length_scales_game_time_not_real_time)
{
    DP_CHECK_NEAR(daylength::scale(24), 1.0f, 0.0f);
    DP_CHECK_NEAR(daylength::scale(48), 0.5f, 0.0001f);
    DP_CHECK_NEAR(static_cast<float>(daylength::gameHourSeconds(48)), 120.0f, 0.001f);
    // A game hour at 48 minutes a day takes twice the ticks of one at 24.
    const auto ticksForAnHour = [](int minutes)
    {
        double hours = 0.0;
        int ticks = 0;
        while (hours < 1.0 - 1e-9)
        {
            hours += (1.0 / 60.0) * daylength::hoursPerSecond(minutes);
            ++ticks;
        }
        return ticks;
    };
    DP_CHECK_EQ(ticksForAnHour(24), 3600);
    DP_CHECK_EQ(ticksForAnHour(48), 7200);
}
