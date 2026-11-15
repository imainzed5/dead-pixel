#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class ItemDatabase;

// Where a recipe has to be made.
enum class CraftStation
{
    None,
    Workbench, // within 3 tiles of a workbench
    Fire       // within 3 tiles of a fire (phase 6 adds stoves and campfires)
};

// A tool a recipe needs in the pack: one item, or any item carrying a tag
// ("tool:hammer" is met by a Hammer or an Axe).
struct CraftTool
{
    int itemId = -1;
    std::string tag;
    int iconItemId = -1; // the item drawn for it: itemId, or the first item with the tag
};

struct CraftingRecipe
{
    int inputA = -1;
    int inputB = -1; // -1: a one-input recipe
    int countA = 1;  // how many of each input one craft uses
    int countB = 1;
    int outputItemId = -1;
    int outputCount = 1;
    std::string label;
    std::vector<CraftTool> tools;
    CraftStation station = CraftStation::None;
    int craftTimeSeconds = 60;
    std::string category = "ALL";
    std::string notes;
    // Clues to each ingredient, shown when you hold only the other one.
    std::string hintA;
    std::string hintB;
    bool startsKnown = false;
    // The soft gate (phase 5): below this skill's level the craft takes twice
    // as long and fails 40 % of the time. skill -1: anyone can.
    int skill = -1; // a Skill, as int
    int skillLevel = 0;
};

class CraftingSystem
{
public:
    // Inputs, outputs and tools are named by item key (or tag, for tools).
    // A recipe naming a key the database lacks is skipped and listed in errors().
    bool loadRecipesFromFile(const std::string& jsonPath, const ItemDatabase& db);

    [[nodiscard]] const CraftingRecipe* findRecipe(int itemA, int itemB) const;
    [[nodiscard]] const std::vector<CraftingRecipe>& recipes() const { return mRecipes; }
    [[nodiscard]] const std::vector<std::string>& errors() const { return mErrors; }

    static std::pair<int, int> normalizePair(int itemA, int itemB);
    // Known-recipe pairs from older saves, rewritten to today's inputs: the
    // molotov was Water Bottle + Soaked Rag and is now Empty Bottle + Soaked Rag.
    static std::pair<int, int> migrateKnownPair(int itemA, int itemB);

private:
    static std::uint64_t makeKey(int itemA, int itemB);

    std::vector<CraftingRecipe> mRecipes;
    std::unordered_map<std::uint64_t, std::size_t> mRecipeIndex;
    std::vector<std::string> mErrors;
};
