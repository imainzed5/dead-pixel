#pragma once

#include "ecs/World.h"

class NeedsSystem
{
public:
    void update(World& world, float dtSeconds, double gameHours) const;
};
