#include "world/FirstDays.h"
#include "core/Tuning.h"

#include <algorithm>
#include <cmath>

namespace firstdays
{
namespace
{
std::string count(int have, int want)
{
    return " (" + std::to_string(std::clamp(have, 0, want)) + "/" + std::to_string(want) + ")";
}

std::string duration(double hours)
{
    const int minutes = std::max(0, static_cast<int>(std::ceil(hours * 60.0)));
    const int h = minutes / 60;
    const int m = minutes % 60;
    if (h == 0) return std::to_string(m) + "m";
    return std::to_string(h) + "h " + (m < 10 ? "0" : "") + std::to_string(m) + "m";
}
} // namespace

bool tapsRunning(int day, double hours)
{
    return day < kTapsDieDay || (day == kTapsDieDay && hours < kTapsDieHour);
}

bool pressureLow(int day, double hours)
{
    return tapsRunning(day, hours) && (day > 2 || (day == 2 && hours >= kPressureDropHour));
}

int nightIndex(int day, double hours)
{
    if (hours >= kDuskHour) return day;
    if (hours < kDawnHour) return std::max(0, day - 1);
    return 0;
}

int planDay(int day, double hours)
{
    const int night = nightIndex(day, hours);
    return night > 0 ? night : day;
}

int nightCallers(int night)
{
    if (night <= 0) return 0;
    // Tuned: callers per night over the first three nights, then added each night.
    const int perNight = static_cast<int>(Tuning::get("night.callers.base", 2.0f));
    const int growth = static_cast<int>(Tuning::get("night.callers.growth", 1.0f));
    if (night <= 3) return night * perNight;
    return 3 * perNight + (night - 3) * growth;
}

std::vector<CallerGroup> callerSchedule(int callers)
{
    std::vector<CallerGroup> groups;
    if (callers <= 0) return groups;
    const int n = callers <= 2 ? callers : std::min(3, (callers + 1) / 2);
    // The first group comes an hour and a half after dusk, the last half an
    // hour before the small hours end.
    constexpr double kFirst = 1.5;
    constexpr double kLast = 6.5;
    for (int i = 0; i < n; ++i)
    {
        CallerGroup group;
        group.hoursAfterDusk = n == 1 ? 2.5 : kFirst + (kLast - kFirst) * static_cast<double>(i) / static_cast<double>(n - 1);
        group.count = callers / n + (i < callers % n ? 1 : 0);
        groups.push_back(group);
    }
    return groups;
}

Plan planFor(int day, double hours, const Facts& facts)
{
    Plan plan;
    const std::string closed = facts.ways > 0 ? count(facts.waysClosed, facts.ways) : "";
    const int night = nightIndex(day, hours);
    if (night > 0)
    {
        if (night > kPlanDays) return plan;
        static const char* const kTitles[kPlanDays] = {
            "Night 1: hold out",
            "Night 2: hold out",
            "Night 3: the worst yet",
        };
        plan.title = kTitles[night - 1];
        plan.steps.push_back({"Sleep in your bed", facts.sleptTonight});
        plan.steps.push_back({"Keep the house closed" + closed, facts.waysClosed >= facts.ways});
        return plan;
    }

    switch (day)
    {
    case 1:
        plan.title = "Day 1: secure the house";
        plan.steps.push_back({facts.searchable > 0 ? "Search the house" + count(facts.searched, facts.searchable)
                                                   : "Search the house",
                              facts.searched >= facts.searchable});
        plan.steps.push_back({"Fill bottles at the tap", facts.usedTap});
        plan.steps.push_back({"Close up the house" + closed, facts.waysClosed >= facts.ways});
        break;
    case 2:
        plan.title = "Day 2: stock up";
        plan.steps.push_back({"Fill bottles" + count(facts.waterBottles, kStockBottleGoal),
                              facts.waterBottles >= kStockBottleGoal});
        plan.steps.push_back({"Find planks" + count(facts.planks, kPlankGoal), facts.planks >= kPlankGoal});
        plan.steps.push_back({"Find food" + count(facts.food, kFoodGoal), facts.food >= kFoodGoal});
        break;
    case 3:
        plan.title = "Day 3: the taps are dry";
        plan.steps.push_back({facts.rainCollector ? "Rain collector built" : "Find or boil water" + count(facts.waterBottles, kWaterGoal),
                              facts.rainCollector || facts.waterBottles >= kWaterGoal});
        plan.steps.push_back({"Find food" + count(facts.food, kFoodGoal), facts.food >= kFoodGoal});
        plan.steps.push_back({"Board up the house" + (facts.ways > 0 ? count(facts.waysBoarded, facts.ways) : ""),
                              facts.waysBoarded >= facts.ways});
        break;
    default:
        break;
    }
    return plan;
}

std::string clockLine(double hours)
{
    if (hours >= kDawnHour && hours < kDuskHour)
    {
        return "Dark in " + duration(kDuskHour - hours);
    }
    const double untilDawn = hours >= kDuskHour ? 24.0 - hours + kDawnHour : kDawnHour - hours;
    return "Dawn in " + duration(untilDawn);
}

DayCard dayCard(int day)
{
    DayCard card;
    card.title = "Day " + std::to_string(day);
    switch (day)
    {
    case 1: card.line = "Make the house safe before dark."; break;
    case 2: card.line = "The water pressure is dropping. Fill every bottle."; break;
    case 3: card.line = "The taps are dry. Build a rain collector, or find water and boil it."; break;
    case 4: card.line = "Nothing restocks. What you need is out there."; break;
    default: break;
    }
    return card;
}
} // namespace firstdays
