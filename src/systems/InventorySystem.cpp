#include "systems/InventorySystem.h"

#include <algorithm>
#include <array>

AddItemResult InventorySystem::addItem(Inventory& inv, const ItemDatabase& db, int itemId, int count)
{
    AddItemResult result{};
    const ItemDef* def = db.find(itemId);
    if (!def || count <= 0) return result;

    // Weight check: reject if adding would exceed carry limit
    const float currentWeight = totalWeight(inv, db);
    const float addWeight = def->weight * static_cast<float>(count);
    if (currentWeight + addWeight > inv.maxWeight + 0.01f)
    {
        // Allow partial add up to weight limit
        const float room = inv.maxWeight - currentWeight;
        if (room <= 0.01f || def->weight <= 0.0f)
        {
            result.overflow = count;
            return result;
        }
        const int maxByWeight = static_cast<int>(room / def->weight);
        if (maxByWeight <= 0)
        {
            result.overflow = count;
            return result;
        }
        count = std::min(count, maxByWeight);
    }

    int remaining = count;

    // First try stacking into existing slots
    const int usable = std::clamp(inv.maxSlots, 0, Inventory::kMaxSlots);
    for (int i = 0; i < usable; ++i)
    {
        auto& slot = inv.slots[static_cast<std::size_t>(i)];
        if (remaining <= 0) break;
        if (slot.itemId == itemId && slot.count < def->maxStack)
        {
            const int space = def->maxStack - slot.count;
            const int toAdd = std::min(remaining, space);
            slot.count += toAdd;
            remaining -= toAdd;
            result.added += toAdd;
        }
    }

    // Then use empty slots
    for (int i = 0; i < usable; ++i)
    {
        auto& slot = inv.slots[static_cast<std::size_t>(i)];
        if (remaining <= 0) break;
        if (slot.itemId < 0)
        {
            const int toAdd = std::min(remaining, def->maxStack);
            slot.itemId = itemId;
            slot.count = toAdd;
            slot.condition = 1.0f;
            remaining -= toAdd;
            result.added += toAdd;
        }
    }

    result.overflow = remaining;
    return result;
}

bool InventorySystem::removeItem(Inventory& inv, int itemId, int count)
{
    int remaining = count;

    // Remove from last slots first (preserve hotbar items)
    for (int i = Inventory::kMaxSlots - 1; i >= 0; --i)
    {
        if (remaining <= 0) break;
        auto& slot = inv.slots[static_cast<std::size_t>(i)];
        if (slot.itemId == itemId)
        {
            const int toRemove = std::min(remaining, slot.count);
            slot.count -= toRemove;
            remaining -= toRemove;
            if (slot.count <= 0)
            {
                slot.itemId = -1;
                slot.count = 0;
                slot.condition = 1.0f;
            }
        }
    }

    return remaining <= 0;
}

int InventorySystem::countItem(const Inventory& inv, int itemId)
{
    int total = 0;
    for (const auto& slot : inv.slots)
    {
        if (slot.itemId == itemId)
        {
            total += slot.count;
        }
    }
    return total;
}

float InventorySystem::totalWeight(const Inventory& inv, const ItemDatabase& db)
{
    float weight = 0.0f;
    for (const auto& slot : inv.slots)
    {
        if (slot.itemId >= 0)
        {
            const ItemDef* def = db.find(slot.itemId);
            if (def)
            {
                weight += def->weight * static_cast<float>(slot.count);
            }
        }
    }
    return weight;
}

bool InventorySystem::isFull(const Inventory& inv, const ItemDatabase& db, int itemId)
{
    const ItemDef* def = db.find(itemId);
    if (!def) return true;

    const int usable = std::clamp(inv.maxSlots, 0, Inventory::kMaxSlots);
    for (int i = 0; i < usable; ++i)
    {
        const auto& slot = inv.slots[static_cast<std::size_t>(i)];
        if (slot.itemId < 0) return false; // empty slot available
        if (slot.itemId == itemId && slot.count < def->maxStack) return false;
    }
    return true;
}

void InventorySystem::degradeFood(Inventory& inv, const ItemDatabase& db, float gameHoursElapsed)
{
    for (auto& slot : inv.slots)
    {
        if (slot.itemId < 0) continue;
        const ItemDef* def = db.find(slot.itemId);
        if (!def) continue;
        if (def->spoilHours <= 0.0f) continue;

        // Condition drops from 1.0 to 0.0 over spoilHours
        const float decay = gameHoursElapsed / def->spoilHours;
        slot.condition = std::max(0.0f, slot.condition - decay);
    }
}

int InventorySystem::countTagged(const Inventory& inv, const ItemDatabase& db, const char* tag)
{
    int total = 0;
    for (const auto& slot : inv.slots)
    {
        if (slot.itemId < 0 || slot.count <= 0) continue;
        const ItemDef* def = db.find(slot.itemId);
        if (def != nullptr && def->hasTag(tag)) total += slot.count;
    }
    return total;
}

float InventorySystem::bestWarmth(const Inventory& inv, const ItemDatabase& db)
{
    float best = 0.0f;
    for (const auto& slot : inv.slots)
    {
        if (slot.itemId < 0 || slot.count <= 0) continue;
        const ItemDef* def = db.find(slot.itemId);
        if (def != nullptr) best = std::max(best, def->warmth);
    }
    return best;
}

AddItemResult InventorySystem::addItemWithCondition(Inventory& inv, const ItemDatabase& db, int itemId, int count, float condition)
{
    // Remember what each slot held, add, then blend the condition into the
    // slots that grew.
    std::array<int, Inventory::kMaxSlots> before{};
    for (std::size_t i = 0; i < inv.slots.size(); ++i)
    {
        before[i] = inv.slots[i].itemId == itemId ? inv.slots[i].count : 0;
    }
    const AddItemResult result = addItem(inv, db, itemId, count);
    for (std::size_t i = 0; i < inv.slots.size(); ++i)
    {
        InventorySlot& slot = inv.slots[i];
        if (slot.itemId != itemId || slot.count <= before[i]) continue;
        const float added = static_cast<float>(slot.count - before[i]);
        const float kept = static_cast<float>(before[i]);
        const float previous = before[i] > 0 ? slot.condition : condition;
        slot.condition = (previous * kept + condition * added) / (kept + added);
    }
    return result;
}

void InventorySystem::consumeOne(Inventory& inv, const ItemDatabase& db, int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= Inventory::kMaxSlots) return;
    InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    if (slot.itemId < 0 || slot.count <= 0) return;
    const ItemDef* def = db.find(slot.itemId);
    slot.count -= 1;
    if (slot.count <= 0)
    {
        slot = InventorySlot{};
    }
    if (def != nullptr && def->leavesId >= 0)
    {
        addItem(inv, db, def->leavesId, 1);
    }
}
