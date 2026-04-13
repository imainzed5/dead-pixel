#include "systems/BuildSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "components/Health.h"
#include "components/Interactable.h"
#include "components/Inventory.h"
#include "components/Sprite.h"
#include "components/Structure.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "systems/InventorySystem.h"

const BuildRecipe BuildSystem::kRecipes[kRecipeCount] = {
    {StructureType::Barricade,     "Barricade",      "env_barricade",     9,  3, 120.0f},  // 3 wood planks
    {StructureType::NoiseTrap,     "Noise Trap",     "env_noise_trap",    13, 2, 30.0f},   // 2 glass shards
    {StructureType::Campfire,      "Campfire",       "env_campfire",      9,  2, 60.0f},   // 2 wood planks
    {StructureType::SleepingArea,  "Sleeping Area",  "env_sleeping_area", 11, 2, 80.0f},   // 2 rope
    {StructureType::SupplyCache,   "Supply Cache",   "env_crate",         9,  3, 150.0f},  // 3 wood planks
    {StructureType::RainCollector, "Rain Collector", "env_crate",         10, 2, 100.0f},  // 2 scrap metal
};

namespace
{
constexpr float kStructureInteractRange = 48.0f;

Entity findNearestStructure(World& world, const glm::vec2& playerCenter, float kSpriteSize)
{
    Entity bestEntity = kInvalidEntity;
    float bestDistSq = kStructureInteractRange * kStructureInteractRange;

    world.forEach<Structure, Transform>(
        [&](Entity entity, Structure&, Transform& st)
        {
            const glm::vec2 sc(st.x + kSpriteSize * 0.5f, st.y + kSpriteSize * 0.5f);
            const glm::vec2 delta = sc - playerCenter;
            const float distSq = glm::dot(delta, delta);
            if (distSq < bestDistSq)
            {
                bestDistSq = distSq;
                bestEntity = entity;
            }
        });

    return bestEntity;
}
}

