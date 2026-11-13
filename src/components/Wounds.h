#pragma once

#include <vector>

enum class WoundType
{
    Cut,
    Bruise,
    Bite,
    Fracture // arms and legs only (phase 5)
};

enum class BodyPart
{
    Head,
    Torso,
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg
};

enum class WoundDepth
{
    Shallow,
    Deep
};

// One wound on the player. What it does, how it heals and what treats it are
// in world/WoundRules.h.
struct Wound
{
    WoundType type = WoundType::Bruise;
    BodyPart part = BodyPart::Torso;
    WoundDepth depth = WoundDepth::Shallow;
    bool dressed = false;  // a dressing on it (goes dirty after 12 game hours)
    bool stitched = false; // a deep wound sewn shut
    bool splinted = false; // a fracture set
    bool infected = false;
    // Healing progress in real seconds at the normal rate (60 = one game
    // hour); it closes when this reaches the wound's heal time.
    float healTimer = 0.0f;
    float infectionTimer = 0.0f; // game hours a dirty bite has festered
    bool contaminated = false;   // dirty: a bite infects, a cut may
    float dressingHours = 0.0f;  // game hours the current dressing has been on
    float infectedHours = 0.0f;  // game hours infected and untreated
    // Hopeless tone only: an infected bite left a day turns. Nothing cures it.
    bool turning = false;
    float turningHours = 0.0f;
    float turnDeathHours = 18.0f; // how long the turning takes (12 to 24)
};

struct Wounds
{
    std::vector<Wound> active;
};
