#pragma once

#include "ecs/World.h"

class MovementSystem
{
public:
    void update(World& world, float dtSeconds) const;
};
