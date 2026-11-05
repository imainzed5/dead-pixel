// Doors: closed doors block like walls, paths still lead through them, and
// enough zombie blows break them for good.
#include "TestFramework.h"

#include "ai/Pathfinder.h"
#include "world/Doors.h"

namespace
{
// A 7x5 room: walls all round, one doorway on the left wall at (0, 2).
struct Room
{
    TileMap map;
    LayoutData layout;

    Room()
    {
        map.resize(9, 5, 32, 32);
        std::vector<int> collision(9 * 5, 0);
        for (int x = 1; x <= 7; ++x)
        {
            collision[static_cast<std::size_t>(0 * 9 + x)] = 1;
            collision[static_cast<std::size_t>(4 * 9 + x)] = 1;
        }
        for (int y = 0; y < 5; ++y)
        {
            collision[static_cast<std::size_t>(y * 9 + 1)] = 1;
            collision[static_cast<std::size_t>(y * 9 + 7)] = 1;
        }
        collision[static_cast<std::size_t>(2 * 9 + 1)] = 0; // the doorway
        map.setCollisionLayer(collision);
        layout.doors.push_back(LayoutData::DoorPos{1, 2});
    }
};
} // namespace

DP_TEST(doors_start_closed_and_solid)
{
    Room room;
    Doors doors;
    doors.reset(room.map, room.layout);
    DP_CHECK(room.map.isClosedDoor(1, 2));
    DP_CHECK(room.map.isSolid(1, 2));
    DP_CHECK(doors.toggle(room.map, glm::ivec2(1, 2)) == DoorState::Open);
    DP_CHECK(!room.map.isSolid(1, 2));
    DP_CHECK(doors.toggle(room.map, glm::ivec2(1, 2)) == DoorState::Closed);
    DP_CHECK(room.map.isSolid(1, 2));
}

DP_TEST(doors_paths_lead_through_closed_doors)
{
    Room room;
    Doors doors;
    doors.reset(room.map, room.layout);
    // From outside on the left to the middle of the room.
    const auto path = Pathfinder::findPath(room.map, glm::ivec2(0, 2), glm::ivec2(4, 2));
    DP_CHECK(!path.empty());
    bool throughDoor = false;
    for (const glm::ivec2& step : path)
    {
        throughDoor = throughDoor || step == glm::ivec2(1, 2);
    }
    DP_CHECK(throughDoor);
}

DP_TEST(doors_break_under_enough_blows_and_stay_broken)
{
    Room room;
    Doors doors;
    doors.reset(room.map, room.layout);
    const glm::ivec2 tile(1, 2);

    DP_CHECK(doors.batter(room.map, tile, 10.0f) == Doors::Blow::Bang);
    DP_CHECK(doors.batter(room.map, tile, 10.0f) == Doors::Blow::Thud); // too soon for another bang
    doors.tick(Doors::kBangInterval);
    DP_CHECK(doors.batter(room.map, tile, 10.0f) == Doors::Blow::Bang);

    Doors::Blow last = Doors::Blow::None;
    for (int i = 0; i < 20 && last != Doors::Blow::Breaks; ++i)
    {
        last = doors.batter(room.map, tile, 10.0f);
    }
    DP_CHECK(last == Doors::Blow::Breaks);
    DP_CHECK(room.map.doorState(1, 2) == DoorState::Broken);
    DP_CHECK(!room.map.isSolid(1, 2));
    DP_CHECK(doors.toggle(room.map, tile) == DoorState::Broken);
    DP_CHECK(doors.batter(room.map, tile, 10.0f) == Doors::Blow::None);
    // An open door takes no blows.
    Doors fresh;
    fresh.reset(room.map, room.layout);
    fresh.open(room.map, tile);
    DP_CHECK(fresh.batter(room.map, tile, 10.0f) == Doors::Blow::None);
}

DP_TEST(doors_restore_saved_states)
{
    Room room;
    Doors doors;
    doors.reset(room.map, room.layout);
    const glm::ivec2 tile(1, 2);

    doors.setState(room.map, tile, DoorState::Broken);
    DP_CHECK(room.map.doorState(1, 2) == DoorState::Broken);
    DP_CHECK_EQ(doors.tilesIn(room.map, DoorState::Broken).size(), std::size_t{1});
    DP_CHECK(doors.tilesIn(room.map, DoorState::Closed).empty());

    // Tiles that hold no door are ignored.
    doors.setState(room.map, glm::ivec2(3, 3), DoorState::Open);
    DP_CHECK(room.map.doorState(3, 3) == DoorState::None);

    // A restored closed door is whole again.
    doors.setState(room.map, tile, DoorState::Closed);
    DP_CHECK(doors.find(tile)->hp == Doors::kMaxHp);
    DP_CHECK(room.map.isSolid(1, 2));
}
