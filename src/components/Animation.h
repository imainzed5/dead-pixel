#pragma once

#include <string>

struct Animation
{
    std::string currentAnim;
    int currentFrame = 0;
    float frameTimer = 0.0f;
    float frameDuration = 0.15f;
    bool looping = true;
    bool finished = false;
};
