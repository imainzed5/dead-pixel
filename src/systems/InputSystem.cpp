#include "systems/InputSystem.h"

#include <glm/geometric.hpp>

#include <cmath>

#include "components/Facing.h"
#include "components/PlayerInput.h"
#include "components/Stamina.h"
#include "components/Traits.h"
#include "components/Transform.h"
#include "components/Velocity.h"

void InputSystem::update(World& world, const Input& input, const glm::vec2& mouseWorld) const
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
                !facing.crouching &&
                glm::dot(moveInput, moveInput) > 0.0f;

            float speedMultiplier = 1.0f;
            if (facing.crouching)
            {
                speedMultiplier = kCrouchSpeedMultiplier;
            }
            else if (facing.running)
            {
                speedMultiplier = kRunSpeedMultiplier;
            }

            if (world.hasComponent<Stamina>(entity))
            {
                const Stamina& stamina = world.getComponent<Stamina>(entity);
                if (stamina.maximum > 0.0f && stamina.current <= stamina.maximum * 0.2f)
                {
                    speedMultiplier *= 0.6f;
                }
            }

            // Quick trait: +15% speed
            if (world.hasComponent<Traits>(entity))
            {
                const Traits& traits = world.getComponent<Traits>(entity);
                if (traits.positive == PositiveTrait::Quick)
                {
                    speedMultiplier *= 1.15f;
                }
            }

            velocity.dx = moveInput.x * (kBaseSpeed * speedMultiplier);
            velocity.dy = moveInput.y * (kBaseSpeed * speedMultiplier);

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
