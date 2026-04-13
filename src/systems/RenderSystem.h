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
};
