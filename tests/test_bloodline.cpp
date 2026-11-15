// What a bloodline learns (phase 10): skill echoes from the graves, the
// traits earned by what happened (at most two, most recent first), the
// cemetery's plot and rows, and when staying for good is offered.
#include "TestFramework.h"

#include <string>
#include <vector>

#include "legacy/Cemetery.h"
#include "legacy/LegacyRules.h"
#include "map/MapGenerator.h"
#include "map/TileIds.h"
#include "world/Gate.h"

namespace
{
GraveRecord graveOf(const char* name, int run, std::vector<std::pair<int, int>> skills)
{
    GraveRecord g;
    g.name = name;
    g.runIndex = run;
    g.cause = "Killed by zombies on Day 3";
    g.skills = std::move(skills);
    return g;
}

CharacterHistoryRecord lifeOf(const char* name, int run, const char* cause, bool read = false)
{
    CharacterHistoryRecord h;
    h.name = name;
    h.runIndex = run;
    h.cause = cause;
    h.journalRead = read;
    return h;
}

constexpr int kCarpentry = static_cast<int>(Skill::Carpentry);
} // namespace

DP_TEST(bloodline_four_carpenters_read_give_the_fifth_carpentry_two)
{
    legacy::LegacyState state;
    for (int run = 1; run <= 4; ++run)
    {
        const std::string name = "C" + std::to_string(run);
        state.graves.push_back(graveOf(name.c_str(), run, {{kCarpentry, 3}}));
        state.history.push_back(lifeOf(name.c_str(), run, "Killed by zombies", true));
    }
    const auto echoes = legacy::skillEchoes(state);
    DP_CHECK_EQ(echoes[static_cast<std::size_t>(kCarpentry)], 2);
    DP_CHECK_EQ(echoes[static_cast<std::size_t>(Skill::Fighting)], 0);
}

DP_TEST(bloodline_unread_journals_count_half)
{
    legacy::LegacyState state;
    // Two Carpentry 3 graves, nobody read them: 3 halves... (3+3)/2 = 3, /3 = 1.
    state.graves.push_back(graveOf("A", 1, {{kCarpentry, 3}}));
    state.graves.push_back(graveOf("B", 2, {{kCarpentry, 3}}));
    state.history.push_back(lifeOf("A", 1, "x"));
    state.history.push_back(lifeOf("B", 2, "x"));
    { const auto e = legacy::skillEchoes(state); DP_CHECK_EQ(e[static_cast<std::size_t>(kCarpentry)], 1); }
    // One Carpentry 2, unread: one, not enough.
    legacy::LegacyState one;
    one.graves.push_back(graveOf("A", 1, {{kCarpentry, 2}}));
    { const auto e = legacy::skillEchoes(one); DP_CHECK_EQ(e[static_cast<std::size_t>(kCarpentry)], 0); }
    // A marker set in the cemetery counts as read.
    one.graves[0].buried = true;
    one.graves[0].skills = {{kCarpentry, 3}};
    { const auto e = legacy::skillEchoes(one); DP_CHECK_EQ(e[static_cast<std::size_t>(kCarpentry)], 1); }
    // A companion's grave teaches nothing.
    legacy::LegacyState companion;
    GraveRecord c = graveOf("Mo", 1, {{kCarpentry, 6}});
    c.travelledWith = "Ann";
    companion.graves.push_back(c);
    { const auto e = legacy::skillEchoes(companion); DP_CHECK_EQ(e[static_cast<std::size_t>(kCarpentry)], 0); }
    DP_CHECK_EQ(legacy::echoLine("Carpentry", 1), std::string("Carpentry runs in the family (1)."));
}

