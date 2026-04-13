#pragma once

#include "ecs/World.h"

class BleedingSystem
{
public:
    void update(World& world, float dtSeconds) const;
};
