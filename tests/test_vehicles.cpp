// Vehicles (phase 11): the key, the wire and the crank; wear and the
// condition bands; hits on walls and bodies; the arcade drive and the eight
// facings; where the town parks them; the body against the town's tiles.
#include "TestFramework.h"

#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <glm/geometric.hpp>

#include "map/MapGenerator.h"
#include "systems/VehicleSystem.h"
#include "world/VehicleRules.h"

using namespace vehiclerules;

namespace
{
constexpr float kPi = 3.14159265358979f;

float roll(std::mt19937& rng)
{
    return static_cast<float>(rng() % 100000u) / 100000.0f;
}

// A flat world: open ground, with walls where `wall` says.
VehicleSystem::Env openGround(std::function<bool(int, int)> wall, std::function<bool(int, int)> thin = nullptr)
{
    VehicleSystem::Env env;
    env.tile = [wall, thin](int x, int y)
    {
        if (thin && thin(x, y)) return VehicleSystem::TileKind::Thin;
        return wall(x, y) ? VehicleSystem::TileKind::Wall : VehicleSystem::TileKind::Open;
    };
    return env;
}

// Drive straight along +x with full throttle for `seconds`.
void driveFor(VehicleKind kind, glm::vec2& centre, float& heading, float& speed, const VehicleSystem::Env& env,
              float seconds, std::vector<VehicleSystem::Hit>& hits, glm::vec2 want = glm::vec2(1.0f, 0.0f))
{
    DriveInput in;
    in.want = want;
    in.top = info(kind).topSpeed;
    for (int i = 0; i < static_cast<int>(seconds * 60.0f); ++i)
    {
        VehicleSystem::step(kind, centre, heading, speed, in, env, 1.0f / 60.0f, hits);
    }
}
} // namespace

// --- Starting one ----------------------------------------------------------------

DP_TEST(vehicles_hotwire_fails_at_least_once_in_ten_at_mechanics_0)
{
    // Ten tries at Mechanics 0, as the game rolls them (its own seeded stream).
    for (std::uint32_t seed = 1; seed <= 20; ++seed)
    {
        std::mt19937 rng(seed ^ 0x7EC1u);
        int fails = 0;
        for (int i = 0; i < 10; ++i) fails += hotwireFails(roll(rng), 0) ? 1 : 0;
        DP_CHECK(fails >= 1);
    }
    DP_CHECK_NEAR(hotwireFailChance(0), 0.40f, 0.0001f);
    DP_CHECK_NEAR(hotwireFailChance(2), 0.20f, 0.0001f);
}

DP_TEST(vehicles_hotwire_never_fails_at_mechanics_5)
{
    std::mt19937 rng(99u);
    for (int i = 0; i < 1000; ++i) DP_CHECK(!hotwireFails(roll(rng), 5));
    DP_CHECK_NEAR(hotwireFailChance(4), 0.0f, 0.0001f);
    // Mechanics shortens the wire.
    DP_CHECK_NEAR(hotwireSeconds(0), kHotwireSeconds, 0.001f);
    DP_CHECK(hotwireSeconds(5) < hotwireSeconds(0));
}

DP_TEST(vehicles_starts_fail_only_under_50)
{
    DP_CHECK_NEAR(startFailChance(80.0f), 0.0f, 0.0001f);
    DP_CHECK_NEAR(startFailChance(50.0f), 0.0f, 0.0001f);
    DP_CHECK_NEAR(startFailChance(40.0f), 0.20f, 0.0001f);
    DP_CHECK_NEAR(startFailChance(20.0f), 0.60f, 0.0001f);
    DP_CHECK(!startFails(0.0f, 60.0f));
    DP_CHECK(startFails(0.1f, 40.0f));
    DP_CHECK(!startFails(0.25f, 40.0f));
}

// --- Condition and wear ------------------------------------------------------------

