#pragma once

#include <GL/glew.h>
#include <glm/vec4.hpp>

struct Sprite
{
    GLuint textureId = 0;
    glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    int layer = 0;
    bool visible = true;
    // Side of the square draw quad in world pixels at zoom 1. Humanoid frames
    // are authored at 64 px; ground pickups draw smaller than furniture.
    float drawSize = 32.0f;
};

namespace SpriteSizes
{
inline constexpr float kHumanoid = 64.0f;
inline constexpr float kPickup = 24.0f;
inline constexpr float kDefault = 32.0f;
} // namespace SpriteSizes
