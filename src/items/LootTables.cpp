#include "items/LootTables.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include "core/Tuning.h"
#include "items/ItemDatabase.h"

namespace
{
// A float in [0, 1) from the generator, the same on every compiler.
float unit(std::mt19937& rng)
{
    return static_cast<float>(rng() >> 8) * (1.0f / 16777216.0f);
}

int between(std::mt19937& rng, int lo, int hi)
{
    if (hi <= lo) return lo;
    return lo + static_cast<int>(rng() % static_cast<std::uint32_t>(hi - lo + 1));
}
} // namespace

bool LootTables::load(const std::string& path, const ItemDatabase& db, std::string* error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        if (error) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return loadFromString(buffer.str(), db, error);
}

bool LootTables::loadFromString(const std::string& jsonText, const ItemDatabase& db, std::string* error)
{
    mClasses.clear();
    const auto failWith = [&](const std::string& message)
    {
        std::cerr << "LootTables: " << message << '\n';
        if (error) *error = message;
        mClasses.clear();
        return false;
    };

    const nlohmann::json root = nlohmann::json::parse(jsonText, nullptr, false);
    if (root.is_discarded() || !root.contains("classes") || !root["classes"].is_object())
    {
        return failWith("loot.json needs an object \"classes\"");
    }

    for (const auto& [key, classJson] : root["classes"].items())
    {
        LootClass lootClass;
        lootClass.stockedChance = classJson.value("stockedChance", 0.45f);
        lootClass.extraRollChance = classJson.value("extraRollChance", 0.25f);
        if (classJson.contains("rolls") && classJson["rolls"].is_array() && classJson["rolls"].size() == 2)
        {
            lootClass.minRolls = std::max(0, classJson["rolls"][0].get<int>());
            lootClass.maxRolls = std::max(lootClass.minRolls, classJson["rolls"][1].get<int>());
        }
        if (!classJson.contains("entries") || !classJson["entries"].is_array())
        {
            return failWith("class '" + key + "' has no entries");
        }
        for (const auto& entryJson : classJson["entries"])
        {
            const std::string itemKey = entryJson.value("item", std::string{});
            const ItemDef* def = db.findByKey(itemKey);
            if (def == nullptr)
            {
                return failWith("class '" + key + "' names unknown item '" + itemKey + "'");
            }
            LootEntry entry;
            entry.itemId = def->id;
            entry.weight = std::max(0.0f, entryJson.value("weight", 1.0f));
            entry.min = std::max(1, entryJson.value("min", 1));
            entry.max = std::max(entry.min, entryJson.value("max", entry.min));
            lootClass.entries.push_back(entry);
        }
        mClasses[key] = std::move(lootClass);
    }
    return true;
}

const LootClass* LootTables::find(std::string_view classKey) const
{
    const auto it = mClasses.find(std::string(classKey));
    return it != mClasses.end() ? &it->second : nullptr;
}

void LootTables::roll(std::string_view classKey, std::mt19937& rng, float suppliesScale,
                      std::vector<std::pair<int, int>>& out) const
{
    const LootClass* lootClass = find(classKey);
    if (lootClass == nullptr || lootClass->entries.empty())
    {
        return;
    }
    // Most cupboards are already bare; tone and tuning scale how many are not.
    const float stocked = std::clamp(
        lootClass->stockedChance * suppliesScale * Tuning::get("loot.stocked.scale", 1.0f), 0.0f, 0.9f);
    if (unit(rng) >= stocked)
    {
        return;
    }

    float totalWeight = 0.0f;
    for (const LootEntry& entry : lootClass->entries) totalWeight += entry.weight;
    if (totalWeight <= 0.0f)
    {
        return;
    }

    for (int stack = 0; stack < lootClass->maxRolls; ++stack)
    {
        if (stack >= lootClass->minRolls && unit(rng) >= lootClass->extraRollChance)
        {
            break;
        }
        float pick = unit(rng) * totalWeight;
        const LootEntry* chosen = &lootClass->entries.back();
        for (const LootEntry& entry : lootClass->entries)
        {
            if (pick < entry.weight)
            {
                chosen = &entry;
                break;
            }
            pick -= entry.weight;
        }
        const int count = between(rng, chosen->min, chosen->max);
        const auto merged = std::find_if(out.begin(), out.end(),
                                         [&](const std::pair<int, int>& s) { return s.first == chosen->itemId; });
        if (merged != out.end())
        {
            merged->second += count;
        }
        else
        {
            out.emplace_back(chosen->itemId, count);
        }
    }
}
