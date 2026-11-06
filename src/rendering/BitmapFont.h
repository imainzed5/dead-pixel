#pragma once

#include <GL/glew.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include "rendering/Texture.h"

class SpriteBatch;

enum class TextAlign
{
    Left,
    Center,
    Right
};

struct GlyphMetrics
{
    float u = 0.0f;
    float v = 0.0f;
    float uvW = 0.0f;
    float uvH = 0.0f;
    float cellW = 0.0f;
    float cellH = 0.0f;
};

class BitmapFont
{
public:
    bool loadFromFiles(const std::string& pngPath, const std::string& jsonPath);

    void drawText(
        SpriteBatch& batch,
        const glm::vec2& position,
        const std::string& text,
        const glm::vec4& color,
        float scale = 1.0f,
        TextAlign align = TextAlign::Left) const;

    [[nodiscard]] glm::vec2 measureText(const std::string& text, float scale = 1.0f) const;

    void bindTexture(GLenum textureUnit) const;

    // Deferred mode: while on, drawText queues text instead of emitting quads,
    // so panels drawn with the sprite sheet bound can still place text. The
    // queue is drawn later with the font texture bound.
    void beginDeferred() const { mDeferring = true; }
    void drawDeferred(SpriteBatch& batch) const;

    [[nodiscard]] float lineHeight(float scale = 1.0f) const { return mCellH * scale; }

private:
    Texture mTexture;
    float mCellW = 0.0f;
    float mCellH = 0.0f;
    std::unordered_map<char, GlyphMetrics> mGlyphs;

    struct PendingText
    {
        glm::vec2 position{0.0f};
        std::string text;
        glm::vec4 color{1.0f};
        float scale = 1.0f;
        TextAlign align = TextAlign::Left;
    };
    mutable bool mDeferring = false;
    mutable std::vector<PendingText> mPending;
};
