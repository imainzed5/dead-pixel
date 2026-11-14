#pragma once

#include <string>
#include <string_view>
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

// Per-item weapon values that win over the class defaults (phase 3 fills these).
struct WeaponOverrides
{
    float arc = -1.0f;        // degrees; < 0 = class default
    float range = -1.0f;      // px
    float damage = -1.0f;
    float stamina = -1.0f;
    float knockback = -1.0f;  // px
    int durability = -1;
    float windup = -1.0f;     // s
    float recovery = -1.0f;   // s
    int magazine = -1;
    std::string ammo;         // item key of the ammunition
    int pellets = -1;
    int noise = -1;           // noise tier
    bool stick = false;       // a heavy blow may lodge it
    bool knockdown = false;   // any hit puts them on the floor
    bool reach = false;       // hits before a lunge lands
};

// How an item flies when thrown (phase 3).
struct ThrowDef
{
    bool throwable = false;
    float rangeTiles = 0.0f;
    float damage = 0.0f;
    int noiseTier = 0;
    float noiseSeconds = 0.0f;
    std::string effect = "none"; // none, fire, ring, flare
};

struct ItemDef
{
    int id = 0;
    std::string key; // unique snake-case name used by code and other data files
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
    // Crafted weapons improve on their category's stats.
    float weaponDamageScale = 1.0f;
    float weaponRangeScale = 1.0f;
    float weaponDurabilityScale = 1.0f;
    bool weaponCausesBleeding = false;

    std::vector<std::string> tags; // tool:hammer, fuel, light, warm, ...
    std::string leavesKey;         // item left behind when one is used up
    int leavesId = -1;             // resolved from leavesKey after load
    float warmth = 0.0f;           // clothing insulation while carried; the best item counts
    float depressionRelief = 0.0f;
    float sleepDebtMask = 0.0f;    // game hours the item hides tiredness
    float rawIllness = 0.0f;       // illness when eaten uncooked
    // What it does to a wound (phase 5): dress, dress_clean, stitch, splint,
    // cure, disinfect; empty when nothing. See world/WoundRules.h.
    std::string treats;
    // Phase 6: where it is useful, for the pack's detail line ("Fits: ...").
    std::string fits;
    float carryBonus = 0.0f; // kg added to the carry limit while carried (one counts)
    WeaponOverrides weapon;
    ThrowDef throwDef;

    [[nodiscard]] bool hasTag(std::string_view tag) const;
};

class ItemDatabase
{
public:
    bool loadFromFile(const std::string& jsonPath);

    [[nodiscard]] const ItemDef* find(int itemId) const;
    [[nodiscard]] const ItemDef* findByName(const std::string& name) const;
    [[nodiscard]] const ItemDef* findByKey(std::string_view key) const;
    // The id for a key, or -1 (logged once per missing key).
    [[nodiscard]] int idOf(std::string_view key) const;
    [[nodiscard]] const std::vector<ItemDef>& allItems() const { return mItems; }

private:
    std::vector<ItemDef> mItems;
    std::unordered_map<int, std::size_t> mIdIndex;
    std::unordered_map<std::string, std::size_t> mNameIndex;
    std::unordered_map<std::string, std::size_t> mKeyIndex;
    mutable std::vector<std::string> mMissingLogged;
};
