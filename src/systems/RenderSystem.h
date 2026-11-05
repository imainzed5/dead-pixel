#pragma once

#include "ecs/World.h"
#include "rendering/Camera.h"
#include "rendering/SpriteBatch.h"

class RenderSystem
{
public:
    void render(World& world, SpriteBatch& spriteBatch, const Camera& camera, float alpha = 1.0f) const;

private:
    static constexpr float kSpriteSize = 32.0f;

    // Humanoid frames in the sprite sheet are authored at 64x64 and their feet sit
    // around source pixel row 49. We render entities at 32x32 world size, so we
    // shift humanoid sprites down by 7.5 screen pixels (at zoom 1.0) to align
    // visual feet with the world/collision anchor point.
    static constexpr float kHumanoidVisualYOffset = 7.5f;
};
