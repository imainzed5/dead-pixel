#pragma once

#include <string>

#include "ecs/World.h"
#include "items/ItemDatabase.h"

struct SurvivorInteractionResult
{
    bool occurred = false;
    std::string message;
};

enum class SurvivorInteractionMode
{
    None,
    Observing,
    Approaching,
    Trading
};

class SurvivorInteractionSystem
{
public:
    void update(World& world, Entity playerEntity, float dtSeconds);

    // Player-initiated interactions
    SurvivorInteractionResult tryObserve(World& world, Entity playerEntity) const;
    SurvivorInteractionResult tryApproach(World& world, Entity playerEntity);
    SurvivorInteractionResult tryTrade(World& world, Entity playerEntity, const ItemDatabase& itemDb,
                                        int offerSlot);

    [[nodiscard]] SurvivorInteractionMode currentMode() const { return mMode; }
    [[nodiscard]] Entity targetSurvivor() const { return mTargetSurvivor; }
    void cancelInteraction();

private:
    SurvivorInteractionMode mMode = SurvivorInteractionMode::None;
    Entity mTargetSurvivor = 0;
    float mApproachTimer = 0.0f;

    static constexpr float kObserveRange = 192.0f;
    static constexpr float kApproachRange = 64.0f;
    static constexpr float kTradeRange = 48.0f;
    static constexpr float kApproachDuration = 2.0f; // seconds to signal intent
};
