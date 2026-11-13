#pragma once

#include "components/Inventory.h"
#include "save/SaveManager.h"
#include "world/Furniture.h"

class ItemDatabase;

// Putting things into containers and taking them out, and what the world
// remembers of it. Plain rules, apart from the game, so they can be tested.
namespace storage
{
// A container's room: 12 slots and 30 kg by default, fridges 8, crates 16.
[[nodiscard]] Inventory emptyContainer(FurnitureKind kind);

// The loot class (assets/data/loot.json) a piece of furniture rolls from.
[[nodiscard]] const char* lootClassFor(FurnitureLoot loot);

// Moves up to `count` of what is in `from.slots[fromSlot]` into `to`, keeping
// its condition. Stops at `to`'s slot and weight limits. Returns how many moved.
int moveItem(Inventory& from, int fromSlot, Inventory& to, const ItemDatabase& db, int count);

// The index of the nth filled slot (in slot order), or -1.
[[nodiscard]] int nthFilledSlot(const Inventory& inv, int n);
[[nodiscard]] int filledSlots(const Inventory& inv);

// A container's contents as world memory, and back.
[[nodiscard]] ContainerRecord toRecord(const Inventory& inv, int tileX, int tileY, int runIndex);
void fillFromRecord(Inventory& inv, const ContainerRecord& record, const ItemDatabase& db);

// How fast food spoils in a container: half speed in a fridge with the power
// on (phase 6 turns the power off; until then it is always on).
[[nodiscard]] float spoilScale(FurnitureKind kind, bool powerOn);
} // namespace storage
