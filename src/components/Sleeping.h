#pragma once

struct Sleeping
{
    float durationRemaining = 0.0f;   // game-hours left to sleep
    float quality = 1.0f;             // 0.0 = terrible, 1.0 = perfect
    float debtRecoveryRate = 11.0f;   // sleep debt recovered per game-hour (x quality)
    float fatigueRecoveryRate = 20.0f;// fatigue recovered per game-hour
    bool interrupted = false;
    // How safe the room was when you lay down (Game::sleepSafety): 0 outdoors,
    // 1 a way in open, 2 all closed, 3 all boarded or blocked; -1 not yet rated.
    int safety = -1;
    float hoursSlept = 0.0f; // phase 5: a full night leaves you Rested
};
