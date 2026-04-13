#include "items/ItemDatabase.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
ItemCategory parseCategory(const std::string& str)
{
    if (str == "food")     return ItemCategory::Food;
    if (str == "drink")    return ItemCategory::Drink;
    if (str == "medical")  return ItemCategory::Medical;
    if (str == "weapon")   return ItemCategory::Weapon;
    if (str == "material") return ItemCategory::Material;
    return ItemCategory::Food;
}
}

bool ItemDatabase::loadFromFile(const std::string& jsonPath)
{
    std::ifstream input(jsonPath, std::ios::binary);
    if (!input)
    {
        std::cerr << "ItemDatabase: Cannot open " << jsonPath << '\n';
        return false;
    }

    std::ostringstream buf;
    buf << input.rdbuf();
    const std::string contents = buf.str();

    nlohmann::json root = nlohmann::json::parse(contents, nullptr, false);
    if (root.is_discarded() || !root.contains("items"))
    {
        std::cerr << "ItemDatabase: Invalid JSON in " << jsonPath << '\n';
        return false;
    }

    mItems.clear();
    mIdIndex.clear();
    mNameIndex.clear();

    for (const auto& obj : root["items"])
    {
        ItemDef def;
        def.id = obj.value("id", 0);
        def.name = obj.value("name", std::string("Unknown"));
        def.category = parseCategory(obj.value("category", std::string("food")));
        def.weight = obj.value("weight", 1.0f);
        def.maxStack = obj.value("maxStack", 1);
        def.sprite = obj.value("sprite", std::string(""));
        def.hungerRestore = obj.value("hungerRestore", 0.0f);
        def.thirstRestore = obj.value("thirstRestore", 0.0f);
        def.healthRestore = obj.value("healthRestore", 0.0f);
        def.illnessRelief = obj.value("illnessRelief", 0.0f);
        def.panicRelief = obj.value("panicRelief", 0.0f);
        def.woundTreatment = obj.value("woundTreatment", 0);
        def.stopsBleeding = obj.value("stopsBleeding", false);
        def.curesInfection = obj.value("curesInfection", false);
        def.spoilHours = obj.value("spoilHours", 0.0f);
        def.weaponCategory = obj.value("weaponCategory", -1);

        const std::size_t idx = mItems.size();
        mIdIndex[def.id] = idx;
        mNameIndex[def.name] = idx;
        mItems.push_back(std::move(def));
    }

    std::cout << "ItemDatabase: Loaded " << mItems.size() << " items.\n";
    return true;
}

const ItemDef* ItemDatabase::find(int itemId) const
{
    auto it = mIdIndex.find(itemId);
    if (it == mIdIndex.end()) return nullptr;
    return &mItems[it->second];
}

const ItemDef* ItemDatabase::findByName(const std::string& name) const
{
    auto it = mNameIndex.find(name);
    if (it == mNameIndex.end()) return nullptr;
    return &mItems[it->second];
}
