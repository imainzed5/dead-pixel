// The kinds of the dead and the threat modes (overhaul phase 4): shares by
// day and mode, the data file matching the built-in numbers, The Unknown's
// rules by seed.
#include "TestFramework.h"

#include <numeric>
#include <set>

#include "world/ZombieKinds.h"

using namespace zombiekinds;

namespace
{
float total(const std::array<float, kZombieKindCount>& s)
{
    return std::accumulate(s.begin(), s.end(), 0.0f);
}
float share(const std::array<float, kZombieKindCount>& s, ZombieKind k)
{
    return s[static_cast<std::size_t>(k)];
}
} // namespace

DP_TEST(zombie_kind_shares_on_day_one_follow_the_table)
{
    const Table table;
    const auto s = shares(table, table.mode(1), 1, 0.0f); // Stalker: plain shares
    DP_CHECK_NEAR(total(s), 1.0f, 1e-4f);
    DP_CHECK_NEAR(share(s, ZombieKind::Runner), 0.06f, 1e-4f);
    DP_CHECK_NEAR(share(s, ZombieKind::Brute), 0.06f, 1e-4f);
    DP_CHECK_NEAR(share(s, ZombieKind::Crawler), 0.10f, 1e-4f);
    DP_CHECK_NEAR(share(s, ZombieKind::Screamer), 0.04f, 1e-4f);
    DP_CHECK_NEAR(share(s, ZombieKind::Shambler), 0.74f, 1e-4f);
}

DP_TEST(zombie_kind_shares_grow_by_day_to_their_caps)
{
    const Table table;
    const ModeRules& stalker = table.mode(1);
    const auto day5 = shares(table, stalker, 5, 0.0f);
    DP_CHECK_NEAR(share(day5, ZombieKind::Runner), 0.06f + 0.015f * 4.0f, 1e-4f);
    const auto day40 = shares(table, stalker, 40, 0.0f);
    DP_CHECK_NEAR(share(day40, ZombieKind::Runner), 0.18f, 1e-4f);
    DP_CHECK_NEAR(share(day40, ZombieKind::Brute), 0.14f, 1e-4f);
    DP_CHECK_NEAR(share(day40, ZombieKind::Screamer), 0.08f, 1e-4f);
    DP_CHECK_NEAR(total(day40), 1.0f, 1e-4f);
}

DP_TEST(swarm_halves_runners_and_brutes)
{
    const Table table;
    const auto swarm = shares(table, table.mode(0), 1, 0.0f);
    DP_CHECK_NEAR(share(swarm, ZombieKind::Runner), 0.03f, 1e-4f);
    DP_CHECK_NEAR(share(swarm, ZombieKind::Brute), 0.03f, 1e-4f);
}

DP_TEST(mutating_starts_with_shamblers_only_and_worsens)
{
    const Table table;
    const ModeRules& mutating = table.mode(2);
    const auto day1 = shares(table, mutating, 1, 0.0f);
    DP_CHECK_NEAR(share(day1, ZombieKind::Shambler), 1.0f, 1e-4f);
    const auto day11 = shares(table, mutating, 11, 0.0f);
    DP_CHECK(share(day11, ZombieKind::Runner) > 0.15f);
    const auto day60 = shares(table, mutating, 60, 0.0f);
    DP_CHECK(share(day60, ZombieKind::Runner) <= 0.35f + 1e-4f);
    DP_CHECK_NEAR(total(day60), 1.0f, 1e-4f);
    DP_CHECK_NEAR(doorDamageScale(mutating, 9), 1.0f, 1e-6f);
    DP_CHECK_NEAR(doorDamageScale(mutating, 10), 2.0f, 1e-6f);
    DP_CHECK(!climbsWindows(mutating, 14));
    DP_CHECK(climbsWindows(mutating, 15));
    DP_CHECK(!climbsWindows(table.mode(0), 40));
}

