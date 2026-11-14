#include "world/Storage.h"

#include <algorithm>
#include <random>
#include <string>

#include <glm/geometric.hpp>

#include "items/ItemDatabase.h"
#include "systems/InventorySystem.h"

namespace storage
{
Inventory emptyContainer(FurnitureKind kind)
{
    constexpr int kContainerSlots = 12;
    constexpr int kFridgeSlots = 8;
    constexpr int kCrateSlots = 16;
    constexpr float kContainerWeightKg = 30.0f;

    Inventory inv{};
    inv.maxSlots = kind == FurnitureKind::Fridge ? kFridgeSlots
                 : kind == FurnitureKind::Crates ? kCrateSlots
                                                 : kContainerSlots;
    inv.maxWeight = kContainerWeightKg;
    return inv;
}

const char* lootClassFor(FurnitureLoot loot)
{
    switch (loot)
    {
    case FurnitureLoot::Fridge: return "fridge";
    case FurnitureLoot::Kitchen: return "kitchen";
    case FurnitureLoot::Bathroom: return "bathroom";
    case FurnitureLoot::Bedroom: return "bedroom";
    case FurnitureLoot::Living: return "living";
    case FurnitureLoot::Store: return "store";
    case FurnitureLoot::Checkout: return "checkout";
    case FurnitureLoot::Storage: return "storage";
    case FurnitureLoot::Workshop: return "workshop";
    case FurnitureLoot::None: break;
    }
    return "";
}

int moveItem(Inventory& from, int fromSlot, Inventory& to, const ItemDatabase& db, int count)
{
    if (fromSlot < 0 || fromSlot >= Inventory::kMaxSlots || count <= 0)
    {
        return 0;
    }
    InventorySlot& slot = from.slots[static_cast<std::size_t>(fromSlot)];
    if (slot.itemId < 0 || slot.count <= 0)
    {
        return 0;
    }
    const int wanted = std::min(count, slot.count);
    const AddItemResult result = InventorySystem::addItemWithCondition(to, db, slot.itemId, wanted, slot.condition, slot.loaded);
    slot.count -= result.added;
    if (slot.count <= 0)
    {
        slot = InventorySlot{};
    }
    return result.added;
}

int nthFilledSlot(const Inventory& inv, int n)
{
    int seen = 0;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const InventorySlot& slot = inv.slots[static_cast<std::size_t>(i)];
        if (slot.itemId < 0 || slot.count <= 0) continue;
        if (seen == n) return i;
        ++seen;
    }
    return -1;
}

int filledSlots(const Inventory& inv)
{
    int filled = 0;
    for (const InventorySlot& slot : inv.slots)
    {
        if (slot.itemId >= 0 && slot.count > 0) ++filled;
    }
    return filled;
}

ContainerRecord toRecord(const Inventory& inv, int tileX, int tileY, int runIndex)
{
    ContainerRecord record;
    record.tileX = tileX;
    record.tileY = tileY;
    record.runIndex = runIndex;
    for (const InventorySlot& slot : inv.slots)
    {
        if (slot.itemId < 0 || slot.count <= 0) continue;
        record.items.push_back(ContainerItemRecord{slot.itemId, slot.count, slot.condition, slot.loaded});
    }
    return record;
}

void fillFromRecord(Inventory& inv, const ContainerRecord& record, const ItemDatabase& db)
{
    const int maxSlots = inv.maxSlots;
    const float maxWeight = inv.maxWeight;
    inv = Inventory{};
    inv.maxSlots = maxSlots;
    inv.maxWeight = maxWeight;
    for (const ContainerItemRecord& item : record.items)
    {
        InventorySystem::addItemWithCondition(inv, db, item.itemId, item.count, item.condition, item.loaded);
    }
}

float spoilScale(FurnitureKind kind, bool powerOn)
{
    return kind == FurnitureKind::Fridge && powerOn ? 0.5f : 1.0f;
}
std::vector<RareFind> rareFinds(const std::vector<FurniturePiece>& pieces, const glm::vec2& centreTile,
                                std::uint32_t worldSeed)
{
    std::vector<glm::ivec2> wardrobes;
    for (const FurniturePiece& piece : pieces)
    {
        if (piece.kind == FurnitureKind::Wardrobe) wardrobes.push_back(piece.tile);
    }
    // Farthest first; the tile breaks ties so the order never depends on
    // the order pieces were made in.
    const auto distance = [&](const glm::ivec2& t) { return glm::length(glm::vec2(t) - centreTile); };
    std::sort(wardrobes.begin(), wardrobes.end(),
              [&](const glm::ivec2& a, const glm::ivec2& b)
              {
                  const float da = distance(a);
                  const float db = distance(b);
                  if (da != db) return da > db;
                  return a.x != b.x ? a.x < b.x : a.y < b.y;
              });

    // The guns and the heavy blades: the outer third of the town's wardrobes
    // (at least enough for all), as in phase 3.
    const std::vector<RareFind> wanted = {
        {{}, "pistol", 1, 3},  {{}, "pistol", 1, 5},   {{}, "shotgun", 1, 0}, {{}, "machete", 1, 0},
        {{}, "machete", 1, 0}, {{}, "sledgehammer", 1, 0}, {{}, "sledgehammer", 1, 0},
    };
    std::vector<glm::ivec2> rest = wardrobes;
    const std::size_t pool = std::min(wardrobes.size(), std::max(wanted.size(), wardrobes.size() / 3));
    wardrobes.resize(pool);
    std::mt19937 rng(worldSeed ^ 0x6A5Fu);
    std::shuffle(wardrobes.begin(), wardrobes.end(), rng);

    std::vector<RareFind> finds;
    for (std::size_t i = 0; i < wanted.size() && i < wardrobes.size(); ++i)
    {
        RareFind find = wanted[i];
        find.tile = wardrobes[i];
        finds.push_back(find);
        if (std::string(find.item) == "shotgun")
        {
            finds.push_back({wardrobes[i], "shotgun_shells", 4, 0});
        }
    }

    // Phase 6: three radios, four backpacks and six keepsakes anywhere in
    // town, never in a wardrobe that already holds a find.
    const std::vector<RareFind> more = {
        {{}, "radio", 1, 0},    {{}, "radio", 1, 0},    {{}, "radio", 1, 0},    {{}, "backpack", 1, 0},
        {{}, "backpack", 1, 0}, {{}, "backpack", 1, 0}, {{}, "backpack", 1, 0}, {{}, "keepsake", 1, 0},
        {{}, "keepsake", 1, 0}, {{}, "keepsake", 1, 0}, {{}, "keepsake", 1, 0}, {{}, "keepsake", 1, 0},
        {{}, "keepsake", 1, 0},
    };
    std::erase_if(rest, [&](const glm::ivec2& t)
                  { return std::any_of(finds.begin(), finds.end(), [&](const RareFind& f) { return f.tile == t; }); });
    std::mt19937 moreRng(worldSeed ^ 0x6A60u);
    std::shuffle(rest.begin(), rest.end(), moreRng);
    for (std::size_t i = 0; i < more.size() && !rest.empty(); ++i)
    {
        RareFind find = more[i];
        find.tile = rest[i % rest.size()];
        finds.push_back(find);
    }
    return finds;
}
} // namespace storage
