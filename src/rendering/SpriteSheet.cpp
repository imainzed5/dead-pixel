#include "rendering/SpriteSheet.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>

bool SpriteSheet::loadFromFiles(const std::string& pngPath, const std::string& jsonPath)
{
    if (!mTexture.loadFromFile(pngPath))
    {
        std::cerr << "SpriteSheet: failed to load texture: " << pngPath << '\n';
        return false;
    }

    std::ifstream file(jsonPath);
    if (!file)
    {
        std::cerr << "SpriteSheet: failed to open: " << jsonPath << '\n';
        return false;
    }

    nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
    if (root.is_discarded())
    {
        std::cerr << "SpriteSheet: failed to parse JSON.\n";
        return false;
    }

    const float imgW = static_cast<float>(root.value("imageWidth", 1));
    const float imgH = static_cast<float>(root.value("imageHeight", 1));

    mSprites.clear();

    if (!root.contains("sprites") || !root["sprites"].is_object())
    {
        return false;
    }

    for (auto& [name, value] : root["sprites"].items())
    {
        SpriteRegion region;
        const float px = static_cast<float>(value.value("x", 0));
        const float py = static_cast<float>(value.value("y", 0));
        const float pw = static_cast<float>(value.value("w", 32));
        const float ph = static_cast<float>(value.value("h", 32));
        region.uvRect = glm::vec4(px / imgW, py / imgH, pw / imgW, ph / imgH);

        if (value.contains("frames") && value["frames"].is_array())
        {
            for (auto& frame : value["frames"])
            {
                const float fx = static_cast<float>(frame.value("x", 0));
                const float fy = static_cast<float>(frame.value("y", 0));
                const float fw = static_cast<float>(frame.value("w", 32));
                const float fh = static_cast<float>(frame.value("h", 32));
                region.frames.emplace_back(fx / imgW, fy / imgH, fw / imgW, fh / imgH);
            }
        }

        mSprites[name] = std::move(region);
    }

    return !mSprites.empty();
}

const SpriteRegion* SpriteSheet::find(const std::string& name) const
{
    const auto it = mSprites.find(name);
    return it != mSprites.end() ? &it->second : nullptr;
}

glm::vec4 SpriteSheet::uvRect(const std::string& name) const
{
    const auto* region = find(name);
    return region ? region->uvRect : glm::vec4(0.0f);
}

glm::vec4 SpriteSheet::frameUv(const std::string& name, int frame) const
{
    const auto* region = find(name);
    if (!region)
    {
        return glm::vec4(0.0f);
    }

    if (region->frames.empty())
    {
        return region->uvRect;
    }

    return region->frames[frame % static_cast<int>(region->frames.size())];
}

int SpriteSheet::frameCount(const std::string& name) const
{
    const auto* region = find(name);
    if (!region || region->frames.empty())
    {
        return 1;
    }

    return static_cast<int>(region->frames.size());
}

void SpriteSheet::bindTexture(GLenum textureUnit) const
{
    mTexture.bind(textureUnit);
}
