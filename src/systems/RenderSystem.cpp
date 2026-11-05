#include "systems/RenderSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "components/Facing.h"
#include "components/PlayerInput.h"
#include "components/Sprite.h"
#include "components/SurvivorAI.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"

namespace
{
struct DrawCommand
{
    Entity entity = kInvalidEntity;
    glm::vec2 position{0.0f, 0.0f};
    glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    int layer = 0;
};
}

void RenderSystem::render(World& world, SpriteBatch& spriteBatch, const Camera& camera, float alpha) const
{
    std::vector<DrawCommand> drawCommands;
    drawCommands.reserve(world.livingEntities().size());

    world.forEach<Transform, Sprite>(
        [&](Entity entity, Transform& transform, Sprite& sprite)
        {
            if (!sprite.visible)
            {
                return;
            }

            const float lerpX = transform.prevX + (transform.x - transform.prevX) * alpha;
            const float lerpY = transform.prevY + (transform.y - transform.prevY) * alpha;

            drawCommands.push_back(
                DrawCommand{
                    entity,
                    glm::vec2(lerpX, lerpY),
                    sprite.uvRect,
                    sprite.color,
                    sprite.layer});
        });

    // Primary sort by layer, secondary sort by iso depth (worldX + worldY)
    std::sort(
        drawCommands.begin(),
        drawCommands.end(),
        [](const DrawCommand& left, const DrawCommand& right)
        {
            if (left.layer != right.layer)
            {
                return left.layer < right.layer;
            }
            return (left.position.x + left.position.y) < (right.position.x + right.position.y);
        });

    const float zoom = camera.zoom();
    const float scaledSize = kSpriteSize * zoom;

    for (const DrawCommand& command : drawCommands)
    {
        glm::vec4 uvRect = command.uvRect;
        glm::vec4 color = command.color;

        if (world.hasComponent<Facing>(command.entity))
        {
            const Facing& facing = world.getComponent<Facing>(command.entity);
            if (std::cos(facing.angleRadians) < 0.0f)
            {
                uvRect.x = command.uvRect.x + command.uvRect.z;
                uvRect.z = -command.uvRect.z;
            }

            if (facing.crouching)
            {
                color *= glm::vec4(0.75f, 1.0f, 0.75f, 1.0f);
            }
        }

        // Convert world center to screen position
        const glm::vec2 worldCenter = command.position + glm::vec2(kSpriteSize * 0.5f, kSpriteSize * 0.5f);
        const glm::vec2 screenCenter = camera.worldToScreen(worldCenter);
        const glm::vec2 viewportSize = camera.viewportSize();

        const bool isHumanoid =
            world.hasComponent<PlayerInput>(command.entity) ||
            world.hasComponent<ZombieAI>(command.entity) ||
            world.hasComponent<SurvivorAI>(command.entity);

        const float visualYOffset = isHumanoid ? (kHumanoidVisualYOffset * zoom) : 0.0f;

        // Draw sprite centered on its iso screen position with optional humanoid
        // foot-pivot correction so visual feet sit on the physical world anchor.
        const glm::vec2 drawPos(
            screenCenter.x - scaledSize * 0.5f,
            screenCenter.y - scaledSize + visualYOffset);

        // Frustum cull in draw-rect space so offsets are correctly respected.
        if (drawPos.x + scaledSize < 0.0f ||
            drawPos.y + scaledSize < 0.0f ||
            drawPos.x > viewportSize.x ||
            drawPos.y > viewportSize.y)
        {
            continue;
        }

        spriteBatch.draw(drawPos, glm::vec2(scaledSize, scaledSize), uvRect, color);
    }
}
