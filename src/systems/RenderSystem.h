#pragma once

#include <functional>
#include <vector>

#include "ecs/World.h"
#include "rendering/Camera.h"
#include "rendering/SpriteBatch.h"
#include "rendering/StaticSprite.h"

// Extra inputs for the entity pass: non-entity sprites that sort with entities
// (walls, roofs) and a filter for entities that should not be drawn.
struct RenderExtras
{
    const std::vector<StaticSprite>* statics = nullptr;
    std::function<bool(int tileX, int tileY)> isHiddenAt;
};

class RenderSystem
{
public:
    // Sprite layers at or below this are flat on the ground (blood, decals) and
    // draw under everything that stands up.
    static constexpr int kFlatLayerMax = 3;

    void render(World& world, SpriteBatch& spriteBatch, const Camera& camera, float alpha = 1.0f,
                const RenderExtras* extras = nullptr) const;

private:
    static constexpr float kSpriteSize = 32.0f;

    // Humanoid frames in the sprite sheet are authored at 64x64 and their feet sit
    // around source pixel row 49. We render entities at 32x32 world size, so we
    // shift humanoid sprites down by 7.5 screen pixels (at zoom 1.0) to align
    // visual feet with the world/collision anchor point.
    // Transparent rows below the feet in a humanoid frame, as a fraction of its
    // height; the draw is shifted down by this so feet sit on the world anchor.
    static constexpr float kHumanoidFootPadding = 14.0f / 64.0f;
};
