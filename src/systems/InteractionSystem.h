#pragma once

#include <string>

#include "core/Input.h"
#include "ecs/Types.h"
#include "ecs/World.h"

class ItemDatabase;

struct ProximityPrompt
{
    Entity entity = kInvalidEntity;
    std::string text;
};

class InteractionSystem
{
public:
    [[nodiscard]] std::string update(World& world, const Input& input, Entity playerEntity, const ItemDatabase* itemDb = nullptr) const;
    [[nodiscard]] ProximityPrompt getProximityPrompt(World& world, Entity playerEntity) const;
};
