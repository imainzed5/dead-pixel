#pragma once

#include "ecs/Types.h"

struct Projectile
{
    float vx = 0.0f;
    float vy = 0.0f;
    float damage = 55.0f;
    float rangeRemaining = 192.0f;
    float speed = 600.0f;
    Entity source = 0xFFFFFFFF;
};
