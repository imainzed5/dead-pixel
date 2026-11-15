#include "world/Horde.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "world/ZombieKinds.h"

namespace horde
{
namespace
{
std::uint32_t mix(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

// Nearest tile to `want` (searching outward) that passes `ok`, or `want`.
glm::ivec2 snap(const RouteInput& in, glm::ivec2 want, const std::function<bool(int, int)>& ok, int maxRadius)
{
    want.x = std::clamp(want.x, 0, std::max(0, in.width - 1));
    want.y = std::clamp(want.y, 0, std::max(0, in.height - 1));
    if (!ok) return want;
    for (int r = 0; r <= maxRadius; ++r)
    {
        for (int dy = -r; dy <= r; ++dy)
        {
            for (int dx = -r; dx <= r; ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                const int x = want.x + dx;
                const int y = want.y + dy;
                if (x < 0 || y < 0 || x >= in.width || y >= in.height) continue;
                if (ok(x, y)) return {x, y};
            }
        }
    }
    return {-1, -1};
}
} // namespace

std::vector<int> hordeNights(int lastNight, std::uint32_t runSeed, const zombiekinds::ModeRules& mode)
{
    std::vector<int> nights;
    int night = std::max(1, mode.hordeFirstNight);
    const int lo = std::max(1, mode.hordeEveryMin);
    const int hi = std::max(lo, mode.hordeEveryMax);
    while (night <= lastNight)
    {
        nights.push_back(night);
        const std::uint32_t h = mix(runSeed ^ static_cast<std::uint32_t>(night * 2654435761u));
        night += lo + static_cast<int>(h % static_cast<std::uint32_t>(hi - lo + 1));
    }
    return nights;
}

bool isHordeNight(int night, std::uint32_t runSeed, const zombiekinds::ModeRules& mode)
{
    if (night < 1) return false;
    const std::vector<int> nights = hordeNights(night, runSeed, mode);
    return !nights.empty() && nights.back() == night;
}

int hordeSize(int day)
{
    return std::clamp(14 + 2 * std::max(0, day - 7), 14, 40);
}

std::vector<glm::ivec2> route(const RouteInput& in)
{
    std::vector<glm::ivec2> waypoints;
    if (in.width <= 0 || in.height <= 0) return waypoints;
    const std::uint32_t h = mix(in.seed ^ 0xC0FFEEu);
    const float angle = static_cast<float>(h % 3600u) * (6.2831853f / 3600.0f);
    const glm::vec2 dir(std::cos(angle), std::sin(angle));
    const glm::vec2 side(-dir.y, dir.x);
    const glm::vec2 home(in.home);

    // The pass: 6 to 10 tiles off the home, to one side of the way.
    const float passTiles = kPassMinTiles + static_cast<float>((h >> 12) % 100u) / 99.0f * (kPassMaxTiles - kPassMinTiles);
    const float sideSign = ((h >> 20) & 1u) != 0u ? 1.0f : -1.0f;
    const glm::vec2 pass = home + side * passTiles * sideSign;

    // From the edge behind, along `dir`, to the edge ahead.
    const auto toEdge = [&](const glm::vec2& from, const glm::vec2& d)
    {
        float t = 1e9f;
        if (d.x > 1e-4f) t = std::min(t, (static_cast<float>(in.width - 2) - from.x) / d.x);
        if (d.x < -1e-4f) t = std::min(t, (1.0f - from.x) / d.x);
        if (d.y > 1e-4f) t = std::min(t, (static_cast<float>(in.height - 2) - from.y) / d.y);
        if (d.y < -1e-4f) t = std::min(t, (1.0f - from.y) / d.y);
        return from + d * std::max(0.0f, t);
    };
    const glm::vec2 start = toEdge(pass, -dir);
    const glm::vec2 end = toEdge(pass, dir);

    const auto roadOrWalkable = [&](glm::vec2 p, int roadRadius) -> glm::ivec2
    {
        const glm::ivec2 want(static_cast<int>(std::round(p.x)), static_cast<int>(std::round(p.y)));
        glm::ivec2 tile = snap(in, want, in.isRoad, roadRadius);
        if (tile.x < 0) tile = snap(in, want, in.walkable, 12);
        return tile;
    };

    // The pass itself must stay 6 to 10 tiles off the home: only roads in
    // that ring count.
    glm::ivec2 passTile{-1, -1};
    {
        const glm::ivec2 want(static_cast<int>(std::round(pass.x)), static_cast<int>(std::round(pass.y)));
        const auto inRing = [&](int x, int y, const std::function<bool(int, int)>& ok)
        {
            const float d = glm::length(glm::vec2(static_cast<float>(x), static_cast<float>(y)) - home);
            return d >= kPassMinTiles && d <= kPassMaxTiles && (!ok || ok(x, y));
        };
        passTile = snap(in, want, [&](int x, int y) { return inRing(x, y, in.isRoad); }, 6);
        if (passTile.x < 0) passTile = snap(in, want, [&](int x, int y) { return inRing(x, y, in.walkable); }, 10);
        if (passTile.x < 0) passTile = roadOrWalkable(pass, 4);
    }

    const int legs = 3;
    const glm::ivec2 startTile = roadOrWalkable(start, 10);
    if (startTile.x >= 0) waypoints.push_back(startTile);
    for (int i = 1; i < legs; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(legs);
        const glm::ivec2 w = roadOrWalkable(start + (pass - start) * t, 8);
        if (w.x >= 0) waypoints.push_back(w);
    }
    if (passTile.x >= 0) waypoints.push_back(passTile);
    for (int i = 1; i < legs; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(legs);
        const glm::ivec2 w = roadOrWalkable(pass + (end - pass) * t, 8);
        if (w.x >= 0) waypoints.push_back(w);
    }
    const glm::ivec2 endTile = roadOrWalkable(end, 10);
    if (endTile.x >= 0) waypoints.push_back(endTile);
    // No doubles in a row.
    waypoints.erase(std::unique(waypoints.begin(), waypoints.end()), waypoints.end());
    return waypoints;
}
} // namespace horde
