#pragma once

#include <string>
#include <unordered_map>
#include <vector>

enum class ItemCategory
{
    Food,
    Drink,
    Medical,
    Weapon,
    Material
};

struct ItemDef
{
    int id = 0;
    std::string name;
    ItemCategory category = ItemCategory::Food;
    float weight = 1.0f;
    int maxStack = 1;
    std::string sprite;
    float hungerRestore = 0.0f;
    float thirstRestore = 0.0f;
    float healthRestore = 0.0f;
    float illnessRelief = 0.0f;
    float panicRelief = 0.0f;
    int woundTreatment = 0;
    bool stopsBleeding = false;
    bool curesInfection = false;
    float spoilHours = 0.0f;
    int weaponCategory = -1; // -1 = not a weapon
};

class ItemDatabase
{
public:
    bool loadFromFile(const std::string& jsonPath);

    [[nodiscard]] const ItemDef* find(int itemId) const;
    [[nodiscard]] const ItemDef* findByName(const std::string& name) const;
    [[nodiscard]] const std::vector<ItemDef>& allItems() const { return mItems; }

private:
    std::vector<ItemDef> mItems;
    std::unordered_map<int, std::size_t> mIdIndex;
    std::unordered_map<std::string, std::size_t> mNameIndex;
};
