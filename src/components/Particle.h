#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

struct Particle
{
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 velocity{0.0f, 0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float size = 2.0f;
    float lifetime = 1.0f;
    float age = 0.0f;
    float fadeRate = 1.0f; // alpha reduction per second
    float gravity = 0.0f; // downward acceleration
};
