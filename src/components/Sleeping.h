#pragma once

struct Sleeping
{
    float durationRemaining = 0.0f;   // game-hours left to sleep
    float quality = 1.0f;             // 0.0 = terrible, 1.0 = perfect
    float debtRecoveryRate = 11.0f;   // sleep debt recovered per game-hour (x quality)
    float fatigueRecoveryRate = 20.0f;// fatigue recovered per game-hour
    bool interrupted = false;
};
