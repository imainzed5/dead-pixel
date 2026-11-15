#include "world/BloodTrail.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace bloodtrail
{
namespace
{
// Enough to show the way, few enough to scan every half second.
constexpr std::size_t kMaxMarkers = 400;
} // namespace

float lifeSeconds(Place place, bool raining, float scale, float dryOutdoorSeconds)
{
    switch (place)
    {
    case Place::Water: return 0.0f;
    case Place::Indoor: return kIndoorSeconds * scale;
    case Place::Outdoor:
    default: return (raining ? kRainSeconds : dryOutdoorSeconds) * scale;
    }
}

void Trail::clear()
{
    mMarkers.clear();
    mWalked = 0.0f;
}

void Trail::walk(const glm::vec2& from, const glm::vec2& to, float strength, Place place, float tileSize)
{
    if (strength <= 0.0f)
    {
        mWalked = 0.0f;
        return;
    }
    mWalked += glm::length(to - from);
    const float every = kDropEveryTiles * std::max(1.0f, tileSize);
    if (mWalked < every) return;
    mWalked -= every * std::floor(mWalked / every);
    addMarker(to, strength, place);
}

void Trail::addMarker(const glm::vec2& world, float strength, Place place, bool pool)
{
    if (place == Place::Water) return; // washed away at once
    Marker m;
    m.world = world;
    m.strength = strength;
    m.place = place;
    m.pool = pool;
    m.life = pool ? kPoolSeconds : lifeSeconds(place, false);
    m.seq = mNextSeq++;
    mMarkers.push_back(m);
    if (mMarkers.size() > kMaxMarkers) mMarkers.erase(mMarkers.begin());
}

void Trail::update(float dtSeconds, bool raining, float scale, float dryOutdoorSeconds)
{
    for (Marker& m : mMarkers)
    {
        m.age += dtSeconds;
        if (!m.pool) m.life = lifeSeconds(m.place, raining, scale, dryOutdoorSeconds);
    }
    std::erase_if(mMarkers, [](const Marker& m) { return m.age >= m.life; });
}

float Trail::freshness(const Marker& m)
{
    if (m.life <= 0.0f) return 0.0f;
    return m.strength * std::max(0.0f, 1.0f - m.age / m.life);
}

const Marker* Trail::freshestNear(const glm::vec2& at, float radius, std::uint32_t afterSeq) const
{
    const Marker* best = nullptr;
    float bestFresh = 0.0f;
    const float r2 = radius * radius;
    for (const Marker& m : mMarkers)
    {
        if (m.seq <= afterSeq) continue;
        const glm::vec2 d = m.world - at;
        if (glm::dot(d, d) > r2) continue;
        const float f = freshness(m);
        // The later of two near-equal ones: that is the way the trail goes.
        if (f > 0.0f && (best == nullptr || f > bestFresh + 0.02f || (f > bestFresh - 0.02f && m.seq > best->seq)))
        {
            best = &m;
            bestFresh = f;
        }
    }
    return best;
}

void Trail::restore(const std::vector<Marker>& markers)
{
    mMarkers = markers;
    mWalked = 0.0f;
    mNextSeq = 1;
    for (const Marker& m : mMarkers) mNextSeq = std::max(mNextSeq, m.seq + 1);
}
} // namespace bloodtrail
