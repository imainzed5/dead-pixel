#include "systems/ThrownSystem.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

#include "components/Health.h"
#include "components/Projectile.h"
#include "components/Sprite.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "world/CombatRules.h"

namespace
{
constexpr float kHitRadius = 12.0f;
// Only near the end of its flight is it low enough to strike anyone.
constexpr float kLowShare = 0.7f;
} // namespace

bool ThrownSystem::passable(const TileMap& tileMap, int tileX, int tileY)
{
    if (!tileMap.inBounds(tileX, tileY)) return false;
    if (!tileMap.blocksSight(tileX, tileY)) return true;
    return tileMap.windowOpen(tileX, tileY);
}

Entity ThrownSystem::launch(World& world, int itemId, const glm::vec2& from, const glm::vec2& to, Entity thrower,
                            float fuseSeconds, unsigned int textureId, const glm::vec4& uvRect)
{
    const glm::vec2 delta = to - from;
    const float distance = glm::length(delta);
    const glm::vec2 dir = distance > 1e-3f ? delta / distance : glm::vec2(1.0f, 0.0f);

    Projectile proj{};
    proj.kind = ProjectileKind::Thrown;
    proj.itemId = itemId;
    proj.vx = dir.x;
    proj.vy = dir.y;
    proj.speed = combatrules::kThrowSpeedPixelsPerSecond;
    proj.flightTotal = std::max(1.0f, distance);
    proj.rangeRemaining = proj.flightTotal;
    proj.source = thrower;
    proj.fuseSeconds = fuseSeconds;
    proj.damage = combatrules::kThrowHitDamage;

    // Transform is the sprite's top-left; the centre rides on the flight line.
    Transform t{};
    t.x = t.prevX = from.x - 16.0f;
    t.y = t.prevY = from.y - 16.0f;

    Sprite sprite{};
    sprite.textureId = textureId;
    sprite.uvRect = uvRect;
    sprite.layer = 11;
    sprite.drawSize = SpriteSizes::kPickup;

    const Entity e = world.createEntity();
    world.addComponent<Transform>(e, t);
    world.addComponent<Projectile>(e, proj);
    world.addComponent<Sprite>(e, sprite);
    return e;
}

void ThrownSystem::update(World& world, const TileMap& tileMap, float dtSeconds, std::vector<ThrowLanding>& landed)
{
    std::vector<Entity> done;
    const float tile = static_cast<float>(std::max(1, tileMap.tileWidth()));
    world.forEach<Projectile, Transform>(
        [&](Entity e, Projectile& proj, Transform& t)
        {
            if (proj.kind != ProjectileKind::Thrown) return;
            if (proj.fuseSeconds > 0.0f)
            {
                proj.fuseSeconds = std::max(0.0f, proj.fuseSeconds - dtSeconds);
            }

            const glm::vec2 dir(proj.vx, proj.vy);
            const float step = std::min(proj.speed * dtSeconds, proj.flightTotal - proj.flown);
            glm::vec2 centre(t.x + 16.0f, t.y + 16.0f);

            ThrowLanding landing;
            landing.itemId = proj.itemId;
            landing.thrower = proj.source;
            bool down = false;

            // Move in small steps so it cannot slip through a wall corner.
            float moved = 0.0f;
            while (moved < step && !down)
            {
                const float s = std::min(4.0f, step - moved);
                const glm::vec2 next = centre + dir * s;
                if (!passable(tileMap, static_cast<int>(std::floor(next.x / tile)), static_cast<int>(std::floor(next.y / tile))))
                {
                    landing.hitWall = true;
                    down = true;
                    break;
                }
                centre = next;
                moved += s;
                proj.flown += s;

                if (proj.flown >= proj.flightTotal * kLowShare)
                {
                    world.forEach<ZombieAI, Transform, Health>(
                        [&](Entity z, ZombieAI&, Transform& zt, Health& h)
                        {
                            if (landing.hitZombie != kInvalidEntity || h.current <= 0.0f) return;
                            if (glm::length(glm::vec2(zt.x + 16.0f, zt.y + 16.0f) - centre) < kHitRadius) landing.hitZombie = z;
                        });
                    if (landing.hitZombie != kInvalidEntity) down = true;
                }
            }
            if (proj.flown >= proj.flightTotal - 0.01f) down = true;

            t.prevX = t.x;
            t.prevY = t.y;
            t.x = centre.x - 16.0f;
            t.y = centre.y - 16.0f;
            proj.arcHeight = down ? 0.0f : combatrules::throwArcHeight(proj.flown / proj.flightTotal, proj.flightTotal);
            if (world.hasComponent<Sprite>(e))
            {
                world.getComponent<Sprite>(e).sink = -proj.arcHeight;
            }

            if (down)
            {
                landing.position = centre;
                landing.fuseSeconds = proj.fuseSeconds;
                landed.push_back(landing);
                done.push_back(e);
            }
        });
    for (Entity e : done)
    {
        world.destroyEntity(e);
    }
}
