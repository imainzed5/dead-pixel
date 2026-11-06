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
    // Characters: showing the back view (walking away up the screen) rather
    // than the front. Kept between frames so a pause keeps the last look.
    bool facingAway = false;
};
