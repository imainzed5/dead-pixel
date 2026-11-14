// Buildings with a purpose (phase 6): kinds follow from the layout alone,
// the town gets the shops it must have, locks and alarms are chosen per
// building, and every kind's interior can be walked end to end.
#include "TestFramework.h"

#include <algorithm>
#include <cstdio>
#include <set>

#include "map/MapGenerator.h"
#include "map/TileIds.h"
#include "world/BuildingKinds.h"
#include "world/Furniture.h"

namespace
{
LayoutData townFor(std::uint32_t seed)
{
    MapGenerator generator;
    return generator.generateLayout(seed);
}

bool hasKind(const std::vector<BuildingKind>& kinds, BuildingKind kind)
{
    return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}
} // namespace

DP_TEST(building_kinds_are_the_same_for_the_same_layout)
{
    const LayoutData a = townFor(1234);
    const LayoutData b = townFor(1234);
    DP_CHECK_EQ(a.buildings.size(), b.buildings.size());
    const auto kindsA = buildingkinds::assign(a);
    const auto kindsB = buildingkinds::assign(b);
    DP_CHECK(kindsA == kindsB);
    // The generator stores them in the layout too.
    for (std::size_t i = 0; i < a.buildings.size(); ++i)
    {
        DP_CHECK_EQ(a.buildings[i].kind, static_cast<int>(kindsA[i]));
    }
}

DP_TEST(building_kinds_meet_the_guarantees)
{
    for (const std::uint32_t seed : {1234u, 42u, 777u})
    {
        const LayoutData layout = townFor(seed);
        std::vector<std::string> log;
        const auto kinds = buildingkinds::assign(layout, &log);
        const bool all = hasKind(kinds, BuildingKind::Pharmacy) && hasKind(kinds, BuildingKind::Grocery) &&
                         hasKind(kinds, BuildingKind::Hardware) && hasKind(kinds, BuildingKind::Clinic) &&
                         hasKind(kinds, BuildingKind::Police) && hasKind(kinds, BuildingKind::Garage);
        // Every guarantee kept, or the reason logged.
        DP_CHECK(all || !log.empty());
        // A kind always matches the building's archetype.
        for (std::size_t i = 0; i < kinds.size(); ++i)
        {
            if (kinds[i] == BuildingKind::House) continue;
            DP_CHECK(buildingkinds::archetypeOf(kinds[i]) != BuildingArchetype::Residential);
        }
    }
}

DP_TEST(building_kinds_names_round_trip)
{
    for (int k = 0; k < static_cast<int>(BuildingKind::Count); ++k)
    {
        const auto kind = static_cast<BuildingKind>(k);
        BuildingKind parsed = BuildingKind::Count;
        DP_CHECK(buildingkinds::fromName(buildingkinds::key(kind), parsed));
        DP_CHECK(parsed == kind);
        DP_CHECK(buildingkinds::fromName(buildingkinds::name(kind), parsed));
        DP_CHECK(parsed == kind);
    }
    BuildingKind unused = BuildingKind::House;
    DP_CHECK(!buildingkinds::fromName("castle", unused));
    // Signs: a pictogram for every kind that has one, none for houses.
    DP_CHECK(buildingkinds::signOf(BuildingKind::House) == BuildingSign::None);
    DP_CHECK(std::string(buildingkinds::signSprite(buildingkinds::signOf(BuildingKind::Pharmacy))) == "sign_cross");
    DP_CHECK(std::string(buildingkinds::signSprite(buildingkinds::signOf(BuildingKind::Police))) == "sign_badge");
}