DP_TEST(vehicles_condition_bands)
{
    DP_CHECK(band(100.0f) == Band::Good);
    DP_CHECK(band(70.0f) == Band::Good);
    DP_CHECK(band(69.9f) == Band::Worn);
    DP_CHECK(band(40.0f) == Band::Worn);
    DP_CHECK(band(39.0f) == Band::Damaged);
    DP_CHECK(band(10.0f) == Band::Damaged);
    DP_CHECK(band(9.9f) == Band::Broken);
    DP_CHECK(runs(10.0f));
    DP_CHECK(!runs(9.0f));
    DP_CHECK_NEAR(bandSpeedScale(Band::Worn), 0.86f, 0.0001f);
    DP_CHECK_NEAR(bandSpeedScale(Band::Damaged), 0.72f, 0.0001f);
}

DP_TEST(vehicles_wear_rain_and_off_road)
{
    DP_CHECK_NEAR(wearPerGameHour(false, false), 0.5f, 0.0001f);
    DP_CHECK_NEAR(wearPerGameHour(true, false), 0.75f, 0.0001f);
    DP_CHECK_NEAR(wearPerGameHour(false, true), 1.0f, 0.0001f);
    DP_CHECK_NEAR(wearPerGameHour(true, true), 1.5f, 0.0001f);
    DP_CHECK_NEAR(fuelUsed(VehicleKind::Sedan, 1.0f), 6.0f, 0.0001f);
    DP_CHECK_NEAR(fuelUsed(VehicleKind::Pickup, 0.5f), 4.0f, 0.0001f);
    DP_CHECK_NEAR(fuelUsed(VehicleKind::Bicycle, 3.0f), 0.0f, 0.0001f);
}

DP_TEST(vehicles_top_speed_by_condition_tyres_and_ground)
{
    DP_CHECK_NEAR(topSpeed(VehicleKind::Sedan, {100.0f, 4, false}), 250.0f, 0.01f);
    DP_CHECK_NEAR(topSpeed(VehicleKind::Sedan, {50.0f, 4, false}), 250.0f * 0.86f, 0.01f);
    DP_CHECK_NEAR(topSpeed(VehicleKind::Sedan, {20.0f, 4, false}), 250.0f * 0.72f, 0.01f);
    DP_CHECK_NEAR(topSpeed(VehicleKind::Sedan, {5.0f, 4, false}), 0.0f, 0.01f);
    DP_CHECK_NEAR(topSpeed(VehicleKind::Sedan, {100.0f, 3, false}), 250.0f * 0.7f, 0.01f);
    DP_CHECK_NEAR(topSpeed(VehicleKind::Pickup, {100.0f, 4, true}), 210.0f * 0.7f, 0.01f);
    DP_CHECK_NEAR(topSpeed(VehicleKind::Bicycle, {100.0f, 4, false}), 170.0f, 0.01f);
    // Snow halves turning; standing still a car does not turn.
    DP_CHECK_NEAR(turnRate(VehicleKind::Sedan, 100.0f, true), turnRate(VehicleKind::Sedan, 100.0f, false) * 0.5f, 0.0001f);
    DP_CHECK(turnRate(VehicleKind::Sedan, 200.0f, false) < turnRate(VehicleKind::Sedan, 60.0f, false));
    DP_CHECK_NEAR(turnRate(VehicleKind::Sedan, 0.0f, false), 0.0f, 0.0001f);
    DP_CHECK(std::string(speedWord(0.0f)) == "Idle");
    DP_CHECK(std::string(speedWord(100.0f)) == "Slow");
    DP_CHECK(std::string(speedWord(200.0f)) == "Fast");
}

DP_TEST(vehicles_repair_and_lift)
{
    DP_CHECK_NEAR(repairAmount(0, false), 25.0f, 0.001f);
    DP_CHECK(repairAmount(3, false) > repairAmount(0, false));
    DP_CHECK_NEAR(repairAmount(0, true), 50.0f, 0.001f);
}

// --- Hits ----------------------------------------------------------------------------

