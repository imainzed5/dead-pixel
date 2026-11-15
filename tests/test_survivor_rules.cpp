#include "TestFramework.h"

#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

#include "items/ItemDatabase.h"
#include "world/SurvivorRules.h"

using namespace survivorrules;

namespace
{
Situation calm()
{
    Situation s;
    s.p = Personality{0.4f, 0.5f, 0.6f};
    return s;
}

std::vector<HomeChoice> someHouses(int n)
{
    std::vector<HomeChoice> houses;
    for (int i = 0; i < n; ++i)
    {
        HomeChoice h;
        h.building = i + 1;
        h.tile = glm::ivec2(10 + i * 12, 20);
        h.big = (i % 4 == 0);
        houses.push_back(h);
    }
    return houses;
}

std::vector<std::string> someNames()
{
    return {"Ana", "Tomas", "Ruth", "Dev", "Marta", "Owen", "Ines", "Karl", "Lena", "Pavel", "June",
            "Hector", "Nadia", "Sol", "Greta", "Abel", "Mira", "Bram", "Tess", "Viktor", "Rosa"};
}
} // namespace

// --- Scoring ---------------------------------------------------------------

DP_TEST(survivor_with_nothing_to_do_idles)
{
    DP_CHECK(choose(calm()) == Option::Idle);
}

DP_TEST(survivor_timid_one_flees_the_dead_in_range)
{
    Situation s = calm();
    s.p.aggression = 0.2f; // flee range 4.8 tiles
    s.deadInFleeRange = 1;
    s.deadInFightRange = 1;
    s.nearestDeadTiles = 2.5f;
    s.armed = true;
    DP_CHECK(choose(s) == Option::Flee);
}

DP_TEST(survivor_aggressive_armed_one_fights_one_or_two)
{
    Situation s = calm();
    s.p.aggression = 0.85f; // flee range 0.9 tiles
    s.deadInFightRange = 2;
    s.nearestDeadTiles = 2.0f;
    s.armed = true;
    DP_CHECK(choose(s) == Option::Fight);
}

DP_TEST(survivor_never_fights_three_in_reach)
{
    Situation s = calm();
    s.p.aggression = 0.95f;
    s.deadInFightRange = 3;
    s.nearestDeadTiles = 1.5f;
    s.armed = true;
    DP_CHECK(choose(s) == Option::Flee);
}

DP_TEST(survivor_badly_hurt_runs_rather_than_fights)
{
    Situation s = calm();
    s.p.aggression = 0.9f;
    s.deadInFightRange = 1;
    s.nearestDeadTiles = 2.0f;
    s.health = 0.2f;
    s.armed = true;
    DP_CHECK(choose(s) == Option::Flee);
}

DP_TEST(survivor_hungry_with_food_eats)
{
    Situation s = calm();
    s.hunger = 20.0f;
    DP_CHECK(choose(s) == Option::Eat);
    s.hasFood = false;
    DP_CHECK(choose(s) != Option::Eat);
}

DP_TEST(survivor_thirstier_than_hungry_drinks_first)
{
    Situation s = calm();
    s.hunger = 40.0f;
    s.thirst = 10.0f;
    DP_CHECK(choose(s) == Option::Drink);
}

DP_TEST(survivor_low_stock_by_day_scavenges_but_not_at_night_or_the_stayer)
{
    Situation s = calm();
    s.stockDays = 0.5f;
    s.containerKnown = true;
    DP_CHECK(choose(s) == Option::Scavenge);
    s.night = true;
    DP_CHECK(choose(s) != Option::Scavenge);
    s.night = false;
    s.stayer = true;
    DP_CHECK(choose(s) != Option::Scavenge);
    s.stayer = false;
    s.containerKnown = false;
    DP_CHECK(choose(s) != Option::Scavenge);
}

DP_TEST(survivor_tired_at_night_sleeps)
{
    Situation s = calm();
    s.night = true;
    s.sleepDebt = 40.0f;
    DP_CHECK(choose(s) == Option::Sleep);
    s.night = false;
    DP_CHECK(choose(s) == Option::Idle);
}

DP_TEST(survivor_competent_with_boards_fortifies_an_open_home)
{
    Situation s = calm();
    s.hasBoards = true;
    s.homeOpen = true;
    DP_CHECK(choose(s) == Option::Fortify);
    s.p.competence = 0.3f;
    DP_CHECK(choose(s) != Option::Fortify);
}

