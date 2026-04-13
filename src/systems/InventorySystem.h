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
};
