#include "systems/RenderSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

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
    float size = 32.0f;
    bool flipX = false;
};
}

void RenderSystem::render(World& world, SpriteBatch& spriteBatch, const Camera& camera, float alpha,
                          const RenderExtras* extras) const
{
    std::vector<DrawCommand> drawCommands;
    drawCommands.reserve(world.livingEntities().size());
    const bool filterHidden = extras != nullptr && static_cast<bool>(extras->isHiddenAt);

    world.forEach<Transform, Sprite>(
        [&](Entity entity, Transform& transform, Sprite& sprite)
        {
            if (!sprite.visible)
            {
                return;
            }

            const float lerpX = transform.prevX + (transform.x - transform.prevX) * alpha;
            const float lerpY = transform.prevY + (transform.y - transform.prevY) * alpha;

            if (filterHidden)
            {
                const int tileX = static_cast<int>(std::floor((lerpX + kSpriteSize * 0.5f) / kSpriteSize));
                const int tileY = static_cast<int>(std::floor((lerpY + kSpriteSize * 0.5f) / kSpriteSize));
                if (extras->isHiddenAt(tileX, tileY))
                {
                    return;
                }
            }

            drawCommands.push_back(
                DrawCommand{
                    entity,
                    glm::vec2(lerpX, lerpY),
                    sprite.uvRect,
                    sprite.color,
                    sprite.layer,
                    sprite.drawSize,
                    sprite.flipX});
        });

    // Draw order: flat decals first (by layer, then depth); then everything
    // that stands up, entities and structures together, back to front by iso
    // depth (worldX + worldY). On a tie structures go first, so someone in a
    // doorway stands in front of its frame.
    struct SortItem
    {
        bool flat = false;
        float depth = 0.0f;
        int order = 0; // 0 = structure, 1 = entity
        int layer = 0;
        int index = 0;
    };
    std::vector<SortItem> items;
    const std::vector<StaticSprite>* statics = extras != nullptr ? extras->statics : nullptr;
    items.reserve(drawCommands.size() + (statics != nullptr ? statics->size() : 0));
    for (std::size_t i = 0; i < drawCommands.size(); ++i)
    {
        const DrawCommand& command = drawCommands[i];
        items.push_back(SortItem{
            command.layer <= kFlatLayerMax,
            command.position.x + command.position.y,
            1,
            command.layer,
            static_cast<int>(i)});
    }
    if (statics != nullptr)
    {
        for (std::size_t i = 0; i < statics->size(); ++i)
        {
            items.push_back(SortItem{false, (*statics)[i].depth, 0, 0, static_cast<int>(i)});
        }
    }
    std::sort(
        items.begin(),
        items.end(),
        [](const SortItem& left, const SortItem& right)
        {
            if (left.flat != right.flat)
            {
                return left.flat;
            }
            if (left.flat && left.layer != right.layer)
            {
                return left.layer < right.layer;
            }
            if (left.depth != right.depth)
            {
                return left.depth < right.depth;
            }
            if (left.order != right.order)
            {
                return left.order < right.order;
            }
            return left.layer < right.layer;
        });

    const float zoom = camera.zoom();

    for (const SortItem& item : items)
    {
        if (item.order == 0)
        {
            const StaticSprite& s = (*statics)[static_cast<std::size_t>(item.index)];
            spriteBatch.draw(s.position, s.size, s.uvRect, s.color);
            continue;
        }

        const DrawCommand& command = drawCommands[static_cast<std::size_t>(item.index)];
        const float scaledSize = command.size * zoom;
        glm::vec4 uvRect = command.uvRect;
        const glm::vec4 color = command.color;
        if (command.flipX)
        {
            uvRect.x = command.uvRect.x + command.uvRect.z;
            uvRect.z = -command.uvRect.z;
        }

        // Convert world center to screen position
        const glm::vec2 worldCenter = command.position + glm::vec2(kSpriteSize * 0.5f, kSpriteSize * 0.5f);
        const glm::vec2 screenCenter = camera.worldToScreen(worldCenter);
        const glm::vec2 viewportSize = camera.viewportSize();

        const bool isHumanoid =
            world.hasComponent<PlayerInput>(command.entity) ||
            world.hasComponent<ZombieAI>(command.entity) ||
            world.hasComponent<SurvivorAI>(command.entity);

        const float visualYOffset = isHumanoid ? (kHumanoidFootPadding * scaledSize) : 0.0f;

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
