#include "rendering/SpriteBatch.h"

#include <cstddef>

SpriteBatch::~SpriteBatch()
{
    shutdown();
}

bool SpriteBatch::initialize()
{
    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    glGenBuffers(1, &mEbo);

    if (mVao == 0 || mVbo == 0 || mEbo == 0)
    {
        shutdown();
        return false;
    }

    glBindVertexArray(mVao);

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, uv)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<const void*>(offsetof(Vertex, color)));

    glBindVertexArray(0);
    return true;
}

void SpriteBatch::shutdown()
{
    if (mEbo != 0)
    {
        glDeleteBuffers(1, &mEbo);
        mEbo = 0;
    }

    if (mVbo != 0)
    {
        glDeleteBuffers(1, &mVbo);
        mVbo = 0;
    }

    if (mVao != 0)
    {
        glDeleteVertexArrays(1, &mVao);
        mVao = 0;
    }

    mVertices.clear();
    mIndices.clear();
}

void SpriteBatch::begin()
{
    mVertices.clear();
    mIndices.clear();
}

void SpriteBatch::draw(
    const glm::vec2& position,
    const glm::vec2& size,
    const glm::vec4& uvRect,
    const glm::vec4& color)
{
    const GLuint baseIndex = static_cast<GLuint>(mVertices.size());

    const float left = position.x;
    const float right = position.x + size.x;
    const float top = position.y;
    const float bottom = position.y + size.y;

    const float u0 = uvRect.x;
    const float u1 = uvRect.x + uvRect.z;
    const float vTop = uvRect.y;
    const float vBottom = uvRect.y + uvRect.w;

    mVertices.push_back({glm::vec2(left, top), glm::vec2(u0, vTop), color});
    mVertices.push_back({glm::vec2(right, top), glm::vec2(u1, vTop), color});
    mVertices.push_back({glm::vec2(right, bottom), glm::vec2(u1, vBottom), color});
    mVertices.push_back({glm::vec2(left, bottom), glm::vec2(u0, vBottom), color});

    mIndices.push_back(baseIndex + 0);
    mIndices.push_back(baseIndex + 1);
    mIndices.push_back(baseIndex + 2);
    mIndices.push_back(baseIndex + 0);
    mIndices.push_back(baseIndex + 2);
    mIndices.push_back(baseIndex + 3);
}

void SpriteBatch::end()
{
    if (mVertices.empty())
    {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mVertices.size() * sizeof(Vertex)),
        mVertices.data(),
        GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(mIndices.size() * sizeof(GLuint)),
        mIndices.data(),
        GL_DYNAMIC_DRAW);
}

void SpriteBatch::flush() const
{
    if (mIndices.empty())
    {
        return;
    }

    glBindVertexArray(mVao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mIndices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
