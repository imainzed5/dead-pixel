#pragma once

#include <string>
#include <vector>

// The first three days of a run, as rules with no game state of their own:
// a plan for each day and night, taps that run dry on day 3, and nights that
// bring more of the dead each time. Game gathers the facts and acts on them.
namespace firstdays
{
// Days with a plan of their own. From the morning after, the mode's
// objective takes over.
constexpr int kPlanDays = 3;

constexpr double kDuskHour = 20.0;
constexpr double kDawnHour = 6.0;

// The water goes off in the small hours of day 3.
constexpr int kTapsDieDay = 3;
constexpr double kTapsDieHour = 3.0;
// From noon on day 2 the pressure drops, and the tap says so.
constexpr double kPressureDropHour = 12.0;

// What the day 2 and day 3 plans ask for: more than the home holds, so
// day 2 means going out.
constexpr int kStockBottleGoal = 5; // filled while the taps run
constexpr int kWaterGoal = 3;       // carried once they are dry
constexpr int kPlankGoal = 4;
constexpr int kFoodGoal = 6;

[[nodiscard]] bool tapsRunning(int day, double hours);
[[nodiscard]] bool pressureLow(int day, double hours);

// The night under way: night N runs from dusk on day N to dawn on day N + 1.
// 0 in daylight.
[[nodiscard]] int nightIndex(int day, double hours);

// The day whose plan is showing: the last dawn's day. Nights belong to the
// day before them.
[[nodiscard]] int planDay(int day, double hours);

// How many of the dead come calling on night N, before tone scaling.
[[nodiscard]] int nightCallers(int night);

// Callers set out in groups spread across the night.
struct CallerGroup
{
    double hoursAfterDusk = 0.0;
    int count = 0;
};
[[nodiscard]] std::vector<CallerGroup> callerSchedule(int callers);

// What Game knows about the player's progress.
struct Facts
{
    int searched = 0;       // home containers opened
    int searchable = 0;
    // The home's ways in: its doors and ground-floor windows.
    int ways = 0;
    int waysClosed = 0;     // doors boarded or blocked, windows boarded, blocked or curtained
    int waysBoarded = 0;    // boarded (or blocked by furniture), every one
    bool usedTap = false;
    bool sleptTonight = false;
    int waterBottles = 0;   // full ones carried
    int planks = 0;
    int food = 0;
    bool rainCollector = false;
};

struct Step
{
    std::string text;
    bool done = false;
};

struct Plan
{
    std::string title;
    std::vector<Step> steps;
};

// The plan showing at this hour, or an empty title once the first days are
// over.
[[nodiscard]] Plan planFor(int day, double hours, const Facts& facts);

// "Dark in 3h 20m" by day, "Dawn in 2h 05m" by night.
[[nodiscard]] std::string clockLine(double hours);

// The card at dawn: a heading and one line for the day ahead.
struct DayCard
{
    std::string title;
    std::string line;
};
[[nodiscard]] DayCard dayCard(int day);
} // namespace firstdays