DP_TEST(vehicles_wall_hit_hurts_above_120)
{
    DP_CHECK(!wallHit(110.0f).hurts);
    const WallHit hit = wallHit(200.0f);
    DP_CHECK(hit.hurts);
    DP_CHECK_NEAR(hit.vehicleDamage, 10.0f, 0.001f);
    DP_CHECK_NEAR(hit.driverDamage, 5.0f, 0.001f);
}

DP_TEST(vehicles_running_down_the_dead)
{
    const BodyHit fast = bodyHit(200.0f, false);
    DP_CHECK(fast.thrown);
    DP_CHECK_NEAR(fast.damage, 65.0f, 0.001f);
    DP_CHECK_NEAR(fast.conditionLoss, 5.0f, 0.001f);
    DP_CHECK_NEAR(fast.speedKeep, 0.65f, 0.001f);
    // A Brute stops the car.
    const BodyHit brute = bodyHit(200.0f, true);
    DP_CHECK(!brute.thrown);
    DP_CHECK_NEAR(brute.speedKeep, 0.0f, 0.001f);
    DP_CHECK_NEAR(brute.conditionLoss, 15.0f, 0.001f);
    // Below 140 it is blocked against them.
    const BodyHit slow = bodyHit(100.0f, false);
    DP_CHECK(slow.blocked);
    DP_CHECK_NEAR(slow.damage, 0.0f, 0.001f);
}

// --- The drive -------------------------------------------------------------------------

DP_TEST(vehicles_drive_accelerates_turns_and_reverses)
{
    DriveState s;
    DriveInput in;
    in.want = glm::vec2(1.0f, 0.0f);
    in.top = 250.0f;
    for (int i = 0; i < 60; ++i) drive(VehicleKind::Sedan, s, in, 1.0f / 60.0f);
    DP_CHECK_NEAR(s.speed, 120.0f, 1.0f); // a second of throttle
    DP_CHECK_NEAR(s.heading, 0.0f, 0.0001f);
    // Turning toward +y.
    in.want = glm::vec2(0.0f, 1.0f);
    for (int i = 0; i < 120; ++i) drive(VehicleKind::Sedan, s, in, 1.0f / 60.0f);
    DP_CHECK_NEAR(s.heading, kPi * 0.5f, 0.05f);
    // Want straight behind: it brakes, then backs up, never past its reverse cap.
    in.want = glm::vec2(0.0f, -1.0f);
    for (int i = 0; i < 600; ++i) drive(VehicleKind::Sedan, s, in, 1.0f / 60.0f);
    DP_CHECK(s.speed < 0.0f);
    DP_CHECK(s.speed >= -info(VehicleKind::Sedan).reverseSpeed - 0.01f);
    // No throttle (dry tank): it only coasts down.
    DriveState c;
    c.speed = 100.0f;
    DriveInput dry;
    dry.want = glm::vec2(1.0f, 0.0f);
    dry.canThrottle = false;
    for (int i = 0; i < 30; ++i) drive(VehicleKind::Sedan, c, dry, 1.0f / 60.0f);
    DP_CHECK(c.speed < 100.0f);
}

DP_TEST(vehicles_a_bicycle_turns_round_instead_of_backing)
{
    DriveState s;
    s.heading = 0.0f;
    DriveInput in;
    in.want = glm::vec2(-1.0f, 0.0f);
    in.top = 170.0f;
    for (int i = 0; i < 180; ++i) drive(VehicleKind::Bicycle, s, in, 1.0f / 60.0f);
    DP_CHECK(s.speed >= 0.0f);
    DP_CHECK(std::cos(s.heading) < -0.9f); // it faces the other way now
}

