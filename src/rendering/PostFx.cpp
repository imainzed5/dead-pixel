#include "rendering/PostFx.h"

#include <array>

PostFx::~PostFx()
{
    shutdown();
}

bool PostFx::initialize(const std::string& vertexPath, const std::string& fragmentPath)
{
    if (!mShader.loadFromFiles(vertexPath, fragmentPath)) return false;
    // One quad over the screen: x, y (clip space), u, v.
    constexpr std::array<float, 16> kQuad = {
        -1.0f, -1.0f, 0.0f, 0.0f, //
        1.0f,  -1.0f, 1.0f, 0.0f, //
        -1.0f, 1.0f,  0.0f, 1.0f, //
        1.0f,  1.0f,  1.0f, 1.0f, //
    };
    glGenVertexArrays(1, &mVao);
    glGenBuffers(1, &mVbo);
    if (mVao == 0 || mVbo == 0) return false;
    glBindVertexArray(mVao);
    glBindBuffer(GL_ARRAY_BUFFER, mVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void*>(2 * sizeof(float)));
    glBindVertexArray(0);
    mReady = true;
    return true;
}

void PostFx::shutdown()
{
    if (mTexture != 0) glDeleteTextures(1, &mTexture);
    if (mFramebuffer != 0) glDeleteFramebuffers(1, &mFramebuffer);
    if (mVbo != 0) glDeleteBuffers(1, &mVbo);
    if (mVao != 0) glDeleteVertexArrays(1, &mVao);
    mTexture = mFramebuffer = mVbo = mVao = 0;
    mWidth = mHeight = 0;
    mReady = false;
    mActive = false;
}

bool PostFx::resize(int width, int height)
{
    if (mFramebuffer != 0 && width == mWidth && height == mHeight) return true;
    if (mTexture == 0) glGenTextures(1, &mTexture);
    if (mFramebuffer == 0) glGenFramebuffers(1, &mFramebuffer);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    // Nearest: the pixel art stays hard-edged (the zoom when held is small).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(mPrevFramebuffer));
    mWidth = width;
    mHeight = height;
    return complete;
}

bool PostFx::begin(int width, int height, float clearR, float clearG, float clearB)
{
    mActive = false;
    if (!mReady || width <= 0 || height <= 0) return false;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &mPrevFramebuffer);
    if (!resize(width, height)) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glViewport(0, 0, width, height);
    glClearColor(clearR, clearG, clearB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    mActive = true;
    return true;
}

void PostFx::end(const moodrules::Params& p, float timeSeconds)
{
    if (!mActive) return;
    mActive = false;
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(mPrevFramebuffer));
    glViewport(0, 0, mWidth, mHeight);
    const GLboolean blend = glIsEnabled(GL_BLEND);
    glDisable(GL_BLEND);

    mShader.use();
    mShader.setInt("uScene", 0);
    mShader.setVec2("uSize", glm::vec2(static_cast<float>(mWidth), static_cast<float>(mHeight)));
    mShader.setFloat("uTime", timeSeconds);
    mShader.setFloat("uSaturation", p.saturation);
    mShader.setFloat("uVignette", p.vignette);
    mShader.setFloat("uBlur", p.blur);
    mShader.setFloat("uBlink", p.blink);
    mShader.setFloat("uColdEdge", p.coldEdge);
    mShader.setFloat("uWarmCast", p.warmCast);
    mShader.setFloat("uShimmer", p.shimmer);
    mShader.setFloat("uDoubleImage", p.doubleImage);
    mShader.setFloat("uHaze", p.haze);
    mShader.setFloat("uZoom", p.zoom);
    mShader.setFloat("uDimEdges", p.dimEdges);
    mShader.setFloat("uShakePx", p.shakePx);
    mShader.setFloat("uRedPulse", p.redPulse);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glBindVertexArray(mVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    if (blend == GL_TRUE) glEnable(GL_BLEND);
}
