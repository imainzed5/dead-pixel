// The service gate (phase 9): what the box says is missing, the last
// stand's schedule, who did what, and where the gate stands in real towns:
// on a road end, reachable from the start, the Sergeant's loop walkable.
#include "TestFramework.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "map/MapGenerator.h"
#include "world/BuildingKinds.h"
#include "world/Gate.h"

namespace
{
LayoutData stampedTown(std::uint32_t seed, int district = 0)
{
    MapGenerator generator;
    LayoutData layout = generator.generateLayout(seed);
    std::string why;
    const bool ok = gate::stamp(layout, district, &why);
    if (!ok) std::printf("  seed %u: %s\n", seed, why.c_str());
    return layout;
}

// Tiles reachable on foot from `from` (no collision), the gate shut.
std::vector<std::uint8_t> reachable(const LayoutData& layout, const glm::ivec2& from)
{
    const auto at = [&](const glm::ivec2& t) { return static_cast<std::size_t>(t.y * layout.width + t.x); };
    std::vector<std::uint8_t> seen(static_cast<std::size_t>(layout.width * layout.height), 0);
    const auto blocked = [&](const glm::ivec2& t)
    {
        if (t.x < 0 || t.y < 0 || t.x >= layout.width || t.y >= layout.height) return true;
        if (layout.collision[at(t)] != 0) return true;
        if (t == layout.gate.generator) return true;
        return std::find(layout.gate.gate.begin(), layout.gate.gate.end(), t) != layout.gate.gate.end();
    };
    if (blocked(from)) return seen;
    std::vector<glm::ivec2> stack{from};
    seen[at(from)] = 1;
    const glm::ivec2 dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!stack.empty())
    {
        const glm::ivec2 t = stack.back();
        stack.pop_back();
        for (const glm::ivec2& d : dirs)
        {
            const glm::ivec2 n = t + d;
            if (blocked(n) || seen[at(n)] != 0) continue;
            seen[at(n)] = 1;
            stack.push_back(n);
        }
    }
    return seen;
}

bool nextToReached(const LayoutData& layout, const std::vector<std::uint8_t>& seen, const glm::ivec2& tile)
{
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            const glm::ivec2 n = tile + glm::ivec2(dx, dy);
            if (n.x < 0 || n.y < 0 || n.x >= layout.width || n.y >= layout.height) continue;
            if (seen[static_cast<std::size_t>(n.y * layout.width + n.x)] != 0) return true;
        }
    }
    return false;
}
} // namespace

DP_TEST(gate_box_says_what_is_missing)
{
    gate::State s;
    DP_CHECK_EQ(gate::missingText(s, false), std::string("Needs: fuse, fuel 0 of 4, generator, key."));
    s.fuseFitted = true;
    s.fuel = 1;
    DP_CHECK_EQ(gate::missingText(s, true), std::string("Needs: fuel 1 of 4, generator."));
    s.freed = true;
    DP_CHECK(!gate::ready(s));
    DP_CHECK(gate::ready(s, 1)); // a restart needs any fuel
    s.fuel = 4;
    DP_CHECK(gate::ready(s));
    DP_CHECK_EQ(gate::missingText(s, false), std::string("Needs: key."));
    DP_CHECK_EQ(gate::missingText(s, true), std::string());
    s.fuel = 0;
    DP_CHECK(!gate::ready(s, 1));
}

DP_TEST(gate_stand_schedule)
{
    DP_CHECK_EQ(gate::waveSize(0), 3);
    DP_CHECK_EQ(gate::waveSize(1), 4);
    DP_CHECK_EQ(gate::waveSize(5), 8);
    DP_CHECK_EQ(gate::baseCrowd(0), 4);
    DP_CHECK_EQ(gate::baseCrowd(60), 9);
    DP_CHECK_EQ(gate::baseCrowd(74), 10);
    DP_CHECK_EQ(gate::baseCrowd(75), 20); // overwhelmed: doubled
    DP_CHECK_EQ(gate::stage(0.0f), 0);
    DP_CHECK_EQ(gate::stage(60.0f), 1);
    DP_CHECK_EQ(gate::stage(130.0f), 2);
    DP_CHECK_EQ(gate::stage(180.0f), 3);
    DP_CHECK_EQ(gate::openGateExtra(10), 5);
    DP_CHECK_EQ(gate::openGateExtra(0), 0);

    gate::Stand stand;
    stand.elapsed = 59.0f;
    DP_CHECK_EQ(gate::wavesDue(stand), 2);
    DP_CHECK_EQ(gate::wavesDue(stand), 0);
    stand.elapsed = 61.0f;
    DP_CHECK_EQ(gate::wavesDue(stand), 1);
}

