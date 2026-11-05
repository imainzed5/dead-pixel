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
    case LeadKind::Grave: what = lead.name + "'s grave"; break;
    case LeadKind::Survivor: what = lead.name + " lives on, alone"; break;
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

} // namespace legacy
