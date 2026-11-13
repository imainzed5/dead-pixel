#include "TestFramework.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <map>
#include <random>
#include <string>

#include "items/ItemDatabase.h"
#include "items/ItemIds.h"
#include "items/LootTables.h"
#include "systems/BuildSystem.h"
#include "systems/CraftingSystem.h"

namespace
{
std::string assetPath(const char* relative)
{
    return std::string(DEAD_PIXEL_ASSETS_DIR) + "/" + relative;
}

ItemDatabase loadItems()
{
    ItemDatabase db;
    DP_CHECK(db.loadFromFile(assetPath("data/items.json")));
    return db;
}

// Two items, enough for loot rules.
ItemDatabase tinyDatabase()
{
    const std::string path = "test_content_items.json";
    {
        std::ofstream out(path, std::ios::trunc);
        out << R"({"items": [
            {"id": 0, "key": "beans", "name": "Beans", "category": "food", "weight": 0.4, "maxStack": 5, "sprite": "item_canned_food"},
            {"id": 1, "key": "rag", "name": "Rag", "category": "material", "weight": 0.1, "maxStack": 5, "sprite": "item_rag"}
        ]})";
    }
    ItemDatabase db;
    DP_CHECK(db.loadFromFile(path));
    std::remove(path.c_str());
    return db;
}
} // namespace

DP_TEST(every_item_key_the_code_uses_resolves)
{
    const ItemDatabase db = loadItems();
    ItemIds ids;
    DP_CHECK(ids.resolve(db));
    for (const auto& [key, field] : ids.entries())
    {
        DP_CHECK(*field >= 0);
        const ItemDef* def = db.findByKey(key);
        DP_CHECK(def != nullptr && def->id == *field);
    }
    // Ids 0-29 keep their meaning in old saves.
    DP_CHECK_EQ(db.findByKey("canned_food")->id, 0);
    DP_CHECK_EQ(db.findByKey("empty_bottle")->id, 29);
    // Using up a water bottle leaves an empty one, by data.
    DP_CHECK_EQ(db.findByKey("water_bottle")->leavesId, ids.emptyBottle);
}

DP_TEST(every_recipe_and_build_names_items_that_exist)
{
    const ItemDatabase db = loadItems();
    CraftingSystem crafting;
    DP_CHECK(crafting.loadRecipesFromFile(assetPath("data/recipes.json"), db));
    DP_CHECK(crafting.errors().empty());
    DP_CHECK(!crafting.recipes().empty());
    for (const CraftingRecipe& recipe : crafting.recipes())
    {
        DP_CHECK(db.find(recipe.inputA) != nullptr);
        DP_CHECK(recipe.inputB < 0 || db.find(recipe.inputB) != nullptr);
        DP_CHECK(db.find(recipe.outputItemId) != nullptr);
    }
    for (int i = 0; i < BuildSystem::kRecipeCount; ++i)
    {
        DP_CHECK(db.findByKey(BuildSystem::kRecipes[i].materialKey) != nullptr);
    }

    // The molotov is an empty bottle and a soaked rag now; an old notebook's
    // (water bottle, soaked rag) is read as that.
    const auto molotov = CraftingSystem::normalizePair(db.idOf("empty_bottle"), db.idOf("soaked_rag"));
    DP_CHECK(CraftingSystem::migrateKnownPair(3, 25) == molotov);
}

DP_TEST(every_item_sprite_is_in_the_atlas)
{
    const ItemDatabase db = loadItems();
    std::ifstream file(assetPath("sprites/spritesheet.json"));
    DP_CHECK(file.good());
    const nlohmann::json atlas = nlohmann::json::parse(file);
    for (const ItemDef& item : db.allItems())
    {
        DP_CHECK(atlas["sprites"].contains(item.sprite));
    }
}

DP_TEST(the_loot_file_loads_and_names_known_items)
{
    const ItemDatabase db = loadItems();
    LootTables loot;
    std::string error;
    DP_CHECK(loot.load(assetPath("data/loot.json"), db, &error));
    DP_CHECK(error.empty());
    for (const char* key : {"fridge", "kitchen", "bathroom", "bedroom", "living", "store", "checkout", "storage",
                            "workshop", "trunk"})
    {
        DP_CHECK(loot.find(key) != nullptr);
    }
}

DP_TEST(a_loot_class_with_one_entry_always_yields_it)
{
    const ItemDatabase db = tinyDatabase();
    LootTables loot;
    DP_CHECK(loot.loadFromString(
        R"({"classes": {"jar": {"stockedChance": 0.9, "rolls": [1, 1], "entries": [{"item": "beans", "min": 2, "max": 2}]}}})",
        db));
    std::mt19937 rng(7u);
    int stocked = 0;
    for (int i = 0; i < 1000; ++i)
    {
        std::vector<std::pair<int, int>> out;
        loot.roll("jar", rng, 1.0f, out);
        if (out.empty()) continue;
        ++stocked;
        DP_CHECK_EQ(out.size(), std::size_t{1});
        DP_CHECK_EQ(out[0].first, 0);
        DP_CHECK_EQ(out[0].second, 2);
    }
    DP_CHECK(stocked > 850 && stocked < 950);
}

DP_TEST(loot_weights_hold_over_many_rolls)
{
    const ItemDatabase db = tinyDatabase();
    LootTables loot;
    DP_CHECK(loot.loadFromString(R"({"classes": {"box": {"stockedChance": 0.9, "rolls": [1, 1], "entries": [
        {"item": "beans", "weight": 3}, {"item": "rag", "weight": 1}]}}})",
                                 db));
    std::mt19937 rng(11u);
    std::map<int, int> counts;
    int total = 0;
    for (int i = 0; i < 10000; ++i)
    {
        std::vector<std::pair<int, int>> out;
        loot.roll("box", rng, 1.0f, out);
        for (const auto& [id, count] : out)
        {
            counts[id] += 1;
            ++total;
        }
    }
    DP_CHECK(total > 8000);
    const double beans = static_cast<double>(counts[0]) / static_cast<double>(total);
    DP_CHECK_NEAR(beans, 0.75, 0.03);
}

DP_TEST(loot_naming_an_unknown_item_fails_with_a_message)
{
    const ItemDatabase db = tinyDatabase();
    LootTables loot;
    std::string error;
    DP_CHECK(!loot.loadFromString(R"({"classes": {"box": {"entries": [{"item": "unobtainium"}]}}})", db, &error));
    DP_CHECK(error.find("unobtainium") != std::string::npos);
    DP_CHECK(loot.find("box") == nullptr);
}
