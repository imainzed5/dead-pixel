#pragma once

#include <array>
#include <cstddef>

// What the character has learned by doing (phase 5). Rules, thresholds and
// every per-level effect live in world/SkillRules.h.
enum class Skill : int
{
    Carpentry,
    Fighting,
    Fitness,
    Scavenging,
    Medicine,
    Mechanics,
    LivingOffTheLand
};

constexpr std::size_t kSkillCount = 7;

struct Skills
{
    std::array<float, kSkillCount> xp{};
    std::array<int, kSkillCount> level{};
    // Absolute game hour (day * 24 + hour) each was last used; unused for
    // three days, a skill slowly loses what it gained past its level.
    std::array<double, kSkillCount> lastUsedHour{};

    [[nodiscard]] int of(Skill skill) const { return level[static_cast<std::size_t>(skill)]; }
};
