#pragma once

#include "ecs/World.h"
#include "rendering/SpriteSheet.h"

class AnimationSystem
{
public:
    void update(World& world, const SpriteSheet& sheet, float dtSeconds);
};
