#pragma once

#include <glm/vec2.hpp>

#include "core/Input.h"
#include "ecs/World.h"

class InputSystem
{
public:
    void update(World& world, const Input& input, const glm::vec2& mouseWorld) const;

private:
    static constexpr float kBaseSpeed = 180.0f;
    static constexpr float kRunSpeedMultiplier = 1.75f;
    static constexpr float kCrouchSpeedMultiplier = 0.5f;
    static constexpr float kSpriteSize = 32.0f;
};