DP_TEST(vehicles_eight_facings_reuse_mirrors)
{
    DP_CHECK_EQ(facing8(0.0f).dir, 0);
    DP_CHECK(!facing8(0.0f).mirrored);
    DP_CHECK_EQ(facing8(kPi * 0.5f).dir, 2);
    DP_CHECK(facing8(kPi * 0.5f).mirrored);
    DP_CHECK_EQ(facing8(kPi * 0.5f).drawn, 0);
    DP_CHECK_EQ(facing8(kPi).dir, 4);
    DP_CHECK_EQ(facing8(kPi).drawn, 6);
    DP_CHECK_EQ(facing8(-kPi * 0.5f).dir, 6);
    DP_CHECK(!facing8(-kPi * 0.5f).mirrored);
    DP_CHECK_EQ(facing8(kPi * 0.75f).drawn, 7);
    // Every facing draws from one of the five drawn ones.
    for (int d = 0; d < 8; ++d)
    {
        const Facing8 f = facing8(static_cast<float>(d) * kPi * 0.25f);
        DP_CHECK_EQ(f.dir, d);
        DP_CHECK(f.drawn == 0 || f.drawn == 1 || f.drawn == 5 || f.drawn == 6 || f.drawn == 7);
    }
}

// --- Where the town parks them ------------------------------------------------------------

DP_TEST(vehicles_town_parks_the_counts_and_they_fit)
{
    for (std::uint32_t seed : {1234u, 7u, 99u, 2026u})
    {
        MapGenerator generator;
        const LayoutData layout = generator.generateLayout(seed);
        const std::vector<Spot> spots = place(layout, seed);
        int sedans = 0, pickups = 0, bicycles = 0, keyed = 0;
        for (const Spot& s : spots)
        {
            if (s.kind == VehicleKind::Sedan) ++sedans;
            if (s.kind == VehicleKind::Pickup) ++pickups;
            if (s.kind == VehicleKind::Bicycle) ++bicycles;
            if (s.keyInside) ++keyed;
            DP_CHECK(fits(layout, s.kind, s.centre, s.heading));
            DP_CHECK(s.condition >= 10.0f && s.condition <= 100.0f);
        }
        DP_CHECK(sedans >= kSedansMin && sedans <= kSedansMax);
        DP_CHECK(pickups >= kPickupsMin && pickups <= kPickupsMax);
        DP_CHECK(bicycles >= kBicyclesMin && bicycles <= kBicyclesMax);
        // Two never share ground.
        for (std::size_t i = 0; i < spots.size(); ++i)
        {
            for (std::size_t j = i + 1; j < spots.size(); ++j)
            {
                DP_CHECK(glm::length(spots[i].centre - spots[j].centre) > 24.0f);
            }
        }
        // The same seed parks them the same.
        const std::vector<Spot> again = place(layout, seed);
        DP_CHECK_EQ(again.size(), spots.size());
        for (std::size_t i = 0; i < std::min(again.size(), spots.size()); ++i)
        {
            DP_CHECK(again[i].centre == spots[i].centre);
            DP_CHECK(again[i].kind == spots[i].kind);
        }
    }
}

// --- The body against the town ---------------------------------------------------------------

DP_TEST(vehicles_body_stops_at_a_wall)
{
    // A wall across x = 20 tiles.
    const VehicleSystem::Env env = openGround([](int x, int) { return x >= 20; });
    glm::vec2 centre(5.0f * 32.0f, 10.0f * 32.0f);
    float heading = 0.0f;
    float speed = 0.0f;
    std::vector<VehicleSystem::Hit> hits;
    driveFor(VehicleKind::Sedan, centre, heading, speed, env, 6.0f, hits);
    DP_CHECK(!hits.empty());
    DP_CHECK(hits.front().type == VehicleSystem::Hit::Type::Wall);
    DP_CHECK(hits.front().impact > 120.0f);
    // The body never got into the wall.
    DP_CHECK(centre.x + info(VehicleKind::Sedan).length * 0.5f <= 20.0f * 32.0f + 2.0f);
    DP_CHECK(!VehicleSystem::blocked(VehicleKind::Sedan, centre, heading, env));
}