DP_TEST(building_locks_spare_the_home_and_follow_the_kind)
{
    const LayoutData layout = townFor(1234);
    // A home: whichever house has a door.
    int home = 0;
    for (const LayoutData::DoorPos& door : layout.doors)
    {
        const int b = buildingkinds::buildingAt(layout, glm::ivec2(door.x, door.y));
        if (b > 0 && buildingkinds::kindOf(layout.buildings[static_cast<std::size_t>(b - 1)]) == BuildingKind::House)
        {
            home = b;
            break;
        }
    }
    DP_CHECK(home > 0);
    const auto locks = buildingkinds::lockedDoors(layout, home);
    DP_CHECK(!locks.empty());
    std::set<int> lockedBuildings;
    for (const buildingkinds::DoorLock& lock : locks)
    {
        DP_CHECK(lock.building != home);
        lockedBuildings.insert(lock.building);
        const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(lock.building - 1)];
        // Only alarmed kinds ring.
        if (lock.alarm) DP_CHECK(buildingkinds::alarmChance(buildingkinds::kindOf(b)) > 0.0f);
    }
    // A locked building's street door is locked.
    for (const int building : lockedBuildings)
    {
        const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(building - 1)];
        if (!buildingkinds::buildingLocked(b)) continue;
        const glm::ivec2 street = buildingkinds::streetDoor(layout, building);
        if (street.x < 0) continue;
        DP_CHECK(std::any_of(locks.begin(), locks.end(), [&](const buildingkinds::DoorLock& l) { return l.tile == street; }));
    }
    // The same every time.
    const auto again = buildingkinds::lockedDoors(layout, home);
    DP_CHECK_EQ(again.size(), locks.size());
    // Police always alarmed, houses never.
    DP_CHECK_NEAR(buildingkinds::alarmChance(BuildingKind::Police), 1.0f, 0.0001);
    DP_CHECK_NEAR(buildingkinds::alarmChance(BuildingKind::House), 0.0f, 0.0001);
    DP_CHECK_NEAR(buildingkinds::alarmChance(BuildingKind::Pharmacy), 0.6f, 0.0001);
}

DP_TEST(every_kind_is_furnished_with_its_pieces)
{
    // Across a real town, the kinds get the pieces the tables give them.
    const LayoutData layout = townFor(1234);
    const auto pieces = furniture::furnish(layout, 1234u);
    const auto kindHas = [&](BuildingKind kind, FurnitureKind piece)
    {
        return std::any_of(pieces.begin(), pieces.end(), [&](const FurniturePiece& p) {
            return p.kind == piece && p.buildingKind == static_cast<int>(kind);
        });
    };
    DP_CHECK(kindHas(BuildingKind::Grocery, FurnitureKind::StoreShelf));
    DP_CHECK(kindHas(BuildingKind::Pharmacy, FurnitureKind::PharmacyShelf));
    DP_CHECK(kindHas(BuildingKind::Clinic, FurnitureKind::Cot));
    DP_CHECK(kindHas(BuildingKind::Police, FurnitureKind::Locker));
    // One armoury locker for each police station.
    int armouries = 0;
    for (const FurniturePiece& p : pieces) armouries += p.armoury ? 1 : 0;
    int police = 0;
    for (const auto& b : layout.buildings) police += buildingkinds::kindOf(b) == BuildingKind::Police ? 1 : 0;
    DP_CHECK_EQ(armouries, police);
    // Loot classes follow the building.
    for (const FurniturePiece& p : pieces)
    {
        if (p.armoury) DP_CHECK(furniture::lootClass(p) == "armoury");
        if (p.kind == FurnitureKind::Fridge) DP_CHECK(furniture::lootClass(p) == "fridge");
    }
    // Floor lamps in houses, none in shops.
    DP_CHECK(std::any_of(pieces.begin(), pieces.end(), [](const FurniturePiece& p) { return p.kind == FurnitureKind::FloorLamp; }));
    for (const FurniturePiece& p : pieces)
    {
        if (p.kind == FurnitureKind::FloorLamp) DP_CHECK(p.buildingKind == static_cast<int>(BuildingKind::House));
    }
    // A searched container has its ajar frame; a lamp its lit one.
    DP_CHECK(furniture::hasAltSprite(FurnitureKind::Locker));
    DP_CHECK(furniture::hasAltSprite(FurnitureKind::FloorLamp));
    DP_CHECK(!furniture::hasAltSprite(FurnitureKind::Bed));
}

