#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

// Life after day 3 (phase 13): from day 4 the objective panel shows one
// standing aim, chosen each dawn from the world's facts, with up to three
// steps. Pure rules: Game gathers the facts, shows the aim and rewards it.
// Also the dawn card's line, from assets/data/day_cards.json.
namespace aims
{
// The day the first days' plans hand over to aims.
constexpr int kFirstAimDay = 4;
constexpr float kDoneRelief = 4.0f; // depression off, once per aim per life

// In the order they are tried.
enum class Kind
{
    None,
    Water,   // Water that refills
    House,   // A house that holds
    Tonight, // a horde is due tonight
    Power,   // The power is going
    Food,    // Food for a week
    Work,    // Somewhere to work
    Cold,    // Before the cold
    WayOut,  // The way out (P09)
    Someone, // Someone out there
    HoldOn   // nothing pressing: the day and the season
};
constexpr int kKindCount = static_cast<int>(Kind::HoldOn) + 1;

constexpr int kBottlesWanted = 4;  // fewer clean bottles than this: water aim
constexpr int kBoilsWanted = 3;
constexpr int kBatteriesWanted = 2;
constexpr int kCandlesWanted = 2;
constexpr int kFoodDaysWanted = 7;
constexpr int kFoodPerDay = 3;     // food items a day, near enough
constexpr int kColdFromDay = 36;   // autumn after this day
constexpr int kPlanksWanted = 20;
constexpr float kLeadTiles = 60.0f;

struct Facts
{
    int day = 1;
    std::string season;          // "Autumn"
    bool autumn = false;
    // Water.
    bool rainCollector = false;
    int cleanBottles = 0;        // carried and kept at home
    int boiled = 0;              // bottles boiled since the aim was chosen
    // The house.
    int ways = 0;
    int waysClosed = 0;          // boarded or blocked
    bool alarm = false;          // a can alarm hung at home
    // Tonight.
    bool hordeTonight = false;
    bool lightsOut = false;      // nothing lit at home, the flashlight off
    bool quiet = false;          // nothing loud made since dusk
    bool wayOut = false;         // two ways out of the house, or a vehicle that runs near it
    // The power.
    bool powerFailed = false;
    int fridgeFood = 0;          // food still in the home's fridges
    int batteries = 0;
    int candles = 0;
    // Food.
    int food = 0;                // food items carried and kept at home
    bool snare = false;
    bool bed = false;            // a garden bed
    // Work.
    bool crate = false;
    bool workbench = false;
    // The cold.
    bool coat = false;
    int planks = 0;              // carried and kept at home
    // The way out (P09): the next step known, "" before lead 1.
    bool wayOutMode = false;
    std::string wayOutStep;
    // Someone out there: a lead within 60 tiles not yet visited this life.
    std::string leadName;        // "Ada's grave"
    std::string leadWhere;       // "north-east, 40 tiles"
};

struct Step
{
    std::string text;
    bool done = false;
};

struct Aim
{
    Kind kind = Kind::None;
    std::string title;
    std::string detail; // under the title; "" for the clock line
    std::vector<Step> steps;
};

// The first aim whose condition holds (HoldOn when none does).
[[nodiscard]] Kind choose(const Facts& facts);
[[nodiscard]] bool holds(Kind kind, const Facts& facts);
// The aim with its steps and counts as the facts stand now.
[[nodiscard]] Aim build(Kind kind, const Facts& facts);
// Every step done (an aim without steps is never complete).
[[nodiscard]] bool complete(const Aim& aim);
[[nodiscard]] const char* key(Kind kind);
[[nodiscard]] Kind fromKey(const std::string& key);

// "north-east" for a step on the screen (x right, y down).
[[nodiscard]] std::string compass(float dx, float dy);

// The dawn card's line (phase 13). One line each; empty means "Day N" alone.
struct CardFacts
{
    int day = 1;
    bool hordeTonight = false;
    bool seasonFirstDay = false;
    std::string season;          // "Summer"
    bool firstDayWithoutPower = false;
    bool frost = false;
};

struct DayCards
{
    std::vector<std::string> days;  // index = day (0 unused)
    std::string horde;
    std::string noPower;
    std::string season;             // "{season}." when a season starts
    std::string tenth;              // every tenth day ("{day} days.")
    std::string frost;
    std::string demoEnd;            // "This is the end of the demo's week. ... {name}."
};
[[nodiscard]] DayCards defaultCards();
[[nodiscard]] DayCards cardsFromJson(const nlohmann::json& json);
[[nodiscard]] std::string cardLine(const DayCards& cards, const CardFacts& facts);
// Replaces "{key}" with value.
[[nodiscard]] std::string fill(std::string text, const std::string& key, const std::string& value);

// Death hints (phase 13): one plain line under the cause, from the cause and
// the run's facts; each at most twice per save (Game counts).
struct DeathFacts
{
    bool thirst = false;
    bool infection = false;
    bool nightOneAtHomeUnboarded = false;
    bool grabbedNoShoves = false;
};
struct DeathHint
{
    std::string id;
    std::string line;
};
[[nodiscard]] std::vector<DeathHint> deathHintsFromJson(const nlohmann::json& json);
[[nodiscard]] std::vector<DeathHint> defaultDeathHints();
// The ids that apply, most telling first.
[[nodiscard]] std::vector<std::string> deathHintIds(const DeathFacts& facts);
} // namespace aims
