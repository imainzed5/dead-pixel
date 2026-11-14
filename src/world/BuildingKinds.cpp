#include "world/BuildingKinds.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace
{
using K = BuildingKind;

struct KindInfo
{
    const char* name;
    const char* key;
    BuildingArchetype archetype;
    BuildingSign sign;
    float locked;
    float alarm;
};

constexpr std::array<KindInfo, static_cast<std::size_t>(K::Count)> kKinds = {{
    {"House", "house", BuildingArchetype::Residential, BuildingSign::None, 0.25f, 0.0f},
    {"Grocery", "grocery", BuildingArchetype::Commercial, BuildingSign::Cart, 0.20f, 0.30f},
    {"Pharmacy", "pharmacy", BuildingArchetype::Commercial, BuildingSign::Cross, 0.60f, 0.60f},
    {"Hardware", "hardware", BuildingArchetype::Commercial, BuildingSign::Wrench, 0.30f, 0.30f},
    {"Clinic", "clinic", BuildingArchetype::Commercial, BuildingSign::Cross, 0.40f, 0.0f},
    {"Police", "police", BuildingArchetype::Commercial, BuildingSign::Badge, 1.00f, 1.00f},
    {"Diner", "diner", BuildingArchetype::Commercial, BuildingSign::Cup, 0.10f, 0.0f},
    {"Office", "office", BuildingArchetype::Commercial, BuildingSign::None, 0.30f, 0.0f},
    {"Garage", "garage", BuildingArchetype::Industrial, BuildingSign::Wrench, 0.30f, 0.0f},
    {"Warehouse", "warehouse", BuildingArchetype::Industrial, BuildingSign::Crate, 0.50f, 0.0f},
    {"Workshop", "workshop", BuildingArchetype::Industrial, BuildingSign::None, 0.20f, 0.0f},
    {"Guard Hut", "guard_hut", BuildingArchetype::Commercial, BuildingSign::None, 0.0f, 0.0f},
}};

const KindInfo& infoOf(BuildingKind kind)
{
    return kKinds[std::min(static_cast<std::size_t>(kind), kKinds.size() - 1)];
}

std::uint32_t hash3(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

constexpr std::uint32_t kSaltKind = 0xB01Du;
constexpr std::uint32_t kSaltLock = 0x10C4u;
constexpr std::uint32_t kSaltBack = 0xBAC4u;
constexpr std::uint32_t kSaltAlarm = 0xA1A3u;

bool inside(const LayoutData::BuildingRect& b, const glm::ivec2& t)
{
    return t.x >= b.x && t.y >= b.y && t.x < b.x + b.w && t.y < b.y + b.h;
}
} // namespace

namespace buildingkinds
{
const char* name(BuildingKind kind) { return infoOf(kind).name; }
const char* key(BuildingKind kind) { return infoOf(kind).key; }
BuildingArchetype archetypeOf(BuildingKind kind) { return infoOf(kind).archetype; }
BuildingSign signOf(BuildingKind kind) { return infoOf(kind).sign; }
float lockedChance(BuildingKind kind) { return infoOf(kind).locked; }
float alarmChance(BuildingKind kind) { return infoOf(kind).alarm; }

bool fromName(const std::string& text, BuildingKind& out)
{
    std::string lower;
    for (const char c : text) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (std::size_t i = 0; i < kKinds.size(); ++i)
    {
        // The key, or the name ("Guard Hut" as well as "guard_hut").
        std::string name;
        for (const char c : std::string(kKinds[i].name)) name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower == kKinds[i].key || lower == name)
        {
            out = static_cast<BuildingKind>(i);
            return true;
        }
    }
    return false;
}

const char* signSprite(BuildingSign sign)
{
    switch (sign)
    {
    case BuildingSign::Cross: return "sign_cross";
    case BuildingSign::Wrench: return "sign_wrench";
    case BuildingSign::Cart: return "sign_cart";
    case BuildingSign::Badge: return "sign_badge";
    case BuildingSign::Cup: return "sign_cup";
    case BuildingSign::Crate: return "sign_crate";
    case BuildingSign::None: break;
    }
    return "";
}

float roll(const LayoutData::BuildingRect& rect, std::uint32_t salt)
{
    const std::uint32_t h = hash3(static_cast<std::uint32_t>(rect.x) * 73856093u ^ static_cast<std::uint32_t>(rect.w),
                                  static_cast<std::uint32_t>(rect.y) * 19349663u ^ static_cast<std::uint32_t>(rect.h), salt);
    return static_cast<float>(h >> 8) / 16777216.0f;
}

std::vector<BuildingKind> assign(const LayoutData& layout, std::vector<std::string>* log)
{
    const std::size_t count = layout.buildings.size();
    std::vector<BuildingKind> kinds(count, K::House);
    std::vector<bool> taken(count, false);
    const glm::vec2 centre(static_cast<float>(layout.width) * 0.5f, static_cast<float>(layout.height) * 0.5f);
    const auto distance = [&](std::size_t i)
    {
        const LayoutData::BuildingRect& b = layout.buildings[i];
        const glm::vec2 c(static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f,
                          static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f);
        const glm::vec2 d = c - centre;
        return std::sqrt(d.x * d.x + d.y * d.y);
    };
    for (std::size_t i = 0; i < count; ++i)
    {
        if (archetypeForWidth(layout.buildings[i].w) == BuildingArchetype::Residential) taken[i] = true;
    }

    // Guarantees, in table order: the nearest free building of the archetype.
    constexpr std::array<K, 6> kGuaranteed = {K::Grocery, K::Pharmacy, K::Hardware, K::Clinic, K::Police, K::Garage};
    for (const K kind : kGuaranteed)
    {
        const BuildingArchetype archetype = archetypeOf(kind);
        int best = -1;
        float bestDistance = 0.0f;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (taken[i] || archetypeForWidth(layout.buildings[i].w) != archetype) continue;
            const float d = distance(i);
            if (best < 0 || d < bestDistance)
            {
                best = static_cast<int>(i);
                bestDistance = d;
            }
        }
        if (best < 0)
        {
            if (log != nullptr) log->push_back(std::string("no building left for a ") + name(kind));
            continue;
        }
        kinds[static_cast<std::size_t>(best)] = kind;
        taken[static_cast<std::size_t>(best)] = true;
    }

    // The rest, by their own dice.
    for (std::size_t i = 0; i < count; ++i)
    {
        if (taken[i]) continue;
        const float r = roll(layout.buildings[i], kSaltKind);
        if (archetypeForWidth(layout.buildings[i].w) == BuildingArchetype::Commercial)
        {
            kinds[i] = r < 0.25f ? K::Grocery : r < 0.40f ? K::Diner : K::Office;
        }
        else
        {
            kinds[i] = r < 0.40f ? K::Warehouse : K::Workshop;
        }
    }
    return kinds;
}

