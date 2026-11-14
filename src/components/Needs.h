#pragma once

// Rates are per real second (60 real seconds = 1 game hour). See NeedsSystem.
struct Needs
{
    // Hunger: 100 = full. ~36 per game day (about one and a half cans).
    float hunger = 100.0f;
    float maxHunger = 100.0f;
    float hungerDecayPerSecond = 0.025f;

    // Thirst: 100 = hydrated. ~65 per game day (about two water bottles).
    float thirst = 100.0f;
    float maxThirst = 100.0f;
    float thirstDecayPerSecond = 0.045f;

    // Fatigue (short-term; builds from running, recovers at rest)
    float fatigue = 0.0f;
    float maxFatigue = 100.0f;
    float fatigueGainPerSecond = 0.0f;   // set by movement state
    float fatigueRecoveryPerSecond = 5.0f;

    // Sleep debt (long-term; only recovered by sleeping). ~70 after 20 waking hours.
    float sleepDebt = 0.0f;
    float maxSleepDebt = 100.0f;
    float sleepDebtGainPerSecond = 0.058f;

    // Body temperature in Celsius; drifts toward what the environment allows.
    float bodyTemperature = 37.0f;

    // Degrees of ambient warmth added by worn clothing. Recomputed each tick
    // from the inventory, not saved.
    float clothingInsulation = 0.0f;

    // Illness (0 = healthy, 100 = severe)
    float illness = 0.0f;

    // Phase 5: what was taken, and when (absolute game hours, day * 24 + hour).
    float coffeeHours = 0.0f;         // game hours a coffee still hides tiredness
    float coffeeJitterHours = 0.0f;   // two coffees too close: the shakes
    double lastCoffeeHour = -1000.0;
    double lastSedativeHour = -1000.0;   // Painkillers or Spirits: spoils a night
    double lastPainkillerHour = -1000.0; // stitches hurt less within 2 h
    float restedHours = 0.0f;         // after a full night: "Rested"

    // Phase 6: dirty water or bad food (hunger and thirst drain x1.5), and
    // the warmth of a hot meal (+5 degrees). Game hours left.
    float badStomachHours = 0.0f;
    float hotMealHours = 0.0f;
};
