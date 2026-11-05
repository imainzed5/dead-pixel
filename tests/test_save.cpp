#include "TestFramework.h"

#include <atomic>
#include <filesystem>
#include <string>

#include "map/MapGenerator.h"
#include "save/SaveManager.h"

namespace
{
// SaveManager writes to "save/" relative to the working directory, so each test
// runs inside its own fresh temporary directory.
class ScopedTempCwd
{
public:
    ScopedTempCwd()
    {
        static std::atomic<int> counter{0};
        mOriginal = std::filesystem::current_path();
        mPath = std::filesystem::temp_directory_path() /
                ("dead_pixel_test_" + std::to_string(++counter) + "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
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
} // namespace

DP_TEST(save_new_slot_uses_seed_override_and_persists_meta)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(424242u);

    SaveManager first;
    DP_CHECK(first.loadOrCreateSlot("test_slot"));
    DP_CHECK_EQ(first.worldSeed(), 424242u);
    DP_CHECK_EQ(first.runNumber(), 1);

    SaveManager second;
    DP_CHECK(second.loadOrCreateSlot("test_slot"));
    DP_CHECK_EQ(second.worldSeed(), 424242u);
}

DP_TEST(save_world_layout_round_trips)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(99u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("layout_slot"));
    DP_CHECK(!save.hasWorldLayout());

    MapGenerator gen;
    const LayoutData original = gen.generateLayout(save.worldSeed());
    DP_CHECK(save.saveWorldLayout(original));
    DP_CHECK(save.hasWorldLayout());

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("layout_slot"));
    const LayoutData loaded = reloaded.loadWorldLayout();
    DP_CHECK_EQ(loaded.width, original.width);
    DP_CHECK_EQ(loaded.height, original.height);
    DP_CHECK(loaded.ground == original.ground);
    DP_CHECK(loaded.collision == original.collision);
    DP_CHECK(loaded.surface == original.surface);
    DP_CHECK(loaded.district == original.district);
    DP_CHECK_EQ(loaded.buildings.size(), original.buildings.size());
    DP_CHECK_EQ(loaded.doors.size(), original.doors.size());
}

DP_TEST(save_death_records_grave_and_advances_run)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(7u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("grave_slot"));
    const std::uint32_t firstRunSeed = save.currentRunSeed();

    GraveRecord grave;
    grave.name = "Ada Vance";
    grave.x = 320.0f;
    grave.y = 480.0f;
    grave.day = 4;
    grave.runIndex = 1;
    grave.cause = "Bitten";
    grave.items = {{0, 2}, {5, 1}};
    grave.risen = true;
    DP_CHECK(save.recordDeath(grave));
    DP_CHECK_EQ(save.runNumber(), 2);
    DP_CHECK(save.currentRunSeed() != firstRunSeed);

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("grave_slot"));
    DP_CHECK_EQ(reloaded.runNumber(), 2);
    DP_CHECK_EQ(reloaded.graves().size(), std::size_t{1});
    DP_CHECK_EQ(reloaded.graves()[0].name, std::string("Ada Vance"));
    DP_CHECK_EQ(reloaded.graves()[0].day, 4);
    DP_CHECK_EQ(reloaded.graves()[0].cause, std::string("Bitten"));
    DP_CHECK_NEAR(reloaded.graves()[0].x, 320.0f, 0.001);
    DP_CHECK(reloaded.graves()[0].risen);
    DP_CHECK(!reloaded.graves()[0].laidToRest);
    DP_CHECK_EQ(reloaded.graves()[0].items.size(), std::size_t{2});
    DP_CHECK_EQ(reloaded.graves()[0].items[1].first, 5);

    // A later run puts them down and takes the pack.
    GraveRecord rested = reloaded.graves()[0];
    rested.risen = false;
    rested.laidToRest = true;
    rested.items.clear();
    reloaded.updateGrave(0, rested);
    DP_CHECK(reloaded.flushLegacy());

    SaveManager third;
    DP_CHECK(third.loadOrCreateSlot("grave_slot"));
    DP_CHECK(!third.graves()[0].risen);
    DP_CHECK(third.graves()[0].laidToRest);
    DP_CHECK(third.graves()[0].items.empty());
}

