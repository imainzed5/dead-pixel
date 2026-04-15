#include "systems/InteractionSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/geometric.hpp>

#include "components/Bleeding.h"
#include "components/Combat.h"
#include "components/Health.h"
#include "components/Interactable.h"
#include "components/Inventory.h"
#include "components/Needs.h"
#include "components/Sleeping.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "items/ItemDatabase.h"
#include "systems/InventorySystem.h"

namespace
{
constexpr float kInteractRangePixels = 32.0f;
constexpr float kSpriteHalfSize = 16.0f;
constexpr float kSleepThreatRangePixels = 128.0f;
constexpr float kMinimumSleepDebt = 18.0f;
constexpr float kMinimumFatigue = 25.0f;

int interactionPriority(InteractableType type)
{
    switch (type)
    {
    case InteractableType::FoodPickup:
    case InteractableType::WaterPickup:
    case InteractableType::WeaponPickup:
    case InteractableType::BandagePickup:
        return 4;
    case InteractableType::Container:
        return 3;
    case InteractableType::JournalNote:
    case InteractableType::GraveMarker:
        return 2;
    case InteractableType::SleepSpot:
    default:
        return 1;
    }
}
}

std::string InteractionSystem::update(World& world, const Input& input, Entity playerEntity, const ItemDatabase* itemDb) const
{
    if (!input.wasKeyPressed(SDL_SCANCODE_E))
    {
        return {};
    }

    if (playerEntity == kInvalidEntity ||
        !world.hasComponent<Transform>(playerEntity) ||
        !world.hasComponent<Needs>(playerEntity))
    {
        return {};
    }

    if (world.hasComponent<Health>(playerEntity))
    {
        const Health& health = world.getComponent<Health>(playerEntity);
        if (health.current <= 0.0f)
        {
            return {};
        }
    }

    Needs& playerNeeds = world.getComponent<Needs>(playerEntity);
    const Transform& playerTransform = world.getComponent<Transform>(playerEntity);
    const glm::vec2 playerCenter(playerTransform.x + kSpriteHalfSize, playerTransform.y + kSpriteHalfSize);

    const bool hasInventory = world.hasComponent<Inventory>(playerEntity) && itemDb != nullptr;

    Entity bestEntity = kInvalidEntity;
    float bestDistanceSq = kInteractRangePixels * kInteractRangePixels;
    int bestPriority = -1;

    world.forEach<Interactable, Transform>(
        [&](Entity entity, Interactable& interactable, Transform& transform)
        {
            const glm::vec2 itemCenter(transform.x + kSpriteHalfSize, transform.y + kSpriteHalfSize);
            const glm::vec2 delta = itemCenter - playerCenter;
            const float distanceSq = glm::dot(delta, delta);
            const int priority = interactionPriority(interactable.type);

            if (distanceSq < bestDistanceSq - 0.01f ||
                (std::abs(distanceSq - bestDistanceSq) <= 0.01f && priority > bestPriority))
            {
                bestEntity = entity;
                bestDistanceSq = distanceSq;
                bestPriority = priority;
            }
        });

    if (bestEntity == kInvalidEntity)
    {
        return {};
    }

    const Interactable& interactable = world.getComponent<Interactable>(bestEntity);
    switch (interactable.type)
    {
    case InteractableType::FoodPickup:
    {
        if (hasInventory)
        {
            Inventory& inv = world.getComponent<Inventory>(playerEntity);
            // Try to find a matching food item in the database by checking items that restore hunger
            int foodId = 0; // default: canned food
            const auto& allItems = itemDb->allItems();
            for (const auto& item : allItems)
            {
                if (item.category == ItemCategory::Food && std::abs(item.hungerRestore - interactable.hungerRestore) < 5.0f)
                {
                    foodId = item.id;
                    break;
                }
            }
            auto result = InventorySystem::addItem(inv, *itemDb, foodId, 1);
            if (result.added > 0)
            {
                world.destroyEntity(bestEntity);
                return "Picked up food.";
            }
            return "Inventory full.";
        }
        playerNeeds.hunger = std::min(playerNeeds.maxHunger, playerNeeds.hunger + interactable.hungerRestore);
        world.destroyEntity(bestEntity);
        return "Ate food (hunger restored).";
    }

    case InteractableType::WaterPickup:
    {
        if (hasInventory)
        {
            Inventory& inv = world.getComponent<Inventory>(playerEntity);
            int waterId = 3; // water bottle id
            auto result = InventorySystem::addItem(inv, *itemDb, waterId, 1);
            if (result.added > 0)
            {
                world.destroyEntity(bestEntity);
                return "Picked up water.";
            }
            return "Inventory full.";
        }
        playerNeeds.thirst = std::min(playerNeeds.maxThirst, playerNeeds.thirst + interactable.thirstRestore);
        world.destroyEntity(bestEntity);
        return "Drank water (thirst restored).";
    }

    case InteractableType::SleepSpot:
    {
        if (world.hasComponent<Sleeping>(playerEntity))
        {
            return "Already sleeping.";
        }

        if (playerNeeds.sleepDebt < kMinimumSleepDebt && playerNeeds.fatigue < kMinimumFatigue)
        {
            return "Not tired enough to sleep.";
        }

        bool nearbyThreat = false;
        world.forEach<ZombieAI, Transform>(
            [&](Entity, ZombieAI&, Transform& transform)
            {
                if (nearbyThreat)
                {
                    return;
                }

                const glm::vec2 zombieCenter(transform.x + kSpriteHalfSize, transform.y + kSpriteHalfSize);
                const glm::vec2 delta = zombieCenter - playerCenter;
                if (glm::dot(delta, delta) < kSleepThreatRangePixels * kSleepThreatRangePixels)
                {
                    nearbyThreat = true;
                }
            });

        if (nearbyThreat)
        {
            return "Too dangerous to sleep here.";
        }

        Sleeping sleeping{};
        sleeping.durationRemaining = interactable.sleepHours;
        sleeping.quality = interactable.sleepQuality;
        world.addComponent<Sleeping>(playerEntity, sleeping);
        return "Lying down to sleep...";
    }

    case InteractableType::WeaponPickup:
    {
        if (hasInventory)
        {
            Inventory& inv = world.getComponent<Inventory>(playerEntity);
            // Map weapon category to item ID
            int weaponItemId = 5; // default: knife
            switch (static_cast<WeaponCategory>(interactable.weaponCategory))
            {
            case WeaponCategory::Bladed: weaponItemId = 5; break;  // knife
            case WeaponCategory::Blunt: weaponItemId = 6; break;   // bat
            default: weaponItemId = 7; break;                       // pipe
            }
            auto result = InventorySystem::addItem(inv, *itemDb, weaponItemId, 1);
            if (result.added > 0)
            {
                world.destroyEntity(bestEntity);
                return "Picked up weapon.";
            }
            return "Inventory full.";
        }
        if (!world.hasComponent<Combat>(playerEntity))
        {
            return {};
        }
        Combat& combat = world.getComponent<Combat>(playerEntity);
        const auto cat = static_cast<WeaponCategory>(interactable.weaponCategory);
        combat.weapon = makeWeaponStats(cat);
        world.destroyEntity(bestEntity);
        switch (cat)
        {
        case WeaponCategory::Bladed:   return "Picked up knife.";
        case WeaponCategory::Blunt:    return "Picked up bat.";
        case WeaponCategory::Ranged:   return "Picked up pistol.";
        case WeaponCategory::Unarmed:  return "Fighting bare-handed.";
        default:                       return "Picked up weapon.";
        }
    }

    case InteractableType::BandagePickup:
    {
        if (hasInventory)
        {
            Inventory& inv = world.getComponent<Inventory>(playerEntity);
            auto result = InventorySystem::addItem(inv, *itemDb, 4, 1); // bandage id = 4
            if (result.added > 0)
            {
                world.destroyEntity(bestEntity);
                return "Picked up bandage.";
            }
            return "Inventory full.";
        }
        if (world.hasComponent<Bleeding>(playerEntity))
        {
            world.removeComponent<Bleeding>(playerEntity);
        }
        world.destroyEntity(bestEntity);
        return "Applied bandage.";
    }

    case InteractableType::Container:
    {
        if (!hasInventory || !world.hasComponent<Inventory>(bestEntity))
        {
            return "Nothing useful inside.";
        }
        Inventory& playerInv = world.getComponent<Inventory>(playerEntity);
        Inventory& containerInv = world.getComponent<Inventory>(bestEntity);

        int totalTransferred = 0;
        for (auto& slot : containerInv.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0)
            {
                continue;
            }
            auto result = InventorySystem::addItem(playerInv, *itemDb, slot.itemId, slot.count);
            if (result.added > 0)
            {
                slot.count -= result.added;
                if (slot.count <= 0)
                {
                    slot.itemId = -1;
                    slot.count = 0;
                    slot.condition = 1.0f;
                }
                totalTransferred += result.added;
            }
        }

        if (totalTransferred == 0)
        {
            return "Container empty or inventory full.";
        }
        return "Looted " + std::to_string(totalTransferred) + " items.";
    }

    case InteractableType::GraveMarker:
        return interactable.message.empty() ? "An old grave marker." : interactable.message;

    case InteractableType::JournalNote:
        return interactable.message.empty() ? "A crumpled note. Nothing legible." : interactable.message;
    }

    return {};
}