DP_TEST(gate_opens_in_three_minutes_on_four_cans)
{
    gate::State s;
    s.fuseFitted = true;
    s.freed = true;
    s.fuel = 4;
    gate::Stand stand;
    gate::start(stand);
    DP_CHECK(stand.running && stand.started);
    DP_CHECK_NEAR(stand.grind, 0.0f, 0.001); // the first start does not grind
    int burned = 0;
    for (int i = 0; i < 60 * 179; ++i) burned += gate::run(stand, s, 1.0f / 60.0f);
    DP_CHECK(stand.running && !s.open);
    DP_CHECK_EQ(gate::stage(stand.opened), 2);
    for (int i = 0; i < 120 && stand.running; ++i) burned += gate::run(stand, s, 1.0f / 60.0f);
    DP_CHECK(s.open);
    DP_CHECK(!stand.running);
    // A can a minute: two burned by 2 minutes, the third right at the end.
    DP_CHECK(burned >= 2 && burned <= 3);
    DP_CHECK_EQ(s.fuel, 4 - burned);
    DP_CHECK(s.fuel >= 1);
}

DP_TEST(gate_restart_grinds_and_a_dry_tank_stops_it)
{
    gate::State s;
    s.fuseFitted = true;
    s.freed = true;
    s.fuel = 1;
    gate::Stand stand;
    gate::start(stand);
    for (int i = 0; i < 70; ++i) gate::run(stand, s, 1.0f);
    DP_CHECK(!stand.running); // dry at 60 s
    DP_CHECK_NEAR(stand.opened, 60.0f, 0.01);
    DP_CHECK_EQ(s.fuel, 0);
    DP_CHECK(!s.open);

    // Two more cans: the restart grinds 30 s before it moves again.
    s.fuel = 2;
    DP_CHECK(gate::ready(s, 1));
    gate::start(stand);
    DP_CHECK_NEAR(stand.grind, gate::kGrindSeconds, 0.001);
    for (int i = 0; i < 30; ++i) gate::run(stand, s, 1.0f);
    DP_CHECK_NEAR(stand.opened, 60.0f, 0.01);
    for (int i = 0; i < 10; ++i) gate::run(stand, s, 1.0f);
    DP_CHECK_NEAR(stand.opened, 70.0f, 0.01);
}

DP_TEST(gate_contributors_in_the_order_they_first_did_something)
{
    gate::State s;
    gate::credit(s, "Ann Hale", "fuse", 1);
    gate::credit(s, "Ann Hale", "can", 1);
    gate::credit(s, "Ann Hale", "can", 1);
    gate::credit(s, "Bo Reyes", "repair", 2);
    gate::credit(s, "Bo Reyes", "freed", 2);
    gate::credit(s, "Cy Dunn", "sergeant", 3);
    gate::credit(s, "Ann Hale", "held", 1);
    const std::vector<std::string> names = gate::contributorNames(s);
    DP_CHECK(names == std::vector<std::string>({"Ann Hale", "Bo Reyes", "Cy Dunn"}));
    const std::vector<std::string> lines = gate::contributorLines(s);
    DP_CHECK_EQ(lines.size(), std::size_t{3});
    if (lines.size() == 3)
    {
        DP_CHECK(lines[0].rfind("Ann Hale:", 0) == 0);
        DP_CHECK(lines[0].find("2 cans") != std::string::npos);
        DP_CHECK(lines[1].find("worked on the generator") == std::string::npos); // freeing it says it
        for (const std::string& line : lines)
        {
            DP_CHECK(line.back() == '.');
            DP_CHECK(line.find('!') == std::string::npos);
        }
    }
}

