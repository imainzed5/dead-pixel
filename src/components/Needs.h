#pragma once

struct Needs
{
    // Hunger
    float hunger = 100.0f;
    float maxHunger = 100.0f;
    float hungerDecayPerSecond = 0.02f;
    float starvationDamagePerSecond = 2.5f;

    // Thirst (decays faster than hunger)
    float thirst = 100.0f;
    float maxThirst = 100.0f;
    float thirstDecayPerSecond = 0.035f;
    float dehydrationDamagePerSecond = 3.0f;

    // Fatigue (short-term; builds from running, recovers at rest)
    float fatigue = 0.0f;
    float maxFatigue = 100.0f;
    float fatigueGainPerSecond = 0.0f;   // set by movement state
    float fatigueRecoveryPerSecond = 5.0f;

    // Sleep debt (long-term; only recovered by sleeping)
    float sleepDebt = 0.0f;
    float maxSleepDebt = 100.0f;
    float sleepDebtGainPerSecond = 0.008f;

    // Body temperature (target 37.0; night = cold)
    float bodyTemperature = 37.0f;

    // Illness (0 = healthy, 100 = severe)
    float illness = 0.0f;
};
