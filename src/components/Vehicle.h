#pragma once

#include <cstdint>

#include <glm/vec2.hpp>

#include "ecs/Types.h"

// The three vehicles of the town (phase 11). Saved as an int: keep the numbers.
enum class VehicleKind : std::uint8_t
{
    Sedan = 0,
    Pickup = 1,
    Bicycle = 2,
    Count
};

// A vehicle is its own body: it has a place and a heading in the world, a
// trunk, a key or a wire, and wear. VehicleSystem moves it; GameVehicles.cpp
// gets the player in and out and does what E does at it. The world remembers
// all of it (SaveManager's vehicle records).
struct Vehicle
{
    VehicleKind kind = VehicleKind::Sedan;
    int color = 0; // which paint (vehiclerules::colorName)

    // The body. `heading` in radians in world space (0 = +x); `speed` in
    // world px/s along it (negative: reversing); `steer` -1..1 as last driven.
    float heading = 0.0f;
    float speed = 0.0f;
    float steer = 0.0f;

    // 0-100 condition (vehiclerules::band); fuel units.
    float condition = 100.0f;
    float fuel = 0.0f;
    float maxFuel = 30.0f;
    int tyres = 4; // good ones; each flat cuts the top speed

    // The trunk: an entity with an Inventory (and TrunkOf pointing back).
    Entity trunk = kInvalidEntity;

    // Starting it. `started`: hot-wired or keyed once, startable for good.
    // `keyKnown`: the player has the key (found in it or in a house).
    // `keyInside`: the key was left in it (found when you get in).
    // `keyTile`: the container of the nearest house holding the key, or -1.
    bool started = false;
    bool keyKnown = false;
    bool keyInside = false;
    glm::ivec2 keyTile{-1, -1};

    // A live alarm for the first days of a life (its own battery).
    bool alarm = false;
    int alarmSpentRun = 0;    // the life it rang out in (dead for the rest of it)
    float alarmSeconds = 0.0f; // > 0: ringing

    // A window broken by the dead: the driver can be grabbed through it.
    bool windowBroken = false;
    float batterSeconds = 0.0f; // battered by three or more

    // A pickup's load: the origin of the furniture piece riding in the bed.
    bool hasCargo = false;
    glm::ivec2 cargoOrigin{0, 0};

    // Someone's car (phase 8): the building they live in, or 0.
    int ownerBuilding = 0;

    // Runtime state.
    bool engineOn = false;
    bool playerInside = false;
    bool headlights = false;
    bool usedThisLife = false; // driven or ridden: one stage worse next life
    float engineNoiseTimer = 0.0f;
    float collisionDamageCooldown = 0.0f;
    float crankTimer = 0.0f; // > 0: the starter turning over
};

// The trunk's entity points back at its vehicle (so the container code can
// tell a trunk from a cupboard and keep it out of the tile records).
struct TrunkOf
{
    Entity vehicle = kInvalidEntity;
};
