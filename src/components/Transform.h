#pragma once

struct Transform
{
    float x = 0.0f;
    float y = 0.0f;
    float prevX = 0.0f;
    float prevY = 0.0f;
    float rotation = 0.0f;
    int floor = 0; // Multi-floor prep: default ground floor
};
