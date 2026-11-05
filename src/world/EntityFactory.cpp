#include "world/EntityFactory.h"

#include "components/Animation.h"
#include "components/Health.h"
#include "components/Sprite.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"
#include "rendering/SpriteSheet.h"

namespace EntityFactory
{
namespace
{
constexpr float kSpriteSize = 32.0f;
constexpr int kZombieSpriteLayer = 8;
} // namespace

Entity spawnZombie(World& world, const SpriteSheet& spriteSheet, const ZombieParams& params)
{
    const Entity zombie = world.createEntity();

    Transform transform{};
    transform.x = params.topLeftWorld.x;
    transform.y = params.topLeftWorld.y;
    transform.prevX = transform.x;
    transform.prevY = transform.y;

    Sprite sprite{};
    sprite.textureId = spriteSheet.textureId();
    sprite.uvRect = spriteSheet.uvRect("zombie_idle");
    sprite.color = glm::vec4(1.0f);
    sprite.layer = kZombieSpriteLayer;
    sprite.drawSize = SpriteSizes::kHumanoid;

    Animation animation{};
    animation.currentAnim = "zombie_idle";
    animation.frameDuration = 0.18f;

    ZombieAI ai{};
    ai.targetWorld = glm::vec2(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f);
    ai.baseMoveSpeed = params.baseMoveSpeed;
    ai.moveSpeed = params.baseMoveSpeed;
    ai.spawnDay = params.spawnDay;

    world.addComponent<Transform>(zombie, transform);
    world.addComponent<Sprite>(zombie, sprite);
    world.addComponent<Animation>(zombie, animation);
    world.addComponent<Velocity>(zombie, Velocity{});
    world.addComponent<ZombieAI>(zombie, ai);
    world.addComponent<Health>(zombie, Health{params.health, params.health});
    return zombie;
}
} // namespace EntityFactory
