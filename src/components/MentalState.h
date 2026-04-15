#pragma once

struct MentalState
{
    // Panic: 0 = calm, 100 = full panic
    float panic = 0.0f;
    float panicDecayPerSecond = 3.0f; // decay when safe

    // Depression: 0 = fine, 100 = retirement
    float depression = 0.0f;
    float depressionDecayPerSecond = 0.1f;

    // Desensitisation: 0 = fresh, 100 = numb
    float desensitisation = 0.0f;

    // Timers for effects
    float panicDriftAngle = 0.0f;     // random drift direction that shifts over time
    float panicDriftTimer = 0.0f;     // time until drift angle changes
    float screamCooldown = 0.0f;      // prevent constant screaming

    // Depression crisis window (M5)
    bool inCrisis = false;            // true when depression >= 100
    double crisisStartHour = 0.0;     // game-hour when crisis began
    int crisisStartDay = 1;           // in-game day when crisis began
    float crisisRecoveryBuffer = 0.0f;// depression reduction during crisis before retirement

    // Social recovery
    float socialRecoveryTimer = 0.0f; // cooldown for NPC proximity recovery
};
