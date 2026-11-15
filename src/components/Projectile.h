#pragma once

#include "ecs/Types.h"

// Something in flight. Bullets (survivors' shots) fly straight and hit the
// first body; thrown things (phase 3) arc to where they were aimed and land.
enum class ProjectileKind
{
    Bullet,
    Thrown
};

struct Projectile
{
    float vx = 0.0f;
    float vy = 0.0f;
    float damage = 55.0f;
    float rangeRemaining = 192.0f;
    float speed = 600.0f;
    Entity source = 0xFFFFFFFF;

    // Thrown things: what it is, how far it has to go and has gone (px),
    // how high it rides now (px, drawn by lifting the sprite), and an alarm
    // clock's seconds before it rings (< 0: not wound).
    ProjectileKind kind = ProjectileKind::Bullet;
    int itemId = -1;
    float flightTotal = 0.0f;
    float flown = 0.0f;
    float arcHeight = 0.0f;
    float fuseSeconds = -1.0f;
};
