// The first three days: taps that die on day 3, nights that bring more of the
// dead, and the plan the objective panel shows.
#include "TestFramework.h"

#include "world/FirstDays.h"

DP_TEST(firstdays_taps_run_until_the_small_hours_of_day_3)
{
    DP_CHECK(firstdays::tapsRunning(1, 8.0));
    DP_CHECK(firstdays::tapsRunning(2, 23.9));
    DP_CHECK(firstdays::tapsRunning(3, 2.9));
    DP_CHECK(!firstdays::tapsRunning(3, 3.0));
    DP_CHECK(!firstdays::tapsRunning(5, 12.0));

    DP_CHECK(!firstdays::pressureLow(1, 18.0));
    DP_CHECK(!firstdays::pressureLow(2, 11.9));
    DP_CHECK(firstdays::pressureLow(2, 12.0));
    DP_CHECK(firstdays::pressureLow(3, 1.0));
    DP_CHECK(!firstdays::pressureLow(3, 4.0)); // dry, not low
}

DP_TEST(firstdays_nights_belong_to_the_day_before)
{
    DP_CHECK_EQ(firstdays::nightIndex(1, 12.0), 0);
    DP_CHECK_EQ(firstdays::nightIndex(1, 20.0), 1);
    DP_CHECK_EQ(firstdays::nightIndex(2, 3.0), 1);
    DP_CHECK_EQ(firstdays::nightIndex(2, 6.0), 0);
    DP_CHECK_EQ(firstdays::nightIndex(3, 22.0), 3);

    DP_CHECK_EQ(firstdays::planDay(2, 3.0), 1);
    DP_CHECK_EQ(firstdays::planDay(2, 9.0), 2);
}

DP_TEST(firstdays_each_night_brings_more_callers)
{
    int last = 0;
    for (int night = 1; night <= 6; ++night)
    {
        const int callers = firstdays::nightCallers(night);
        DP_CHECK(callers > last);
        last = callers;

        // Every caller is in a group, the groups come in order, and all of
        // them before dawn.
        int total = 0;
        double previous = -1.0;
        for (const firstdays::CallerGroup& group : firstdays::callerSchedule(callers))
        {
            DP_CHECK(group.count > 0);
            DP_CHECK(group.hoursAfterDusk > previous);
            DP_CHECK(group.hoursAfterDusk < 24.0 - firstdays::duskHour(1) + firstdays::dawnHour(2));
            previous = group.hoursAfterDusk;
            total += group.count;
        }
        DP_CHECK_EQ(total, callers);
    }
    DP_CHECK_EQ(firstdays::nightCallers(0), 0);
    DP_CHECK(firstdays::callerSchedule(0).empty());
    // Night 1 sends them one at a time.
    DP_CHECK_EQ(firstdays::callerSchedule(firstdays::nightCallers(1)).size(), std::size_t{2});
}

DP_TEST(firstdays_plans_follow_the_days_and_nights)
{
    firstdays::Facts facts;
    facts.searchable = 5;
    facts.searched = 2;
    facts.ways = 2;
    facts.waysClosed = 1;

    const firstdays::Plan day1 = firstdays::planFor(1, 9.0, facts);
    DP_CHECK(day1.title.find("Day 1") == 0);
    DP_CHECK_EQ(day1.steps.size(), std::size_t{3});
    DP_CHECK(day1.steps[0].text == "Search the house (2/5)");
    DP_CHECK(!day1.steps[0].done);
    DP_CHECK(day1.steps[2].text == "Close up the house (1/2)");

    facts.searched = 5;
    facts.waysClosed = 2;
    facts.usedTap = true;
    for (const firstdays::Step& step : firstdays::planFor(1, 9.0, facts).steps)
    {
        DP_CHECK(step.done);
    }

    // After dark the plan is the night's, until dawn on the next day.
    DP_CHECK(firstdays::planFor(1, 21.0, facts).title.find("Night 1") == 0);
    DP_CHECK(firstdays::planFor(2, 4.0, facts).title.find("Night 1") == 0);
    DP_CHECK(firstdays::planFor(2, 7.0, facts).title.find("Day 2") == 0);

    // Counts stop at the goal.
    facts.planks = 9;
    DP_CHECK(firstdays::planFor(2, 7.0, facts).steps[1].text == "Find planks (4/4)");

    // A rain collector settles day 3's water.
    facts.rainCollector = true;
    DP_CHECK(firstdays::planFor(3, 10.0, facts).steps[0].done);

    // Day 3 asks for every way in boarded.
    facts.waysBoarded = 1;
    DP_CHECK(firstdays::planFor(3, 10.0, facts).steps[2].text == "Board up the house (1/2)");
    DP_CHECK(!firstdays::planFor(3, 10.0, facts).steps[2].done);
    DP_CHECK(firstdays::planFor(1, 21.0, facts).steps[1].text == "Keep the house closed (2/2)");

    // From the morning of day 4, no plan.
    DP_CHECK(firstdays::planFor(3, 22.0, facts).title.find("Night 3") == 0);
    DP_CHECK(firstdays::planFor(4, 7.0, facts).title.empty());
    DP_CHECK(firstdays::planFor(4, 22.0, facts).title.empty());
}

DP_TEST(firstdays_clock_line_counts_down_to_dark_and_dawn)
{
    DP_CHECK(firstdays::clockLine(8.0) == "Dark in 12h 00m");
    DP_CHECK(firstdays::clockLine(19.5) == "Dark in 30m");
    DP_CHECK(firstdays::clockLine(22.0) == "Dawn in 8h 00m");
    DP_CHECK(firstdays::clockLine(5.0) == "Dawn in 1h 00m");
}
