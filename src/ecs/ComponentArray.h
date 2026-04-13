#pragma once

#include "ecs/Types.h"

#include <array>
#include <cstddef>
#include <stdexcept>

class IComponentArray
{
public:
    virtual ~IComponentArray() = default;
    virtual void entityDestroyed(Entity entity) = 0;
};

template <typename T>
class ComponentArray final : public IComponentArray
{
public:
    void insertData(Entity entity, const T& component)
    {
        if (entity >= kMaxEntities)
        {
            throw std::runtime_error("Entity index out of bounds in insertData.");
        }

        if (hasData(entity))
        {
            throw std::runtime_error("Component added to same entity twice.");
        }

        const std::size_t newIndex = mSize;
        mEntityToIndex[entity] = newIndex;
        mIndexToEntity[newIndex] = entity;
        mComponentData[newIndex] = component;
        mHasData[entity] = true;
        ++mSize;
    }

    void removeData(Entity entity)
    {
        if (!hasData(entity))
        {
            throw std::runtime_error("Removing component that does not exist.");
        }

        const std::size_t removedIndex = mEntityToIndex[entity];
        const std::size_t lastIndex = mSize - 1;

        if (removedIndex != lastIndex)
        {
            mComponentData[removedIndex] = mComponentData[lastIndex];

            const Entity movedEntity = mIndexToEntity[lastIndex];
            mEntityToIndex[movedEntity] = removedIndex;
            mIndexToEntity[removedIndex] = movedEntity;
        }

        mHasData[entity] = false;
        --mSize;
    }

    T& getData(Entity entity)
    {
        if (!hasData(entity))
        {
            throw std::runtime_error("Accessing component that does not exist.");
        }

        return mComponentData[mEntityToIndex[entity]];
    }

    const T& getData(Entity entity) const
    {
        if (!hasData(entity))
        {
            throw std::runtime_error("Accessing component that does not exist.");
        }

        return mComponentData[mEntityToIndex[entity]];
    }

    [[nodiscard]] bool hasData(Entity entity) const
    {
        if (entity >= kMaxEntities)
        {
            return false;
        }

        return mHasData[entity];
    }

    void entityDestroyed(Entity entity) override
    {
        if (hasData(entity))
        {
            removeData(entity);
        }
    }

private:
    std::array<T, kMaxEntities> mComponentData{};
    std::array<std::size_t, kMaxEntities> mEntityToIndex{};
    std::array<Entity, kMaxEntities> mIndexToEntity{};
    std::array<bool, kMaxEntities> mHasData{};
    std::size_t mSize = 0;
};
