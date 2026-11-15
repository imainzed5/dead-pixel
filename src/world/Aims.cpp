#include "world/Aims.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <nlohmann/json.hpp>

namespace aims
{
namespace
{
std::string count(int have, int need)
{
    return " (" + std::to_string(std::min(have, need)) + "/" + std::to_string(need) + ")";
}

struct KindInfo
{
    Kind kind;
    const char* key;
    const char* title;
};

constexpr std::array<KindInfo, kKindCount> kKinds = {{
    {Kind::None, "none", ""},
    {Kind::Water, "water", "Water that refills"},
    {Kind::House, "house", "A house that holds"},
    {Kind::Tonight, "tonight", "Tonight"},
    {Kind::Power, "power", "The power is going"},
    {Kind::Food, "food", "Food for a week"},
    {Kind::Work, "work", "Somewhere to work"},
    {Kind::Cold, "cold", "Before the cold"},
    {Kind::WayOut, "way_out", "The way out"},
    {Kind::Someone, "someone", "Someone out there"},
    {Kind::HoldOn, "hold_on", "Hold on"},
}};

std::string text(const nlohmann::json& json, const char* key, const std::string& fallback)
{
    if (!json.contains(key) || !json[key].is_string()) return fallback;
    return json[key].get<std::string>();
}
} // namespace

bool holds(Kind kind, const Facts& f)
{
    switch (kind)
    {
    case Kind::Water: return !f.rainCollector && f.cleanBottles < kBottlesWanted;
    case Kind::House: return f.ways > 0 && f.waysClosed < f.ways;
    case Kind::Tonight: return f.hordeTonight;
    case Kind::Power: return f.day >= kFirstAimDay && !f.powerFailed;
    case Kind::Food: return f.food < kFoodDaysWanted * kFoodPerDay;
    case Kind::Work: return !f.crate || !f.workbench;
    case Kind::Cold: return f.autumn && f.day > kColdFromDay && (!f.coat || f.planks < kPlanksWanted);
    case Kind::WayOut: return f.wayOutMode && !f.wayOutStep.empty();
    case Kind::Someone: return !f.leadName.empty();
    case Kind::HoldOn: return true;
    case Kind::None: break;
    }
    return false;
}

Kind choose(const Facts& facts)
{
    for (int i = static_cast<int>(Kind::Water); i < kKindCount; ++i)
    {
        const Kind kind = static_cast<Kind>(i);
        if (holds(kind, facts)) return kind;
    }
    return Kind::HoldOn;
}

Aim build(Kind kind, const Facts& f)
{
    Aim aim;
    aim.kind = kind;
    aim.title = kKinds[static_cast<std::size_t>(kind)].title;
    switch (kind)
    {
    case Kind::Water:
    {
        // Either one fills the need.
        const bool done = f.rainCollector || f.boiled >= kBoilsWanted;
        aim.steps.push_back({"Build a rain collector", done});
        aim.steps.push_back({"Or boil water" + count(f.boiled, kBoilsWanted), done});
        break;
    }
    case Kind::House:
        aim.steps.push_back({"Close up the house" + count(f.waysClosed, f.ways), f.waysClosed >= f.ways});
        aim.steps.push_back({"Hang an alarm", f.alarm});
        break;
    case Kind::Tonight:
        aim.steps.push_back({"Lights out before dark", f.lightsOut});
        aim.steps.push_back({"Stay quiet", f.quiet});
        aim.steps.push_back({"Have a way out", f.wayOut});
        break;
    case Kind::Power:
        aim.steps.push_back({"Eat what's in the fridges", f.fridgeFood <= 0});
        aim.steps.push_back({"Find batteries" + count(f.batteries, kBatteriesWanted), f.batteries >= kBatteriesWanted});
        aim.steps.push_back({"Find candles" + count(f.candles, kCandlesWanted), f.candles >= kCandlesWanted});
        break;
    case Kind::Food:
    {
        const int want = kFoodDaysWanted * kFoodPerDay;
        aim.steps.push_back({"Find food" + count(f.food, want), f.food >= want});
        aim.steps.push_back({"Set a snare", f.snare});
        aim.steps.push_back({"Plant a bed", f.bed});
        break;
    }
    case Kind::Work:
        aim.steps.push_back({"Build a crate", f.crate});
        aim.steps.push_back({"Build a workbench", f.workbench});
        break;
    case Kind::Cold:
        aim.steps.push_back({"Find a winter coat", f.coat});
        aim.steps.push_back({"Store wood" + count(f.planks, kPlanksWanted), f.planks >= kPlanksWanted});
        break;
    case Kind::WayOut:
        aim.steps.push_back({f.wayOutStep, false});
        break;
    case Kind::Someone:
        aim.steps.push_back({"Go to " + f.leadName, f.leadName.empty()});
        aim.detail = f.leadWhere;
        break;
    case Kind::HoldOn:
        aim.detail = "Day " + std::to_string(f.day) + "." + (f.season.empty() ? "" : " " + f.season + ".");
        break;
    case Kind::None: break;
    }
    return aim;
}

bool complete(const Aim& aim)
{
    if (aim.steps.empty()) return false;
    return std::all_of(aim.steps.begin(), aim.steps.end(), [](const Step& s) { return s.done; });
}

const char* key(Kind kind)
{
    return kKinds[static_cast<std::size_t>(kind)].key;
}

Kind fromKey(const std::string& text)
{
    for (const KindInfo& info : kKinds)
    {
        if (text == info.key) return info.kind;
    }
    return Kind::None;
}

std::string compass(float dx, float dy)
{
    if (std::fabs(dx) < 0.001f && std::fabs(dy) < 0.001f) return "here";
    // Screen y grows downward: up the screen is north.
    static const char* const kNames[8] = {"east", "south-east", "south", "south-west",
                                          "west", "north-west", "north", "north-east"};
    const float angle = std::atan2(dy, dx); // -pi..pi, 0 east, pi/2 south
    int sector = static_cast<int>(std::lround(angle / (3.14159265f / 4.0f)));
    sector = ((sector % 8) + 8) % 8;
    return kNames[sector];
}

std::string fill(std::string text, const std::string& key, const std::string& value)
{
    const std::string token = "{" + key + "}";
    for (std::size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size()))
    {
        text.replace(at, token.size(), value);
    }
    return text;
}

