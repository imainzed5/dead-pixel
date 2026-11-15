// A home base: furniture pushed and broken up, a doorway blocked with a
// piece the dead must break, stations built where they may stand, and what
// of it all the next life finds.
#include "TestFramework.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>

#include "ai/Pathfinder.h"
#include "map/TileIds.h"
#include "save/SaveManager.h"
#include "systems/BuildSystem.h"
#include "world/Doors.h"
#include "world/FurnitureUse.h"

namespace
{
// A 7x5 house on a 10x7 lot: walls round (1,1)-(7,5), floor inside, a
// doorway on the west wall at (1, 3). Building id 1 covers the footprint.
struct House
{
    TileMap map;
    LayoutData layout;
    Doors doors;

    explicit House(int floorGid = TileGid::kFloor)
    {
        const int w = 10;
        const int h = 7;
        map.resize(w, h, 32, 32);
        std::vector<int> collision(static_cast<std::size_t>(w * h), 0);
        std::vector<int> building(static_cast<std::size_t>(w * h), 0);
        std::vector<int> ground(static_cast<std::size_t>(w * h), TileGid::kGrass);
        for (int y = 1; y <= 5; ++y)
        {
            for (int x = 1; x <= 7; ++x)
            {
                const auto i = static_cast<std::size_t>(y * w + x);
                building[i] = 1;
                const bool edge = x == 1 || y == 1 || x == 7 || y == 5;
                collision[i] = edge ? 1 : 0;
                ground[i] = edge ? TileGid::kWall : floorGid;
            }
        }
        collision[static_cast<std::size_t>(3 * w + 1)] = 0; // the doorway
        map.setCollisionLayer(collision);
        map.setBuildingLayer(building);
        map.setGroundLayer(ground);
        layout.doors.push_back(LayoutData::DoorPos{1, 3});
        doors.reset(map, layout);
    }
};

class ScopedTempCwd
{
public:
    ScopedTempCwd()
    {
        static std::atomic<int> counter{0};
        mOriginal = std::filesystem::current_path();
        mPath = std::filesystem::temp_directory_path() /
                ("dead_pixel_base_test_" + std::to_string(++counter) + "_" +
                 std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::remove_all(mPath);
        std::filesystem::create_directories(mPath);
        std::filesystem::current_path(mPath);
    }
    ~ScopedTempCwd()
    {
        std::error_code ec;
        std::filesystem::current_path(mOriginal, ec);
        std::filesystem::remove_all(mPath, ec);
    }
    ScopedTempCwd(const ScopedTempCwd&) = delete;
    ScopedTempCwd& operator=(const ScopedTempCwd&) = delete;

private:
    std::filesystem::path mOriginal;
    std::filesystem::path mPath;
};

FurniturePiece piece(FurnitureKind kind, glm::ivec2 tile)
{
    FurniturePiece p;
    p.kind = kind;
    p.tile = tile;
    p.origin = tile;
    p.building = 1;
    return p;
}
} // namespace

DP_TEST(base_pushing_needs_free_floor)
{
    House house;
    const std::vector<glm::ivec2> nobody;
    DP_CHECK(furnitureuse::canPushInto(house.map, {3, 3}, nobody));
    DP_CHECK(!furnitureuse::canPushInto(house.map, {1, 2}, nobody)); // wall
    DP_CHECK(!furnitureuse::canPushInto(house.map, {1, 3}, nobody)); // the doorway itself
    DP_CHECK(!furnitureuse::canPushInto(house.map, {-1, 3}, nobody)); // off the map
    DP_CHECK(!furnitureuse::canPushInto(house.map, {3, 3}, {glm::ivec2(3, 3)})); // someone stands there
    house.map.setFurniture(4, 3, true);
    DP_CHECK(!furnitureuse::canPushInto(house.map, {4, 3}, nobody)); // another piece
    // Outside is fine too.
    DP_CHECK(furnitureuse::canPushInto(house.map, {8, 3}, nobody));
}

DP_TEST(base_a_push_carries_a_piece_through_an_open_doorway)
{
    House house;
    const std::vector<glm::ivec2> nobody;
    DP_CHECK(furnitureuse::pushTarget(house.map, {3, 3}, {1, 0}, nobody) == glm::ivec2(4, 3));
    // A closed door stops it; an open one lets it through to the ground beyond.
    DP_CHECK(furnitureuse::pushTarget(house.map, {2, 3}, {-1, 0}, nobody) == glm::ivec2(-1));
    house.doors.toggle(house.map, {1, 3});
    DP_CHECK(furnitureuse::pushTarget(house.map, {2, 3}, {-1, 0}, nobody) == glm::ivec2(0, 3));
    // Not when someone stands in the doorway or on the far side.
    DP_CHECK(furnitureuse::pushTarget(house.map, {2, 3}, {-1, 0}, {glm::ivec2(1, 3)}) == glm::ivec2(-1));
    DP_CHECK(furnitureuse::pushTarget(house.map, {2, 3}, {-1, 0}, {glm::ivec2(0, 3)}) == glm::ivec2(-1));
    // Into a wall: nowhere.
    DP_CHECK(furnitureuse::pushTarget(house.map, {2, 2}, {0, -1}, nobody) == glm::ivec2(-1));
}

DP_TEST(base_a_piece_inside_a_doorway_blocks_that_door)
{
    House house;
    DP_CHECK(furnitureuse::blockedDoor(house.map, {2, 3}) == glm::ivec2(1, 3));
    DP_CHECK(furnitureuse::blockedDoor(house.map, {3, 3}) == glm::ivec2(-1));
    DP_CHECK(furnitureuse::blockedDoor(house.map, {2, 2}) == glm::ivec2(-1));
    // Outside the house, in front of the door: not a barricade.
    DP_CHECK(furnitureuse::blockedDoor(house.map, {0, 3}) == glm::ivec2(-1));
}

DP_TEST(base_the_dead_route_through_a_blocked_doorway_at_a_cost)
{
    House house;
    // The door stands open; a couch is pushed into the floor behind it.
    house.doors.toggle(house.map, {1, 3});
    house.map.setFurniture(2, 3, true);
    house.map.setDoorwayBlocked(2, 3, true);
    DP_CHECK_NEAR(Pathfinder::stepCost(house.map, {2, 3}, Pathfinder::Mover::Dead),
                  Pathfinder::kBlockedDoorwayCost, 0.001);
    DP_CHECK(Pathfinder::stepCost(house.map, {2, 3}, Pathfinder::Mover::Living) < 0.0f);
    const auto path = Pathfinder::findPath(house.map, {0, 3}, {4, 3});
    DP_CHECK(!path.empty());
    DP_CHECK(std::find(path.begin(), path.end(), glm::ivec2(2, 3)) != path.end());
    // Plain furniture elsewhere is not a way in.
    house.map.setFurniture(5, 2, true);
    DP_CHECK(Pathfinder::stepCost(house.map, {5, 2}, Pathfinder::Mover::Dead) < 0.0f);
}

DP_TEST(base_breaking_furniture_takes_time_and_gives_planks)
{
    using furnitureuse::Tool;
    // Light pieces faster than heavy ones; better tools faster still.
    DP_CHECK_NEAR(furnitureuse::breakSeconds(FurnitureKind::Crates, Tool::Hands, false), 8.0, 0.001);
    DP_CHECK_NEAR(furnitureuse::breakSeconds(FurnitureKind::Bookshelf, Tool::Hands, false), 14.0, 0.001);
    DP_CHECK_NEAR(furnitureuse::breakSeconds(FurnitureKind::Bookshelf, Tool::Hammer, false), 9.0, 0.001);
    DP_CHECK_NEAR(furnitureuse::breakSeconds(FurnitureKind::Bookshelf, Tool::AxeOrCrowbar, true), 5.0, 0.001);
    // Fixed pieces: only metal ones, only with a crowbar.
    DP_CHECK(furnitureuse::breakSeconds(FurnitureKind::Counter, Tool::AxeOrCrowbar, true) < 0.0f);
    DP_CHECK(furnitureuse::breakSeconds(FurnitureKind::Fridge, Tool::Hammer, false) < 0.0f);
    DP_CHECK_NEAR(furnitureuse::breakSeconds(FurnitureKind::Fridge, Tool::AxeOrCrowbar, true),
                  furnitureuse::kScrapSeconds, 0.001);

    DP_CHECK_EQ(furnitureuse::breakYield(FurnitureKind::Bookshelf, false).planks, 3);
    DP_CHECK_EQ(furnitureuse::breakYield(FurnitureKind::Bookshelf, true).planks, 4); // a saw adds one
    DP_CHECK_EQ(furnitureuse::breakYield(FurnitureKind::Crates, true).planks, 1);    // not under two
    DP_CHECK_EQ(furnitureuse::breakYield(FurnitureKind::Bed, false).rags, 2);
    DP_CHECK_EQ(furnitureuse::breakYield(FurnitureKind::Fridge, false).scrap, 1);
    DP_CHECK(!furnitureuse::movable(FurnitureKind::Counter));
    DP_CHECK(furnitureuse::movable(FurnitureKind::Couch));
    DP_CHECK(furnitureuse::useInfo(FurnitureKind::Couch).barrierHp > 0.0f);
}

DP_TEST(base_furniture_memory_applies_to_a_fresh_furnishing)
{
    std::vector<FurniturePiece> pieces = {piece(FurnitureKind::Couch, {3, 3}), piece(FurnitureKind::Table, {4, 2}),
                                          piece(FurnitureKind::Bed, {5, 4})};
    pieces[0].tile = {2, 3}; // pushed into the doorway
    const std::vector<glm::ivec2> removed = {glm::ivec2(5, 4)};
    pieces.erase(pieces.begin() + 2); // broken up
    const furnitureuse::Memory memory = furnitureuse::memoryOf(pieces, removed);
    DP_CHECK_EQ(memory.moves.size(), 1u);
    DP_CHECK_EQ(memory.removed.size(), 1u);

    std::vector<FurniturePiece> fresh = {piece(FurnitureKind::Couch, {3, 3}), piece(FurnitureKind::Table, {4, 2}),
                                         piece(FurnitureKind::Bed, {5, 4})};
    furnitureuse::apply(fresh, memory);
    DP_CHECK_EQ(fresh.size(), 2u);
    DP_CHECK(fresh[0].tile == glm::ivec2(2, 3));
    DP_CHECK(fresh[0].origin == glm::ivec2(3, 3));
    DP_CHECK(fresh[1].tile == glm::ivec2(4, 2));

    // A stale move onto a tile another piece holds is undone.
    furnitureuse::Memory stale;
    stale.moves.push_back(furnitureuse::Move{glm::ivec2(3, 3), glm::ivec2(4, 2)});
    std::vector<FurniturePiece> again = {piece(FurnitureKind::Couch, {3, 3}), piece(FurnitureKind::Table, {4, 2})};
    furnitureuse::apply(again, stale);
    DP_CHECK(again[0].tile == glm::ivec2(3, 3));
}

DP_TEST(base_stations_stand_where_their_recipe_says)
{
    House house;
    const std::vector<glm::ivec2> nobody;
    DP_CHECK(BuildSystem::placeable(house.map, {3, 3}, StructureType::Workbench, nobody));
    DP_CHECK(!BuildSystem::placeable(house.map, {8, 3}, StructureType::Workbench, nobody)); // outdoors
    DP_CHECK(BuildSystem::placeable(house.map, {8, 3}, StructureType::RainCollector, nobody));
    DP_CHECK(!BuildSystem::placeable(house.map, {3, 3}, StructureType::RainCollector, nobody)); // indoors
    DP_CHECK(!BuildSystem::placeable(house.map, {1, 2}, StructureType::Bedroll, nobody));       // wall
    DP_CHECK(!BuildSystem::placeable(house.map, {1, 3}, StructureType::Bedroll, nobody));       // doorway
    DP_CHECK(!BuildSystem::placeable(house.map, {3, 3}, StructureType::Bedroll, {glm::ivec2(3, 3)}));
    house.map.setFurniture(4, 3, true);
    DP_CHECK(!BuildSystem::placeable(house.map, {4, 3}, StructureType::StorageCrate, nobody));
    // A campfire: outdoors, or indoors on a hard floor, not on wood.
    DP_CHECK(BuildSystem::placeable(house.map, {8, 3}, StructureType::Campfire, nobody));
    DP_CHECK(BuildSystem::placeable(house.map, {3, 3}, StructureType::Campfire, nobody));
    House wooden(TileGid::kWood);
    DP_CHECK(!BuildSystem::placeable(wooden.map, {3, 3}, StructureType::Campfire, nobody));

    const StationRecipe* bench = BuildSystem::recipeFor(StructureType::Workbench);
    DP_CHECK(bench != nullptr);
    DP_CHECK(bench->needsHammer);
    DP_CHECK(bench->needsSaw);
    DP_CHECK(BuildSystem::recipeFor(StructureType::SpikeBoard) == nullptr); // placed, not built
}

DP_TEST(base_what_lasts_into_the_next_life)
{
    DP_CHECK(BuildSystem::carriesOver(StructureType::Workbench, false));
    DP_CHECK(BuildSystem::carriesOver(StructureType::StorageCrate, false));
    DP_CHECK(BuildSystem::carriesOver(StructureType::RainCollector, false));
    DP_CHECK(BuildSystem::carriesOver(StructureType::SpikeBoard, false));
    DP_CHECK(!BuildSystem::carriesOver(StructureType::Campfire, false));
    DP_CHECK(!BuildSystem::carriesOver(StructureType::Bedroll, false));
    DP_CHECK(!BuildSystem::carriesOver(StructureType::Light, false));
    DP_CHECK(BuildSystem::carriesOver(StructureType::CanAlarm, false));
    DP_CHECK(!BuildSystem::carriesOver(StructureType::CanAlarm, true));
    DP_CHECK(BuildSystem::wearsBetweenLives(StructureType::Workbench));
    DP_CHECK(BuildSystem::wearsBetweenLives(StructureType::RainCollector));
    DP_CHECK(BuildSystem::wearsBetweenLives(StructureType::StorageCrate)); // phase 10
    // Rain wears wooden things outdoors twice as fast.
    DP_CHECK(BuildSystem::decayPerHour(StructureType::StorageCrate, true, true) >
             BuildSystem::decayPerHour(StructureType::StorageCrate, true, false));
}

DP_TEST(base_memory_survives_a_save)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(4321u);
    {
        SaveManager save;
        DP_CHECK(save.loadOrCreateSlot("base_slot"));
        FurnitureMemoryRecord memory;
        memory.moves.push_back(FurnitureMoveRecord{3, 3, 2, 3});
        memory.removed.emplace_back(5, 4);
        save.setFurnitureMemory(memory);
        DP_CHECK(save.saveFurnitureMemory());
        save.setWindows({WindowRecord{7, 2, true, 450.0f, false}, WindowRecord{4, 1, false, 0.0f, true}});
        save.setBaseRecord(BaseRecord{3, 120.0f, 96.0f, 2});
        save.setLastHome(LastHomeRecord{3, true});
        DP_CHECK(save.saveWorldMemory());
        StructureRecord collector;
        collector.type = static_cast<int>(StructureType::RainCollector);
        collector.x = 64.0f;
        collector.y = 96.0f;
        collector.water = 1.5f;
        StructureRecord alarm;
        alarm.type = static_cast<int>(StructureType::CanAlarm);
        alarm.targetX = 1;
        alarm.targetY = 3;
        StructureRecord spikes;
        spikes.type = static_cast<int>(StructureType::SpikeBoard);
        spikes.uses = 2;
        DP_CHECK(save.saveStructures({collector, alarm, spikes}));
    }
    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("base_slot"));
    const FurnitureMemoryRecord& memory = save.furnitureMemory();
    DP_CHECK_EQ(memory.moves.size(), 1u);
    if (!memory.moves.empty())
    {
        DP_CHECK_EQ(memory.moves[0].fromX, 3);
        DP_CHECK_EQ(memory.moves[0].toX, 2);
    }
    DP_CHECK_EQ(memory.removed.size(), 1u);
    DP_CHECK_EQ(save.windows().size(), 2u);
    if (save.windows().size() == 2u)
    {
        DP_CHECK(save.windows()[0].broken);
        DP_CHECK_NEAR(save.windows()[0].boardHp, 450.0f, 0.01);
        DP_CHECK(save.windows()[1].covered);
    }
    DP_CHECK_EQ(save.baseRecord().building, 3);
    DP_CHECK_EQ(save.baseRecord().runIndex, 2);
    DP_CHECK_EQ(save.lastHome().building, 3);
    DP_CHECK(save.lastHome().diedAway);
    DP_CHECK_EQ(save.structures().size(), 3u);
    if (save.structures().size() == 3u)
    {
        DP_CHECK_NEAR(save.structures()[0].water, 1.5f, 0.01);
        DP_CHECK_EQ(save.structures()[1].targetX, 1);
        DP_CHECK_EQ(save.structures()[1].targetY, 3);
        DP_CHECK_EQ(save.structures()[2].uses, 2);
    }
}
