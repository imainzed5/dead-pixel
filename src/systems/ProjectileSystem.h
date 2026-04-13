#pragma once

#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"

class ProjectileSystem
{
public:
    void update(World& world, const TileMap& tileMap, NoiseModel& noiseModel, float dtSeconds);
};
