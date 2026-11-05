#pragma once

struct Fire
{
    // 0-2 intensity scale used for spread chance, damage, and visuals.
    float intensity = 0.6f;

    // Fire lifetime countdown in seconds.
    float lifeRemaining = 22.0f;

    // Internal timers.
    float spreadTimer = 0.0f;
    float noiseTimer = 0.0f;
};