DP_TEST(bloodline_traits_are_earned_by_what_happened)
{
    legacy::LegacyState none;
    DP_CHECK(legacy::bloodlineFor(none).empty());

    // Three starved or thirsty lives: Scavenger, earned at the third.
    legacy::LegacyState hungry;
    hungry.history = {lifeOf("A", 1, "Starved on Day 9"), lifeOf("B", 2, "Dehydrated on Day 4")};
    DP_CHECK(legacy::bloodlineFor(hungry).empty());
    hungry.history.push_back(lifeOf("C", 3, "Starved on Day 7"));
    DP_CHECK(legacy::bloodlineFor(hungry) == std::vector<BloodlineTrait>{BloodlineTrait::Scavenger});

    // A death by fire, or thirty tiles burned in a life: Firewise.
    legacy::LegacyState fire;
    fire.history = {lifeOf("A", 1, "Burned alive on Day 2")};
    DP_CHECK(legacy::bloodlineFor(fire) == std::vector<BloodlineTrait>{BloodlineTrait::Firewise});
    legacy::LegacyState burner;
    burner.history = {lifeOf("A", 1, "Bled out")};
    burner.history[0].tilesBurned = 30;
    DP_CHECK(legacy::bloodlineFor(burner) == std::vector<BloodlineTrait>{BloodlineTrait::Firewise});

    // Four graves of the line: Vigilant. Three risen laid to rest: Gravedigger.
    legacy::LegacyState graves;
    for (int run = 1; run <= 4; ++run) graves.graves.push_back(graveOf("G", run, {}));
    DP_CHECK(legacy::bloodlineFor(graves) == std::vector<BloodlineTrait>{BloodlineTrait::Vigilant});
    for (int i = 0; i < 3; ++i)
    {
        graves.graves[static_cast<std::size_t>(i)].laidToRest = true;
        graves.graves[static_cast<std::size_t>(i)].restRun = 6 + i;
    }
    const auto line = legacy::bloodlineFor(graves);
    DP_CHECK_EQ(line.size(), std::size_t{2});
    DP_CHECK(line[0] == BloodlineTrait::Gravedigger); // earned in life 8, after Vigilant's 4
    DP_CHECK(line[1] == BloodlineTrait::Vigilant);
}

DP_TEST(bloodline_keeps_two_most_recent_first)
{
    legacy::LegacyState state;
    CharacterHistoryRecord retired = lifeOf("A", 1, "Psychological retirement");
    retired.retired = true;
    state.history.push_back(retired);
    state.history.push_back(lifeOf("B", 2, "Burned alive"));
    CharacterHistoryRecord out = lifeOf("C", 3, "");
    out.escaped = true;
    state.history.push_back(out);
    const auto line = legacy::bloodlineFor(state);
    DP_CHECK_EQ(line.size(), std::size_t{2});
    DP_CHECK(line[0] == BloodlineTrait::Wayfinder);
    DP_CHECK(line[1] == BloodlineTrait::Firewise);
}

DP_TEST(bloodline_a_chosen_stay_hardens_only_at_peace)
{
    legacy::LegacyState troubled;
    CharacterHistoryRecord stayed = lifeOf("A", 1, "Stayed");
    stayed.retired = true;
    stayed.chosen = true;
    stayed.depression = 70.0f;
    troubled.history.push_back(stayed);
    DP_CHECK(legacy::bloodlineFor(troubled).empty());
    troubled.history[0].depression = 30.0f;
    DP_CHECK(legacy::bloodlineFor(troubled) == std::vector<BloodlineTrait>{BloodlineTrait::Hardened});
    // An older save's retirement with no history still hardens.
    legacy::LegacyState old;
    old.retirements.push_back(RetirementRecord{"Old", 0.0f, 0.0f, 4, 2});
    DP_CHECK(legacy::bloodlineFor(old) == std::vector<BloodlineTrait>{BloodlineTrait::Hardened});
}