DP_TEST(survivor_wounded_with_medicine_tends)
{
    Situation s = calm();
    s.wounds = 1;
    s.hasMedicine = true;
    DP_CHECK(choose(s) == Option::Tend);
}

DP_TEST(survivor_wary_watches_the_player)
{
    Situation s = calm();
    s.playerWatched = true;
    DP_CHECK(choose(s) == Option::Watch);
}

DP_TEST(survivor_keeps_a_choice_unless_another_clearly_wins)
{
    Situation s = calm();
    s.playerWatched = true;      // watch 0.45
    s.hasBoards = true;
    s.homeOpen = true;
    s.p.competence = 0.6f;       // fortify 0.43
    s.current = Option::Fortify;
    DP_CHECK(choose(s) == Option::Fortify);
    s.current = Option::Idle;
    DP_CHECK(choose(s) == Option::Watch);
}

DP_TEST(survivor_flee_range_shrinks_with_aggression)
{
    DP_CHECK_NEAR(fleeTiles(0.0f), 6.0f, 1e-4f);
    DP_CHECK_NEAR(fleeTiles(0.5f), 3.0f, 1e-4f);
    DP_CHECK_NEAR(fleeTiles(1.0f), 0.0f, 1e-4f);
    DP_CHECK(blowSeconds(1.0f) < blowSeconds(0.0f));
}

// --- Attitude --------------------------------------------------------------

DP_TEST(survivor_attitude_follows_the_rules)
{
    DP_CHECK(attitude(Personality{0.8f, 0.9f, 0.5f}, 0.0f, 50.0f, false) == Attitude::Hostile);
    DP_CHECK(attitude(Personality{0.3f, 0.3f, 0.5f}, 0.75f, 0.0f, false) == Attitude::Hostile);
    DP_CHECK(attitude(Personality{0.3f, 0.5f, 0.5f}, 0.75f, 0.0f, false) == Attitude::Wary);
    DP_CHECK(attitude(Personality{0.3f, 0.7f, 0.5f}, 0.0f, 0.0f, false) == Attitude::Open);
    DP_CHECK(attitude(Personality{0.3f, 0.7f, 0.5f}, 0.0f, -5.0f, false) == Attitude::Wary);
    DP_CHECK(attitude(Personality{0.3f, 0.7f, 0.5f}, 0.0f, 80.0f, true) == Attitude::Hostile);
}

DP_TEST(survivor_desperation_rises_with_hunger_and_empty_stock)
{
    DP_CHECK_NEAR(desperation(100.0f, 100.0f, 5.0f), 0.0f, 1e-4f);
    DP_CHECK_NEAR(desperation(100.0f, 100.0f, 0.0f), 0.4f, 1e-4f);
    DP_CHECK_NEAR(desperation(0.0f, 100.0f, 0.0f), 1.0f, 1e-4f);
    DP_CHECK(desperation(20.0f, 100.0f, 1.0f) > desperation(40.0f, 100.0f, 1.0f));
}

DP_TEST(survivor_robs_only_when_desperate_and_distrustful)
{
    DP_CHECK(wouldRob(0.85f, 0.0f, false));
    DP_CHECK(!wouldRob(0.75f, 0.0f, false));
    DP_CHECK(!wouldRob(0.85f, 10.0f, false));
    DP_CHECK(!wouldRob(0.95f, -20.0f, true));
}

DP_TEST(survivor_trust_gains_are_halved_for_a_loner)
{
    DP_CHECK_NEAR(trustChange(10.0f, false), 10.0f, 1e-4f);
    DP_CHECK_NEAR(trustChange(10.0f, true), 5.0f, 1e-4f);
    DP_CHECK_NEAR(trustChange(-40.0f, true), -40.0f, 1e-4f);
    DP_CHECK_NEAR(clampTrust(140.0f), 100.0f, 1e-4f);
}

DP_TEST(survivor_trust_for_a_new_life)
{
    DP_CHECK_NEAR(trustForNewLife(45.0f, false, false), 0.0f, 1e-4f);
    DP_CHECK_NEAR(trustForNewLife(70.0f, true, false), 15.0f, 1e-4f);
    DP_CHECK_NEAR(trustForNewLife(50.0f, true, false), 0.0f, 1e-4f);
    DP_CHECK_NEAR(trustForNewLife(-30.0f, false, true), 50.0f, 1e-4f);
}

