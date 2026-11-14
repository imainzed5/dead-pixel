#include "world/Senses.h"
#include "world/WeatherRules.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace senses
{
float sightRangeTiles(const SightInput& in)
{
    float range = in.baseTiles;
    if (in.dark) range *= kDarkScale;
    if (in.playerLit) range *= kLitScale;
    range *= std::clamp(in.weatherVisibility, 0.05f, 1.5f);
    if (in.crouched) range *= in.moving ? kCrouchMovingScale : kCrouchStillScale;
    range *= in.extraScale;
    return std::max(0.0f, range);
}

bool inCone(const glm::vec2& facing, const glm::vec2& toTarget, float coneDegrees)
{
    const float f2 = glm::dot(facing, facing);
    const float t2 = glm::dot(toTarget, toTarget);
    if (f2 < 1e-6f || t2 < 1e-6f) return true;
    const float cosAngle = glm::dot(facing, toTarget) / std::sqrt(f2 * t2);
    const float half = coneDegrees * 0.5f * 3.14159265f / 180.0f;
    return cosAngle >= std::cos(half);
}

float awarenessFillPerSecond(float distanceTiles, float rangeTiles, bool runner)
{
    if (rangeTiles <= 0.0f) return 0.0f;
    const float rate = std::max(0.0f, 1.6f - distanceTiles / rangeTiles);
    return runner ? rate * 2.0f : rate;
}

float stepAwareness(float awareness, bool seen, float distanceTiles, float rangeTiles, bool runner, bool drains,
                    float dtSeconds)
{
    if (seen)
    {
        return std::min(1.0f, awareness + awarenessFillPerSecond(distanceTiles, rangeTiles, runner) * dtSeconds);
    }
    if (!drains) return awareness;
    return std::max(0.0f, awareness - kAwarenessDrainPerSecond * dtSeconds);
}

float weatherHearingScale(WeatherCondition condition)
{
    return weatherrules::noiseScale(condition);
}

float windScale(NoiseTier tier, const glm::vec2& windTo, float windStrength, const glm::vec2& sourceToListener)
{
    if (static_cast<int>(tier) < static_cast<int>(NoiseTier::Loud)) return 1.0f;
    const float w2 = glm::dot(windTo, windTo);
    const float d2 = glm::dot(sourceToListener, sourceToListener);
    if (w2 < 1e-6f || d2 < 1e-6f || windStrength <= 0.0f) return 1.0f;
    const float along = glm::dot(windTo, sourceToListener) / std::sqrt(w2 * d2); // +1 downwind
    return 1.0f + 0.2f * along * std::clamp(windStrength, 0.0f, 1.0f);
}

NoiseTier tierHeard(NoiseTier emitted, const Shell& source, const Shell& listener)
{
    if (source.building == listener.building) return emitted;
    int drop = 0;
    if (source.building > 0 && source.sealed) ++drop;
    if (listener.building > 0 && listener.sealed) ++drop;
    // Never below a whisper: something still carries.
    if (drop == 0 || emitted == NoiseTier::None) return emitted;
    const int tier = std::max(static_cast<int>(NoiseTier::Whisper), static_cast<int>(emitted) - drop);
    return static_cast<NoiseTier>(tier);
}

float heardRadiusTiles(NoiseTier emitted, const Shell& source, const Shell& listener, WeatherCondition weather,
                       float hearing, float windFactor)
{
    return heardRadiusTiles(emitted, source, listener, weatherHearingScale(weather), hearing, windFactor);
}

float heardRadiusTiles(NoiseTier emitted, const Shell& source, const Shell& listener, float weatherScale,
                       float hearing, float windFactor)
{
    const NoiseTier tier = tierHeard(emitted, source, listener);
    return static_cast<float>(NoiseModel::tierRadiusTiles(tier)) * weatherScale * hearing * windFactor;
}
} // namespace senses
