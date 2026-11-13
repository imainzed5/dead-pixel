#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <functional>
#include <utility>
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

// Who made a noise. The dead ignore their own kind's noises except a
// Screamer's scream and the groan of one that is chasing (phase 4).
enum class NoiseSource
{
    World = 0, // the living, things, doors being battered
    Zombie,
    Scream,
    Groan
};

struct NoiseEvent
{
    glm::vec2 worldPosition{0.0f, 0.0f};
    int radiusTiles = 0;
    float remainingSeconds = 0.0f;
    float totalDurationSeconds = 0.0f;
    Entity sourceEntity = kInvalidEntity;
    NoiseTier tier = NoiseTier::None;
    NoiseSource source = NoiseSource::World;
    // Made indoors: the building, and whether all its doors and windows were
    // shut (then it carries one tier less outside).
    int building = 0;
    bool sealed = false;
};

// A loud noise leaves a place the dead linger at once they reach it.
struct NoiseAttractor
{
    glm::vec2 worldPosition{0.0f, 0.0f};
    float remainingSeconds = 0.0f;
};

class NoiseModel
{
public:
    void update(float dtSeconds);
    void emitNoise(
        const glm::vec2& worldPosition,
        NoiseTier tier,
        float durationSeconds,
        Entity sourceEntity = kInvalidEntity,
        NoiseSource source = NoiseSource::World);
    void clear();

    // Where a noise at a world position stands: (building id, shell shut).
    // Set by the game; without it every noise is outdoors.
    using ShellResolver = std::function<std::pair<int, bool>(const glm::vec2&)>;
    void setShellResolver(ShellResolver resolver) { mShellResolver = std::move(resolver); }

    [[nodiscard]] const std::vector<NoiseAttractor>& attractors() const { return mAttractors; }
    // The attractor within `radius` world px of `at`, or nullptr.
    [[nodiscard]] const NoiseAttractor* attractorNear(const glm::vec2& at, float radius) const;
    static constexpr float kLoudAttractorSeconds = 60.0f;
    static constexpr float kExplosiveAttractorSeconds = 180.0f;

    [[nodiscard]] float getNoiseAtPosition(const glm::vec2& worldPosition) const;
    [[nodiscard]] const std::vector<NoiseEvent>& activeEvents() const { return mActiveEvents; }
    // Every noise ever emitted (for the harness metrics).
    [[nodiscard]] std::uint64_t emittedTotal() const { return mEmittedTotal; }

    [[nodiscard]] static int tierRadiusTiles(NoiseTier tier);
    [[nodiscard]] static NoiseTier adjustTier(NoiseTier tier, int delta);
    [[nodiscard]] static glm::vec4 tierColor(NoiseTier tier);
    [[nodiscard]] static const char* tierName(NoiseTier tier);

private:
    std::vector<NoiseEvent> mActiveEvents;
    std::vector<NoiseAttractor> mAttractors;
    ShellResolver mShellResolver;
    std::uint64_t mEmittedTotal = 0;
};
