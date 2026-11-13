#include "TestFramework.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "items/ItemDatabase.h"
#include "save/SaveManager.h"
#include "systems/CraftingSystem.h"
#include "systems/InventorySystem.h"
#include "world/Storage.h"

namespace
{
ItemDatabase items()
{
    ItemDatabase db;
    DP_CHECK(db.loadFromFile(std::string(DEAD_PIXEL_ASSETS_DIR) + "/data/items.json"));
    return db;
}

class TempCwd
{
public:
    TempCwd()
    {
        static std::atomic<int> counter{0};
        mOriginal = std::filesystem::current_path();
        mPath = std::filesystem::temp_directory_path() /
                ("dead_pixel_storage_" + std::to_string(++counter) + "_" +
                 std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::remove_all(mPath);
        std::filesystem::create_directories(mPath);
        std::filesystem::current_path(mPath);
    }
    ~TempCwd()
    {
        std::error_code ec;
        std::filesystem::current_path(mOriginal, ec);
        std::filesystem::remove_all(mPath, ec);
    }
    TempCwd(const TempCwd&) = delete;
    TempCwd& operator=(const TempCwd&) = delete;

private:
    std::filesystem::path mOriginal;
    std::filesystem::path mPath;
};

nlohmann::json readJson(const std::string& path)
{
    std::ifstream in(path);
    return nlohmann::json::parse(in, nullptr, false);
}

void writeJson(const std::string& path, const nlohmann::json& json)
{
    std::ofstream out(path, std::ios::trunc);
    out << json.dump();
}
} // namespace

DP_TEST(storage_moves_things_in_and_out_keeping_condition)
{
    const ItemDatabase db = items();
    const int bread = db.idOf("bread");
    const int planks = db.idOf("wood_planks");

    Inventory pack{};
    Inventory cupboard = storage::emptyContainer(FurnitureKind::Wardrobe);
    DP_CHECK_EQ(cupboard.maxSlots, 12);
    DP_CHECK_NEAR(cupboard.maxWeight, 30.0f, 0.001);
    DP_CHECK_EQ(storage::emptyContainer(FurnitureKind::Fridge).maxSlots, 8);

    DP_CHECK(InventorySystem::addItemWithCondition(pack, db, bread, 3, 0.4f).added == 3);
    InventorySystem::addItem(pack, db, planks, 2);

    // One in.
    const int breadSlot = storage::nthFilledSlot(pack, 0);
    DP_CHECK_EQ(storage::moveItem(pack, breadSlot, cupboard, db, 1), 1);
    DP_CHECK_EQ(InventorySystem::countItem(pack, bread), 2);
    DP_CHECK_EQ(InventorySystem::countItem(cupboard, bread), 1);
    DP_CHECK_NEAR(cupboard.slots[static_cast<std::size_t>(storage::nthFilledSlot(cupboard, 0))].condition, 0.4f, 0.001);

    // The rest of the stack in, then all of it back out.
    DP_CHECK_EQ(storage::moveItem(pack, breadSlot, cupboard, db, 99), 2);
    DP_CHECK_EQ(InventorySystem::countItem(cupboard, bread), 3);
    DP_CHECK_EQ(storage::filledSlots(cupboard), 1);
    DP_CHECK_EQ(storage::moveItem(cupboard, storage::nthFilledSlot(cupboard, 0), pack, db, 99), 3);
    DP_CHECK_EQ(storage::filledSlots(cupboard), 0);
    DP_CHECK_EQ(InventorySystem::countItem(pack, bread), 3);
}

DP_TEST(storage_stops_at_slot_and_weight_limits)
{
    const ItemDatabase db = items();
    // Slots: a two-slot box takes two kinds of thing, not a third.
    Inventory box{};
    box.maxSlots = 2;
    box.maxWeight = 100.0f;
    Inventory pack{};
    InventorySystem::addItem(pack, db, db.idOf("knife"), 1);
    InventorySystem::addItem(pack, db, db.idOf("pipe"), 1);
    InventorySystem::addItem(pack, db, db.idOf("hammer"), 1);
    DP_CHECK_EQ(storage::moveItem(pack, storage::nthFilledSlot(pack, 0), box, db, 1), 1);
    DP_CHECK_EQ(storage::moveItem(pack, storage::nthFilledSlot(pack, 0), box, db, 1), 1);
    DP_CHECK_EQ(storage::moveItem(pack, storage::nthFilledSlot(pack, 0), box, db, 1), 0);
    DP_CHECK_EQ(storage::filledSlots(pack), 1); // what did not fit stays in the pack

    // Weight: only as many as the box can bear.
    const int planks = db.idOf("wood_planks");
    const ItemDef* plank = db.find(planks);
    DP_CHECK(plank != nullptr && plank->weight > 0.0f);
    Inventory light{};
    light.maxSlots = 12;
    light.maxWeight = plank->weight * 2.5f;
    Inventory carrier{};
    carrier.maxWeight = 1000.0f;
    InventorySystem::addItem(carrier, db, planks, 5);
    DP_CHECK_EQ(storage::moveItem(carrier, storage::nthFilledSlot(carrier, 0), light, db, 5), 2);
    DP_CHECK_EQ(InventorySystem::countItem(carrier, planks), 3);
}

DP_TEST(storage_records_round_trip_through_world_memory)
{
    TempCwd cwd;
    const ItemDatabase db = items();
    Inventory fridge = storage::emptyContainer(FurnitureKind::Fridge);
    InventorySystem::addItemWithCondition(fridge, db, db.idOf("bread"), 2, 0.6f);
    InventorySystem::addItem(fridge, db, db.idOf("bandage"), 1);

    {
        SaveManager save;
        DP_CHECK(save.loadOrCreateSlot("storage_slot"));
        save.setContainerRecord(storage::toRecord(fridge, 12, 7, 3));
        DP_CHECK(save.saveWorldMemory());
    }

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("storage_slot"));
    const ContainerRecord* record = reloaded.containerRecord(12, 7);
    DP_CHECK(record != nullptr);
    if (record == nullptr) return;
    DP_CHECK_EQ(record->runIndex, 3);
    Inventory back = storage::emptyContainer(FurnitureKind::Fridge);
    storage::fillFromRecord(back, *record, db);
    DP_CHECK_EQ(back.maxSlots, 8);
    DP_CHECK_EQ(InventorySystem::countItem(back, db.idOf("bread")), 2);
    DP_CHECK_EQ(InventorySystem::countItem(back, db.idOf("bandage")), 1);
    DP_CHECK_NEAR(back.slots[static_cast<std::size_t>(storage::nthFilledSlot(back, 0))].condition, 0.6f, 0.001);