DP_TEST(vehicles_thin_things_go_down_at_speed_only)
{
    // A hydrant at (20, 10): slow, it stops the car; fast, it is run down.
    const auto thin = [](int x, int y) { return x == 20 && y == 10; };
    const VehicleSystem::Env env = openGround([](int, int) { return false; }, thin);
    {
        glm::vec2 centre(16.5f * 32.0f, 10.5f * 32.0f);
        float heading = 0.0f;
        float speed = 0.0f;
        std::vector<VehicleSystem::Hit> hits;
        DriveInput in;
        in.want = glm::vec2(1.0f, 0.0f);
        in.top = 60.0f;
        for (int i = 0; i < 300; ++i) VehicleSystem::step(VehicleKind::Sedan, centre, heading, speed, in, env, 1.0f / 60.0f, hits);
        DP_CHECK(centre.x < 20.0f * 32.0f);
        for (const auto& h : hits) DP_CHECK(h.type != VehicleSystem::Hit::Type::Thin);
    }
    {
        glm::vec2 centre(2.5f * 32.0f, 10.5f * 32.0f);
        float heading = 0.0f;
        float speed = 0.0f;
        std::vector<VehicleSystem::Hit> hits;
        driveFor(VehicleKind::Sedan, centre, heading, speed, env, 6.0f, hits);
        int thinHits = 0;
        for (const auto& h : hits) thinHits += h.type == VehicleSystem::Hit::Type::Thin ? 1 : 0;
        DP_CHECK_EQ(thinHits, 1);
        DP_CHECK(centre.x > 21.0f * 32.0f);
    }
}

DP_TEST(vehicles_bodies_are_pushed_out_of_a_parked_car)
{
    const glm::vec2 centre(100.0f, 100.0f);
    // Beside the car's middle, overlapping.
    const glm::vec2 inside(100.0f, 100.0f + 14.0f);
    const glm::vec2 push = VehicleSystem::pushOut(VehicleKind::Sedan, centre, 0.0f, inside, 10.0f);
    DP_CHECK(push.y > 0.0f);
    DP_CHECK(VehicleSystem::distanceToBody(VehicleKind::Sedan, centre, 0.0f, inside + push) >= 9.9f);
    // Clear of it: nothing.
    const glm::vec2 clear(100.0f, 160.0f);
    const glm::vec2 none = VehicleSystem::pushOut(VehicleKind::Sedan, centre, 0.0f, clear, 10.0f);
    DP_CHECK_NEAR(none.x, 0.0f, 0.0001f);
    DP_CHECK_NEAR(none.y, 0.0f, 0.0001f);
    // The rear (tailgate) and the driver's side are outside the body.
    DP_CHECK(VehicleSystem::distanceToBody(VehicleKind::Pickup, centre, 0.0f, rear(VehicleKind::Pickup, centre, 0.0f)) > 5.0f);
    DP_CHECK(VehicleSystem::distanceToBody(VehicleKind::Sedan, centre, 0.0f, side(VehicleKind::Sedan, centre, 0.0f, true)) > 10.0f);
}

DP_TEST(vehicles_kinds_by_name)
{
    VehicleKind k = VehicleKind::Sedan;
    DP_CHECK(parseKind("pickup", k) && k == VehicleKind::Pickup);
    DP_CHECK(parseKind("Bike", k) && k == VehicleKind::Bicycle);
    DP_CHECK(parseKind("car", k) && k == VehicleKind::Sedan);
    DP_CHECK(!parseKind("tank", k));
    char buffer[48];
    DP_CHECK(std::string(describe(VehicleKind::Sedan, 0, buffer, sizeof(buffer))) == "the red sedan");
    DP_CHECK(std::string(describe(VehicleKind::Bicycle, 0, buffer, sizeof(buffer))) == "the bicycle");
    DP_CHECK(!info(VehicleKind::Bicycle).engine);
    DP_CHECK_EQ(info(VehicleKind::Pickup).trunkSlots, 20);
    DP_CHECK_NEAR(info(VehicleKind::Pickup).hearing, 1.2f, 0.0001f);
}
