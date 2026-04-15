#include "systems/SurvivorInteractionSystem.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <glm/geometric.hpp>

#include "components/Health.h"
#include "components/Inventory.h"
#include "components/SurvivorAI.h"
#include "components/Transform.h"

namespace
{
constexpr float kSpriteHalf = 16.0f;

glm::vec2 center(const Transform& t)
{
    return glm::vec2(t.x + kSpriteHalf, t.y + kSpriteHalf);
}

Entity findNearestLivingSurvivor(World& world, const glm::vec2& pos, float maxRange)
{
    Entity nearest = kInvalidEntity;
    float bestDist2 = maxRange * maxRange;

    world.forEach<SurvivorAI, Transform, Health>(
        [&](Entity e, SurvivorAI& ai, Transform& t, Health& h)
        {
            if (!ai.alive || h.current <= 0.0f) return;
            const glm::vec2 sc = center(t);
            const float d2 = glm::dot(sc - pos, sc - pos);
            if (d2 < bestDist2)
            {
                bestDist2 = d2;
                nearest = e;
            }
        });

    return nearest;
}
}

void SurvivorInteractionSystem::update(World& world, Entity playerEntity, float dtSeconds)
{
    if (mMode == SurvivorInteractionMode::None) return;
    if (playerEntity == kInvalidEntity) { cancelInteraction(); return; }

    // Validate target is still alive and in range
    if (mTargetSurvivor == kInvalidEntity ||
        !world.hasComponent<SurvivorAI>(mTargetSurvivor) ||
        !world.hasComponent<Transform>(mTargetSurvivor) ||
        !world.hasComponent<Health>(mTargetSurvivor))
    {
        cancelInteraction();
        return;
    }

    const auto& ai = world.getComponent<SurvivorAI>(mTargetSurvivor);
    const auto& sh = world.getComponent<Health>(mTargetSurvivor);
    if (!ai.alive || sh.current <= 0.0f) { cancelInteraction(); return; }

    if (!world.hasComponent<Transform>(playerEntity)) { cancelInteraction(); return; }

    const glm::vec2 pc = center(world.getComponent<Transform>(playerEntity));
    const glm::vec2 sc = center(world.getComponent<Transform>(mTargetSurvivor));
    const float dist = glm::distance(pc, sc);

    // Break interaction if too far
    if (dist > kObserveRange * 1.5f)
    {
        cancelInteraction();
        return;
    }

    if (mMode == SurvivorInteractionMode::Approaching)
    {
        mApproachTimer += dtSeconds;
    }
}

SurvivorInteractionResult SurvivorInteractionSystem::tryObserve(World& world, Entity playerEntity) const
{
    SurvivorInteractionResult result;
    if (playerEntity == kInvalidEntity || !world.hasComponent<Transform>(playerEntity))
        return result;

    const glm::vec2 pc = center(world.getComponent<Transform>(playerEntity));
    Entity target = findNearestLivingSurvivor(world, pc, kObserveRange);

    if (target == kInvalidEntity)
    {
        result.message = "No survivors nearby.";
        return result;
    }

    const auto& ai = world.getComponent<SurvivorAI>(target);
    const auto& th = world.getComponent<Health>(target);

    std::ostringstream ss;
    ss << ai.name << " | ";

    switch (ai.attitude)
    {
    case SurvivorAttitude::Neutral:   ss << "Neutral"; break;
    case SurvivorAttitude::Organised: ss << "Organised"; break;
    case SurvivorAttitude::Desperate: ss << "Desperate"; break;
    case SurvivorAttitude::Hostile:   ss << "Hostile"; break;
    }

    ss << " | HP:" << static_cast<int>(th.current)
       << " | Hunger:" << static_cast<int>(ai.hunger)
       << " | Thirst:" << static_cast<int>(ai.thirst);

    if (ai.personality.competence > 0.7f)
        ss << " [Competent]";
    else if (ai.personality.competence < 0.3f)
        ss << " [Inexperienced]";

    result.occurred = true;
    result.message = ss.str();
    return result;
}