DP_TEST(an_infected_district_doubles_runners_and_screamers)
{
    const Table table;
    const auto calm = shares(table, table.mode(1), 1, 20.0f);
    const auto turned = shares(table, table.mode(1), 1, 60.0f);
    DP_CHECK_NEAR(share(turned, ZombieKind::Runner), 2.0f * share(calm, ZombieKind::Runner), 1e-4f);
    DP_CHECK_NEAR(share(turned, ZombieKind::Screamer), 2.0f * share(calm, ZombieKind::Screamer), 1e-4f);
}

DP_TEST(pick_walks_the_shares_in_order)
{
    const std::array<float, kZombieKindCount> s{0.5f, 0.2f, 0.1f, 0.1f, 0.1f};
    // The rare kinds first, so a low roll is a rare one.
    DP_CHECK(pick(s, 0.05f) == ZombieKind::Screamer);
    DP_CHECK(pick(s, 0.15f) == ZombieKind::Crawler);
    DP_CHECK(pick(s, 0.25f) == ZombieKind::Brute);
    DP_CHECK(pick(s, 0.45f) == ZombieKind::Runner);
    DP_CHECK(pick(s, 0.55f) == ZombieKind::Shambler);
    DP_CHECK(pick(s, 0.999f) == ZombieKind::Shambler);
}

DP_TEST(zombies_json_matches_the_built_in_numbers)
{
    Table file;
    std::string error;
    DP_CHECK(file.load("assets/data/zombies.json", &error));
    const Table builtIn;
    for (int k = 0; k < kZombieKindCount; ++k)
    {
        const KindStats& a = file.stats(static_cast<ZombieKind>(k));
        const KindStats& b = builtIn.stats(static_cast<ZombieKind>(k));
        DP_CHECK_NEAR(a.share, b.share, 1e-6f);
        DP_CHECK_NEAR(a.hpMin, b.hpMin, 1e-6f);
        DP_CHECK_NEAR(a.hpMax, b.hpMax, 1e-6f);
        DP_CHECK_NEAR(a.speedMin, b.speedMin, 1e-6f);
        DP_CHECK_NEAR(a.damage, b.damage, 1e-6f);
        DP_CHECK_NEAR(a.sightTiles, b.sightTiles, 1e-6f);
        DP_CHECK_NEAR(a.doorDamage, b.doorDamage, 1e-6f);
    }
    for (int m = 0; m < 6; ++m)
    {
        DP_CHECK_NEAR(file.mode(m).population, builtIn.mode(m).population, 1e-6f);
        DP_CHECK_NEAR(file.mode(m).migration, builtIn.mode(m).migration, 1e-6f);
        DP_CHECK_EQ(file.mode(m).hordeFirstNight, builtIn.mode(m).hordeFirstNight);
    }
}

DP_TEST(rolled_health_and_speed_stay_in_the_kind_ranges)
{
    const Table table;
    const Rolled lo = roll(table, ZombieKind::Brute, 0.0f, 0.0f);
    const Rolled hi = roll(table, ZombieKind::Brute, 0.999f, 0.999f);
    DP_CHECK(lo.health >= 150.0f && hi.health <= 190.0f);
    DP_CHECK(lo.speed >= 0.57f * kBaseSpeed - 0.01f && hi.speed <= 0.63f * kBaseSpeed + 0.01f);
}

DP_TEST(the_unknown_picks_two_different_rules_by_seed)
{
    std::set<int> seen;
    for (std::uint32_t seed = 1; seed < 200; ++seed)
    {
        const auto rules = unknownRules(seed);
        DP_CHECK(rules.first != rules.second);
        DP_CHECK(unknownRules(seed) == rules);
        seen.insert(static_cast<int>(rules.first));
        seen.insert(static_cast<int>(rules.second));
    }
    DP_CHECK_EQ(static_cast<int>(seen.size()), kUnknownRuleCount);
}