// --- Worth and trade -------------------------------------------------------

DP_TEST(survivor_worth_follows_need)
{
    Wants none;
    Wants hungry;
    hungry.hungry = true;
    Wants hurt;
    hurt.hurt = true;
    DP_CHECK_NEAR(worth(10.0f, Kind::Food, none), 10.0f, 1e-4f);
    DP_CHECK_NEAR(worth(10.0f, Kind::Food, hungry), 20.0f, 1e-4f);
    DP_CHECK_NEAR(worth(30.0f, Kind::Medicine, hurt), 90.0f, 1e-4f);
    DP_CHECK_NEAR(worth(30.0f, Kind::Medicine, hungry), 30.0f, 1e-4f);
    DP_CHECK_NEAR(worth(8.0f, Kind::Drink, hungry), 8.0f, 1e-4f);
}

DP_TEST(survivor_hungry_one_takes_a_can_for_two_bandages)
{
    ItemDatabase db;
    DP_CHECK(db.loadFromFile(std::string(DEAD_PIXEL_ASSETS_DIR) + "/data/items.json"));
    const ItemDef* can = db.findByKey("canned_food");
    const ItemDef* bandage = db.findByKey("bandage");
    DP_CHECK(can != nullptr && bandage != nullptr);
    if (can == nullptr || bandage == nullptr) return;
    DP_CHECK_NEAR(can->value, 10.0f, 1e-4f);
    DP_CHECK_NEAR(bandage->value, 8.0f, 1e-4f);
    Wants hungry;
    hungry.hungry = true;
    const float given = worth(can->value, Kind::Food, hungry);
    const float taken = 2.0f * worth(bandage->value, Kind::Medicine, hungry);
    DP_CHECK(verdict(given, taken) == Verdict::Fair);
    // Not hungry, the can is not enough.
    DP_CHECK(verdict(worth(can->value, Kind::Food, Wants{}), taken) == Verdict::WantMore);
}

DP_TEST(survivor_trade_verdicts)
{
    DP_CHECK(verdict(0.0f, 10.0f) == Verdict::WantMore);
    DP_CHECK(verdict(8.0f, 10.0f) == Verdict::WantMore);
    DP_CHECK(verdict(10.0f, 10.0f) == Verdict::Fair);
    DP_CHECK(verdict(15.0f, 10.0f) == Verdict::Generous);
    DP_CHECK(verdict(5.0f, 0.0f) == Verdict::Generous);
    DP_CHECK_NEAR(verdictTrust(Verdict::Fair), 5.0f, 1e-4f);
    DP_CHECK_NEAR(verdictTrust(Verdict::Generous), 10.0f, 1e-4f);
    DP_CHECK(willTrade(Attitude::Open, 0.0f));
    DP_CHECK(!willTrade(Attitude::Wary, 5.0f));
    DP_CHECK(willTrade(Attitude::Wary, 10.0f));
    DP_CHECK(!willTrade(Attitude::Hostile, 90.0f));
}

DP_TEST(survivor_keeps_two_days_and_a_weapon_back_from_trade)
{
    std::vector<StockLine> stock;
    stock.push_back(StockLine{0, 4, Kind::Food, 30.0f, 0.0f, 0.0f});    // 4 cans: keeps 2
    stock.push_back(StockLine{3, 4, Kind::Drink, 0.0f, 40.0f, 0.0f});   // 4 bottles: keeps 3
    stock.push_back(StockLine{5, 1, Kind::Weapon, 0.0f, 0.0f, 1.0f});   // a knife: kept
    stock.push_back(StockLine{4, 2, Kind::Medicine, 0.0f, 0.0f, 0.0f}); // bandages: spare
    const std::vector<int> spare = spareCounts(stock);
    DP_CHECK_EQ(spare[0], 2);
    DP_CHECK_EQ(spare[1], 1);
    DP_CHECK_EQ(spare[2], 0);
    DP_CHECK_EQ(spare[3], 2);
    DP_CHECK(stockDays(stock) > 3.0f);
}