DP_TEST(gate_stands_on_a_road_end_reachable_from_the_start)
{
    for (const std::uint32_t seed : {1234u, 42u, 777u})
    {
        const LayoutData layout = stampedTown(seed);
        const LayoutData::GateSite& site = layout.gate;
        DP_CHECK(site.placed);
        if (!site.placed) continue;
        DP_CHECK_EQ(site.gate.size(), std::size_t{3});
        // The three tiles on the town's edge, side by side.
        for (const glm::ivec2& t : site.gate)
        {
            const bool edge = t.x == 0 || t.y == 0 || t.x == layout.width - 1 || t.y == layout.height - 1;
            DP_CHECK(edge);
        }
        // The hut: the last building, a guard hut.
        DP_CHECK_EQ(site.hut, static_cast<int>(layout.buildings.size()));
        DP_CHECK(buildingkinds::kindOf(layout.buildings.back()) == BuildingKind::GuardHut);
        // From the start: the box, the generator and the way through.
        const std::vector<std::uint8_t> seen = reachable(layout, layout.playerStart);
        const glm::ivec2 inside = gate::insideGate(layout);
        const bool insideOk = seen[static_cast<std::size_t>(inside.y * layout.width + inside.x)] != 0;
        const bool boxOk = nextToReached(layout, seen, site.box);
        const bool generatorOk = nextToReached(layout, seen, site.generator);
        if (!insideOk || !boxOk || !generatorOk)
        {
            std::printf("  seed %u: inside %d box %d generator %d\n", seed, insideOk, boxOk, generatorOk);
        }
        DP_CHECK(insideOk && boxOk && generatorOk);
        // Each gate tile is next to the walkable road inside.
        for (const glm::ivec2& t : site.gate) DP_CHECK(nextToReached(layout, seen, t));
        // The yard covers the hut.
        DP_CHECK(gate::inYard(site, site.box));
        // The same town stamps the same.
        const LayoutData again = stampedTown(seed);
        DP_CHECK(again.gate.gate == site.gate && again.gate.box == site.box);
    }
}

DP_TEST(gate_sergeant_walks_a_loop_of_five_reachable_road_tiles)
{
    for (const std::uint32_t seed : {1234u, 42u, 777u})
    {
        const LayoutData layout = stampedTown(seed);
        const int building = gate::sergeantBuilding(layout, seed);
        DP_CHECK(building > 0 && building < static_cast<int>(layout.buildings.size()));
        DP_CHECK(building != layout.gate.hut);
        const std::vector<glm::ivec2> loop = gate::sergeantLoop(layout, building);
        DP_CHECK_EQ(loop.size(), static_cast<std::size_t>(gate::kLoopPoints));
        const std::vector<std::uint8_t> seen = reachable(layout, layout.playerStart);
        for (const glm::ivec2& t : loop)
        {
            const bool ok = seen[static_cast<std::size_t>(t.y * layout.width + t.x)] != 0;
            if (!ok) std::printf("  seed %u: loop point %d,%d not reachable\n", seed, t.x, t.y);
            DP_CHECK(ok);
        }
        DP_CHECK_EQ(gate::sergeantBuilding(layout, seed), building);
    }
}

DP_TEST(gate_goes_where_the_old_objective_stood)
{
    MapGenerator generator;
    const LayoutData base = generator.generateLayout(1234u);
    const int farthest = gate::farthestDistrict(base);
    DP_CHECK(farthest > 0);
    // Another district than the farthest: the gate goes there.
    int other = 0;
    for (const LayoutData::DistrictDef& d : base.districts)
    {
        if (d.id > 0 && d.id != farthest) other = d.id;
    }
    DP_CHECK(other > 0);
    LayoutData a = base;
    DP_CHECK(gate::stamp(a, other));
    DP_CHECK_EQ(a.gate.district, other);
    LayoutData b = base;
    DP_CHECK(gate::stamp(b, 0));
    DP_CHECK_EQ(b.gate.district, farthest);
    // Stamped once: a second stamp changes nothing.
    const std::size_t buildings = a.buildings.size();
    DP_CHECK(gate::stamp(a, farthest));
    DP_CHECK_EQ(a.buildings.size(), buildings);
    DP_CHECK_EQ(a.gate.district, other);
}
