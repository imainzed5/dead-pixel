#include "world/ZombieKinds.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace zombiekinds
{
namespace
{
constexpr const char* kModeKeys[6] = {"swarm", "stalker", "mutating", "society", "unknown", "hybrid"};

std::array<KindStats, kZombieKindCount> defaultKinds()
{
    std::array<KindStats, kZombieKindCount> k{};
    k[0] = KindStats{0.70f, 0.0f, 1.0f, 30.0f, 70.0f, 0.8f, 1.2f, 7.0f, 8.0f, 1.0f, 1.0f};
    k[1] = KindStats{0.06f, 0.015f, 0.18f, 25.0f, 40.0f, 0.95f, 1.05f, 6.0f, 9.0f, 1.0f, 0.5f};
    k[2] = KindStats{0.06f, 0.010f, 0.14f, 150.0f, 190.0f, 0.57f, 0.63f, 14.0f, 7.0f, 0.7f, 3.0f};
    k[3] = KindStats{0.10f, 0.0f, 0.10f, 20.0f, 35.0f, 0.33f, 0.37f, 5.0f, 5.0f, 1.0f, 0.0f};
    k[4] = KindStats{0.04f, 0.005f, 0.08f, 30.0f, 45.0f, 0.85f, 0.95f, 4.0f, 10.0f, 1.5f, 0.5f};
    return k;
}

std::array<ModeRules, 6> defaultModes()
{
    std::array<ModeRules, 6> m{};
    // Swarm
    m[0].population = 1.45f; m[0].health = 0.8f; m[0].speed = 0.94f; m[0].runnerBruteScale = 0.5f;
    m[0].clumpTiles = 9.0f; m[0].hordeFirstNight = 5; m[0].hordeEveryMin = 3; m[0].hordeEveryMax = 4;
    m[0].migration = 1.3f;
    // Stalker
    m[1].population = 0.5f; m[1].awarenessDrains = false; m[1].giveUpSeconds = 600.0f; m[1].trailScale = 3.0f;
    m[1].follower = true; m[1].migration = 1.6f;
    // Mutating
    m[2].startPure = true; m[2].mutatingFromDay = 1; m[2].mutatePerDay = 0.02f; m[2].mutateCap = 0.35f;
    m[2].doorsDoubleFromDay = 10; m[2].climbWindowsFromDay = 15;
    // Infected society
    m[3].population = 0.7f; m[3].speed = 0.82f; m[3].migration = 0.8f;
    // The Unknown
    m[4].unknownRules = 2; m[4].migration = 1.1f;
    // Hybrid: the swarm, then mutation on top from day 8
    m[5] = m[0];
    m[5].migration = 1.25f;
    m[5].mutatingFromDay = 8; m[5].mutatePerDay = 0.02f; m[5].mutateCap = 0.35f;
    m[5].doorsDoubleFromDay = 10; m[5].climbWindowsFromDay = 15;
    return m;
}

void readKind(const nlohmann::json& j, KindStats& k)
{
    k.share = j.value("share", k.share);
    k.sharePerDay = j.value("perDay", k.sharePerDay);
    k.shareCap = j.value("cap", std::max(k.share, k.shareCap));
    if (j.contains("hp") && j["hp"].is_array() && j["hp"].size() == 2)
    {
        k.hpMin = j["hp"][0].get<float>();
        k.hpMax = j["hp"][1].get<float>();
    }
    if (j.contains("speed") && j["speed"].is_array() && j["speed"].size() == 2)
    {
        k.speedMin = j["speed"][0].get<float>();
        k.speedMax = j["speed"][1].get<float>();
    }
    k.damage = j.value("damage", k.damage);
    k.sightTiles = j.value("sight", k.sightTiles);
    k.hearing = j.value("hearing", k.hearing);
    k.doorDamage = j.value("door", k.doorDamage);
}

void readMode(const nlohmann::json& j, ModeRules& m)
{
    m.population = j.value("population", m.population);
    m.health = j.value("health", m.health);
    m.speed = j.value("speed", m.speed);
    m.migration = j.value("migration", m.migration);
    m.runnerBruteScale = j.value("runnerBruteScale", m.runnerBruteScale);
    m.clumpTiles = j.value("clumpTiles", m.clumpTiles);
    m.hordeFirstNight = j.value("hordeFirstNight", m.hordeFirstNight);
    if (j.contains("hordeEvery") && j["hordeEvery"].is_array() && j["hordeEvery"].size() == 2)
    {
        m.hordeEveryMin = j["hordeEvery"][0].get<int>();
        m.hordeEveryMax = j["hordeEvery"][1].get<int>();
    }
    m.awarenessDrains = j.value("awarenessDrains", m.awarenessDrains);
    m.giveUpSeconds = j.value("giveUpSeconds", m.giveUpSeconds);
    m.trailScale = j.value("trailScale", m.trailScale);
    m.follower = j.value("follower", m.follower);
    m.startPure = j.value("startPure", m.startPure);
    m.mutatingFromDay = j.value("mutatingFromDay", m.mutatingFromDay);
    m.mutatePerDay = j.value("mutatePerDay", m.mutatePerDay);
    m.mutateCap = j.value("mutateCap", m.mutateCap);
    m.doorsDoubleFromDay = j.value("doorsDoubleFromDay", m.doorsDoubleFromDay);
    m.climbWindowsFromDay = j.value("climbWindowsFromDay", m.climbWindowsFromDay);
    m.unknownRules = j.value("unknownRules", m.unknownRules);
}

std::uint32_t mix(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}
} // namespace

Table::Table() : mKinds(defaultKinds()), mModes(defaultModes()) {}

bool Table::load(const std::string& path, std::string* error)
{
    std::ifstream in(path);
    if (!in)
    {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::stringstream text;
    text << in.rdbuf();
    return loadFromString(text.str(), error);
}

bool Table::loadFromString(const std::string& jsonText, std::string* error)
{
    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(jsonText);
    }
    catch (const std::exception& e)
    {
        if (error != nullptr) *error = e.what();
        return false;
    }
    std::array<KindStats, kZombieKindCount> kinds = defaultKinds();
    std::array<ModeRules, 6> modes = defaultModes();
    if (root.contains("kinds") && root["kinds"].is_object())
    {
        for (int i = 0; i < kZombieKindCount; ++i)
        {
            const char* key = kindKey(static_cast<ZombieKind>(i));
            if (root["kinds"].contains(key)) readKind(root["kinds"][key], kinds[static_cast<std::size_t>(i)]);
        }
    }
    if (root.contains("modes") && root["modes"].is_object())
    {
        for (int i = 0; i < 6; ++i)
        {
            if (root["modes"].contains(kModeKeys[i])) readMode(root["modes"][kModeKeys[i]], modes[static_cast<std::size_t>(i)]);
        }
    }
    mKinds = kinds;
    mModes = modes;
    return true;
}

const ModeRules& Table::mode(int modeIndex) const
{
    return mModes[static_cast<std::size_t>(std::clamp(modeIndex, 0, 5))];
}

std::array<float, kZombieKindCount> shares(const Table& table, const ModeRules& mode, int day, float districtInfection)
{
    std::array<float, kZombieKindCount> s{};
    const float days = static_cast<float>(std::max(0, day - 1));
    const bool pureDay = mode.startPure && day <= 1;
    for (int i = 1; i < kZombieKindCount; ++i)
    {
        const KindStats& k = table.stats(static_cast<ZombieKind>(i));
        float share = std::min(k.shareCap, k.share + k.sharePerDay * days);
        if (mode.startPure)
        {
            // It starts with the plain dead; the rest grow in below.
            share = static_cast<ZombieKind>(i) == ZombieKind::Crawler && !pureDay ? k.share : 0.0f;
        }
        s[static_cast<std::size_t>(i)] = share;
    }
    s[static_cast<std::size_t>(ZombieKind::Runner)] *= mode.runnerBruteScale;
    s[static_cast<std::size_t>(ZombieKind::Brute)] *= mode.runnerBruteScale;

    // Mutation: Runner, Brute and Screamer grow a little each day.
    if (mode.mutatingFromDay > 0 && day >= mode.mutatingFromDay)
    {
        const float grown = mode.mutatePerDay * static_cast<float>(day - mode.mutatingFromDay);
        for (ZombieKind kind : {ZombieKind::Runner, ZombieKind::Brute, ZombieKind::Screamer})
        {
            float& share = s[static_cast<std::size_t>(kind)];
            share = std::min(mode.mutateCap, share + grown);
        }
    }

    if (districtInfection > 50.0f)
    {
        s[static_cast<std::size_t>(ZombieKind::Runner)] *= 2.0f;
        s[static_cast<std::size_t>(ZombieKind::Screamer)] *= 2.0f;
    }

    float others = 0.0f;
    for (int i = 1; i < kZombieKindCount; ++i) others += s[static_cast<std::size_t>(i)];
    if (others > 0.95f)
    {
        // Never fewer than one in twenty plain dead.
        for (int i = 1; i < kZombieKindCount; ++i) s[static_cast<std::size_t>(i)] *= 0.95f / others;
        others = 0.95f;
    }
    s[0] = 1.0f - others;
    return s;
}

ZombieKind pick(const std::array<float, kZombieKindCount>& s, float roll01)
{
    float acc = 0.0f;
    for (int i = kZombieKindCount - 1; i >= 1; --i)
    {
        acc += s[static_cast<std::size_t>(i)];
        if (roll01 < acc) return static_cast<ZombieKind>(i);
    }
    return ZombieKind::Shambler;
}

const char* kindKey(ZombieKind kind)
{
    switch (kind)
    {
    case ZombieKind::Runner: return "runner";
    case ZombieKind::Brute: return "brute";
    case ZombieKind::Crawler: return "crawler";
    case ZombieKind::Screamer: return "screamer";
    case ZombieKind::Shambler:
    default: return "shambler";
    }
}

bool parseKind(const std::string& key, ZombieKind& out)
{
    for (int i = 0; i < kZombieKindCount; ++i)
    {
        if (key == kindKey(static_cast<ZombieKind>(i)))
        {
            out = static_cast<ZombieKind>(i);
            return true;
        }
    }
    return false;
}

const char* spriteSet(ZombieKind kind)
{
    switch (kind)
    {
    case ZombieKind::Runner: return "runner";
    case ZombieKind::Brute: return "brute";
    case ZombieKind::Crawler: return "crawler";
    case ZombieKind::Screamer: return "screamer";
    case ZombieKind::Shambler:
    default: return "zombie";
    }
}

Table& activeTable()
{
    static Table table;
    return table;
}

Rolled roll(const Table& table, ZombieKind kind, float healthRoll01, float speedRoll01)
{
    const KindStats& k = table.stats(kind);
    Rolled r;
    r.health = k.hpMin + (k.hpMax - k.hpMin) * std::clamp(healthRoll01, 0.0f, 1.0f);
    r.speed = kBaseSpeed * (k.speedMin + (k.speedMax - k.speedMin) * std::clamp(speedRoll01, 0.0f, 1.0f));
    return r;
}

float doorDamageScale(const ModeRules& mode, int day)
{
    return mode.doorsDoubleFromDay > 0 && day >= mode.doorsDoubleFromDay ? 2.0f : 1.0f;
}

bool climbsWindows(const ModeRules& mode, int day)
{
    return mode.climbWindowsFromDay > 0 && day >= mode.climbWindowsFromDay;
}

const char* unknownRuleKey(UnknownRule rule)
{
    switch (rule)
    {
    case UnknownRule::DormantByDay: return "dormant_by_day";
    case UnknownRule::BlindHearing: return "blind_hearing";
    case UnknownRule::DrawnToLight: return "drawn_to_light";
    case UnknownRule::RainCalm: return "rain_calm";
    case UnknownRule::IgnoreStill: return "ignore_still";
    }
    return "";
}

const char* unknownRuleLine(UnknownRule rule)
{
    switch (rule)
    {
    case UnknownRule::DormantByDay: return "By day they stand. At night they run.";
    case UnknownRule::BlindHearing: return "They do not see well, but they hear.";
    case UnknownRule::DrawnToLight: return "Light brings them from far off.";
    case UnknownRule::RainCalm: return "Rain quiets them. When it stops, they come.";
    case UnknownRule::IgnoreStill: return "If you keep still, they look past you.";
    }
    return "";
}

std::pair<UnknownRule, UnknownRule> unknownRules(std::uint32_t runSeed)
{
    const std::uint32_t h = mix(runSeed ^ 0x51ED27u);
    const int first = static_cast<int>(h % kUnknownRuleCount);
    const int step = 1 + static_cast<int>((h / kUnknownRuleCount) % (kUnknownRuleCount - 1));
    return {static_cast<UnknownRule>(first), static_cast<UnknownRule>((first + step) % kUnknownRuleCount)};
}
} // namespace zombiekinds
