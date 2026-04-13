#include "systems/InventorySystem.h"

#include <algorithm>

AddItemResult InventorySystem::addItem(Inventory& inv, const ItemDatabase& db, int itemId, int count)
{
    AddItemResult result{};
    const ItemDef* def = db.find(itemId);
    if (!def || count <= 0) return result;

    // Weight check: reject if adding would exceed carry limit
    const float currentWeight = totalWeight(inv, db);
    const float addWeight = def->weight * static_cast<float>(count);
    if (currentWeight + addWeight > Inventory::kMaxCarryWeight + 0.01f)
    {
        // Allow partial add up to weight limit
        const float room = Inventory::kMaxCarryWeight - currentWeight;
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
    for (auto& slot : inv.slots)
    {
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
    for (auto& slot : inv.slots)
    {
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

    for (const auto& slot : inv.slots)
    {
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