DP_TEST(bloodline_stay_for_good_is_offered_from_day_ten_at_a_base_when_well)
{
    legacy::StayCheck check;
    check.day = 10;
    check.atBase = true;
    check.hunger = 60.0f;
    check.thirst = 70.0f;
    check.health = 80.0f;
    DP_CHECK(legacy::stayBlocker(check).empty());
    legacy::StayCheck early = check;
    early.day = 9;
    DP_CHECK(!legacy::stayBlocker(early).empty());
    legacy::StayCheck away = check;
    away.atBase = false;
    DP_CHECK(!legacy::stayBlocker(away).empty());
    legacy::StayCheck hungry = check;
    hungry.hunger = 50.0f;
    DP_CHECK(!legacy::stayBlocker(hungry).empty());
    legacy::StayCheck sick = check;
    sick.infected = true;
    DP_CHECK(!legacy::stayBlocker(sick).empty());
}

DP_TEST(bloodline_cemetery_rows_fill_from_the_back_then_the_gaps)
{
    const glm::ivec2 lo(10, 20);
    const glm::ivec2 hi(18, 27); // 7 by 6 inside
    DP_CHECK_EQ(legacy::cemeterySlots(lo, hi), 42);
    DP_CHECK(legacy::cemeterySlot(lo, hi, 0) == glm::ivec2(11, 21));
    DP_CHECK(legacy::cemeterySlot(lo, hi, 6) == glm::ivec2(17, 21));
    DP_CHECK(legacy::cemeterySlot(lo, hi, 7) == glm::ivec2(11, 23));
    DP_CHECK(legacy::cemeterySlot(lo, hi, 21) == glm::ivec2(11, 22)); // the gaps after the rows
    // Every slot is inside the fence and none repeats.
    std::vector<glm::ivec2> seen;
    for (int i = 0; i < 42; ++i)
    {
        const glm::ivec2 t = legacy::cemeterySlot(lo, hi, i);
        DP_CHECK(t.x > lo.x && t.x < hi.x && t.y > lo.y && t.y < hi.y);
        for (const glm::ivec2& s : seen) DP_CHECK(!(s == t));
        seen.push_back(t);
    }
}

DP_TEST(bloodline_cemetery_is_stamped_on_open_ground_near_the_middle)
{
    for (std::uint32_t seed : {1u, 7u, 1234u, 99u, 31337u})
    {
        MapGenerator generator;
        LayoutData layout = generator.generateLayout(seed);
        gate::stamp(layout, 0);
        std::string why;
        DP_CHECK(cemetery::stamp(layout, &why));
        const LayoutData::CemeterySite& site = layout.cemetery;
        DP_CHECK(site.placed);
        DP_CHECK_EQ(site.fenceMax.x - site.fenceMin.x + 1, cemetery::kWidth);
        DP_CHECK_EQ(site.fenceMax.y - site.fenceMin.y + 1, cemetery::kHeight);
        // Nothing built in it, nothing solid, grass throughout; one gap.
        for (int y = site.fenceMin.y; y <= site.fenceMax.y; ++y)
        {
            for (int x = site.fenceMin.x; x <= site.fenceMax.x; ++x)
            {
                const std::size_t i = static_cast<std::size_t>(y * layout.width + x);
                DP_CHECK_EQ(layout.collision[i], 0);
                DP_CHECK_EQ(layout.ground[i], TileGid::kGrass);
                for (const LayoutData::BuildingRect& b : layout.buildings)
                {
                    DP_CHECK(!(x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h));
                }
            }
        }
        DP_CHECK(!cemetery::isFence(site, site.opening));
        DP_CHECK_EQ(cemetery::fenceTiles(site).size(), std::size_t{2 * (cemetery::kWidth + cemetery::kHeight) - 4 - 1});
        // Not far from the middle of the town.
        const float dx = static_cast<float>(site.fenceMin.x + 4) - static_cast<float>(layout.width) * 0.5f;
        const float dy = static_cast<float>(site.fenceMin.y + 4) - static_cast<float>(layout.height) * 0.5f;
        DP_CHECK(dx * dx + dy * dy < 0.25f * static_cast<float>(layout.width * layout.width));
        // The same town, the same plot.
        MapGenerator again;
        LayoutData second = again.generateLayout(seed);
        gate::stamp(second, 0);
        DP_CHECK(cemetery::stamp(second));
        DP_CHECK(second.cemetery.fenceMin == site.fenceMin);
    }
}
