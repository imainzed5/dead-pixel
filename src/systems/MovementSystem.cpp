#include "systems/MovementSystem.h"

#include "components/PlayerInput.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"

void MovementSystem::update(World& world, float dtSeconds) const
{
    world.forEach<Transform, Velocity>(
        [&](Entity entity, Transform& transform, Velocity& velocity)
        {
            if (world.hasComponent<PlayerInput>(entity))
            {
                return;
            }

            if (world.hasComponent<ZombieAI>(entity))
            {
                return;
            }

            transform.x += velocity.dx * dtSeconds;
            transform.y += velocity.dy * dtSeconds;
        });
}
