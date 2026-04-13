#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <vector>

#include "ecs/Types.h"

enum class NoiseTier
{
    None = 0,
    Whisper = 1,
    Soft = 2,
    Medium = 3,
    Loud = 4,
    Explosive = 5
};

struct NoiseEvent
{
    glm::vec2 worldPosition{0.0f, 0.0f};
    int radiusTiles = 0;
    float remainingSeconds = 0.0f;
    float totalDurationSeconds = 0.0f;
    Entity sourceEntity = kInvalidEntity;
    NoiseTier tier = NoiseTier::None;
};

class NoiseModel
{
public:
    void update(float dtSeconds);
    void emitNoise(
        const glm::vec2& worldPosition,
        NoiseTier tier,
        float durationSeconds,
        Entity sourceEntity = kInvalidEntity);

    [[nodiscard]] float getNoiseAtPosition(const glm::vec2& worldPosition) const;
    [[nodiscard]] const std::vector<NoiseEvent>& activeEvents() const { return mActiveEvents; }

    [[nodiscard]] static int tierRadiusTiles(NoiseTier tier);
    [[nodiscard]] static NoiseTier adjustTier(NoiseTier tier, int delta);
    [[nodiscard]] static glm::vec4 tierColor(NoiseTier tier);
    [[nodiscard]] static const char* tierName(NoiseTier tier);

private:
    std::vector<NoiseEvent> mActiveEvents;
};