bool assignMissing(LayoutData& layout, std::vector<std::string>* log)
{
    bool missing = false;
    for (const LayoutData::BuildingRect& b : layout.buildings)
    {
        if (b.kind < 0 || b.kind >= static_cast<int>(K::Count)) missing = true;
    }
    if (!missing) return false;
    const std::vector<BuildingKind> kinds = assign(layout, log);
    for (std::size_t i = 0; i < kinds.size(); ++i)
    {
        layout.buildings[i].kind = static_cast<int>(kinds[i]);
    }
    return true;
}

BuildingKind kindOf(const LayoutData::BuildingRect& rect)
{
    if (rect.kind >= 0 && rect.kind < static_cast<int>(K::Count)) return static_cast<BuildingKind>(rect.kind);
    switch (archetypeForWidth(rect.w))
    {
    case BuildingArchetype::Residential: return K::House;
    case BuildingArchetype::Commercial: return K::Office;
    case BuildingArchetype::Industrial: return K::Workshop;
    }
    return K::House;
}

bool buildingLocked(const LayoutData::BuildingRect& rect)
{
    return roll(rect, kSaltLock) < lockedChance(kindOf(rect));
}

bool buildingHasAlarm(const LayoutData::BuildingRect& rect)
{
    return roll(rect, kSaltAlarm) < alarmChance(kindOf(rect));
}

int buildingAt(const LayoutData& layout, const glm::ivec2& tile)
{
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        if (inside(layout.buildings[i], tile)) return static_cast<int>(i) + 1;
    }
    return 0;
}

glm::ivec2 streetDoor(const LayoutData& layout, int building)
{
    if (building <= 0 || building > static_cast<int>(layout.buildings.size())) return glm::ivec2(-1);
    const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(building - 1)];
    for (const LayoutData::DoorPos& door : layout.doors)
    {
        if (door.y == b.y + b.h - 1 && door.x > b.x && door.x < b.x + b.w - 1) return glm::ivec2(door.x, door.y);
    }
    return glm::ivec2(-1);
}

std::vector<DoorLock> lockedDoors(const LayoutData& layout, int home)
{
    std::vector<DoorLock> locks;
    for (const LayoutData::DoorPos& door : layout.doors)
    {
        const glm::ivec2 tile(door.x, door.y);
        const int building = buildingAt(layout, tile);
        if (building <= 0 || building == home) continue;
        const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(building - 1)];
        const BuildingKind kind = kindOf(b);
        const bool edge = tile.x == b.x || tile.y == b.y || tile.x == b.x + b.w - 1 || tile.y == b.y + b.h - 1;
        bool locked = false;
        if (!edge)
        {
            // Inside: only a pharmacy keeps its stock room locked.
            locked = kind == K::Pharmacy;
        }
        else if (buildingLocked(b))
        {
            const bool street = tile.y == b.y + b.h - 1;
            // Half of locked buildings leave the back way open.
            locked = street || roll(b, kSaltBack) >= 0.5f;
        }
        if (!locked) continue;
        locks.push_back(DoorLock{tile, building, edge && buildingHasAlarm(b)});
    }
    return locks;
}
} // namespace buildingkinds
