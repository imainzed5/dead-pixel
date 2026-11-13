#include "systems/InputSystem.h"

#include <glm/geometric.hpp>

#include <cmath>

#include "components/Facing.h"
#include "components/PlayerInput.h"
#include "components/Stamina.h"
#include "components/Transform.h"
#include "components/Velocity.h"
#include "world/CombatRules.h"

void InputSystem::update(World& world, const Input& input, const glm::vec2& mouseWorld, float dtSeconds) const
{
    world.forEach<PlayerInput, Velocity, Facing, Transform>(
        [&](Entity entity, PlayerInput&, Velocity& velocity, Facing& facing, Transform& transform)
        {
            glm::vec2 moveInput(0.0f, 0.0f);

            // PZ-style isometric movement: WASD maps to screen directions
            // W = screen up (iso NW) = world (-1, -1)
            // S = screen down (iso SE) = world (+1, +1)
            // A = screen left (iso SW) = world (-1, +1)
            // D = screen right (iso NE) = world (+1, -1)
            if (input.isKeyDown(SDL_SCANCODE_W))
            {
                moveInput.x -= 1.0f;
                moveInput.y -= 1.0f;
            }
            if (input.isKeyDown(SDL_SCANCODE_S))
            {
                moveInput.x += 1.0f;
                moveInput.y += 1.0f;
            }
            if (input.isKeyDown(SDL_SCANCODE_A))
            {
                moveInput.x -= 1.0f;
                moveInput.y += 1.0f;
            }
            if (input.isKeyDown(SDL_SCANCODE_D))
            {
                moveInput.x += 1.0f;
                moveInput.y -= 1.0f;
            }

            if (glm::dot(moveInput, moveInput) > 0.0f)
            {
                moveInput = glm::normalize(moveInput);
            }

            facing.crouching =
                input.isKeyDown(SDL_SCANCODE_C) ||
                input.isKeyDown(SDL_SCANCODE_LCTRL) ||
                input.isKeyDown(SDL_SCANCODE_RCTRL);

            const bool runRequested =
                input.isKeyDown(SDL_SCANCODE_LSHIFT) ||
                input.isKeyDown(SDL_SCANCODE_RSHIFT);

            facing.running =
                runRequested &&
                mModifiers.canRun &&
                !facing.crouching &&
                glm::dot(moveInput, moveInput) > 0.0f;

            float speedMultiplier = 1.0f;
            if (facing.crouching)
            {
                speedMultiplier = kCrouchSpeedMultiplier;
            }
            else if (facing.running)
            {
                // A splinted leg: you hurry, but no faster than a walk.
                speedMultiplier = mModifiers.runAtWalkSpeed ? 1.0f : kRunSpeedMultiplier * mModifiers.runSpeedScale;
            }

            if (world.hasComponent<Stamina>(entity))
            {
                const Stamina& stamina = world.getComponent<Stamina>(entity);
                // Out of breath: slower, but not so slow you cannot get away.
                if (combatrules::exhausted(stamina.current, stamina.maximum))
                {
                    speedMultiplier *= combatrules::kExhaustedMoveScale;
                }
            }

            // Traits, wounds and the mind (Quick, a hurt leg, Numb).
            speedMultiplier *= mModifiers.speedScale;

            const glm::vec2 wanted = moveInput * (kBaseSpeed * speedMultiplier);
            const glm::vec2 current(velocity.dx, velocity.dy);
            const glm::vec2 change = wanted - current;
            const float changeLength = glm::length(change);
            const float step = (glm::dot(wanted, wanted) > 0.0f ? kAcceleration : kDeceleration) * dtSeconds;
            const glm::vec2 next = changeLength <= step ? wanted : current + change / changeLength * step;
            velocity.dx = next.x;
            velocity.dy = next.y;

            const glm::vec2 playerCenter(
                transform.x + kSpriteSize * 0.5f,
                transform.y + kSpriteSize * 0.5f);
            const glm::vec2 toMouse = mouseWorld - playerCenter;

            if (glm::dot(toMouse, toMouse) > 0.0001f)
            {
                facing.angleRadians = std::atan2(toMouse.y, toMouse.x);
            }
        });
}
