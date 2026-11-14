#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

// The other people in town (overhaul phase 8): who they are, how they take
// you, what they choose to do next, what a thing is worth to them, and what
// happened to them while nobody was looking (between lives). Pure: the game
// (core/GameSurvivors.cpp, systems/SurvivorBrain.cpp) reads the world into
// these structs and applies what comes back.
namespace survivorrules
{
// How they live. Loners keep to themselves; a pair shares a house (one stays
// in while the other goes out); a group holds one building together.
enum class Role : std::uint8_t
{
    Loner = 0,
    Pair = 1,
    Group = 2,
    Retired = 3 // an earlier character who stayed on
};

enum class Attitude : std::uint8_t
{
    Open,
    Wary,
    Hostile
};

// What a survivor can choose to do, scored every two seconds.
enum class Option : std::uint8_t
{
    Idle = 0,
    Flee,
    Fight,
    Eat,
    Drink,
    Sleep,
    Scavenge,
    Fortify,
    Tend,
    Watch,
    Count
};
constexpr int kOptionCount = static_cast<int>(Option::Count);

[[nodiscard]] const char* optionName(Option option);   // "scavenge"
[[nodiscard]] const char* attitudeName(Attitude attitude); // "wary"
[[nodiscard]] const char* roleName(Role role);         // "loner"
[[nodiscard]] bool parseAttitude(const std::string& text, Attitude& out);

// --- Numbers --------------------------------------------------------------

constexpr int kSurvivorsPerWorld = 12;
constexpr int kSurvivorsInfectedSociety = 20;
constexpr float kNeedsScale = 0.8f;          // their hunger, thirst and sleep run at 0.8 of yours
constexpr float kThinkSeconds = 2.0f;        // how often they choose again
constexpr float kNearTiles = 60.0f;          // nearer than this they walk and think; beyond, an hour at a time
constexpr float kScavengeRadiusTiles = 40.0f; // how far from home they look
constexpr float kFightTiles = 3.0f;          // the dead this close can be fought
constexpr float kWatchTiles = 12.0f;         // a wary one watches you this close
constexpr float kTalkTiles = 2.0f;           // E reaches a person this close
constexpr float kWeaponDrawnTiles = 8.0f;    // a weapon out within this frightens them
constexpr float kKilledNearTiles = 20.0f;    // people who knew someone you killed, within this, hear of it
constexpr float kLowStockDays = 3.0f;        // below this they go out for more
constexpr float kHungryBelow = 50.0f;        // hunger and thirst are 100 full
constexpr float kFoodPerDay = 0.025f * 0.8f * 60.0f * 24.0f;  // hunger points a day (28.8)
constexpr float kWaterPerDay = 0.045f * 0.8f * 60.0f * 24.0f; // thirst points a day (51.8)

// --- Trust ----------------------------------------------------------------

constexpr float kTrustMin = -100.0f;
constexpr float kTrustMax = 100.0f;
constexpr float kHelloTrust = 3.0f;      // once a day
constexpr float kTradeFairTrust = 5.0f;
constexpr float kTradeGenerousTrust = 10.0f;
constexpr float kWeaponDrawnTrust = -5.0f;
constexpr float kTheftTrust = -40.0f;
constexpr float kKilledFriendTrust = -50.0f;
constexpr float kTradeWaryTrust = 10.0f;     // a wary one trades from this
constexpr float kAskAlongTrust = 40.0f;      // a loner comes along from this
constexpr float kLeaveBelowTrust = 20.0f;    // a companion ignored goes home under this
constexpr float kIgnoredTrustPerDay = -10.0f;
constexpr float kIgnoredHours = 12.0f;
constexpr float kHomeWarnTrust = 20.0f;      // under this, walking into their house is trespass
constexpr float kRememberedTrust = 60.0f;    // a companion who trusted the dead this much...
constexpr float kRememberedStart = 15.0f;    // ...meets the next character at this
constexpr float kBloodlineTrust = 50.0f;     // a retired character greets the bloodline at this

// A change of trust, after the Loner trait (gains are halved). Losses are
// never softened.
[[nodiscard]] float trustChange(float amount, bool lonerTrait);
[[nodiscard]] float clampTrust(float trust);
// Trust a survivor starts a new character's life at: they do not know this
// one, unless the last travelled with them and was trusted (15), or they are
// an earlier character of this bloodline (50).
[[nodiscard]] float trustForNewLife(float oldTrust, bool wasCompanion, bool retired);
// The word the notebook uses: "Owes you.", "Trades.", "Wary.", "Do not go back."
[[nodiscard]] const char* standingWord(float trust, Attitude attitude);

// --- Who they are ---------------------------------------------------------

struct Personality
{
    float aggression = 0.5f;
    float cooperation = 0.5f;
    float competence = 0.5f;
};

// 0..1: how near the end they feel, from hunger and thirst (100 full) and
// how many days their stock would last.
[[nodiscard]] float desperation(float hunger, float thirst, float stockDays);
// Hostile if aggression > 0.7, or desperate (> 0.7) and uncooperative
// (< 0.4); open if cooperation > 0.6 and trust >= 0; wary otherwise.
// `angry`: they turned on you today (trespass, theft, a fight).
[[nodiscard]] Attitude attitude(const Personality& p, float desperation, float trust, bool angry);
// They follow you to rob you: desperate (> 0.8) and trust under 10.
[[nodiscard]] bool wouldRob(float desperation, float trust, bool companion);
// Tiles within which the dead make them run: 6 x (1 - aggression).
[[nodiscard]] float fleeTiles(float aggression);
// Seconds between blows: 1.5 for the hopeless, 0.9 for the best.
[[nodiscard]] float blowSeconds(float competence);

// --- Choosing -------------------------------------------------------------

struct Situation
{
    Personality p;
    float hunger = 100.0f;
    float thirst = 100.0f;
    float sleepDebt = 0.0f;     // 0..100
    float health = 1.0f;        // share of full
    int wounds = 0;
    float stockDays = 5.0f;
    bool hasFood = true;
    bool hasDrink = true;
    bool hasMedicine = false;
    bool armed = false;
    bool hasBoards = false;     // planks and nails in stock
    bool homeOpen = false;      // a door of home not boarded
    int deadInFleeRange = 0;    // the dead within fleeTiles()
    int deadInFightRange = 0;   // within kFightTiles
    float nearestDeadTiles = 99.0f;
    bool night = false;
    bool stayer = false;        // the half of a pair who keeps the house
    bool containerKnown = false; // an unsearched container within reach of home
    bool playerWatched = false; // the player within kWatchTiles and they are wary
    Option current = Option::Idle;
};

using Scores = std::array<float, kOptionCount>;
// Every option's score (0 when it does not apply).
[[nodiscard]] Scores score(const Situation& s);
// The best, holding to the current one unless another beats it by 0.05.
[[nodiscard]] Option choose(const Situation& s);

// --- Worth and trade ------------------------------------------------------

enum class Kind : std::uint8_t
{
    Food,
    Drink,
    Medicine,
    Weapon,
    Other
};

struct Wants
{
    bool hungry = false; // hunger under 50: food counts double
    bool thirsty = false; // thirst under 50: drink counts double
    bool hurt = false;   // wounded or under 60 % health: medicine counts triple
};

// What one of a thing is worth to them.
[[nodiscard]] float worth(float value, Kind kind, const Wants& wants);

enum class Verdict : std::uint8_t
{
    WantMore,
    Fair,
    Generous
};
// What they make of `given` (worth of what you put in) against `taken`
// (worth of what you take). Nothing for nothing wants more.
[[nodiscard]] Verdict verdict(float given, float taken);
[[nodiscard]] const char* verdictText(Verdict v); // "They want more", "Fair", "Generous"
[[nodiscard]] float verdictTrust(Verdict v);      // fair +5, generous +10
// Trust for giving something worth `worth` to them: worth x 2.
[[nodiscard]] float giftTrust(float worth);
// Whether they will trade at all.
[[nodiscard]] bool willTrade(Attitude attitude, float trust);

// One stack of their stock, as the rules read it.
struct StockLine
{
    int itemId = -1;
    int count = 0;
    Kind kind = Kind::Other;
    float hunger = 0.0f; // points one gives
    float thirst = 0.0f;
    float weaponScore = 0.0f; // > 0 for weapons; the best is kept
};
// How many of each line they will part with: all but two days of their food
// and water, and their best weapon. Same order as `stock`.
[[nodiscard]] std::vector<int> spareCounts(const std::vector<StockLine>& stock);
// Days of food and of water their stock holds; the smaller.
[[nodiscard]] float stockDays(const std::vector<StockLine>& stock);

// --- Lines ----------------------------------------------------------------

// A stable index into a pool of `size` lines from the speaker's name and the
// day (and a salt for different situations).
[[nodiscard]] std::size_t lineIndex(const std::string& name, int day, std::uint32_t salt, std::size_t size);

// --- The town's people, made once with the world --------------------------

struct HomeChoice
{
    int building = 0;  // index + 1
    glm::ivec2 tile{0}; // a floor tile inside
    bool garage = false;
    bool big = false;  // a group of five fits
    bool nearStart = false; // within 20 tiles of where the first life starts
};

struct CastMember
{
    std::string name;
    Role role = Role::Loner;
    int partner = -1;   // index in the cast of the other half of a pair
    int group = 0;      // 0 none; 1 the ones at the garage; 2 the society
    int home = -1;      // index into the homes given
    Personality p;
    int outfit = 0;     // 0..2
    bool stayer = false; // the half of a pair who keeps the house
    std::vector<std::pair<std::string, int>> stock; // item key, count
};

// Twelve people (twenty for Infected Society): six loners, two pairs, and the
// two at the garage who are hostile; Infected Society adds a group of five
// and more loners. Homes are chosen from `houses` (never the same twice,
// except a pair or group sharing one) and `garages`; `names` gives the pool.
// The two at the garage never live `nearStart` while another garage or
// house is free (they are somewhere to learn to avoid, not the neighbours on
// the first morning). The same seed always gives the same town.
[[nodiscard]] std::vector<CastMember> castWorld(std::uint32_t seed, bool infectedSociety,
                                                const std::vector<HomeChoice>& houses,
                                                const std::vector<HomeChoice>& garages,
                                                const std::vector<std::string>& names);

// --- Between lives --------------------------------------------------------

struct LifeSurvivor
{
    int id = 0;
    bool alive = true;
    glm::ivec2 home{0};   // a tile of home
    int homeBuilding = 0;
    float competence = 0.5f;
    float danger = 1.0f;  // 0.5 + 1.5 x infection / 100 of their district
    int food = 0;         // things to eat in stock
    int drink = 0;        // things to drink
    int hungryDays = 0;   // days in a row with nothing to eat or drink
    int deathDay = 0;
    bool companioned = false; // a partner or companion buries them (a grave, not a body)
};

struct LifeContainer
{
    glm::ivec2 tile{0};
    int building = 0;
    bool searched = false; // already searched (by anyone)
    bool protectedHome = false; // in a house someone lives in, or the base
    int food = 0;
    int drink = 0;
    int medicine = 0;
};

struct LootEvent
{
    int survivor = 0;  // index in the survivors given
    int container = 0; // index in the containers given
    int day = 0;
};

struct DeathEvent
{
    int survivor = 0;
    int day = 0;
    bool starved = false;
};

struct BetweenLivesResult
{
    std::vector<int> eatenFood;  // per survivor: things eaten
    std::vector<int> drunk;      // per survivor: things drunk
    std::vector<LootEvent> loots;
    std::vector<DeathEvent> deaths;
    std::uint64_t fingerprint = 0; // of the outcome, for the harness
};

constexpr float kDailyDeathChance = 0.015f; // x (2 - competence) x danger
constexpr float kForageChance = 0.2f;       // + 0.4 x competence: a day's foraging finds a meal
constexpr int kStarveDays = 6;              // days in a row with nothing to eat
// The days the dead character lived pass for everyone else: each eats one
// thing a day from stock (drinks one too while there is any: they have rain
// and a tap, so water never kills them); with no food they forage, which
// some days finds nothing; under three days of food they empty the nearest
// unsearched container within 40 tiles of home (two when competent); six
// days in a row with nothing to eat and they starve; and each day the dead
// may take them. Deterministic from `seed` (the world seed and the life
// number). `firstDay` is the day the dead character's life began.
[[nodiscard]] BetweenLivesResult advanceBetweenLives(std::vector<LifeSurvivor>& survivors,
                                                     std::vector<LifeContainer>& containers, int days,
                                                     std::uint32_t seed, int firstDay = 1);
[[nodiscard]] float dailyDeathChance(float competence, float danger);
[[nodiscard]] float forageChance(float competence);
[[nodiscard]] float dangerOf(float infection);

// --- Far away: one game hour at a time ------------------------------------

struct FarHour
{
    bool scavenge = false; // empty one container near home
    bool wound = false;    // 2 % x danger, when out
    bool dies = false;     // 0.5 %, when out
};
// For one far-off survivor and one hour: out for supplies if it is day and
// stock is low and they have not been out today.
[[nodiscard]] FarHour farHour(bool day, float stockDays, bool outToday, float danger, std::uint32_t seed,
                              int survivorId, int absoluteHour);

// A value in [0, 1) from a seed and three numbers (no state).
[[nodiscard]] float unitHash(std::uint32_t seed, std::uint32_t a, std::uint32_t b, std::uint32_t c);
} // namespace survivorrules