    // A fridge keeps food twice as long while the power is on.
    DP_CHECK_NEAR(storage::spoilScale(FurnitureKind::Fridge, true), 0.5f, 0.001);
    DP_CHECK_NEAR(storage::spoilScale(FurnitureKind::Fridge, false), 1.0f, 0.001);
    DP_CHECK_NEAR(storage::spoilScale(FurnitureKind::Counter, true), 1.0f, 0.001);
}

DP_TEST(a_version_one_save_migrates_to_version_two)
{
    TempCwd cwd;
    SaveManager::setSeedOverride(5150u);
    {
        SaveManager fresh;
        DP_CHECK(fresh.loadOrCreateSlot("old_slot"));
        DP_CHECK(fresh.saveWorldMemory());
    }

    // Make it a version 1 slot: looted places, and the old molotov pair.
    nlohmann::json meta = readJson("save/old_slot.json");
    DP_CHECK(!meta.is_discarded());
    meta["saveVersion"] = 1;
    writeJson("save/old_slot.json", meta);
    nlohmann::json memory = nlohmann::json::object();
    memory["version"] = 1;
    memory["looted"] = nlohmann::json::array({{{"x", 3}, {"y", 4}}, {{"x", 9}, {"y", 1}}});
    memory["recipes"] = nlohmann::json::array({{{"a", 3}, {"b", 25}}});
    writeJson("save/old_slot_memory.json", memory);

    SaveManager migrated;
    DP_CHECK(migrated.loadOrCreateSlot("old_slot"));
    DP_CHECK_EQ(migrated.migratedFromVersion(), 1);
    DP_CHECK_EQ(migrated.worldSeed(), 5150u);

    // Looted places are empty records; the recipe is the bottle one now.
    const ContainerRecord* looted = migrated.containerRecord(3, 4);
    DP_CHECK(looted != nullptr && looted->items.empty());
    DP_CHECK(migrated.containerRecord(9, 1) != nullptr);
    DP_CHECK(migrated.isRecipeDiscovered(25, 29));
    DP_CHECK(!migrated.isRecipeDiscovered(3, 25));

    // The old files are kept; the new ones say version 2.
    DP_CHECK(std::filesystem::exists("save/old_slot.json.v1.backup"));
    DP_CHECK(std::filesystem::exists("save/old_slot_memory.json.v1.backup"));
    DP_CHECK_EQ(readJson("save/old_slot.json").value("saveVersion", 0), 2);

    // And it loads again as a plain version 2 slot.
    SaveManager again;
    DP_CHECK(again.loadOrCreateSlot("old_slot"));
    DP_CHECK_EQ(again.migratedFromVersion(), 0);
    DP_CHECK(again.containerRecord(3, 4) != nullptr);
    DP_CHECK(again.isRecipeDiscovered(25, 29));
}
