#include "systems/FireSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace firerules
{
float fuelOf(const TileFacts& facts)
{
    float fuel = 0.0f;
    switch (facts.ground)
    {
    case Ground::Grass:
        if (facts.snow) fuel = 0.0f;
        else if (facts.wet) fuel = 0.2f;
        else if (facts.heatwave) fuel = 0.8f;
        else fuel = 0.5f;
        break;
    case Ground::WoodFloor: fuel = 0.6f; break;
    case Ground::Carpet: fuel = 0.7f; break;
    case Ground::WoodWall: fuel = 0.4f; break;
    case Ground::Tree: fuel = 0.9f; break;
    case Ground::Bare: fuel = 0.0f; break;
    }
    if (facts.woodFurniture) fuel += 0.3f;
    else if (facts.metalFurniture) fuel += 0.05f;
    if (facts.boards) fuel += 0.2f;
    return std::clamp(fuel, 0.0f, 1.0f);
}

float windFactor(const glm::ivec2& dir, const glm::vec2& windTo, float windStrength)
{
    if (windStrength < kWindCarries) return 1.0f;
    const glm::vec2 d(static_cast<float>(dir.x), static_cast<float>(dir.y));
    const float dl = glm::length(d);
    const float wl = glm::length(windTo);
    if (dl < 1e-4f || wl < 1e-4f) return 1.0f;
    const float along = glm::dot(d, windTo) / (dl * wl);
    if (along > 0.5f) return kDownwind;
    if (along < -0.5f) return kUpwind;
    return 1.0f;
}

float spreadChance(float fuel, float heat, float weatherScale, float wind)
{
    if (fuel <= 0.0f) return 0.0f;
    return std::clamp(fuel * (0.25f + 0.3f * std::clamp(heat, 0.0f, 1.0f)) * weatherScale * wind, 0.0f, 1.0f);
}

std::vector<glm::ivec2> spreadDirections(const glm::vec2& windTo, float windStrength)
{
    std::vector<glm::ivec2> dirs{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    if (windStrength > kWindDiagonals)
    {
        // The two diagonals nearest the way the wind blows.
        const glm::ivec2 diagonals[4] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        float best[2] = {-2.0f, -2.0f};
        glm::ivec2 pick[2] = {{0, 0}, {0, 0}};
        for (const glm::ivec2& d : diagonals)
        {
            const float along = (static_cast<float>(d.x) * windTo.x + static_cast<float>(d.y) * windTo.y);
            if (along > best[0])
            {
                best[1] = best[0];
                pick[1] = pick[0];
                best[0] = along;
                pick[0] = d;
            }
            else if (along > best[1])
            {
                best[1] = along;
                pick[1] = d;
            }
        }
        dirs.push_back(pick[0]);
        dirs.push_back(pick[1]);
    }
    return dirs;
}
} // namespace firerules

namespace
{
// A float in [0, 1) from a tile, a seed and a step; the same everywhere.
float roll01(int x, int y, std::uint32_t seed, std::uint32_t step, int salt)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093u ^ static_cast<std::uint32_t>(y) * 19349663u;
    h ^= seed * 2654435761u;
    h ^= (step + static_cast<std::uint32_t>(salt) * 0x9E37u) * 83492791u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}
} // namespace

bool FireSystem::ignite(const glm::ivec2& tile, float heat, bool byPlayer, const Env& env)
{
    auto it = mTiles.find(key(tile));
    if (it != mTiles.end())
    {
        it->second.heat = std::max(it->second.heat, std::clamp(heat, 0.0f, 1.0f));
        it->second.byPlayer = it->second.byPlayer || byPlayer;
        return true;
    }
    const float fuel = env.fuelAt ? env.fuelAt(tile.x, tile.y) : 0.0f;
    if (fuel <= 0.0f) return false;
    Tile t;
    t.fuel = fuel;
    t.startFuel = fuel;
    t.heat = std::clamp(heat, 0.0f, 1.0f);
    // Tiles do not all try their neighbours on the same tick.
    t.spreadTimer = roll01(tile.x, tile.y, env.seed, 0u, 7) * firerules::kSpreadEverySeconds;
    t.byPlayer = byPlayer;
    mTiles.emplace(key(tile), t);
    return true;
}

void FireSystem::restore(const glm::ivec2& tile, float fuel, float heat, bool byPlayer)
{
    if (fuel <= 0.0f || heat <= 0.0f) return;
    Tile t;
    t.fuel = std::clamp(fuel, 0.0f, 1.0f);
    t.startFuel = t.fuel;
    t.heat = std::clamp(heat, 0.0f, 1.0f);
    t.byPlayer = byPlayer;
    mTiles[key(tile)] = t;
}

