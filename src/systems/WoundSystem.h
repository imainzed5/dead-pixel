#pragma once

#include "ecs/World.h"

class WoundSystem
{
public:
    void update(World& world, Entity playerEntity, float dtSeconds, double gameHours);
};
