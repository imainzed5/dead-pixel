#pragma once

#include <cmath>
#include <string>

#include <glm/vec2.hpp>

struct Animation
{
    std::string currentAnim;
    int currentFrame = 0;
    float frameTimer = 0.0f;
    float frameDuration = 0.15f;
    bool looping = true;
    bool finished = false;
    // Characters: which of eight screen directions they face, clockwise from
    // screen right (0 E, 1 SE, 2 S toward the viewer, 3 SW, 4 W, 5 NW, 6 N,
    // 7 NE). Kept between frames so a pause keeps the last look.
    int dir = 1;
    // Frames set from outside (an attack follows its phase timers) rather
    // than advanced by the clock.
    bool driven = false;
};

// Which of eight screen directions (Animation::dir) a world-space direction
// points along. The iso view puts world (x, y) at screen (x - y, (x + y) / 2);
// on the ground that is the angle of (x - y, x + y), clockwise from screen
// right. The current facing is kept until the direction is well into the
// next sector, so a character walking a boundary does not flicker.
inline int facingDir(const glm::vec2& worldDir, int current)
{
    const float gx = worldDir.x - worldDir.y;
    const float gy = worldDir.x + worldDir.y;
    if (gx * gx + gy * gy < 1e-6f)
    {
        return current;
    }
    const float kSector = 0.78539816f; // 45 degrees
    const float angle = std::atan2(gy, gx);
    float off = angle - static_cast<float>(current & 7) * kSector;
    while (off > 3.14159265f) off -= 6.2831853f;
    while (off < -3.14159265f) off += 6.2831853f;
    if (std::abs(off) <= kSector * 0.68f)
    {
        return current & 7;
    }
    const int sector = static_cast<int>(std::lround(angle / kSector));
    return ((sector % 8) + 8) % 8;
}
