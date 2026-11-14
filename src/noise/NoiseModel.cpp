#include "noise/NoiseModel.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

void NoiseModel::update(float dtSeconds)
{
    for (NoiseEvent& event : mActiveEvents)
    {
        event.remainingSeconds = std::max(0.0f, event.remainingSeconds - dtSeconds);
    }

    mActiveEvents.erase(
        std::remove_if(
            mActiveEvents.begin(),
            mActiveEvents.end(),
            [](const NoiseEvent& event)
            {
                return event.remainingSeconds <= 0.0f;
            }),
        mActiveEvents.end());

    for (NoiseAttractor& a : mAttractors) a.remainingSeconds -= dtSeconds;
    std::erase_if(mAttractors, [](const NoiseAttractor& a) { return a.remainingSeconds <= 0.0f; });
}

void NoiseModel::clear()
{
    mActiveEvents.clear();
    mAttractors.clear();
}

const NoiseAttractor* NoiseModel::attractorNear(const glm::vec2& at, float radius) const
{
    const NoiseAttractor* best = nullptr;
    float bestD = radius;
    for (const NoiseAttractor& a : mAttractors)
    {
        const float d = glm::length(a.worldPosition - at);
        if (d <= bestD)
        {
            best = &a;
            bestD = d;
        }
    }
    return best;
}

void NoiseModel::emitNoise(
    const glm::vec2& worldPosition,
    NoiseTier tier,
    float durationSeconds,
    Entity sourceEntity,
    NoiseSource source,
    float reach)
{
    if (tier == NoiseTier::None)
    {
        return;
    }
    ++mEmittedTotal;

    NoiseEvent event;
    event.worldPosition = worldPosition;
    event.radiusTiles = tierRadiusTiles(tier);
    event.remainingSeconds = durationSeconds;
    event.totalDurationSeconds = durationSeconds;
    event.sourceEntity = sourceEntity;
    event.tier = tier;
    event.source = source;
    event.reach = reach > 0.0f ? reach : 1.0f;
    if (mShellResolver)
    {
        const std::pair<int, bool> shell = mShellResolver(worldPosition);
        event.building = shell.first;
        event.sealed = shell.second;
    }

    mActiveEvents.push_back(event);

    // A gunshot, a crash, an alarm: the dead that come stay a while.
    if (tier == NoiseTier::Loud || tier == NoiseTier::Explosive)
    {
        const float seconds = tier == NoiseTier::Explosive ? kExplosiveAttractorSeconds : kLoudAttractorSeconds;
        for (NoiseAttractor& a : mAttractors)
        {
            if (glm::length(a.worldPosition - worldPosition) < 96.0f)
            {
                a.remainingSeconds = std::max(a.remainingSeconds, seconds);
                return;
            }
        }
        mAttractors.push_back(NoiseAttractor{worldPosition, seconds});
    }
}

float NoiseModel::getNoiseAtPosition(const glm::vec2& worldPosition) const
{
    float highestIntensity = 0.0f;

    for (const NoiseEvent& event : mActiveEvents)
    {
        const float radiusWorld = static_cast<float>(event.radiusTiles * 32);
        if (radiusWorld <= 0.0f)
        {
            continue;
        }

        const glm::vec2 delta = worldPosition - event.worldPosition;
        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (distance > radiusWorld)
        {
            continue;
        }

        const float spatialIntensity = 1.0f - (distance / radiusWorld);
        const float timeIntensity = event.totalDurationSeconds > 0.0f
            ? event.remainingSeconds / event.totalDurationSeconds
            : 0.0f;

        highestIntensity = std::max(highestIntensity, spatialIntensity * timeIntensity);
    }

    return highestIntensity;
}

int NoiseModel::tierRadiusTiles(NoiseTier tier)
{
    switch (tier)
    {
    case NoiseTier::Whisper:
        return 3;
    case NoiseTier::Soft:
        return 7;
    case NoiseTier::Medium:
        return 15;
    case NoiseTier::Loud:
        return 30;
    case NoiseTier::Explosive:
        return 60;
    case NoiseTier::None:
    default:
        return 0;
    }
}

NoiseTier NoiseModel::adjustTier(NoiseTier tier, int delta)
{
    if (tier == NoiseTier::None)
    {
        return NoiseTier::None;
    }

    const int current = static_cast<int>(tier);
    const int adjusted = std::clamp(current + delta, 0, static_cast<int>(NoiseTier::Explosive));
    return static_cast<NoiseTier>(adjusted);
}

glm::vec4 NoiseModel::tierColor(NoiseTier tier)
{
    switch (tier)
    {
    case NoiseTier::Whisper:
        return glm::vec4(0.4f, 0.85f, 1.0f, 0.45f);
    case NoiseTier::Soft:
        return glm::vec4(0.45f, 1.0f, 0.5f, 0.45f);
    case NoiseTier::Medium:
        return glm::vec4(1.0f, 0.95f, 0.35f, 0.50f);
    case NoiseTier::Loud:
        return glm::vec4(1.0f, 0.55f, 0.2f, 0.55f);
    case NoiseTier::Explosive:
        return glm::vec4(1.0f, 0.2f, 0.2f, 0.60f);
    case NoiseTier::None:
    default:
        return glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    }
}

const char* NoiseModel::tierName(NoiseTier tier)
{
    switch (tier)
    {
    case NoiseTier::Whisper:
        return "Whisper";
    case NoiseTier::Soft:
        return "Soft";
    case NoiseTier::Medium:
        return "Medium";
    case NoiseTier::Loud:
        return "Loud";
    case NoiseTier::Explosive:
        return "Explosive";
    case NoiseTier::None:
    default:
        return "None";
    }
}
