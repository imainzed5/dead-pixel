#pragma once

#include "ecs/Types.h"

#include <array>
#include <queue>
#include <vector>

class EntityManager
{
public:
    EntityManager();

    Entity createEntity();
    void destroyEntity(Entity entity);

    [[nodiscard]] bool isAlive(Entity entity) const;
    [[nodiscard]] const std::vector<Entity>& livingEntities() const;

private:
    std::queue<Entity> mAvailableEntities;
    std::vector<Entity> mLivingEntities;
    std::array<bool, kMaxEntities> mAlive{};
};
