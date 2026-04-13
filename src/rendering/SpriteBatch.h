#pragma once

#include <GL/glew.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <vector>

class SpriteBatch
{
public:
    SpriteBatch() = default;
    ~SpriteBatch();

    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    bool initialize();
    void shutdown();

    void begin();
    void draw(
        const glm::vec2& position,
        const glm::vec2& size,
        const glm::vec4& uvRect,
        const glm::vec4& color);
    void end();
    void flush() const;

private:
    struct Vertex
    {
        glm::vec2 position;
        glm::vec2 uv;
        glm::vec4 color;
    };

    GLuint mVao = 0;
    GLuint mVbo = 0;
    GLuint mEbo = 0;

    std::vector<Vertex> mVertices;
    std::vector<GLuint> mIndices;
};