bool FireSystem::douse(const glm::ivec2& tile, float amount, Events* events)
{
    auto it = mTiles.find(key(tile));
    if (it == mTiles.end()) return false;
    it->second.heat -= amount;
    if (it->second.heat <= 0.0f)
    {
        if (events != nullptr)
        {
            events->wentOut.push_back(tile);
            if (it->second.startFuel - it->second.fuel >= firerules::kScorchedFuel) events->scorched.push_back(tile);
        }
        mTiles.erase(it);
    }
    return true;
}

void FireSystem::update(float dtSeconds, const Env& env, Events& events)
{
    ++mSteps;
    const std::vector<glm::ivec2> dirs = firerules::spreadDirections(env.windTo, env.windStrength);
    std::vector<std::pair<glm::ivec2, bool>> caught;
    std::vector<std::int64_t> gone;
    for (auto& [k, t] : mTiles)
    {
        const glm::ivec2 tile = tileOf(k);
        const bool open = env.outdoors && env.outdoors(tile.x, tile.y);
        // Rain in the open takes the heat away; otherwise it builds.
        if (open && env.outdoorDieSeconds > 0.0f)
        {
            t.heat -= dtSeconds / env.outdoorDieSeconds;
        }
        else
        {
            t.heat = std::min(1.0f, t.heat + dtSeconds / firerules::kHeatRiseSeconds);
        }
        t.fuel -= firerules::kFuelPerSecond * dtSeconds;
        if (t.fuel <= 0.0f)
        {
            events.burnedOut.push_back(tile);
            gone.push_back(k);
            continue;
        }
        if (t.heat <= 0.0f)
        {
            events.wentOut.push_back(tile);
            if (t.startFuel - t.fuel >= firerules::kScorchedFuel) events.scorched.push_back(tile);
            gone.push_back(k);
            continue;
        }

        t.spreadTimer -= dtSeconds;
        if (t.spreadTimer > 0.0f) continue;
        t.spreadTimer += firerules::kSpreadEverySeconds;
        for (std::size_t i = 0; i < dirs.size(); ++i)
        {
            const glm::ivec2 next = tile + dirs[i];
            if (mTiles.count(key(next)) != 0) continue;
            const float fuel = env.fuelAt ? env.fuelAt(next.x, next.y) : 0.0f;
            if (fuel <= 0.0f) continue;
            const bool grass = env.grass && env.grass(next.x, next.y);
            const float scale = env.spreadScale * (grass ? env.grassScale : 1.0f);
            const float chance = firerules::spreadChance(fuel, t.heat, scale,
                                                         firerules::windFactor(dirs[i], env.windTo, env.windStrength));
            if (roll01(next.x, next.y, env.seed, mSteps, static_cast<int>(i)) < chance)
            {
                caught.emplace_back(next, t.byPlayer);
            }
        }
    }
    for (std::int64_t k : gone) mTiles.erase(k);
    for (const auto& [tile, byPlayer] : caught)
    {
        if (mTiles.count(key(tile)) != 0) continue;
        if (ignite(tile, firerules::kSpreadStartHeat, byPlayer, env)) events.ignited.push_back(tile);
    }
}

const FireSystem::Tile* FireSystem::at(const glm::ivec2& tile) const
{
    const auto it = mTiles.find(key(tile));
    return it == mTiles.end() ? nullptr : &it->second;
}

std::vector<FireSystem::Burning> FireSystem::all() const
{
    std::vector<Burning> out;
    out.reserve(mTiles.size());
    for (const auto& [k, t] : mTiles) out.push_back(Burning{tileOf(k), t.heat, t.fuel, t.byPlayer});
    return out;
}

std::vector<FireSystem::Burning> FireSystem::burningNear(const glm::ivec2& centre, int radius) const
{
    std::vector<Burning> out;
    if (mTiles.empty()) return out;
    for (int y = std::max(0, centre.y - radius); y <= centre.y + radius; ++y)
    {
        auto it = mTiles.lower_bound(key(glm::ivec2(std::max(0, centre.x - radius), y)));
        const std::int64_t end = key(glm::ivec2(centre.x + radius, y));
        for (; it != mTiles.end() && it->first <= end; ++it)
        {
            out.push_back(Burning{tileOf(it->first), it->second.heat, it->second.fuel, it->second.byPlayer});
        }
    }
    return out;
}
