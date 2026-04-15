#include "systems/CraftingSystem.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

std::pair<int, int> CraftingSystem::normalizePair(int itemA, int itemB)
{
    if (itemA <= itemB)
    {
        return {itemA, itemB};
    }
    return {itemB, itemA};
}

std::uint64_t CraftingSystem::makeKey(int itemA, int itemB)
{
    const auto normalized = normalizePair(itemA, itemB);
    const std::uint32_t a = static_cast<std::uint32_t>(normalized.first);
    const std::uint32_t b = static_cast<std::uint32_t>(normalized.second);
    return (static_cast<std::uint64_t>(a) << 32u) | static_cast<std::uint64_t>(b);
}

bool CraftingSystem::loadRecipesFromFile(const std::string& jsonPath)
{
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
        recipe.inputA = recipeJson.value("a", -1);
        recipe.inputB = recipeJson.value("b", -1);
        recipe.outputItemId = recipeJson.value("out", -1);
        recipe.outputCount = std::max(1, recipeJson.value("count", 1));
        recipe.label = recipeJson.value("name", std::string{});
        recipe.craftTimeSeconds = std::max(1, recipeJson.value("craftTimeSeconds", 60));
        recipe.category = recipeJson.value("category", std::string("ALL"));
        recipe.notes = recipeJson.value("notes", std::string{});
        recipe.startsKnown = recipeJson.value("startsKnown", false);
        recipe.requiresWorkbench = recipeJson.value("requiresWorkbench", false);

        recipe.requiredTools.clear();
        if (recipeJson.contains("tools") && recipeJson["tools"].is_array())
        {
            for (const auto& tool : recipeJson["tools"])
            {
                const int toolId = tool.get<int>();
                if (toolId >= 0)
                {
                    recipe.requiredTools.push_back(toolId);
                }
            }
        }

        if (recipe.inputA < 0 || recipe.inputB < 0 || recipe.outputItemId < 0)
        {
            continue;
        }

        const auto normalized = normalizePair(recipe.inputA, recipe.inputB);
        recipe.inputA = normalized.first;
        recipe.inputB = normalized.second;

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

    std::cout << "CraftingSystem: Loaded " << mRecipes.size() << " recipes.\n";
    return !mRecipes.empty();
}

const CraftingRecipe* CraftingSystem::findRecipe(int itemA, int itemB) const
{
    if (itemA < 0 || itemB < 0)
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
