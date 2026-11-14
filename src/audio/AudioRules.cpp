#include "audio/AudioRules.h"

#include <algorithm>
#include <cmath>

namespace audiorules
{
float tierRangeTiles(int tier)
{
    switch (tier)
    {
    case 1: return 4.0f;   // Whisper
    case 2: return 9.0f;   // Soft
    case 3: return 18.0f;  // Medium
    case 4: return 32.0f;  // Loud
    case 5: return 60.0f;  // Explosive
    default: return 0.0f;
    }
}

float distanceGain(float distanceTiles, float rangeTiles)
{
    if (rangeTiles <= kFullTiles) return distanceTiles <= rangeTiles ? 1.0f : 0.0f;
    if (distanceTiles <= kFullTiles) return 1.0f;
    if (distanceTiles >= rangeTiles) return 0.0f;
    return 1.0f - (distanceTiles - kFullTiles) / (rangeTiles - kFullTiles);
}

bool occluded(const Shell& source, const Shell& listener, bool listening)
{
    if (listening || source.building == listener.building) return false;
    // Phase 4's rule: a shell cuts what crosses it only while it is shut.
    return (source.building != 0 && source.sealed) || (listener.building != 0 && listener.sealed);
}

float panFromScreen(float soundScreenX, float listenerScreenX, float halfViewportWidth)
{
    if (halfViewportWidth <= 0.0f) return 0.0f;
    return std::clamp((soundScreenX - listenerScreenX) / halfViewportWidth, -1.0f, 1.0f);
}

int panAngle(float pan)
{
    const float p = std::clamp(pan, -1.0f, 1.0f);
    const int degrees = static_cast<int>(std::lround(p * 90.0f));
    return degrees >= 0 ? degrees : 360 + degrees;
}

int pickVoice(const std::vector<Voice>& voices, int sound, int priority, float volume, double now, int maxSame)
{
    const auto busy = [&](const Voice& v) { return v.sound >= 0 && v.endsAt > now; };

    // Too many of this one already: it takes the place of the quietest.
    int same = 0;
    int quietestSame = -1;
    for (int i = 0; i < static_cast<int>(voices.size()); ++i)
    {
        const Voice& v = voices[static_cast<std::size_t>(i)];
        if (!busy(v) || v.sound != sound) continue;
        ++same;
        if (quietestSame < 0 || v.volume < voices[static_cast<std::size_t>(quietestSame)].volume) quietestSame = i;
    }
    if (same >= maxSame)
    {
        return voices[static_cast<std::size_t>(quietestSame)].volume < volume ? quietestSame : -1;
    }

    for (int i = 0; i < static_cast<int>(voices.size()); ++i)
    {
        if (!busy(voices[static_cast<std::size_t>(i)])) return i;
    }

    // All busy: the quietest of the lowest priority gives way, if outranked.
    int victim = -1;
    for (int i = 0; i < static_cast<int>(voices.size()); ++i)
    {
        const Voice& v = voices[static_cast<std::size_t>(i)];
        if (victim < 0) { victim = i; continue; }
        const Voice& w = voices[static_cast<std::size_t>(victim)];
        if (v.priority < w.priority || (v.priority == w.priority && v.volume < w.volume)) victim = i;
    }
    if (victim < 0) return -1;
    const Voice& w = voices[static_cast<std::size_t>(victim)];
    if (w.priority < priority || (w.priority == priority && w.volume < volume)) return victim;
    return -1;
}

float fadeToward(float current, float target, float dtSeconds, float seconds)
{
    const float step = seconds > 0.0f ? dtSeconds / seconds : 1.0f;
    if (current < target) return std::min(target, current + step);
    return std::max(target, current - step);
}
} // namespace audiorules
