#include "systems/BleedingSystem.h"

#include <algorithm>

#include "components/Bleeding.h"
#include "components/Health.h"

void BleedingSystem::update(World& world, float dtSeconds) const
{
    std::vector<Entity> curedEntities;

    world.forEach<Bleeding>(
        [&](Entity entity, Bleeding& bleeding)
        {
            bleeding.remainingSeconds -= dtSeconds;

            if (bleeding.remainingSeconds <= 0.0f)
            {
                curedEntities.push_back(entity);
                return;
            }

            if (world.hasComponent<Health>(entity))
            {
                Health& health = world.getComponent<Health>(entity);
                health.current = std::max(0.0f, health.current - bleeding.bleedRate * dtSeconds);
            }
        });

    for (Entity entity : curedEntities)
    {
        if (world.hasComponent<Bleeding>(entity))
        {
            world.removeComponent<Bleeding>(entity);
        }
    }
}
