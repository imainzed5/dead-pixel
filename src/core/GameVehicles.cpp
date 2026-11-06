// Cars: getting in and out, refuelling, driving, fuel and wear, running zombies over.
#include "core/GameShared.h"

using namespace gameshared;

// V enters or leaves the nearest car; Shift+V refuels it.
void Game::handleVehicleKey(bool playerAlive)
{
    const bool drivingVehicle = mActiveVehicleEntity != kInvalidEntity;

    if (playerAlive &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Transform>(mPlayerEntity) &&
        mWorld.hasComponent<Inventory>(mPlayerEntity) &&
        mInput.wasKeyPressed(SDL_SCANCODE_V) &&
        !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        Inventory& playerInv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const Transform& playerTransform = mWorld.getComponent<Transform>(mPlayerEntity);
        const glm::vec2 playerCenter(playerTransform.x + kSpriteSize * 0.5f, playerTransform.y + kSpriteSize * 0.5f);

        if (drivingVehicle)
        {
            Vehicle& vehicle = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
            vehicle.playerInside = false;
            vehicle.engineOn = false;
            mActiveVehicleEntity = kInvalidEntity;
            mInteractionMessage = "Exited sedan.";
            mInteractionMessageTimer = 1.7f;
        }
        else
        {
            Entity nearestVehicle = kInvalidEntity;
            float bestDistSq = kVehicleInteractRangePixels * kVehicleInteractRangePixels;
            mWorld.forEach<Vehicle, Transform>(
                [&](Entity entity, Vehicle&, Transform& vt)
                {
                    const glm::vec2 vehicleCenter(vt.x + kSpriteSize * 0.5f, vt.y + kSpriteSize * 0.5f);
                    const glm::vec2 d = vehicleCenter - playerCenter;
                    const float distSq = glm::dot(d, d);
                    if (distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        nearestVehicle = entity;
                    }
                });

            if (nearestVehicle != kInvalidEntity)
            {
                Vehicle& vehicle = mWorld.getComponent<Vehicle>(nearestVehicle);

                const bool refuelRequested =
                    mInput.isKeyDown(SDL_SCANCODE_LSHIFT) ||
                    mInput.isKeyDown(SDL_SCANCODE_RSHIFT);

                if (refuelRequested)
                {
                    if (vehicle.fuel >= vehicle.maxFuel - 0.01f)
                    {
                        mInteractionMessage = "Sedan tank is already full.";
                        mInteractionMessageTimer = 2.0f;
                    }
                    else if (InventorySystem::removeItem(playerInv, 12, 1))
                    {
                        vehicle.fuel = std::min(vehicle.maxFuel, vehicle.fuel + kVehicleRefuelAmount);
                        mInteractionMessage = "Refueled sedan.";
                        mInteractionMessageTimer = 2.0f;
                    }
                    else
                    {
                        mInteractionMessage = "Need a fuel can to refuel.";
                        mInteractionMessageTimer = 2.0f;
                    }
                }
                else if (vehicle.condition < 10.0f)
                {
                    mInteractionMessage = "Sedan is broken.";
                    mInteractionMessageTimer = 2.0f;
                }
                else if (vehicle.fuel <= 0.01f)
                {
                    mInteractionMessage = "Sedan is out of fuel. Shift+V to refuel.";
                    mInteractionMessageTimer = 2.4f;
                }
                else
                {
                    vehicle.playerInside = true;
                    vehicle.engineOn = true;
                    mActiveVehicleEntity = nearestVehicle;
                    mShowCrafting = false;
                    mContainerOpen = false;
                    mOpenContainerEntity = kInvalidEntity;
                    mInteractionMessage = "Driving sedan.";
                    mInteractionMessageTimer = 1.8f;
                }
            }
        }
    }
}

