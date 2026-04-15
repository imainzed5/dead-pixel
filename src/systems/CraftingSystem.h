#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct CraftingRecipe
{
    int inputA = -1;
    int inputB = -1;
    int outputItemId = -1;
    int outputCount = 1;
    std::string label;
    std::vector<int> requiredTools;
    int craftTimeSeconds = 60;
    std::string category = "ALL";
    std::string notes;
    bool startsKnown = false;
    bool requiresWorkbench = false;
};

class CraftingSystem
{
public:
    bool loadRecipesFromFile(const std::string& jsonPath);

    [[nodiscard]] const CraftingRecipe* findRecipe(int itemA, int itemB) const;
    [[nodiscard]] const std::vector<CraftingRecipe>& recipes() const { return mRecipes; }

    static std::pair<int, int> normalizePair(int itemA, int itemB);

private:
    static std::uint64_t makeKey(int itemA, int itemB);

    std::vector<CraftingRecipe> mRecipes;
    std::unordered_map<std::uint64_t, std::size_t> mRecipeIndex;
};
