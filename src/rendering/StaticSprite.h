#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

// A sprite that is not an entity (wall block, roof tile) but still has to sort
// with entities by isometric depth. Positions are already in screen space.
struct StaticSprite
{
    glm::vec2 position{0.0f}; // top-left, screen pixels
    glm::vec2 size{0.0f};
    glm::vec4 uvRect{0.0f};
    glm::vec4 color{1.0f};
    // Same units as entity depth in RenderSystem: worldX + worldY of the
    // top-left corner of a 32 px cell.
    float depth = 0.0f;
};
