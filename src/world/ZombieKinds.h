#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>

// The kinds of the dead and the threat modes, as data and plain rules
// (overhaul phase 4). Loaded from assets/data/zombies.json; the built-in
// defaults are the same numbers, so tests and a missing file agree.
enum class ZombieKind : std::uint8_t
{
    Shambler = 0,
    Runner,
    Brute,
    Crawler,
    Screamer
};
inline constexpr int kZombieKindCount = 5;

namespace zombiekinds
{
struct KindStats
{
    float share = 0.0f;      // of a spawn on day 1
    float sharePerDay = 0.0f;
    float shareCap = 1.0f;
    float hpMin = 30.0f;
    float hpMax = 70.0f;
    float speedMin = 0.8f;   // multiples of kBaseSpeed
    float speedMax = 1.2f;
    float damage = 7.0f;     // a claw
    float sightTiles = 8.0f;
    float hearing = 1.0f;    // multiplies every radius it hears
    float doorDamage = 1.0f; // multiplies a blow on a door, window or board
};

// Base speed of the dead, world px/s.
inline constexpr float kBaseSpeed = 112.0f;

// Runner: the head snaps up, it sprints, then tires.
inline constexpr float kRunnerStartSeconds = 0.5f;
inline constexpr float kRunnerSprintSeconds = 8.0f;
inline constexpr float kRunnerWindedSeconds = 4.0f;
inline constexpr float kRunnerSprintScale = 1.7f;
inline constexpr float kRunnerWindedScale = 1.0f;
inline constexpr float kRunnerWindupSeconds = 0.40f;
// Brute: light blows do not move it; its blow puts you down.
inline constexpr float kBruteKnockbackFloorPixels = 30.0f;
inline constexpr float kBruteKnockdownSeconds = 0.8f;
inline constexpr float kBruteShakeTiles = 4.0f;
// Crawler: hidden low; bites the legs.
inline constexpr float kCrawlerHiddenTiles = 2.0f;
inline constexpr float kCrawlerHobbleSeconds = 20.0f;
inline constexpr float kCrawlerHobbleSlow = 0.25f;
inline constexpr float kCrawlerDormantShare = 0.30f;
inline constexpr float kCrawlerWakeTiles = 2.0f;
inline constexpr float kCrawlerScrapeSeconds = 0.6f;
// Screamer: screams on sight, keeps its distance, flees a rush.
inline constexpr float kScreamSeconds = 1.2f;
inline constexpr float kScreamRepeatSeconds = 10.0f;
inline constexpr float kScreamerKeepMinTiles = 4.0f;
inline constexpr float kScreamerKeepMaxTiles = 6.0f;
inline constexpr float kScreamerFleeScale = 0.9f;

// The hidden rules of The Unknown (two per run, by seed).
enum class UnknownRule : std::uint8_t
{
    DormantByDay = 0, // still by day, frenzied (x1.5) at night
    BlindHearing,     // see little, hear twice as far
    DrawnToLight,     // light draws them from 20 tiles
    RainCalm,         // calm in rain, frenzied when it stops
    IgnoreStill       // they do not see you while you stand still
};
inline constexpr int kUnknownRuleCount = 5;
[[nodiscard]] const char* unknownRuleKey(UnknownRule rule);
// The notebook's line once a rule has been seen three times.
[[nodiscard]] const char* unknownRuleLine(UnknownRule rule);
// Two different rules, chosen by the run seed.
[[nodiscard]] std::pair<UnknownRule, UnknownRule> unknownRules(std::uint32_t runSeed);

// What a threat mode sets (zombies.json "modes").
struct ModeRules
{
    float population = 1.0f;       // starting list and the director's target
    float health = 1.0f;
    float speed = 1.0f;
    float migration = 1.0f;        // the director draws wanderers in this much more often
    float runnerBruteScale = 1.0f; // their shares
    float clumpTiles = 6.0f;
    int hordeFirstNight = 7;
    int hordeEveryMin = 4;
    int hordeEveryMax = 6;
    bool awarenessDrains = true;
    float giveUpSeconds = 90.0f;
    float trailScale = 1.0f;       // how long blood markers last
    bool follower = false;
    bool startPure = false;        // day 1 is Shamblers only
    int mutatingFromDay = 0;       // 0: never; from this day Runner, Brute and Screamer shares grow
    float mutatePerDay = 0.0f;
    float mutateCap = 0.35f;
    int doorsDoubleFromDay = 0;    // 0: never
    int climbWindowsFromDay = 0;   // 0: never
    int unknownRules = 0;
};

class Table
{
public:
    Table(); // the built-in defaults
    // False (keeping the defaults) when the file is missing or unreadable.
    bool load(const std::string& path, std::string* error = nullptr);
    bool loadFromString(const std::string& jsonText, std::string* error = nullptr);

    [[nodiscard]] const KindStats& stats(ZombieKind kind) const { return mKinds[static_cast<std::size_t>(kind)]; }
    // modeIndex: the title screen's order (0 swarm, 1 stalker, 2 mutating,
    // 3 society, 4 unknown, 5 hybrid).
    [[nodiscard]] const ModeRules& mode(int modeIndex) const;

private:
    std::array<KindStats, kZombieKindCount> mKinds{};
    std::array<ModeRules, 6> mModes{};
};

// The share of each kind in a spawn on `day` under `mode`. A district over
// 50 infection doubles the Runner and Screamer shares there. Sums to 1.
[[nodiscard]] std::array<float, kZombieKindCount> shares(const Table& table, const ModeRules& mode, int day,
                                                         float districtInfection);
// Pick a kind from shares with a uniform roll in [0, 1).
[[nodiscard]] ZombieKind pick(const std::array<float, kZombieKindCount>& shares, float roll01);

[[nodiscard]] const char* kindKey(ZombieKind kind); // "shambler", "runner", ...
[[nodiscard]] bool parseKind(const std::string& key, ZombieKind& out);
// The sprite set a kind is drawn from ("zombie", "runner", ...).
[[nodiscard]] const char* spriteSet(ZombieKind kind);

// The table the game loaded (zombies.json); the defaults until then.
[[nodiscard]] Table& activeTable();

// Health and speed (px/s) for a new one of `kind`, from two rolls in [0, 1).
struct Rolled
{
    float health = 50.0f;
    float speed = kBaseSpeed;
};
[[nodiscard]] Rolled roll(const Table& table, ZombieKind kind, float healthRoll01, float speedRoll01);

// Doors and boards take this many times the kind's door damage on `day`.
[[nodiscard]] float doorDamageScale(const ModeRules& mode, int day);
// From this day on (Mutating), the dead climb through whole windows.
[[nodiscard]] bool climbsWindows(const ModeRules& mode, int day);
} // namespace zombiekinds
