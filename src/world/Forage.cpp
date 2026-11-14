#include "world/Forage.h"
#include "world/WeatherRules.h"

#include <algorithm>

namespace
{
std::uint32_t hash(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

float unit(std::uint32_t h)
{
    return static_cast<float>(h >> 8) / 16777216.0f;
}

std::uint32_t tileKey(const glm::ivec2& tile)
{
    return static_cast<std::uint32_t>(tile.x) * 73856093u ^ static_cast<std::uint32_t>(tile.y) * 19349663u;
}
} // namespace

namespace forage
{
Result search(Spot spot, const glm::ivec2& tile, int day, Season season, bool rainedLately, int livingSkill,
              std::uint32_t runSeed)
{
    Result result;
    if (spot == Spot::None) return result;
    const std::uint32_t base = hash(tileKey(tile), static_cast<std::uint32_t>(day), runSeed);
    if (unit(base) < kNothingChance) return result;

    // Bushes fruit until the autumn's day 40; mushrooms come with rain, and
    // all through the autumn (phase 7).
    const bool berrySeason = season != Season::Winter && weatherrules::bushesFruit(day);
    rainedLately = rainedLately || weatherrules::mushroomsCommon(day);
    Find find = Find::Nothing;
    switch (spot)
    {
    case Spot::Bush:
        find = berrySeason ? Find::Berries : Find::Nothing;
        break;
    case Spot::TreeBase:
        find = rainedLately ? Find::Mushrooms : Find::Nothing;
        break;
    case Spot::Tuft:
        find = Find::Herbs;
        break;
    case Spot::Wild:
    {
        const float pick = unit(hash(base, 0x57u, 1u));
        if (pick < 0.4f) find = Find::Herbs;
        else if (pick < 0.75f) find = berrySeason ? Find::Berries : Find::Herbs;
        else find = rainedLately ? Find::Mushrooms : Find::Nothing;
        break;
    }
    case Spot::None:
        break;
    }
    if (find == Find::Nothing) return result;

    result.find = find;
    result.count = 1 + static_cast<int>(unit(hash(base, 0xC0u, 2u)) * 3.0f); // 1 to 3
    // Living off the land: +10 % a level, as a chance of one more.
    if (unit(hash(base, 0xC1u, 3u)) < 0.1f * static_cast<float>(std::max(0, livingSkill))) ++result.count;
    return result;
}

bool regrown(int lastDay, int day)
{
    return day - lastDay >= kRegrowDays;
}

bool badMushroom(int livingSkill, float roll01)
{
    return livingSkill < kKnowsMushroomsLevel && roll01 < kBadMushroomChance;
}

SnareDawn snareAtDawn(const glm::ivec2& tile, int day, int livingSkill, std::uint32_t runSeed)
{
    const float r = unit(hash(tileKey(tile), static_cast<std::uint32_t>(day) * 7u + 1u, runSeed ^ 0x5A4Eu));
    const float catchChance = livingSkill >= 3 ? 0.5f : 0.25f;
    if (r < catchChance) return SnareDawn::Caught;
    if (r < catchChance + 0.10f) return SnareDawn::Sprung;
    return SnareDawn::Set;
}
} // namespace forage

namespace food
{
State stateOf(float condition)
{
    if (condition > 0.6f) return State::Fresh;
    if (condition >= 0.3f) return State::Stale;
    return State::Spoiled;
}

const char* stateName(State state)
{
    switch (state)
    {
    case State::Fresh: return "fresh";
    case State::Stale: return "stale";
    case State::Spoiled: return "spoiled";
    }
    return "";
}

float valueScale(float condition)
{
    switch (stateOf(condition))
    {
    case State::Fresh: return 1.0f;
    case State::Stale: return 0.75f;
    case State::Spoiled: return 0.5f;
    }
    return 1.0f;
}

float illnessFrom(float condition, float rawIllness, float weakStomachScale)
{
    float illness = rawIllness;
    if (stateOf(condition) == State::Spoiled) illness += (1.0f - condition) * 20.0f;
    return illness * weakStomachScale;
}
} // namespace food