DayCards defaultCards()
{
    DayCards cards;
    cards.days = {"",
                  "Make the house safe before dark.",
                  "The water pressure is dropping. Fill every bottle.",
                  "The taps are dry. Build a rain collector, or find water and boil it.",
                  "Nobody is coming.",
                  "",
                  "",
                  "A week."};
    cards.horde = "Something is moving on the far side of town.";
    cards.noPower = "The lights are not coming back.";
    cards.season = "{season}.";
    cards.tenth = "{day} days.";
    cards.frost = "Frost on the windows.";
    cards.demoEnd = "This is the end of the demo's week. The town will remember {name}.";
    return cards;
}

DayCards cardsFromJson(const nlohmann::json& json)
{
    DayCards cards = defaultCards();
    if (!json.is_object()) return cards;
    if (json.contains("days") && json["days"].is_object())
    {
        for (const auto& [day, line] : json["days"].items())
        {
            if (!line.is_string()) continue;
            int index = 0;
            try
            {
                index = std::stoi(day);
            }
            catch (...)
            {
                continue;
            }
            if (index <= 0 || index > 1000) continue;
            if (static_cast<int>(cards.days.size()) <= index) cards.days.resize(static_cast<std::size_t>(index) + 1);
            cards.days[static_cast<std::size_t>(index)] = line.get<std::string>();
        }
    }
    cards.horde = text(json, "horde", cards.horde);
    cards.noPower = text(json, "firstDayWithoutPower", cards.noPower);
    cards.season = text(json, "seasonFirstDay", cards.season);
    cards.tenth = text(json, "everyTenthDay", cards.tenth);
    cards.frost = text(json, "frost", cards.frost);
    cards.demoEnd = text(json, "demoEnd", cards.demoEnd);
    return cards;
}

std::string cardLine(const DayCards& cards, const CardFacts& f)
{
    // Most pressing first: a horde, a new season, the dark, the day's own
    // line, a round number, the first frost.
    if (f.hordeTonight) return cards.horde;
    if (f.seasonFirstDay && f.day > 1) return fill(cards.season, "season", f.season);
    if (f.firstDayWithoutPower) return cards.noPower;
    if (f.day > 0 && f.day < static_cast<int>(cards.days.size()) && !cards.days[static_cast<std::size_t>(f.day)].empty())
    {
        return cards.days[static_cast<std::size_t>(f.day)];
    }
    if (f.day >= 10 && f.day % 10 == 0) return fill(cards.tenth, "day", std::to_string(f.day));
    if (f.frost) return cards.frost;
    return {};
}

std::vector<DeathHint> defaultDeathHints()
{
    return {{"thirst", "Bottles can be filled at any tap until day 3."},
            {"night_one_unboarded", "Boards on the doors would have held."},
            {"infection", "A dirty bite needs disinfecting within three hours."},
            {"grab_no_shove", "Space shoves them off."}};
}

std::vector<DeathHint> deathHintsFromJson(const nlohmann::json& json)
{
    std::vector<DeathHint> hints = defaultDeathHints();
    if (!json.is_object()) return hints;
    for (DeathHint& hint : hints)
    {
        hint.line = text(json, hint.id.c_str(), hint.line);
    }
    return hints;
}

std::vector<std::string> deathHintIds(const DeathFacts& f)
{
    std::vector<std::string> ids;
    if (f.grabbedNoShoves) ids.emplace_back("grab_no_shove");
    if (f.nightOneAtHomeUnboarded) ids.emplace_back("night_one_unboarded");
    if (f.infection) ids.emplace_back("infection");
    if (f.thirst) ids.emplace_back("thirst");
    return ids;
}
} // namespace aims
