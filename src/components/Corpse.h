#pragma once

#include "world/ZombieKinds.h"

// A body left where a zombie fell. Visual only; the oldest are cleared away
// once there are too many.
struct Corpse
{
    float age = 0.0f;
    // What is left of the blow that killed it: the body slides and settles.
    float slideX = 0.0f;
    float slideY = 0.0f;
    int look = 0; // which zombie look it wore
    int dir = 1;  // which way it faced as it fell (see Animation::dir)
    ZombieKind kind = ZombieKind::Shambler; // the build it keeps on the floor
    bool searched = false; // its pockets rolled (phase 4); then it holds an Inventory
};
