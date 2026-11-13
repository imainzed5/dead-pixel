#pragma once

#include <array>

struct InventorySlot
{
    int itemId = -1;         // -1 = empty
    int count = 0;
    float condition = 1.0f;  // 1.0 = fresh, 0.0 = spoiled/broken
};

struct Inventory
{
    static constexpr int kMaxSlots = 20;           // storage for the largest pack or container
    static constexpr float kMaxCarryWeight = 25.0f; // the player's limit

    std::array<InventorySlot, kMaxSlots> slots{};
    int activeSlot = 0; // hotbar selection (0-7)
    // How much this inventory holds: the player 20 slots and 25 kg; a
    // container 12 and 30 by default (fridges 8). Slots past maxSlots stay empty.
    int maxSlots = kMaxSlots;
    float maxWeight = kMaxCarryWeight;
};