DP_TEST(save_world_memory_round_trips)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(11u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("memory_slot"));
    DP_CHECK(save.addLootedPosition(10, 12));
    DP_CHECK(save.addDiscoveredRecipe(3, 5));

    VehicleRecord sedan;
    sedan.x = 100.0f;
    sedan.y = 200.0f;
    sedan.condition = 55.0f;
    sedan.fuel = 9.5f;
    sedan.items = {{4, 2}};
    save.setVehicleRecords({sedan});
    save.setBurnedTiles({{1, 2}, {3, 4}, {1, 2}});
    save.setBrokenDoors({{7, 8}});
    DP_CHECK(save.saveLootedPositions());

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("memory_slot"));
    DP_CHECK(reloaded.loadLootedPositions());
    DP_CHECK_EQ(reloaded.lootedPositions().size(), std::size_t{1});
    DP_CHECK(reloaded.isRecipeDiscovered(3, 5));
    DP_CHECK_EQ(reloaded.vehicles().size(), std::size_t{1});
    DP_CHECK_NEAR(reloaded.vehicles()[0].condition, 55.0f, 0.001);
    DP_CHECK_NEAR(reloaded.vehicles()[0].fuel, 9.5f, 0.001);
    DP_CHECK_EQ(reloaded.vehicles()[0].items.size(), std::size_t{1});
    DP_CHECK_EQ(reloaded.burnedTiles().size(), std::size_t{2}); // duplicates collapse
    DP_CHECK_EQ(reloaded.brokenDoors().size(), std::size_t{1});
    DP_CHECK_EQ(reloaded.brokenDoors()[0].first, 7);
}

DP_TEST(save_checkpoint_round_trips_and_is_run_scoped)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(21u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("cp_slot"));

    RunCheckpoint cp;
    cp.valid = true;
    cp.runSeed = save.currentRunSeed();
    cp.characterName = "Mara Quill";
    cp.currentDay = 3;
    cp.gameHours = 14.5;
    cp.playerX = 640.0f;
    cp.playerY = 320.0f;
    cp.healthCurrent = 62.0f;
    cp.needs.hunger = 40.0f;
    cp.mental.depression = 33.0f;
    cp.inventory.slots[0].itemId = 2;
    cp.inventory.slots[0].count = 3;
    cp.kills = 5;
    cp.openDoors = {{4, 5}};
    cp.brokenDoors = {{6, 7}, {8, 9}};
    cp.burnedTiles = {{1, 1}};
    FireTileRecord fire;
    fire.tileX = 2;
    fire.tileY = 3;
    fire.intensity = 1.2f;
    fire.lifeRemaining = 20.0f;
    cp.fires = {fire};
    cp.equippedSlot = 0;
    ZombieRecord zombie;
    zombie.x = 320.0f;
    zombie.y = 96.0f;
    zombie.health = 18.0f;
    zombie.maxHealth = 55.0f;
    zombie.baseMoveSpeed = 120.0f;
    zombie.spawnDay = 2;
    ZombieRecord dead = zombie;
    dead.health = 0.0f;
    cp.zombies = {zombie, dead};
    DP_CHECK(save.saveCheckpoint(cp));

    const RunCheckpoint loaded = save.loadCheckpoint();
    DP_CHECK(loaded.valid);
    DP_CHECK_EQ(loaded.characterName, std::string("Mara Quill"));
    DP_CHECK_EQ(loaded.currentDay, 3);
    DP_CHECK_NEAR(loaded.gameHours, 14.5, 0.001);
    DP_CHECK_NEAR(loaded.healthCurrent, 62.0f, 0.001);
    DP_CHECK_NEAR(loaded.needs.hunger, 40.0f, 0.001);
    DP_CHECK_NEAR(loaded.mental.depression, 33.0f, 0.001);
    DP_CHECK_EQ(loaded.inventory.slots[0].itemId, 2);
    DP_CHECK_EQ(loaded.inventory.slots[0].count, 3);
    DP_CHECK_EQ(loaded.kills, 5);
    // The town as the run left it.
    DP_CHECK_EQ(loaded.openDoors.size(), std::size_t{1});
    DP_CHECK_EQ(loaded.openDoors[0].second, 5);
    DP_CHECK_EQ(loaded.brokenDoors.size(), std::size_t{2});
    DP_CHECK_EQ(loaded.burnedTiles.size(), std::size_t{1});
    DP_CHECK_EQ(loaded.fires.size(), std::size_t{1});
    DP_CHECK_EQ(loaded.fires[0].tileY, 3);
    DP_CHECK_NEAR(loaded.fires[0].lifeRemaining, 20.0f, 0.001);
    DP_CHECK_EQ(loaded.equippedSlot, 0);
    // The dead still walking, wounds and all; the slain do not come back.
    DP_CHECK(loaded.zombiesSaved);
    DP_CHECK_EQ(loaded.zombies.size(), std::size_t{1});
    DP_CHECK_NEAR(loaded.zombies[0].x, 320.0f, 0.001);
    DP_CHECK_NEAR(loaded.zombies[0].health, 18.0f, 0.001);
    DP_CHECK_NEAR(loaded.zombies[0].maxHealth, 55.0f, 0.001);
    DP_CHECK_EQ(loaded.zombies[0].spawnDay, 2);

    // A death ends the run: the old checkpoint must not restore into the next run.
    GraveRecord grave;
    grave.name = "Mara Quill";
    DP_CHECK(save.recordDeath(grave));
    DP_CHECK(!save.loadCheckpoint().valid);
}
