#include "rendering/Texture.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

Texture::~Texture()
{
    unload();
}

bool Texture::loadFromFile(const std::string& path)
{
    unload();

    stbi_set_flip_vertically_on_load(0);

    int channels = 0;
    unsigned char* pixelData = stbi_load(path.c_str(), &mWidth, &mHeight, &channels, STBI_rgb_alpha);
    if (pixelData == nullptr)
    {
        std::cerr << "Failed to load texture: " << path << " (" << stbi_failure_reason() << ")\n";
        return false;
    }

    glGenTextures(1, &mTextureId);
    glBindTexture(GL_TEXTURE_2D, mTextureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        mWidth,
        mHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixelData);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixelData);
    return true;
}

void Texture::unload()
{
    if (mTextureId != 0)
    {
        glDeleteTextures(1, &mTextureId);
        mTextureId = 0;
        mWidth = 0;
        mHeight = 0;
    }
}

void Texture::bind(GLenum textureUnit) const
{
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, mTextureId);
}