DP_TEST(every_kind_interior_is_walkable_end_to_end)
{
    // In real towns, every shop, station and workplace: each open floor tile
    // is reachable from the street door, and every piece is used from one.
    for (const std::uint32_t seed : {1234u, 42u, 777u})
    {
        const LayoutData layout = townFor(seed);
        const auto pieces = furniture::furnish(layout, seed);
        const auto at = [&](int x, int y) { return static_cast<std::size_t>(y * layout.width + x); };
        std::vector<std::uint8_t> taken(static_cast<std::size_t>(layout.width * layout.height), 0);
        for (const FurniturePiece& p : pieces) taken[at(p.tile.x, p.tile.y)] = 1;
        std::set<int> kindsChecked;
        for (std::size_t i = 0; i < layout.buildings.size(); ++i)
        {
            const LayoutData::BuildingRect& b = layout.buildings[i];
            const BuildingKind kind = buildingkinds::kindOf(b);
            if (kind == BuildingKind::House) continue;
            const glm::ivec2 door = buildingkinds::streetDoor(layout, static_cast<int>(i) + 1);
            if (door.x < 0) continue;
            // Indoor floor only: a footprint can hold a bit of yard.
            const auto floorAt = [&](int x, int y)
            {
                if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) return false;
                if (layout.collision[at(x, y)] != 0) return false;
                const int gid = layout.ground[at(x, y)];
                const int base = gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
                return base == TileGid::kFloor || base == TileGid::kCarpet || base == TileGid::kMetal ||
                       base == TileGid::kWood || base == TileGid::kTileFloor;
            };
            const auto open = [&](int x, int y) { return floorAt(x, y) && taken[at(x, y)] == 0; };
            const glm::ivec2 dirs[4] = {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)};
            // Flood from the street door: through the floor as built, then
            // with the furniture in it. The furniture may not cut anything off.
            const auto flood = [&](bool withFurniture)
            {
                std::vector<std::uint8_t> seen(taken.size(), 0);
                std::vector<glm::ivec2> stack{door};
                seen[at(door.x, door.y)] = 1;
                while (!stack.empty())
                {
                    const glm::ivec2 t = stack.back();
                    stack.pop_back();
                    for (const glm::ivec2& d : dirs)
                    {
                        const glm::ivec2 n = t + d;
                        if (!(withFurniture ? open(n.x, n.y) : floorAt(n.x, n.y)) || seen[at(n.x, n.y)] != 0) continue;
                        seen[at(n.x, n.y)] = 1;
                        stack.push_back(n);
                    }
                }
                return seen;
            };
            const std::vector<std::uint8_t> built = flood(false);
            const std::vector<std::uint8_t> seen = flood(true);
            bool walkable = true;
            for (int y = b.y; y < b.y + b.h; ++y)
            {
                for (int x = b.x; x < b.x + b.w; ++x)
                {
                    if (open(x, y) && built[at(x, y)] != 0 && seen[at(x, y)] == 0) walkable = false;
                }
            }
            for (const FurniturePiece& p : pieces)
            {
                if (p.building != static_cast<int>(i) + 1) continue;
                bool usable = false;
                for (const glm::ivec2& d : dirs)
                {
                    const glm::ivec2 n = p.tile + d;
                    if (open(n.x, n.y) && seen[at(n.x, n.y)] != 0) usable = true;
                }
                if (!usable) walkable = false;
            }
            if (!walkable)
            {
                std::printf("  seed %u: %s (building %zu) is not walkable end to end\n", seed, buildingkinds::name(kind), i + 1);
            }
            DP_CHECK(walkable);
            kindsChecked.insert(static_cast<int>(kind));
        }
        // The kinds every town must have were all among them.
        for (const BuildingKind kind : {BuildingKind::Pharmacy, BuildingKind::Grocery, BuildingKind::Hardware,
                                        BuildingKind::Clinic, BuildingKind::Police, BuildingKind::Garage})
        {
            DP_CHECK(kindsChecked.count(static_cast<int>(kind)) == 1);
        }
    }
}
