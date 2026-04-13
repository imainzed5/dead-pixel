#pragma once

#include <GL/glew.h>

#include <string>

class Texture
{
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool loadFromFile(const std::string& path);
    void unload();
    void bind(GLenum textureUnit) const;

    [[nodiscard]] GLuint id() const { return mTextureId; }
    [[nodiscard]] int width() const { return mWidth; }
    [[nodiscard]] int height() const { return mHeight; }

private:
    GLuint mTextureId = 0;
    int mWidth = 0;
    int mHeight = 0;
};
