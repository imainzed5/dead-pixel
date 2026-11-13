#include "world/EntityFactory.h"

#include "components/Animation.h"
#include "components/Health.h"
#include "components/Sprite.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "components/ZombieAI.h"
#include "rendering/SpriteSheet.h"

#include <string>

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
    const std::string idle = std::string(zombiekinds::spriteSet(params.kind)) + "0_idle_se";
    sprite.uvRect = spriteSheet.uvRect((spriteSheet.find(idle) != nullptr) ? idle : std::string("zombie0_idle_se"));
    sprite.color = glm::vec4(1.0f);
    sprite.layer = kZombieSpriteLayer;
    sprite.drawSize = SpriteSizes::kHumanoid;

    Animation animation{};
    animation.currentAnim = (spriteSheet.find(idle) != nullptr) ? idle : std::string("zombie0_idle_se");
    animation.frameDuration = 0.18f;

    ZombieAI ai{};
    ai.targetWorld = glm::vec2(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f);
    ai.baseMoveSpeed = params.baseMoveSpeed;
    ai.moveSpeed = params.baseMoveSpeed;
    ai.spawnDay = params.spawnDay;
    ai.kind = params.kind;
    ai.dormant = params.dormant;
    const zombiekinds::KindStats& stats = zombiekinds::activeTable().stats(params.kind);
    ai.clawDamage = stats.damage;
    ai.hearing = stats.hearing;
    ai.doorDamage = stats.doorDamage;
    ai.visionRangeTiles = stats.sightTiles;
    // The look comes from where it spawned, so a saved zombie keeps its clothes.
    const unsigned hash = static_cast<unsigned>(static_cast<int>(transform.x)) * 73856093u ^
                          static_cast<unsigned>(static_cast<int>(transform.y)) * 19349663u;
    ai.look = static_cast<int>((hash >> 4) % 3u);

    world.addComponent<Transform>(zombie, transform);
    world.addComponent<Sprite>(zombie, sprite);
    world.addComponent<Animation>(zombie, animation);
    world.addComponent<Velocity>(zombie, Velocity{});
    world.addComponent<ZombieAI>(zombie, ai);
    world.addComponent<Health>(zombie, Health{params.health, params.health});
    return zombie;
}
} // namespace EntityFactory
