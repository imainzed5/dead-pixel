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
    static constexpr int kMaxSlots = 20;
    static constexpr float kMaxCarryWeight = 25.0f;

    std::array<InventorySlot, kMaxSlots> slots{};
    int activeSlot = 0; // hotbar selection (0-7)
};
