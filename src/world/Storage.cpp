#include "world/Storage.h"

#include <algorithm>

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
    const AddItemResult result = InventorySystem::addItemWithCondition(to, db, slot.itemId, wanted, slot.condition);
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
        record.items.push_back(ContainerItemRecord{slot.itemId, slot.count, slot.condition});
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
        InventorySystem::addItemWithCondition(inv, db, item.itemId, item.count, item.condition);
    }
}

float spoilScale(FurnitureKind kind, bool powerOn)
{
    return kind == FurnitureKind::Fridge && powerOn ? 0.5f : 1.0f;
}
} // namespace storage