DP_TEST(survivor_lines_are_stable_by_name_and_day)
{
    const std::size_t a = lineIndex("Ana", 3, 1, 8);
    DP_CHECK_EQ(a, lineIndex("Ana", 3, 1, 8));
    DP_CHECK(a < 8);
    std::set<std::size_t> seen;
    for (int day = 1; day <= 20; ++day) seen.insert(lineIndex("Ana", day, 1, 8));
    DP_CHECK(seen.size() > 3);
}

DP_TEST(survivor_lines_file_has_eight_per_situation)
{
    std::ifstream in(std::string(DEAD_PIXEL_ASSETS_DIR) + "/data/survivor_lines.json");
    DP_CHECK(in.good());
    std::stringstream text;
    text << in.rdbuf();
    const nlohmann::json json = nlohmann::json::parse(text.str(), nullptr, false);
    DP_CHECK(!json.is_discarded());
    for (const char* key : {"hello_open", "hello_wary", "hello_hostile", "warn", "thank", "refuse", "hurt", "rob", "goodbye"})
    {
        DP_CHECK(json.contains(key) && json[key].is_array() && json[key].size() == 8);
        if (!json.contains(key)) continue;
        for (const auto& line : json[key])
        {
            DP_CHECK(line.get<std::string>().find('!') == std::string::npos);
        }
    }
    DP_CHECK(json["names"].size() >= 20);
}

// --- The cast --------------------------------------------------------------

DP_TEST(survivor_cast_has_twelve_with_loners_pairs_and_the_garage)
{
    std::vector<HomeChoice> garages(1);
    garages[0].building = 99;
    garages[0].garage = true;
    const auto cast = castWorld(1234u, false, someHouses(20), garages, someNames());
    DP_CHECK_EQ(cast.size(), static_cast<std::size_t>(kSurvivorsPerWorld));
    int loners = 0, pairs = 0, garage = 0, stayers = 0;
    std::set<std::string> names;
    for (const CastMember& m : cast)
    {
        names.insert(m.name);
        if (m.role == Role::Loner) ++loners;
        if (m.role == Role::Pair) ++pairs;
        if (m.stayer) ++stayers;
        if (m.group == 1)
        {
            ++garage;
            DP_CHECK_EQ(m.home, 20); // the garage, after the houses
            DP_CHECK(attitude(m.p, 0.0f, 0.0f, false) == Attitude::Hostile);
        }
    }
    DP_CHECK_EQ(loners, 6);
    DP_CHECK_EQ(pairs, 4);
    DP_CHECK_EQ(garage, 2);
    DP_CHECK_EQ(stayers, 2);
    DP_CHECK_EQ(names.size(), cast.size());
    // Pairs share a house and point at each other.
    for (std::size_t i = 0; i < cast.size(); ++i)
    {
        if (cast[i].role != Role::Pair) continue;
        const CastMember& other = cast[static_cast<std::size_t>(cast[i].partner)];
        DP_CHECK_EQ(other.home, cast[i].home);
        DP_CHECK_EQ(other.partner, static_cast<int>(i));
    }
    // Loners each have their own house.
    std::set<int> homes;
    for (const CastMember& m : cast)
    {
        if (m.role == Role::Loner) homes.insert(m.home);
    }
    DP_CHECK_EQ(homes.size(), static_cast<std::size_t>(6));
}

DP_TEST(survivor_cast_is_the_same_for_one_seed_and_twenty_for_infected_society)
{
    const auto a = castWorld(77u, false, someHouses(20), {}, someNames());
    const auto b = castWorld(77u, false, someHouses(20), {}, someNames());
    DP_CHECK_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
    {
        DP_CHECK_EQ(a[i].name, b[i].name);
        DP_CHECK_EQ(a[i].home, b[i].home);
        DP_CHECK_NEAR(a[i].p.aggression, b[i].p.aggression, 1e-6f);
    }
    const auto society = castWorld(77u, true, someHouses(24), {}, someNames());
    DP_CHECK_EQ(society.size(), static_cast<std::size_t>(kSurvivorsInfectedSociety));
    int groupOfFive = 0;
    for (const CastMember& m : society) groupOfFive += m.group == 2 ? 1 : 0;
    DP_CHECK_EQ(groupOfFive, 5);
}

