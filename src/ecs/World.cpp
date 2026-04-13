#include "ecs/World.h"

Entity World::createEntity()
{
    return mEntityManager.createEntity();
}

void World::destroyEntity(Entity entity)
{
    mEntityManager.destroyEntity(entity);

    for (const auto& [_, componentArray] : mComponentArrays)
    {
        componentArray->entityDestroyed(entity);
    }
}

const std::vector<Entity>& World::livingEntities() const
{
    return mEntityManager.livingEntities();
}
