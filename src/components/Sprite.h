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
};