DP_TEST(survivor_cast_keeps_the_garage_away_from_the_start)
{
    // Two garages, the first by the start: the hostile two take the other.
    std::vector<HomeChoice> garages(2);
    garages[0].building = 98;
    garages[0].garage = true;
    garages[0].nearStart = true;
    garages[1].building = 99;
    garages[1].garage = true;
    for (std::uint32_t seed = 1; seed <= 20; ++seed)
    {
        for (const CastMember& m : castWorld(seed, false, someHouses(20), garages, someNames()))
        {
            if (m.group == 1) DP_CHECK_EQ(m.home, 21);
        }
    }
    // Only a garage by the start: a house away from it instead.
    std::vector<HomeChoice> houses = someHouses(20);
    for (std::size_t i = 0; i < houses.size(); i += 2) houses[i].nearStart = true;
    std::vector<HomeChoice> near(1);
    near[0].building = 99;
    near[0].garage = true;
    near[0].nearStart = true;
    for (std::uint32_t seed = 1; seed <= 20; ++seed)
    {
        for (const CastMember& m : castWorld(seed, false, houses, near, someNames()))
        {
            if (m.group != 1) continue;
            DP_CHECK(m.home >= 0 && m.home < 20);
            if (m.home >= 0 && m.home < 20) DP_CHECK(!houses[static_cast<std::size_t>(m.home)].nearStart);
        }
    }
}

// --- Between lives ---------------------------------------------------------

namespace
{
void sampleTown(std::vector<LifeSurvivor>& people, std::vector<LifeContainer>& boxes)
{
    people.clear();
    boxes.clear();
    for (int i = 0; i < 6; ++i)
    {
        LifeSurvivor s;
        s.id = i + 1;
        s.home = glm::ivec2(10 + i * 15, 10);
        s.homeBuilding = i + 1;
        s.competence = 0.3f + 0.1f * static_cast<float>(i);
        s.danger = 1.0f;
        s.food = 2;
        s.drink = 2;
        people.push_back(s);
    }
    for (int i = 0; i < 40; ++i)
    {
        LifeContainer c;
        c.tile = glm::ivec2(5 + i * 3, 14);
        c.building = 20 + i / 4;
        c.food = i % 3;
        c.drink = (i + 1) % 2;
        boxes.push_back(c);
    }
}
} // namespace

DP_TEST(survivor_between_lives_is_the_same_twice_for_one_seed)
{
    std::vector<LifeSurvivor> p1, p2;
    std::vector<LifeContainer> c1, c2;
    sampleTown(p1, c1);
    sampleTown(p2, c2);
    const BetweenLivesResult a = advanceBetweenLives(p1, c1, 10, 4242u);
    const BetweenLivesResult b = advanceBetweenLives(p2, c2, 10, 4242u);
    DP_CHECK_EQ(a.fingerprint, b.fingerprint);
    DP_CHECK_EQ(a.loots.size(), b.loots.size());
    DP_CHECK_EQ(a.deaths.size(), b.deaths.size());
    DP_CHECK(!a.loots.empty());
}

DP_TEST(survivor_between_lives_stock_falls_and_containers_near_home_empty)
{
    std::vector<LifeSurvivor> people;
    std::vector<LifeContainer> boxes;
    sampleTown(people, boxes);
    people.resize(1);
    people[0].food = 20;
    people[0].drink = 20;
    people[0].danger = 0.0f; // nothing kills them
    (void)advanceBetweenLives(people, boxes, 5, 1u);
    DP_CHECK_EQ(people[0].food, 15);
    DP_CHECK_EQ(people[0].drink, 15);
    // Plenty: nothing searched.
    for (const LifeContainer& c : boxes) DP_CHECK(!c.searched);

    sampleTown(people, boxes);
    people.resize(1);
    people[0].danger = 0.0f;
    const BetweenLivesResult r = advanceBetweenLives(people, boxes, 3, 1u);
    DP_CHECK(!r.loots.empty());
    for (const LootEvent& loot : r.loots)
    {
        const LifeContainer& c = boxes[static_cast<std::size_t>(loot.container)];
        DP_CHECK(c.searched);
        DP_CHECK_EQ(c.food, 0);
        const glm::vec2 d(c.tile - people[0].home);
        DP_CHECK(d.x * d.x + d.y * d.y <= kScavengeRadiusTiles * kScavengeRadiusTiles);
    }
}

