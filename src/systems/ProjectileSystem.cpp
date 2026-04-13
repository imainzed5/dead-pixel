#include "systems/ProjectileSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/geometric.hpp>

#include "components/Health.h"
#include "components/Projectile.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"

namespace
{
constexpr float kSpriteSize = 32.0f;
constexpr float kHitRadius = 16.0f;
}

void ProjectileSystem::update(World& world, const TileMap& tileMap, NoiseModel& noiseModel, float dtSeconds)
{
    std::vector<Entity> toDestroy;

    world.forEach<Projectile, Transform>(
        [&](Entity projEntity, Projectile& proj, Transform& pt)
        {
            const float dist = proj.speed * dtSeconds;
            pt.x += proj.vx * dist;
            pt.y += proj.vy * dist;
            proj.rangeRemaining -= dist;

            if (proj.rangeRemaining <= 0.0f)
            {
                toDestroy.push_back(projEntity);
                return;
            }

            // Wall collision
            const int tileX = static_cast<int>(pt.x / static_cast<float>(std::max(1, tileMap.tileWidth())));
            const int tileY = static_cast<int>(pt.y / static_cast<float>(std::max(1, tileMap.tileHeight())));
            if (tileMap.isSolid(tileX, tileY))
            {
                toDestroy.push_back(projEntity);
                return;
            }

            // Zombie collision
            const glm::vec2 projCenter(pt.x, pt.y);
            world.forEach<ZombieAI, Transform, Health>(
                [&](Entity ze, ZombieAI&, Transform& zt, Health& zh)
                {
                    if (zh.current <= 0.0f) return;
                    const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                    if (glm::distance(projCenter, zc) < kHitRadius)
                    {
                        zh.current = std::max(0.0f, zh.current - proj.damage);
                        noiseModel.emitNoise(zc, NoiseTier::Medium, 0.5f, proj.source);
                        toDestroy.push_back(projEntity);
                    }
                });
        });

    for (Entity e : toDestroy)
    {
        world.destroyEntity(e);
    }
}