ProximityPrompt InteractionSystem::getProximityPrompt(World& world, Entity playerEntity) const
{
    ProximityPrompt result;

    if (playerEntity == kInvalidEntity ||
        !world.hasComponent<Transform>(playerEntity))
    {
        return result;
    }

    if (world.hasComponent<Health>(playerEntity))
    {
        const Health& health = world.getComponent<Health>(playerEntity);
        if (health.current <= 0.0f)
        {
            return result;
        }
    }

    const Transform& playerTransform = world.getComponent<Transform>(playerEntity);
    const glm::vec2 playerCenter(playerTransform.x + kSpriteHalfSize, playerTransform.y + kSpriteHalfSize);

    Entity bestEntity = kInvalidEntity;
    float bestDistanceSq = kInteractRangePixels * kInteractRangePixels;
    int bestPriority = -1;

    world.forEach<Interactable, Transform>(
        [&](Entity entity, Interactable& interactable, Transform& transform)
        {
            const glm::vec2 itemCenter(transform.x + kSpriteHalfSize, transform.y + kSpriteHalfSize);
            const glm::vec2 delta = itemCenter - playerCenter;
            const float distanceSq = glm::dot(delta, delta);
            const int priority = interactionPriority(interactable.type);

            if (distanceSq < bestDistanceSq - 0.01f ||
                (std::abs(distanceSq - bestDistanceSq) <= 0.01f && priority > bestPriority))
            {
                bestEntity = entity;
                bestDistanceSq = distanceSq;
                bestPriority = priority;
            }
        });

    if (bestEntity == kInvalidEntity)
    {
        return result;
    }

    const Interactable& interactable = world.getComponent<Interactable>(bestEntity);
    result.entity = bestEntity;

    std::string action = interactable.prompt;
    switch (interactable.type)
    {
    case InteractableType::SleepSpot:
    {
        if (world.hasComponent<Sleeping>(playerEntity))
        {
            action = "Already sleeping";
            break;
        }

        if (world.hasComponent<Needs>(playerEntity))
        {
            const Needs& needs = world.getComponent<Needs>(playerEntity);
            if (needs.sleepDebt < kMinimumSleepDebt && needs.fatigue < kMinimumFatigue)
            {
                action = "Sleep (Not tired)";
                break;
            }
        }

        bool nearbyThreat = false;
        world.forEach<ZombieAI, Transform>(
            [&](Entity, ZombieAI&, Transform& transform)
            {
                if (nearbyThreat)
                {
                    return;
                }

                const glm::vec2 zombieCenter(transform.x + kSpriteHalfSize, transform.y + kSpriteHalfSize);
                const glm::vec2 delta = zombieCenter - playerCenter;
                if (glm::dot(delta, delta) < kSleepThreatRangePixels * kSleepThreatRangePixels)
                {
                    nearbyThreat = true;
                }
            });

        if (nearbyThreat)
        {
            action = "Sleep (Too dangerous)";
        }
        break;
    }

    case InteractableType::Container:
    {
        if (world.hasComponent<Inventory>(bestEntity))
        {
            const Inventory& containerInv = world.getComponent<Inventory>(bestEntity);
            bool hasLoot = false;
            for (const auto& slot : containerInv.slots)
            {
                if (slot.itemId >= 0 && slot.count > 0)
                {
                    hasLoot = true;
                    break;
                }
            }
            if (!hasLoot)
            {
                action = "Search (Empty)";
            }
        }
        break;
    }

    default:
        break;
    }

    if (action.empty())
    {
        action = "Interact";
    }
    result.text = "E: " + action;
    return result;
}
