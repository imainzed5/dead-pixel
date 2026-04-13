#pragma once

#include <glm/vec4.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include "rendering/Texture.h"

struct SpriteRegion
{
    glm::vec4 uvRect{0.0f};  // x, y, w, h in UV space
    std::vector<glm::vec4> frames;  // animation frames (UV rects), empty if single frame
};

class SpriteSheet
{
public:
    bool loadFromFiles(const std::string& pngPath, const std::string& jsonPath);

    [[nodiscard]] const SpriteRegion* find(const std::string& name) const;
    [[nodiscard]] glm::vec4 uvRect(const std::string& name) const;
    [[nodiscard]] glm::vec4 frameUv(const std::string& name, int frame) const;
    [[nodiscard]] int frameCount(const std::string& name) const;

    void bindTexture(GLenum textureUnit) const;
    [[nodiscard]] GLuint textureId() const { return mTexture.id(); }

private:
    Texture mTexture;
    std::unordered_map<std::string, SpriteRegion> mSprites;
};
