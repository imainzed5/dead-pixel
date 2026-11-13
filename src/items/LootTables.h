#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class ItemDatabase;

struct LootEntry
{
    int itemId = -1;
    float weight = 1.0f;
    int min = 1;
    int max = 1;
};

// One kind of place things are found in (a fridge, a workshop, a car trunk).
// A container is stocked with chance `stockedChance` (scaled by the tone's
// supplies and by tuning, at most 0.9). A stocked one gets `minRolls`
// stacks, then each further stack up to `maxRolls` with `extraRollChance`.
struct LootClass
{
    float stockedChance = 0.45f;
    int minRolls = 1;
    int maxRolls = 2;
    float extraRollChance = 0.25f;
    std::vector<LootEntry> entries;
};

// Loot classes from assets/data/loot.json, items named by key.
class LootTables
{
public:
    // False, with a message in `error`, when the file is unreadable or names
    // an item the database does not have.
    bool load(const std::string& path, const ItemDatabase& db, std::string* error = nullptr);
    bool loadFromString(const std::string& jsonText, const ItemDatabase& db, std::string* error = nullptr);

    [[nodiscard]] const LootClass* find(std::string_view classKey) const;
    [[nodiscard]] std::size_t size() const { return mClasses.size(); }

    // Rolls one container of class `classKey` and appends (itemId, count)
    // stacks to `out`. Unknown classes yield nothing.
    void roll(std::string_view classKey, std::mt19937& rng, float suppliesScale,
              std::vector<std::pair<int, int>>& out) const;

private:
    std::unordered_map<std::string, LootClass> mClasses;
};