// While driving, the car sets the player's speed; stalls when broken or dry.
void Game::applyVehicleDriving()
{
    const bool drivingVehicle = mActiveVehicleEntity != kInvalidEntity;

    // Vehicle driving overrides on-foot speed profile.
    if (drivingVehicle &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Velocity>(mPlayerEntity) &&
        mWorld.hasComponent<Vehicle>(mActiveVehicleEntity))
    {
        Vehicle& vehicle = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);

        const float driveSpeed = vehicleSpeedFromCondition(vehicle.condition);
        if (driveSpeed <= 0.0f || vehicle.fuel <= 0.01f)
        {
            vehicle.engineOn = false;
            vehicle.playerInside = false;
            mActiveVehicleEntity = kInvalidEntity;
            vel.dx = 0.0f;
            vel.dy = 0.0f;
            mInteractionMessage = "Sedan stalled.";
            mInteractionMessageTimer = 1.8f;
        }
        else
        {
            const float speedSq = vel.dx * vel.dx + vel.dy * vel.dy;
            if (speedSq > 0.001f)
            {
                const float invLen = 1.0f / std::sqrt(speedSq);
                vel.dx = vel.dx * invLen * driveSpeed;
                vel.dy = vel.dy * invLen * driveSpeed;
            }
            else
            {
                vel.dx = 0.0f;
                vel.dy = 0.0f;
            }

            if (mWorld.hasComponent<Facing>(mPlayerEntity))
            {
                Facing& facing = mWorld.getComponent<Facing>(mPlayerEntity);
                facing.running = false;
                facing.crouching = false;
            }
        }
    }
}

// After movement: drag the car along, burn fuel, wear, run over zombies.
void Game::updateVehicleAfterPhysics(float dtSeconds)
{
    const bool drivingVehicle = mActiveVehicleEntity != kInvalidEntity;

    // Post-physics vehicle state update.
    if (drivingVehicle &&
        mActiveVehicleEntity != kInvalidEntity &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Vehicle>(mActiveVehicleEntity) &&
        mWorld.hasComponent<Transform>(mActiveVehicleEntity) &&
        mWorld.hasComponent<Transform>(mPlayerEntity) &&
        mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Vehicle& vehicle = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
        Transform& vehicleTransform = mWorld.getComponent<Transform>(mActiveVehicleEntity);
        const Transform& playerTransform = mWorld.getComponent<Transform>(mPlayerEntity);
        const Velocity& playerVelocity = mWorld.getComponent<Velocity>(mPlayerEntity);

        // Keep the vehicle entity aligned with the player's world position while occupied.
        vehicleTransform.x = playerTransform.x;
        vehicleTransform.y = playerTransform.y;

        const float speedSq = playerVelocity.dx * playerVelocity.dx + playerVelocity.dy * playerVelocity.dy;
        if (speedSq > 1.0f)
        {
            const float weatherWear = 1.0f + mWeatherState.rainIntensity * 0.6f;
            vehicle.fuel = std::max(0.0f, vehicle.fuel - dtSeconds * kVehicleFuelPerSecond * weatherWear);
            vehicle.condition = std::max(0.0f, vehicle.condition - dtSeconds * 0.18f * weatherWear);
        }

        vehicle.collisionDamageCooldown = std::max(0.0f, vehicle.collisionDamageCooldown - dtSeconds);

        const glm::vec2 vehicleCenter(vehicleTransform.x + kSpriteSize * 0.5f, vehicleTransform.y + kSpriteSize * 0.5f);
        if (speedSq > 20.0f && vehicle.collisionDamageCooldown <= 0.0f)
        {
            bool hitZombie = false;
            mWorld.forEach<ZombieAI, Transform, Health>(
                [&](Entity, ZombieAI& zai, Transform& zt, Health& zh)
                {
                    if (hitZombie || zh.current <= 0.0f)
                    {
                        return;
                    }

                    const glm::vec2 zombieCenter(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                    if (glm::distance(vehicleCenter, zombieCenter) <= kVehicleCollisionRangePixels)
                    {
                        zh.current = std::max(0.0f, zh.current - 65.0f);
                        zai.lastDamagedBy = mPlayerEntity; // only the player drives
                        vehicle.condition = std::max(0.0f, vehicle.condition - 6.0f);
                        vehicle.collisionDamageCooldown = 0.22f;
                        hitZombie = true;
                        mAudioManager.playSound(SoundId::WeaponHit, 0.35f);
                    }
                });
        }

        if (vehicle.fuel <= 0.01f || vehicle.condition < 10.0f)
        {
            vehicle.engineOn = false;
            vehicle.playerInside = false;
            mActiveVehicleEntity = kInvalidEntity;
            if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
            {
                Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
                vel.dx = 0.0f;
                vel.dy = 0.0f;
            }
            mInteractionMessage = (vehicle.condition < 10.0f) ? "Sedan broke down." : "Sedan ran out of fuel.";
            mInteractionMessageTimer = 2.2f;
        }
    }
}
