#include "systems/CraftingSystem.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "items/ItemDatabase.h"

std::pair<int, int> CraftingSystem::normalizePair(int itemA, int itemB)
{
    if (itemA <= itemB)
    {
        return {itemA, itemB};
    }
    return {itemB, itemA};
}

std::pair<int, int> CraftingSystem::migrateKnownPair(int itemA, int itemB)
{
    // Item ids in saves never change meaning (0-29), so the old pair is named
    // by number here, once, for the migration.
    constexpr int kOldMolotovBottle = 3;  // water_bottle
    constexpr int kNewMolotovBottle = 29; // empty_bottle
    constexpr int kSoakedRag = 25;        // soaked_rag
    const auto pair = normalizePair(itemA, itemB);
    if (pair == normalizePair(kOldMolotovBottle, kSoakedRag))
    {
        return normalizePair(kNewMolotovBottle, kSoakedRag);
    }
    return pair;
}

std::uint64_t CraftingSystem::makeKey(int itemA, int itemB)
{
    const auto normalized = normalizePair(itemA, itemB);
    const std::uint32_t a = static_cast<std::uint32_t>(normalized.first);
    const std::uint32_t b = static_cast<std::uint32_t>(normalized.second);
    return (static_cast<std::uint64_t>(a) << 32u) | static_cast<std::uint64_t>(b);
}

bool CraftingSystem::loadRecipesFromFile(const std::string& jsonPath, const ItemDatabase& db)
{
    mErrors.clear();
    std::ifstream input(jsonPath, std::ios::binary);
    if (!input)
    {
        std::cerr << "CraftingSystem: Cannot open " << jsonPath << '\n';
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    nlohmann::json root = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (root.is_discarded() || !root.contains("recipes") || !root["recipes"].is_array())
    {
        std::cerr << "CraftingSystem: Invalid JSON in " << jsonPath << '\n';
        return false;
    }

    mRecipes.clear();
    mRecipeIndex.clear();

    for (const auto& recipeJson : root["recipes"])
    {
        CraftingRecipe recipe;
        recipe.label = recipeJson.value("name", std::string{});
        bool ok = true;
        const auto item = [&](const char* field, bool required) -> int
        {
            if (!recipeJson.contains(field) || !recipeJson[field].is_string())
            {
                if (required)
                {
                    mErrors.push_back("recipe '" + recipe.label + "' has no " + field);
                    ok = false;
                }
                return -1;
            }
            const std::string key = recipeJson[field].get<std::string>();
            const ItemDef* def = db.findByKey(key);
            if (def == nullptr)
            {
                mErrors.push_back("recipe '" + recipe.label + "': unknown item '" + key + "' in " + field);
                ok = false;
                return -1;
            }
            return def->id;
        };
        recipe.inputA = item("inputA", true);
        recipe.inputB = item("inputB", false);
        recipe.outputItemId = item("output", true);
        recipe.countA = std::max(1, recipeJson.value("countA", 1));
        recipe.countB = std::max(1, recipeJson.value("countB", 1));
        recipe.outputCount = std::max(1, recipeJson.value("count", 1));
        recipe.craftTimeSeconds = std::max(1, recipeJson.value("craftTimeSeconds", 60));
        recipe.category = recipeJson.value("category", std::string("ALL"));
        recipe.notes = recipeJson.value("notes", std::string{});
        recipe.hintA = recipeJson.value("hintA", std::string{});
        recipe.hintB = recipeJson.value("hintB", std::string{});
        recipe.startsKnown = recipeJson.value("startsKnown", false);

        std::string station = recipeJson.value("station", std::string("none"));
        if (recipeJson.value("requiresWorkbench", false)) station = "workbench";
        if (station == "workbench") recipe.station = CraftStation::Workbench;
        else if (station == "fire") recipe.station = CraftStation::Fire;
        else if (station != "none")
        {
            mErrors.push_back("recipe '" + recipe.label + "': unknown station '" + station + "'");
            ok = false;
        }

        if (recipeJson.contains("tools") && recipeJson["tools"].is_array())
        {
            for (const auto& toolJson : recipeJson["tools"])
            {
                const std::string name = toolJson.is_string() ? toolJson.get<std::string>() : std::string{};
                CraftTool tool;
                if (name.find(':') != std::string::npos)
                {
                    tool.tag = name;
                    for (const ItemDef& def : db.allItems())
                    {
                        if (def.hasTag(name))
                        {
                            tool.iconItemId = def.id;
                            break;
                        }
                    }
                    if (tool.iconItemId < 0)
                    {
                        mErrors.push_back("recipe '" + recipe.label + "': no item carries tag '" + name + "'");
                        ok = false;
                    }
                }
                else if (const ItemDef* def = db.findByKey(name))
                {
                    tool.itemId = def->id;
                    tool.iconItemId = def->id;
                }
                else
                {
                    mErrors.push_back("recipe '" + recipe.label + "': unknown tool '" + name + "'");
                    ok = false;
                }
                recipe.tools.push_back(tool);
            }
        }

        if (!ok)
        {
            continue;
        }

        // Two inputs are kept in id order (the order known recipes are saved
        // in), carrying their counts and clues with them. One input stays in A.
        if (recipe.inputB >= 0 && recipe.inputB < recipe.inputA)
        {
            std::swap(recipe.inputA, recipe.inputB);
            std::swap(recipe.countA, recipe.countB);
            std::swap(recipe.hintA, recipe.hintB);
        }

        const std::size_t index = mRecipes.size();
        const std::uint64_t key = makeKey(recipe.inputA, recipe.inputB);

        // Last duplicate in file wins.
        auto existing = mRecipeIndex.find(key);
        if (existing != mRecipeIndex.end())
        {
            mRecipes[existing->second] = recipe;
            continue;
        }

        mRecipeIndex[key] = index;
        mRecipes.push_back(std::move(recipe));
    }

    for (const std::string& error : mErrors)
    {
        std::cerr << "CraftingSystem: " << error << '\n';
    }
    std::cout << "CraftingSystem: Loaded " << mRecipes.size() << " recipes.\n";
    return !mRecipes.empty();
}

const CraftingRecipe* CraftingSystem::findRecipe(int itemA, int itemB) const
{
    if (itemA < 0 && itemB < 0)
    {
        return nullptr;
    }

    const auto it = mRecipeIndex.find(makeKey(itemA, itemB));
    if (it == mRecipeIndex.end())
    {
        return nullptr;
    }

    return &mRecipes[it->second];
}