DP_TEST(survivor_between_lives_leaves_protected_homes_alone_and_starves_the_empty)
{
    std::vector<LifeSurvivor> people(1);
    people[0].id = 1;
    people[0].home = glm::ivec2(0, 0);
    people[0].danger = 0.0f;
    people[0].competence = 0.0f; // forages badly
    people[0].drink = 30;        // water is never what kills them
    std::vector<LifeContainer> boxes(2);
    boxes[0].tile = glm::ivec2(3, 0);
    boxes[0].protectedHome = true;
    boxes[0].food = 5;
    boxes[0].drink = 5;
    boxes[1].tile = glm::ivec2(200, 0); // too far
    boxes[1].food = 5;
    const BetweenLivesResult r = advanceBetweenLives(people, boxes, 40, 9u);
    DP_CHECK(r.loots.empty());
    DP_CHECK(!people[0].alive);
    DP_CHECK_EQ(r.deaths.size(), static_cast<std::size_t>(1));
    DP_CHECK(r.deaths[0].starved);
    DP_CHECK(r.deaths[0].day > kStarveDays); // not before six days with nothing
}

DP_TEST(survivor_between_lives_forage_keeps_most_alive)
{
    // With nothing in the house but foraging, few starve in ten days.
    std::vector<LifeSurvivor> people;
    for (int i = 0; i < 200; ++i)
    {
        LifeSurvivor s;
        s.id = i;
        s.competence = 0.5f;
        s.danger = 0.0f;
        people.push_back(s);
    }
    std::vector<LifeContainer> none;
    const BetweenLivesResult r = advanceBetweenLives(people, none, 10, 77u);
    DP_CHECK_NEAR(forageChance(0.5f), 0.4f, 1e-6f);
    // A run of six misses at 0.6 each: about 4.7 % a start; well under a quarter starve.
    DP_CHECK(r.deaths.size() < 50);
}

DP_TEST(survivor_death_chance_per_day)
{
    DP_CHECK_NEAR(dailyDeathChance(1.0f, 1.0f), 0.015f, 1e-6f);
    DP_CHECK_NEAR(dailyDeathChance(0.0f, 1.0f), 0.03f, 1e-6f);
    DP_CHECK_NEAR(dailyDeathChance(0.5f, 2.0f), 0.045f, 1e-6f);
    DP_CHECK_NEAR(dangerOf(0.0f), 0.5f, 1e-6f);
    DP_CHECK_NEAR(dangerOf(100.0f), 2.0f, 1e-6f);
    // Over many well-stocked survivors and days, deaths come near the chance.
    std::vector<LifeSurvivor> people;
    for (int i = 0; i < 400; ++i)
    {
        LifeSurvivor s;
        s.id = i;
        s.competence = 0.5f;
        s.danger = 1.0f;
        s.food = 100;
        s.drink = 100;
        people.push_back(s);
    }
    std::vector<LifeContainer> none;
    const BetweenLivesResult r = advanceBetweenLives(people, none, 10, 31u);
    // 1 - (1 - 0.0225)^10 = 0.204: about 82 of 400.
    DP_CHECK(r.deaths.size() > 55 && r.deaths.size() < 110);
}

DP_TEST(survivor_far_hour_goes_out_only_by_day_when_low)
{
    DP_CHECK(!farHour(false, 0.0f, false, 1.0f, 1u, 1, 10).scavenge);
    DP_CHECK(!farHour(true, 5.0f, false, 1.0f, 1u, 1, 10).scavenge);
    DP_CHECK(!farHour(true, 0.0f, true, 1.0f, 1u, 1, 10).scavenge);
    DP_CHECK(farHour(true, 0.0f, false, 1.0f, 1u, 1, 10).scavenge);
    int wounds = 0;
    for (int h = 0; h < 2000; ++h) wounds += farHour(true, 0.0f, false, 1.0f, 5u, 3, h).wound ? 1 : 0;
    DP_CHECK(wounds > 15 && wounds < 70); // about 2 %
}

DP_TEST(survivor_standing_words)
{
    DP_CHECK_EQ(std::string(standingWord(50.0f, Attitude::Open)), std::string("Owes you."));
    DP_CHECK_EQ(std::string(standingWord(0.0f, Attitude::Wary)), std::string("Wary."));
    DP_CHECK_EQ(std::string(standingWord(0.0f, Attitude::Hostile)), std::string("Do not go back."));
    DP_CHECK_EQ(std::string(standingWord(12.0f, Attitude::Wary)), std::string("Trades."));
}
