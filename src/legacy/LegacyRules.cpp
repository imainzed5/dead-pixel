#include "legacy/LegacyRules.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace legacy
{
namespace
{
std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

int kindRank(LeadKind kind)
{
    switch (kind)
    {
    case LeadKind::Risen: return 0;
    case LeadKind::Pack: return 1;
    case LeadKind::Grave: return 2;
    case LeadKind::Survivor: return 3;
    case LeadKind::Base: return 4;
    }
    return 4;
}
} // namespace

bool shouldRise(const std::string& cause, bool infectedBite)
{
    if (infectedBite)
    {
        return true;
    }
    const std::string text = lower(cause);
    for (const char* key : {"zombie", "bitten", "bite", "infection", "turned", "overrun"})
    {
        if (text.find(key) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

const char* screenDirection(const glm::vec2& fromWorld, const glm::vec2& toWorld)
{
    // World to screen is iso: screen x = wx - wy, screen y = (wx + wy) / 2.
    const glm::vec2 d = toWorld - fromWorld;
    const float sx = d.x - d.y;
    const float sy = (d.x + d.y) * 0.5f;
    if (sx * sx + sy * sy < 1e-4f)
    {
        return "here";
    }
    // Angle with screen-up as +90 degrees.
    const float angle = std::atan2(-sy, sx);
    constexpr float kPi = 3.14159265f;
    int sector = static_cast<int>(std::lround(angle / (kPi / 4.0f)));
    sector = ((sector % 8) + 8) % 8;
    static constexpr std::array<const char*, 8> kNames = {"E", "NE", "N", "NW", "W", "SW", "S", "SE"};
    return kNames[static_cast<std::size_t>(sector)];
}

int metresBetween(const glm::vec2& a, const glm::vec2& b, float tileSize)
{
    const glm::vec2 d = b - a;
    const float tiles = std::sqrt(d.x * d.x + d.y * d.y) / std::max(1.0f, tileSize);
    return static_cast<int>(std::lround(tiles));
}

std::vector<Lead> collectLeads(const std::vector<GraveRecord>& graves,
                               const std::vector<RetirementRecord>& retirements)
{
    std::vector<Lead> leads;
    for (std::size_t i = 0; i < graves.size(); ++i)
    {
        const GraveRecord& grave = graves[i];
        Lead lead;
        lead.kind = grave.risen ? LeadKind::Risen : (grave.items.empty() ? LeadKind::Grave : LeadKind::Pack);
        lead.name = grave.name;
        lead.world = glm::vec2(grave.x, grave.y);
        lead.runIndex = grave.runIndex;
        lead.graveIndex = static_cast<int>(i);
        lead.buried = grave.buried;
        leads.push_back(lead);
    }
    for (const RetirementRecord& ret : retirements)
    {
        Lead lead;
        lead.kind = LeadKind::Survivor;
        lead.name = ret.name;
        lead.world = glm::vec2(ret.x, ret.y);
        lead.runIndex = ret.runIndex;
        leads.push_back(lead);
    }
    std::stable_sort(leads.begin(), leads.end(),
                     [](const Lead& a, const Lead& b)
                     {
                         if (kindRank(a.kind) != kindRank(b.kind))
                         {
                             return kindRank(a.kind) < kindRank(b.kind);
                         }
                         return a.runIndex > b.runIndex;
                     });
    return leads;
}

std::string describeLead(const Lead& lead, const glm::vec2& playerWorld, float tileSize)
{
    std::string what;
    switch (lead.kind)
    {
    case LeadKind::Risen: what = lead.name + " walks again"; break;
    case LeadKind::Pack: what = lead.name + "'s grave, pack still there"; break;
    case LeadKind::Grave: what = lead.buried ? lead.name + "'s stone, in the cemetery" : lead.name + "'s grave"; break;
    case LeadKind::Survivor: what = lead.name + " lives on, alone"; break;
    case LeadKind::Base: what = "A house someone held"; break;
    }
    const int metres = metresBetween(playerWorld, lead.world, tileSize);
    if (metres < 2)
    {
        return what + " - right here";
    }
    return what + " - " + std::to_string(metres) + " m " + screenDirection(playerWorld, lead.world);
}

std::string runHeadline(int runNumber, int previousCharacters)
{
    static constexpr std::array<const char*, 10> kCounts = {
        "nobody", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
    const std::string count = previousCharacters >= 0 && previousCharacters < static_cast<int>(kCounts.size())
        ? kCounts[static_cast<std::size_t>(previousCharacters)]
        : std::to_string(previousCharacters);
    return "Run " + std::to_string(runNumber) + " - " + count + " came before you";
}

// --- Phase 10 -------------------------------------------------------------------

bool hungerDeath(const std::string& cause)
{
    const std::string text = lower(cause);
    for (const char* key : {"starv", "dehydrat", "thirst", "hunger"})
    {
        if (text.find(key) != std::string::npos) return true;
    }
    return false;
}

bool fireDeath(const std::string& cause)
{
    const std::string text = lower(cause);
    return text.find("burn") != std::string::npos || text.find("fire") != std::string::npos;
}

std::vector<BloodlineTrait> bloodlineFor(const LegacyState& state)
{
    // Each trait is earned in a life (the run it was earned in); the most
    // recent two are kept. A life is replayed in run order.
    struct Earned
    {
        BloodlineTrait trait;
        int run;
    };
    std::vector<Earned> earned;
    const auto earn = [&](BloodlineTrait trait, int run)
    {
        for (const Earned& e : earned)
        {
            if (e.trait == trait) return;
        }
        earned.push_back(Earned{trait, run});
    };

    std::vector<CharacterHistoryRecord> lives = state.history;
    std::stable_sort(lives.begin(), lives.end(),
                     [](const CharacterHistoryRecord& a, const CharacterHistoryRecord& b) { return a.runIndex < b.runIndex; });
    int hungry = 0;
    for (const CharacterHistoryRecord& life : lives)
    {
        if (life.escaped) earn(BloodlineTrait::Wayfinder, life.runIndex);
        // A retirement in crisis hardens; a chosen one only when it was at peace.
        if (life.retired && (!life.chosen || life.depression < kPeacefulDepression)) earn(BloodlineTrait::Hardened, life.runIndex);
        if (!life.retired && !life.escaped)
        {
            if (hungerDeath(life.cause) && ++hungry == kScavengerDeaths) earn(BloodlineTrait::Scavenger, life.runIndex);
            if (fireDeath(life.cause)) earn(BloodlineTrait::Firewise, life.runIndex);
        }
        if (life.tilesBurned >= kFirewiseTiles) earn(BloodlineTrait::Firewise, life.runIndex);
    }
    // An older save's retirement with no history of its own still hardens.
    for (const RetirementRecord& ret : state.retirements)
    {
        const bool recorded = std::any_of(lives.begin(), lives.end(), [&](const CharacterHistoryRecord& life)
                                          { return life.retired && life.runIndex == ret.runIndex; });
        if (!recorded) earn(BloodlineTrait::Hardened, ret.runIndex);
    }

    // Graves of the line: the fourth makes the line watchful.
    std::vector<int> graveRuns;
    std::vector<int> restRuns;
    for (const GraveRecord& grave : state.graves)
    {
        if (!grave.ofBloodline()) continue;
        graveRuns.push_back(grave.runIndex);
        if (grave.laidToRest) restRuns.push_back(grave.restRun > 0 ? grave.restRun : grave.runIndex + 1);
    }
    std::sort(graveRuns.begin(), graveRuns.end());
    std::sort(restRuns.begin(), restRuns.end());
    if (static_cast<int>(graveRuns.size()) >= kVigilantGraves)
    {
        earn(BloodlineTrait::Vigilant, graveRuns[static_cast<std::size_t>(kVigilantGraves - 1)]);
    }
    if (static_cast<int>(restRuns.size()) >= kGravediggerRests)
    {
        earn(BloodlineTrait::Gravedigger, restRuns[static_cast<std::size_t>(kGravediggerRests - 1)]);
    }

    std::stable_sort(earned.begin(), earned.end(), [](const Earned& a, const Earned& b) { return a.run > b.run; });
    std::vector<BloodlineTrait> out;
    for (const Earned& e : earned)
    {
        if (out.size() >= kMaxBloodlines) break;
        out.push_back(e.trait);
    }
    return out;
}

const CharacterHistoryRecord* lifeOf(const LegacyState& state, const GraveRecord& grave)
{
    if (!grave.ofBloodline()) return nullptr;
    for (const CharacterHistoryRecord& life : state.history)
    {
        if (life.runIndex == grave.runIndex && life.name == grave.name && !life.retired && !life.escaped) return &life;
    }
    return nullptr;
}

bool journalRead(const LegacyState& state, const GraveRecord& grave)
{
    if (grave.buried) return true;
    const CharacterHistoryRecord* life = lifeOf(state, grave);
    return life != nullptr && life->journalRead;
}

std::array<int, kSkillCount> skillEchoes(const LegacyState& state)
{
    // In halves, so a grave nobody read adds half its level exactly.
    std::array<int, kSkillCount> halves{};
    for (const GraveRecord& grave : state.graves)
    {
        if (!grave.ofBloodline()) continue;
        const int weight = journalRead(state, grave) ? 2 : 1;
        for (const auto& [skill, level] : grave.skills)
        {
            if (skill < 0 || skill >= static_cast<int>(kSkillCount)) continue;
            halves[static_cast<std::size_t>(skill)] += std::max(0, level) * weight;
        }
    }
    std::array<int, kSkillCount> out{};
    for (std::size_t i = 0; i < kSkillCount; ++i)
    {
        out[i] = std::min(kEchoCap, halves[i] / (2 * kEchoDivisor));
    }
    return out;
}

std::string echoLine(const char* skillName, int level)
{
    return std::string(skillName) + " runs in the family (" + std::to_string(level) + ").";
}

int cemeterySlots(const glm::ivec2& fenceMin, const glm::ivec2& fenceMax)
{
    const int w = std::max(0, fenceMax.x - fenceMin.x - 1);
    const int h = std::max(0, fenceMax.y - fenceMin.y - 1);
    return w * h;
}

glm::ivec2 cemeterySlot(const glm::ivec2& fenceMin, const glm::ivec2& fenceMax, int order)
{
    // Inside the fence: rows on every other line from the back (the low y
    // side), then the lines between them.
    const int w = std::max(1, fenceMax.x - fenceMin.x - 1);
    const int h = std::max(1, fenceMax.y - fenceMin.y - 1);
    std::vector<int> rows;
    for (int y = 0; y < h; y += 2) rows.push_back(y);
    for (int y = 1; y < h; y += 2) rows.push_back(y);
    const int slots = w * static_cast<int>(rows.size());
    const int i = ((order % slots) + slots) % slots;
    const int row = rows[static_cast<std::size_t>(i / w)];
    return glm::ivec2(fenceMin.x + 1 + i % w, fenceMin.y + 1 + row);
}

std::string stayBlocker(const StayCheck& check)
{
    if (check.day < kStayFromDay) return "Not yet. Ten days first.";
    if (!check.atBase) return "Only at a base: a house shut on three sides, with a bed.";
    if (check.infected) return "Not with a wound going bad.";
    if (check.hunger <= kStayNeeds || check.thirst <= kStayNeeds) return "Not hungry or thirsty.";
    if (check.health <= kStayNeeds) return "Not hurt like this.";
    return {};
}

} // namespace legacy
