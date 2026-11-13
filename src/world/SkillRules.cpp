#include "world/SkillRules.h"

#include <algorithm>
#include <cctype>

namespace skillrules
{
namespace
{
std::size_t index(Skill skill) { return static_cast<std::size_t>(skill); }

std::string normalise(const std::string& text)
{
    std::string out;
    for (char c : text)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) != 0)
        {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    return out;
}

float perLevel(int level, float step) { return step * static_cast<float>(std::clamp(level, 0, kMaxLevel)); }
} // namespace

const char* name(Skill skill)
{
    switch (skill)
    {
    case Skill::Carpentry: return "Carpentry";
    case Skill::Fighting: return "Fighting";
    case Skill::Fitness: return "Fitness";
    case Skill::Scavenging: return "Scavenging";
    case Skill::Medicine: return "Medicine";
    case Skill::Mechanics: return "Mechanics";
    case Skill::LivingOffTheLand: return "Living off the land";
    }
    return "";
}

std::optional<Skill> parse(const std::string& text)
{
    const std::string want = normalise(text);
    for (std::size_t i = 0; i < kSkillCount; ++i)
    {
        const Skill skill = static_cast<Skill>(i);
        const std::string full = normalise(name(skill));
        if (full == want || (want.size() >= 4 && full.rfind(want, 0) == 0)) return skill;
    }
    if (want == "land") return Skill::LivingOffTheLand;
    return std::nullopt;
}

int levelForXp(float xp)
{
    int level = 0;
    while (level < kMaxLevel && xp >= kThresholds[static_cast<std::size_t>(level)]) ++level;
    return level;
}

float xpForLevel(int level)
{
    if (level <= 0) return 0.0f;
    return kThresholds[static_cast<std::size_t>(std::min(level, kMaxLevel) - 1)];
}

float progress(float xp)
{
    const int level = levelForXp(xp);
    if (level >= kMaxLevel) return 1.0f;
    const float from = xpForLevel(level);
    const float to = kThresholds[static_cast<std::size_t>(level)];
    return std::clamp((xp - from) / (to - from), 0.0f, 1.0f);
}

XpResult awardXp(Skills& skills, Skill skill, float amount, double nowHour)
{
    const std::size_t i = index(skill);
    skills.lastUsedHour[i] = nowHour;
    const int before = skills.level[i];
    skills.xp[i] = std::max(skills.xp[i] + std::max(0.0f, amount), xpForLevel(before));
    skills.level[i] = std::max(before, levelForXp(skills.xp[i]));
    return XpResult{skills.level[i] > before, skills.level[i]};
}

void setLevel(Skills& skills, Skill skill, int level, double nowHour)
{
    const std::size_t i = index(skill);
    level = std::clamp(level, 0, kMaxLevel);
    skills.level[i] = level;
    skills.xp[i] = xpForLevel(level);
    skills.lastUsedHour[i] = nowHour;
}

void decay(Skills& skills, double nowHour, float hoursPassed)
{
    for (std::size_t i = 0; i < kSkillCount; ++i)
    {
        if (nowHour - skills.lastUsedHour[i] < kForgetAfterHours) continue;
        const float floor = xpForLevel(skills.level[i]);
        skills.xp[i] = std::max(floor, skills.xp[i] - kForgetXpPerHour * hoursPassed);
    }
}

// --- Carpentry ----------------------------------------------------------------
float boardSeconds(int level) { return kBoardBaseSeconds * (1.0f - perLevel(level, 0.08f)); }
float buildTimeScale(int level) { return 1.0f - perLevel(level, 0.08f); }
float boardStrengthScale(int level) { return 1.0f + perLevel(level, 0.06f); }
bool salvagesNails(int level) { return level >= 2; }
bool canReinforceBoards(int level) { return level >= 3; }
int extraPlanksFromFurniture(int level) { return level >= 4 ? 1 : 0; }

// --- Fighting -------------------------------------------------------------------
float swingStaminaScale(int level) { return 1.0f - perLevel(level, 0.05f); }
float windupScale(int level) { return 1.0f - perLevel(level, 0.04f); }
bool shoveFellsWeakened(int level) { return level >= 2; }
float wearScale(int level) { return level >= 3 ? 0.8f : 1.0f; }
float doubleBlowChance(int level) { return level >= 5 ? 0.1f : 0.0f; }

// --- Fitness ----------------------------------------------------------------------
float staminaBonus(int level) { return perLevel(level, 6.0f); }
float runSpeedScale(int level) { return 1.0f + perLevel(level, 0.02f); }
float carryBonusKg(int level) { return perLevel(level, 1.5f); }

// --- Scavenging -------------------------------------------------------------------
float searchTimeScale(int level) { return 1.0f - perLevel(level, 0.10f); }
bool seesWorthSearching(int level) { return level >= 2; }
float extraItemChance(int level) { return level >= 5 ? 0.25f : level >= 3 ? 0.10f : 0.0f; }

// --- Medicine -----------------------------------------------------------------------
float treatmentTimeScale(int level) { return 1.0f - perLevel(level, 0.10f); }
float healingScale(int level) { return 1.0f + perLevel(level, 0.08f); }
bool seesInfectionEarly(int level) { return level >= 2; }
bool antibioticsHalfDose(int level) { return level >= 4; }

// --- Mechanics ----------------------------------------------------------------------
float hotwireTimeScale(int level) { return 1.0f - perLevel(level, 0.15f); }
float repairScale(int level) { return 1.0f + perLevel(level, 0.10f); }
bool canRunGenerator(int level) { return level >= 3; }

// --- Living off the land ------------------------------------------------------------
float forageScale(int level) { return 1.0f + perLevel(level, 0.10f); }
bool knowsMushrooms(int level) { return level >= 2; }
float snareScale(int level) { return level >= 3 ? 2.0f : 1.0f; }
float cropScale(int level) { return level >= 4 ? 1.25f : 1.0f; }

// --- Recipes ----------------------------------------------------------------------------
float craftTimeScale(int level, int required) { return level < required ? kUnskilledCraftTimeScale : 1.0f; }
float craftFailChance(int level, int required) { return level < required ? kUnskilledFailChance : 0.0f; }
} // namespace skillrules