SurvivorInteractionResult SurvivorInteractionSystem::tryApproach(World& world, Entity playerEntity)
{
    SurvivorInteractionResult result;
    if (playerEntity == kInvalidEntity || !world.hasComponent<Transform>(playerEntity))
        return result;

    const glm::vec2 pc = center(world.getComponent<Transform>(playerEntity));
    Entity target = findNearestLivingSurvivor(world, pc, kApproachRange);

    if (target == kInvalidEntity)
    {
        result.message = "No survivor close enough to approach.";
        return result;
    }

    const auto& ai = world.getComponent<SurvivorAI>(target);

    if (ai.attitude == SurvivorAttitude::Hostile)
    {
        result.occurred = true;
        result.message = ai.name + " is hostile! Back away!";
        return result;
    }

    mMode = SurvivorInteractionMode::Approaching;
    mTargetSurvivor = target;
    mApproachTimer = 0.0f;

    result.occurred = true;
    if (ai.attitude == SurvivorAttitude::Desperate)
        result.message = "Approaching " + ai.name + "... they look desperate.";
    else
        result.message = "Approaching " + ai.name + "...";

    return result;
}

SurvivorInteractionResult SurvivorInteractionSystem::tryTrade(World& world, Entity playerEntity,
                                                               const ItemDatabase& itemDb, int offerSlot)
{
    SurvivorInteractionResult result;

    if (mTargetSurvivor == kInvalidEntity ||
        !world.hasComponent<SurvivorAI>(mTargetSurvivor))
    {
        result.message = "No one to trade with.";
        return result;
    }

    if (!world.hasComponent<Transform>(playerEntity) || !world.hasComponent<Transform>(mTargetSurvivor))
    {
        cancelInteraction();
        return result;
    }

    const glm::vec2 pc = center(world.getComponent<Transform>(playerEntity));
    const glm::vec2 sc = center(world.getComponent<Transform>(mTargetSurvivor));
    if (glm::distance(pc, sc) > kTradeRange)
    {
        result.message = "Too far to trade.";
        return result;
    }

    auto& ai = world.getComponent<SurvivorAI>(mTargetSurvivor);

    if (ai.attitude == SurvivorAttitude::Hostile)
    {
        result.message = ai.name + " won't trade.";
        return result;
    }

    if (mApproachTimer < kApproachDuration)
    {
        result.message = "Still establishing contact...";
        return result;
    }

    // Check player has item to offer
    if (!world.hasComponent<Inventory>(playerEntity)) return result;
    Inventory& playerInv = world.getComponent<Inventory>(playerEntity);

    if (offerSlot < 0 || offerSlot >= Inventory::kMaxSlots) return result;
    auto& slot = playerInv.slots[static_cast<std::size_t>(offerSlot)];
    if (slot.itemId < 0 || slot.count <= 0)
    {
        result.message = "Nothing to offer.";
        return result;
    }

    const ItemDef* item = itemDb.find(slot.itemId);
    if (!item) return result;

    // NPC evaluates based on needs: food/drink highly valued when hungry/thirsty
    bool accepted = false;
    std::string reward;

    if (item->category == ItemCategory::Food && ai.hunger < 50.0f)
    {
        ai.hunger = std::min(100.0f, ai.hunger + item->hungerRestore);
        accepted = true;
        reward = "shared survival info";
    }
    else if (item->category == ItemCategory::Drink && ai.thirst < 50.0f)
    {
        ai.thirst = std::min(100.0f, ai.thirst + item->thirstRestore);
        accepted = true;
        reward = "shared survival info";
    }
    else if (item->category == ItemCategory::Medical)
    {
        accepted = true;
        reward = "a tip about nearby resources";
    }
    else if (ai.personality.cooperation > 0.6f)
    {
        accepted = true;
        reward = "goodwill";
    }

    if (accepted)
    {
        slot.count -= 1;
        if (slot.count <= 0)
        {
            slot.itemId = -1;
            slot.count = 0;
            slot.condition = 1.0f;
        }

        mMode = SurvivorInteractionMode::Trading;
        result.occurred = true;
        result.tradeAccepted = true;
        result.depressionRelief = 5.0f;
        result.panicRelief = 2.0f;
        result.message = ai.name + " accepted " + item->name + " and offered " + reward + ".";
    }
    else
    {
        result.occurred = true;
        result.message = ai.name + " isn't interested in that right now.";
    }

    return result;
}

void SurvivorInteractionSystem::cancelInteraction()
{
    mMode = SurvivorInteractionMode::None;
    mTargetSurvivor = kInvalidEntity;
    mApproachTimer = 0.0f;
}
