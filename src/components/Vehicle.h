#pragma once

struct Vehicle
{
    // 0-100 condition. Higher values drive faster and quieter.
    float condition = 100.0f;

    // Current and maximum fuel units for this vehicle.
    float fuel = 24.0f;
    float maxFuel = 30.0f;

    // Runtime state while driving.
    bool engineOn = false;
    bool playerInside = false;
    float engineNoiseTimer = 0.0f;
    float collisionDamageCooldown = 0.0f;
};
