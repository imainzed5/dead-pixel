#pragma once

#include <GL/glew.h>

#include <string>

#include "rendering/Shader.h"
#include "world/MoodRules.h"

// The full-screen mood pass (phase 12). The world is drawn into a texture
// the size of the window, then one quad puts it back with the mood's
// effects; the HUD is drawn after, so its text stays sharp.
//
// Whatever framebuffer was bound when `begin` ran (the window's, or the
// harness's capture target) is the one `end` draws into.
class PostFx
{
public:
    PostFx() = default;
    ~PostFx();
    PostFx(const PostFx&) = delete;
    PostFx& operator=(const PostFx&) = delete;

    bool initialize(const std::string& vertexPath, const std::string& fragmentPath);
    void shutdown();
    [[nodiscard]] bool ready() const { return mReady; }

    // Redirects drawing into the pass's texture (cleared to `clear`).
    bool begin(int width, int height, float clearR, float clearG, float clearB);
    // Draws the texture back with the effects. Leaves the program unbound.
    void end(const moodrules::Params& params, float timeSeconds);
    [[nodiscard]] bool active() const { return mActive; }

private:
    bool resize(int width, int height);

    Shader mShader;
    GLuint mFramebuffer = 0;
    GLuint mTexture = 0;
    GLuint mVao = 0;
    GLuint mVbo = 0;
    GLint mPrevFramebuffer = 0;
    int mWidth = 0;
    int mHeight = 0;
    bool mReady = false;
    bool mActive = false;
};
