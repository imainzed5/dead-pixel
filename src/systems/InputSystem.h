#pragma once

#include <glm/vec2.hpp>

#include "core/Input.h"
#include "ecs/World.h"

// What the body does to a walk (phase 5), set by the game each tick: traits,
// wounds, the mind and Fitness (world/TraitRules, WoundRules, MindRules,
// SkillRules).
struct MoveModifiers
{
    float speedScale = 1.0f;    // Quick, a hurt leg, Numb
    float runSpeedScale = 1.0f; // Fitness
    bool canRun = true;         // a broken leg, unsplinted
    bool runAtWalkSpeed = false; // a splinted leg
    float accelScale = 1.0f;    // ice: speeding up and stopping take twice as long
};

class InputSystem
{
public:
    void update(World& world, const Input& input, const glm::vec2& mouseWorld, float dtSeconds = 1.0f / 60.0f) const;
    void setModifiers(const MoveModifiers& modifiers) { mModifiers = modifiers; }
    [[nodiscard]] const MoveModifiers& modifiers() const { return mModifiers; }

private:
    static constexpr float kBaseSpeed = 180.0f;
    static constexpr float kRunSpeedMultiplier = 1.75f;
    static constexpr float kCrouchSpeedMultiplier = 0.5f;
    // The player gathers speed and sheds it over a few frames rather than
    // snapping between standing and full speed (world px/s^2).
    static constexpr float kAcceleration = 1700.0f;
    static constexpr float kDeceleration = 2300.0f;
    static constexpr float kSpriteSize = 32.0f;
    MoveModifiers mModifiers;
};
