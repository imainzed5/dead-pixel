#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "ecs/Types.h"

// A uniform grid of square cells (4x4 tiles by default) holding entity
// positions, rebuilt each tick: "who is near here" without visiting every
// entity. Items are kept sorted by cell, so a query reads a few short runs.
class SpatialGrid
{
public:
    enum Kind : std::uint8_t
    {
        Zombie = 1,
        Survivor = 2,
        Corpse = 4
    };

    struct Item
    {
        Entity entity = kInvalidEntity;
        glm::vec2 position{0.0f};
        std::uint8_t kind = Zombie;
    };

    void reset(float worldWidth, float worldHeight, float cellSize)
    {
        mCellSize = std::max(1.0f, cellSize);
        mCols = std::max(1, static_cast<int>(std::ceil(worldWidth / mCellSize)));
        mRows = std::max(1, static_cast<int>(std::ceil(worldHeight / mCellSize)));
        mPending.clear();
        mItems.clear();
        mStart.assign(static_cast<std::size_t>(mCols * mRows + 1), 0);
    }

    void insert(Entity entity, const glm::vec2& position, std::uint8_t kind)
    {
        mPending.push_back(Item{entity, position, kind});
    }

    // Sort what was inserted into cells. Call once after the inserts.
    void finish()
    {
        std::fill(mStart.begin(), mStart.end(), 0);
        for (const Item& item : mPending) ++mStart[static_cast<std::size_t>(cellOf(item.position)) + 1];
        for (std::size_t i = 1; i < mStart.size(); ++i) mStart[i] += mStart[i - 1];
        mItems.resize(mPending.size());
        std::vector<int> fill(mStart.begin(), mStart.end() - 1);
        for (const Item& item : mPending)
        {
            mItems[static_cast<std::size_t>(fill[static_cast<std::size_t>(cellOf(item.position))]++)] = item;
        }
        mPending.clear();
    }

    // Calls fn(const Item&) for every item of `kinds` within `radius` of `at`.
    template <typename Fn>
    void query(const glm::vec2& at, float radius, std::uint8_t kinds, Fn&& fn) const
    {
        if (mItems.empty()) return;
        const int x0 = std::clamp(static_cast<int>(std::floor((at.x - radius) / mCellSize)), 0, mCols - 1);
        const int x1 = std::clamp(static_cast<int>(std::floor((at.x + radius) / mCellSize)), 0, mCols - 1);
        const int y0 = std::clamp(static_cast<int>(std::floor((at.y - radius) / mCellSize)), 0, mRows - 1);
        const int y1 = std::clamp(static_cast<int>(std::floor((at.y + radius) / mCellSize)), 0, mRows - 1);
        const float r2 = radius * radius;
        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                const int cell = y * mCols + x;
                for (int i = mStart[static_cast<std::size_t>(cell)]; i < mStart[static_cast<std::size_t>(cell) + 1]; ++i)
                {
                    const Item& item = mItems[static_cast<std::size_t>(i)];
                    if ((item.kind & kinds) == 0) continue;
                    const glm::vec2 d = item.position - at;
                    if (glm::dot(d, d) <= r2) fn(item);
                }
            }
        }
    }

    [[nodiscard]] int count(const glm::vec2& at, float radius, std::uint8_t kinds) const
    {
        int n = 0;
        query(at, radius, kinds, [&](const Item&) { ++n; });
        return n;
    }

    [[nodiscard]] std::size_t size() const { return mItems.size(); }

private:
    [[nodiscard]] int cellOf(const glm::vec2& p) const
    {
        const int x = std::clamp(static_cast<int>(std::floor(p.x / mCellSize)), 0, mCols - 1);
        const int y = std::clamp(static_cast<int>(std::floor(p.y / mCellSize)), 0, mRows - 1);
        return y * mCols + x;
    }

    float mCellSize = 128.0f;
    int mCols = 1;
    int mRows = 1;
    std::vector<Item> mPending;
    std::vector<Item> mItems;
    std::vector<int> mStart{0, 0};
};
