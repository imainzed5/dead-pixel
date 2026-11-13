#pragma once

#include "components/Inventory.h"
#include "items/ItemDatabase.h"

struct AddItemResult
{
    int added = 0;
    int overflow = 0;
};

class InventorySystem
{
public:
    static AddItemResult addItem(Inventory& inv, const ItemDatabase& db, int itemId, int count = 1);
    static bool removeItem(Inventory& inv, int itemId, int count = 1);
    static int countItem(const Inventory& inv, int itemId);
    static float totalWeight(const Inventory& inv, const ItemDatabase& db);
    static bool isFull(const Inventory& inv, const ItemDatabase& db, int itemId);
    static void degradeFood(Inventory& inv, const ItemDatabase& db, float gameHoursElapsed);
    // Items in the pack carrying `tag` (e.g. "tool:hammer").
    static int countTagged(const Inventory& inv, const ItemDatabase& db, const char* tag);
    // The warmest item carried: clothing counts while it is in the pack.
    static float bestWarmth(const Inventory& inv, const ItemDatabase& db);
    // Adds items that keep a condition of their own (moved between packs and
    // containers). Stacking averages the condition by count.
    static AddItemResult addItemWithCondition(Inventory& inv, const ItemDatabase& db, int itemId, int count, float condition);
    // Uses up one item in `slotIndex`, leaving behind what its data says it
    // leaves (a Water Bottle leaves an Empty Bottle).
    static void consumeOne(Inventory& inv, const ItemDatabase& db, int slotIndex);
};
