#include "rendering/BitmapFont.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

#include "rendering/SpriteBatch.h"

bool BitmapFont::loadFromFiles(const std::string& pngPath, const std::string& jsonPath)
{
    if (!mTexture.loadFromFile(pngPath))
    {
        std::cerr << "BitmapFont: failed to load atlas texture: " << pngPath << '\n';
        return false;
    }

    std::ifstream jsonFile(jsonPath);
    if (!jsonFile)
    {
        std::cerr << "BitmapFont: failed to open metrics file: " << jsonPath << '\n';
        return false;
    }

    nlohmann::json root = nlohmann::json::parse(jsonFile, nullptr, false);
    if (root.is_discarded())
    {
        std::cerr << "BitmapFont: failed to parse metrics JSON.\n";
        return false;
    }

    const float atlasW = static_cast<float>(root.value("atlasWidth", 1));
    const float atlasH = static_cast<float>(root.value("atlasHeight", 1));
    mCellW = static_cast<float>(root.value("cellWidth", 6));
    mCellH = static_cast<float>(root.value("cellHeight", 9));

    mGlyphs.clear();

    if (root.contains("glyphs") && root["glyphs"].is_object())
    {
        for (auto& [key, value] : root["glyphs"].items())
        {
            if (key.empty())
            {
                continue;
            }

            const char ch = key[0];
            GlyphMetrics glyph;

            const float px = static_cast<float>(value.value("x", 0));
            const float py = static_cast<float>(value.value("y", 0));
            const float cw = static_cast<float>(value.value("cellW", 6));
            const float ch2 = static_cast<float>(value.value("cellH", 9));

            // Inset by half a texel on each edge to prevent GL_NEAREST from
            // sampling across cell boundaries in the tiny atlas.
            glyph.u = (px + 0.5f) / atlasW;
            glyph.v = (py + 0.5f) / atlasH;
            glyph.uvW = (cw - 1.0f) / atlasW;
            glyph.uvH = (ch2 - 1.0f) / atlasH;
            glyph.cellW = cw;
            glyph.cellH = ch2;
            mGlyphs[ch] = glyph;
        }
    }

    return !mGlyphs.empty();
}

void BitmapFont::drawText(
    SpriteBatch& batch,
    const glm::vec2& position,
    const std::string& text,
    const glm::vec4& color,
    float scale,
    TextAlign align) const
{
    if (mGlyphs.empty())
    {
        return;
    }

    // Split into lines for alignment
    std::vector<std::string> lines;
    {
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line))
        {
            lines.push_back(line);
        }

        if (lines.empty())
        {
            return;
        }
    }

    const float scaledCellW = mCellW * scale;
    const float scaledCellH = mCellH * scale;

    float cursorY = position.y;

    for (const std::string& line : lines)
    {
        const float lineWidth = static_cast<float>(line.size()) * scaledCellW;
        float cursorX = position.x;

        if (align == TextAlign::Center)
        {
            cursorX = position.x - lineWidth * 0.5f;
        }
        else if (align == TextAlign::Right)
        {
            cursorX = position.x - lineWidth;
        }

        for (const char ch : line)
        {
            const auto it = mGlyphs.find(ch);
            if (it != mGlyphs.end())
            {
                const GlyphMetrics& glyph = it->second;
                const glm::vec4 uvRect(glyph.u, glyph.v, glyph.uvW, glyph.uvH);
                batch.draw(
                    glm::vec2(cursorX, cursorY),
                    glm::vec2(scaledCellW, scaledCellH),
                    uvRect,
                    color);
            }

            cursorX += scaledCellW;
        }

        cursorY += scaledCellH;
    }
}

glm::vec2 BitmapFont::measureText(const std::string& text, float scale) const
{
    const float scaledCellW = mCellW * scale;
    const float scaledCellH = mCellH * scale;

    float maxWidth = 0.0f;
    float currentWidth = 0.0f;
    int lineCount = 1;

    for (const char ch : text)
    {
        if (ch == '\n')
        {
            maxWidth = std::max(maxWidth, currentWidth);
            currentWidth = 0.0f;
            ++lineCount;
        }
        else
        {
            currentWidth += scaledCellW;
        }
    }

    maxWidth = std::max(maxWidth, currentWidth);
    return glm::vec2(maxWidth, static_cast<float>(lineCount) * scaledCellH);
}

void BitmapFont::bindTexture(GLenum textureUnit) const
{
    mTexture.bind(textureUnit);
}
