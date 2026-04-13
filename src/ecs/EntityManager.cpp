#include "ecs/EntityManager.h"

#include <algorithm>
#include <stdexcept>

EntityManager::EntityManager()
{
    for (Entity entity = 0; entity < kMaxEntities; ++entity)
    {
        mAvailableEntities.push(entity);
    }
}

Entity EntityManager::createEntity()
{
    if (mAvailableEntities.empty())
    {
        throw std::runtime_error("Maximum entity count reached.");
    }

    const Entity entity = mAvailableEntities.front();
    mAvailableEntities.pop();

    mAlive[entity] = true;
    mLivingEntities.push_back(entity);
    return entity;
}

void EntityManager::destroyEntity(Entity entity)
{
    if (!isAlive(entity))
    {
        return;
    }

    mAlive[entity] = false;

    auto it = std::find(mLivingEntities.begin(), mLivingEntities.end(), entity);
    if (it != mLivingEntities.end())
    {
        *it = mLivingEntities.back();
        mLivingEntities.pop_back();
    }

    mAvailableEntities.push(entity);
}

bool EntityManager::isAlive(Entity entity) const
{
    if (entity >= kMaxEntities)
    {
        return false;
    }

    return mAlive[entity];
}

const std::vector<Entity>& EntityManager::livingEntities() const
{
    return mLivingEntities;
}