std::string BuildSystem::update(World& world, const Input& input, const TileMap& tileMap,
                          NoiseModel& noiseModel, const ItemDatabase& db,
                          const SpriteSheet& spriteSheet, Entity playerEntity,
                          const glm::vec2& mouseWorld, float dtSeconds, double gameHours)
{
    std::string resultMessage;

    // --- Repair / Dismantle when NOT in build mode ---
    if (!mBuildMode && playerEntity != kInvalidEntity &&
        world.hasComponent<Transform>(playerEntity) &&
        world.hasComponent<Inventory>(playerEntity))
    {
        const Transform& pt = world.getComponent<Transform>(playerEntity);
        const glm::vec2 pc(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);
        Entity nearStruct = findNearestStructure(world, pc, kSpriteSize);

        if (nearStruct != kInvalidEntity)
        {
            Structure& structure = world.getComponent<Structure>(nearStruct);
            Inventory& inv = world.getComponent<Inventory>(playerEntity);

            // R: Repair — costs 1 of the recipe material, restores 40% HP
            if (input.wasKeyPressed(SDL_SCANCODE_R) && structure.hp < structure.maxHp)
            {
                const int recipeIdx = static_cast<int>(structure.type);
                if (recipeIdx >= 0 && recipeIdx < kRecipeCount)
                {
                    const auto& recipe = kRecipes[recipeIdx];
                    if (InventorySystem::countItem(inv, recipe.materialId) >= 1)
                    {
                        InventorySystem::removeItem(inv, recipe.materialId, 1);
                        structure.hp = std::min(structure.maxHp, structure.hp + structure.maxHp * 0.4f);
                        resultMessage = "Repaired " + std::string(recipe.name) + ".";
                    }
                    else
                    {
                        resultMessage = "Need materials to repair.";
                    }
                }
            }

            // X: Dismantle — destroy structure, recover 1 material
            if (input.wasKeyPressed(SDL_SCANCODE_X))
            {
                const int recipeIdx = static_cast<int>(structure.type);
                if (recipeIdx >= 0 && recipeIdx < kRecipeCount)
                {
                    const auto& recipe = kRecipes[recipeIdx];
                    InventorySystem::addItem(inv, db, recipe.materialId, 1);
                    world.destroyEntity(nearStruct);
                    resultMessage = "Dismantled " + std::string(recipe.name) + ". Recovered 1 material.";
                }
            }
        }
    }

    // Toggle build mode
    if (input.wasKeyPressed(SDL_SCANCODE_B))
    {
        mBuildMode = !mBuildMode;
        mSelectedRecipe = 0;
    }

    if (!mBuildMode) return resultMessage;

    // Cycle recipes with scroll or 1-6 keys
    if (input.wasKeyPressed(SDL_SCANCODE_1)) mSelectedRecipe = 0;
    if (input.wasKeyPressed(SDL_SCANCODE_2)) mSelectedRecipe = 1;
    if (input.wasKeyPressed(SDL_SCANCODE_3)) mSelectedRecipe = 2;
    if (input.wasKeyPressed(SDL_SCANCODE_4)) mSelectedRecipe = 3;
    if (input.wasKeyPressed(SDL_SCANCODE_5)) mSelectedRecipe = 4;
    if (input.wasKeyPressed(SDL_SCANCODE_6)) mSelectedRecipe = 5;
    mSelectedRecipe = std::clamp(mSelectedRecipe, 0, kRecipeCount - 1);

    // Snap ghost to tile grid
    const int tw = tileMap.tileWidth();
    const int th = tileMap.tileHeight();
    if (tw > 0 && th > 0)
    {
        const int tileX = static_cast<int>(std::floor(mouseWorld.x / static_cast<float>(tw)));
        const int tileY = static_cast<int>(std::floor(mouseWorld.y / static_cast<float>(th)));
        mGhostPosition = glm::vec2(static_cast<float>(tileX * tw), static_cast<float>(tileY * th));

        // Valid if not solid and not occupied by another structure
        mGhostValid = !tileMap.isSolid(tileX, tileY);

        // Check for existing structures at this tile
        if (mGhostValid)
        {
            world.forEach<Structure, Transform>(
                [&](Entity, Structure&, Transform& st)
                {
                    const int stx = static_cast<int>(std::floor(st.x / static_cast<float>(tw)));
                    const int sty = static_cast<int>(std::floor(st.y / static_cast<float>(th)));
                    if (stx == tileX && sty == tileY)
                    {
                        mGhostValid = false;
                    }
                });
        }
    }
    else
    {
        mGhostValid = false;
    }

    // Place structure on left click
    if (input.wasMouseButtonPressed(SDL_BUTTON_LEFT) && mGhostValid &&
        playerEntity != kInvalidEntity && world.hasComponent<Inventory>(playerEntity))
    {
        const BuildRecipe& recipe = kRecipes[mSelectedRecipe];
        Inventory& inv = world.getComponent<Inventory>(playerEntity);

        if (InventorySystem::countItem(inv, recipe.materialId) >= recipe.materialCost)
        {
            InventorySystem::removeItem(inv, recipe.materialId, recipe.materialCost);

            Entity structEntity = world.createEntity();

            Transform t{};
            t.x = mGhostPosition.x;
            t.y = mGhostPosition.y;

            Sprite s{};
            s.textureId = spriteSheet.textureId();
            s.uvRect = spriteSheet.uvRect(recipe.sprite);
            s.color = glm::vec4(1.0f);
            s.layer = 3;

            Structure structure{};
            structure.type = recipe.type;
            structure.hp = recipe.hp;
            structure.maxHp = recipe.hp;

            world.addComponent<Transform>(structEntity, t);
            world.addComponent<Sprite>(structEntity, s);
            world.addComponent<Structure>(structEntity, structure);

            // Supply Cache: make it a container (interactable + inventory)
            if (recipe.type == StructureType::SupplyCache)
            {
                Interactable cacheInteract{};
                cacheInteract.type = InteractableType::Container;
                cacheInteract.prompt = "Open Cache";
                world.addComponent<Interactable>(structEntity, cacheInteract);
                world.addComponent<Inventory>(structEntity, Inventory{});
            }

            // SleepingArea: make interactable so player can sleep
            if (recipe.type == StructureType::SleepingArea)
            {
                Interactable sleepInteract{};
                sleepInteract.type = InteractableType::SleepSpot;
                sleepInteract.sleepHours = 8.0f;
                sleepInteract.sleepQuality = 0.9f;
                sleepInteract.prompt = "Sleep";
                world.addComponent<Interactable>(structEntity, sleepInteract);
            }

            // Campfire emits soft noise and light
            if (recipe.type == StructureType::Campfire)
            {
                noiseModel.emitNoise(mGhostPosition + glm::vec2(kSpriteSize * 0.5f), NoiseTier::Soft, 2.0f, structEntity);
            }

            // Rain Collector: interactable container for collecting water
            if (recipe.type == StructureType::RainCollector)
            {
                Interactable rcInteract{};
                rcInteract.type = InteractableType::Container;
                rcInteract.prompt = "Collect Water";
                world.addComponent<Interactable>(structEntity, rcInteract);
                world.addComponent<Inventory>(structEntity, Inventory{});
            }

            mBuildMode = false;
        }
    }

    // Cancel with right click or ESC
    if (input.wasMouseButtonPressed(SDL_BUTTON_RIGHT) || input.wasKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        mBuildMode = false;
    }

    // --- Structure updates (decay, noise trap trigger, campfire warmth) ---
    (void)dtSeconds;
    const float hoursPerTick = static_cast<float>(dtSeconds) / 3600.0f; // approximate

    std::vector<Entity> destroyedStructures;

    world.forEach<Structure, Transform>(
        [&](Entity entity, Structure& structure, Transform& st)
        {
            // Decay over time
            structure.hp = std::max(0.0f, structure.hp - structure.decayRatePerHour * hoursPerTick);

            // Update condition
            const float ratio = structure.maxHp > 0.0f ? structure.hp / structure.maxHp : 0.0f;
            if (ratio > 0.6f)
                structure.condition = StructureCondition::Intact;
            else if (ratio > 0.25f)
                structure.condition = StructureCondition::Damaged;
            else
                structure.condition = StructureCondition::Breaking;

            if (structure.hp <= 0.0f)
            {
                destroyedStructures.push_back(entity);
                return;
            }

            const glm::vec2 structCenter(st.x + kSpriteSize * 0.5f, st.y + kSpriteSize * 0.5f);

            // Noise trap: trigger when zombie walks over
            if (structure.type == StructureType::NoiseTrap && !structure.triggered)
            {
                world.forEach<ZombieAI, Transform>(
                    [&](Entity, ZombieAI&, Transform& zt)
                    {
                        if (structure.triggered) return;
                        const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                        if (glm::distance(structCenter, zc) < kSpriteSize)
                        {
                            noiseModel.emitNoise(structCenter, NoiseTier::Loud, 1.0f, entity);
                            structure.triggered = true;
                            destroyedStructures.push_back(entity);
                        }
                    });
            }

            // Barricade: zombies damage it
            if (structure.type == StructureType::Barricade)
            {
                world.forEach<ZombieAI, Transform>(
                    [&](Entity, ZombieAI&, Transform& zt)
                    {
                        const glm::vec2 zc(zt.x + kSpriteSize * 0.5f, zt.y + kSpriteSize * 0.5f);
                        if (glm::distance(structCenter, zc) < kSpriteSize * 1.2f)
                        {
                            structure.hp -= 5.0f * dtSeconds;
                        }
                    });
            }

            // Campfire: continuous soft noise
            if (structure.type == StructureType::Campfire)
            {
                structure.noiseTimer += dtSeconds;
                if (structure.noiseTimer >= 4.0f)
                {
                    noiseModel.emitNoise(structCenter, NoiseTier::Soft, 2.0f, entity);
                    structure.noiseTimer = 0.0f;
                }
            }

            // Rain Collector: generate water bottle every 6 game-hours
            if (structure.type == StructureType::RainCollector)
            {
                const float gameHoursThisTick = dtSeconds / 3600.0f; // approximate
                structure.waterGenTimer += gameHoursThisTick;
                if (structure.waterGenTimer >= 6.0f)
                {
                    structure.waterGenTimer = 0.0f;
                    if (world.hasComponent<Inventory>(entity))
                    {
                        Inventory& inv = world.getComponent<Inventory>(entity);
                        InventorySystem::addItem(inv, db, 3, 1); // water bottle (id 3)
                    }
                }
            }
        });

    for (Entity e : destroyedStructures)
    {
        world.destroyEntity(e);
    }

    return resultMessage;
}
