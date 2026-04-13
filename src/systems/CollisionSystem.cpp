#include "systems/CollisionSystem.h"

#include <glm/common.hpp>

#include "components/Structure.h"
#include "components/Transform.h"
#include "components/Velocity.h"

void CollisionSystem::update(World& world, const TileMap& tileMap, float dtSeconds) const
{
    world.forEach<Transform, Velocity>(
        [&](Entity self, Transform& transform, Velocity& velocity)
        {
            const float minCenterX = kCollisionWidth * 0.5f;
            const float minCenterY = kCollisionHeight * 0.5f;
            const float maxCenterX = tileMap.pixelSize().x - kCollisionWidth * 0.5f;
            const float maxCenterY = tileMap.pixelSize().y - kCollisionHeight * 0.5f;

            auto isBlocked = [&](float proposedX, float proposedY)
            {
                const float centerX = proposedX + kSpriteSize * 0.5f;
                const float centerY = proposedY + kCollisionCenterYOffset;

                // Reject positions whose collision center is outside the map
                if (centerX < minCenterX || centerX > maxCenterX ||
                    centerY < minCenterY || centerY > maxCenterY)
                {
                    return true;
                }

                const float left = centerX - kCollisionWidth * 0.5f;
                const float right = centerX + kCollisionWidth * 0.5f;
                const float top = centerY - kCollisionHeight * 0.5f;
                const float bottom = centerY + kCollisionHeight * 0.5f;

                if (tileMap.isSolidAtWorldRect(left, top, right, bottom))
                {
                    return true;
                }

                // Barricades block movement
                bool structBlocked = false;
                world.forEach<Structure, Transform>(
                    [&](Entity structEntity, Structure& structure, Transform& st)
                    {
                        if (structBlocked) return;
                        if (structEntity == self) return;
                        if (structure.type != StructureType::Barricade) return;
                        if (structure.hp <= 0.0f) return;

                        const float sLeft = st.x + (kSpriteSize - kStructureCollisionSize) * 0.5f;
                        const float sRight = sLeft + kStructureCollisionSize;
                        const float sTop = st.y + (kSpriteSize - kStructureCollisionSize) * 0.5f;
                        const float sBottom = sTop + kStructureCollisionSize;

                        if (left < sRight && right > sLeft && top < sBottom && bottom > sTop)
                        {
                            structBlocked = true;
                        }
                    });

                return structBlocked;
            };

            const float nextX = transform.x + velocity.dx * dtSeconds;
            if (!isBlocked(nextX, transform.y))
            {
                transform.x = nextX;
            }
            else
            {
                velocity.dx = 0.0f;
            }

            const float nextY = transform.y + velocity.dy * dtSeconds;
            if (!isBlocked(transform.x, nextY))
            {
                transform.y = nextY;
            }
            else
            {
                velocity.dy = 0.0f;
            }
        });
}
