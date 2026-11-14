#include "items/ItemDatabase.h"

#include <nlohmann/json.hpp>

#include <algorithm>
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
    mKeyIndex.clear();
    mMissingLogged.clear();

    for (const auto& obj : root["items"])
    {
        ItemDef def;
        def.id = obj.value("id", 0);
        def.name = obj.value("name", std::string("Unknown"));
        def.category = parseCategory(obj.value("category", std::string("food")));
        def.weight = obj.value("weight", 1.0f);
        def.value = obj.value("value", 1.0f);
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
        def.weaponDamageScale = obj.value("damageScale", 1.0f);
        def.weaponRangeScale = obj.value("rangeScale", 1.0f);
        def.weaponDurabilityScale = obj.value("durabilityScale", 1.0f);
        def.weaponCausesBleeding = obj.value("causesBleeding", false);
        def.key = obj.value("key", std::string{});
        if (obj.contains("tags") && obj["tags"].is_array())
        {
            for (const auto& tag : obj["tags"])
            {
                if (tag.is_string()) def.tags.push_back(tag.get<std::string>());
            }
        }
        def.leavesKey = obj.value("leaves", std::string{});
        def.warmth = obj.value("warmth", 0.0f);
        def.depressionRelief = obj.value("depressionRelief", 0.0f);
        def.sleepDebtMask = obj.value("sleepDebtMask", 0.0f);
        def.rawIllness = obj.value("rawIllness", 0.0f);
        def.treats = obj.value("treats", std::string{});
        def.fits = obj.value("fits", std::string{});
        def.carryBonus = obj.value("carryBonus", 0.0f);
        if (obj.contains("weapon") && obj["weapon"].is_object())
        {
            const auto& w = obj["weapon"];
            def.weapon.arc = w.value("arc", -1.0f);
            def.weapon.range = w.value("range", -1.0f);
            def.weapon.damage = w.value("damage", -1.0f);
            def.weapon.stamina = w.value("stamina", -1.0f);
            def.weapon.knockback = w.value("knockback", -1.0f);
            def.weapon.durability = w.value("durability", -1);
            def.weapon.windup = w.value("windup", -1.0f);
            def.weapon.recovery = w.value("recovery", -1.0f);
            def.weapon.magazine = w.value("magazine", -1);
            def.weapon.ammo = w.value("ammo", std::string{});
            def.weapon.pellets = w.value("pellets", -1);
            def.weapon.noise = w.value("noise", -1);
            def.weapon.stick = w.value("stick", false);
            def.weapon.knockdown = w.value("knockdown", false);
            def.weapon.reach = w.value("reach", false);
        }
        if (obj.contains("throw") && obj["throw"].is_object())
        {
            const auto& t = obj["throw"];
            def.throwDef.throwable = true;
            def.throwDef.rangeTiles = t.value("range", 0.0f);
            def.throwDef.damage = t.value("damage", 0.0f);
            def.throwDef.noiseTier = t.value("noiseTier", 0);
            def.throwDef.noiseSeconds = t.value("noiseSeconds", 0.0f);
            def.throwDef.effect = t.value("effect", std::string("none"));
        }
        if (def.key.empty())
        {
            std::cerr << "ItemDatabase: item " << def.id << " (" << def.name << ") has no key\n";
        }

        const std::size_t idx = mItems.size();
        mIdIndex[def.id] = idx;
        mNameIndex[def.name] = idx;
        if (!def.key.empty())
        {
            if (mKeyIndex.count(def.key) > 0)
            {
                std::cerr << "ItemDatabase: duplicate key '" << def.key << "'\n";
            }
            mKeyIndex[def.key] = idx;
        }
        mItems.push_back(std::move(def));
    }

    for (ItemDef& def : mItems)
    {
        if (def.leavesKey.empty()) continue;
        def.leavesId = idOf(def.leavesKey);
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

const ItemDef* ItemDatabase::findByKey(std::string_view key) const
{
    auto it = mKeyIndex.find(std::string(key));
    if (it == mKeyIndex.end()) return nullptr;
    return &mItems[it->second];
}

int ItemDatabase::idOf(std::string_view key) const
{
    if (const ItemDef* def = findByKey(key))
    {
        return def->id;
    }
    const std::string missing(key);
    if (std::find(mMissingLogged.begin(), mMissingLogged.end(), missing) == mMissingLogged.end())
    {
        mMissingLogged.push_back(missing);
        std::cerr << "ItemDatabase: no item with key '" << missing << "'\n";
    }
    return -1;
}

bool ItemDef::hasTag(std::string_view tag) const
{
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}
