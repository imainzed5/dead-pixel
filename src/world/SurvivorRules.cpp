#include "world/SurvivorRules.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace survivorrules
{
namespace
{
std::uint32_t mix32(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

std::string lower(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// A small deterministic stream for making the town.
class Dice
{
public:
    explicit Dice(std::uint32_t seed) : mState(seed ^ 0x5EED5u) {}
    float unit()
    {
        mState = mix32(mState + 0x9E3779B9u);
        return static_cast<float>(mState >> 8) / 16777216.0f;
    }
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
    int below(int n) { return n <= 0 ? 0 : std::min(n - 1, static_cast<int>(unit() * static_cast<float>(n))); }

private:
    std::uint32_t mState;
};
} // namespace

float unitHash(std::uint32_t seed, std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    std::uint32_t h = mix32(seed ^ 0xA5A5A5A5u);
    h = mix32(h ^ (a * 0x9E3779B1u));
    h = mix32(h ^ (b * 0x85EBCA77u));
    h = mix32(h ^ (c * 0xC2B2AE3Du));
    return static_cast<float>(h >> 8) / 16777216.0f;
}

const char* optionName(Option option)
{
    switch (option)
    {
    case Option::Idle: return "idle";
    case Option::Flee: return "flee";
    case Option::Fight: return "fight";
    case Option::Eat: return "eat";
    case Option::Drink: return "drink";
    case Option::Sleep: return "sleep";
    case Option::Scavenge: return "scavenge";
    case Option::Fortify: return "fortify";
    case Option::Tend: return "tend";
    case Option::Watch: return "watch";
    case Option::Count: break;
    }
    return "idle";
}

const char* attitudeName(Attitude attitude)
{
    switch (attitude)
    {
    case Attitude::Open: return "open";
    case Attitude::Wary: return "wary";
    case Attitude::Hostile: return "hostile";
    }
    return "wary";
}

const char* roleName(Role role)
{
    switch (role)
    {
    case Role::Loner: return "loner";
    case Role::Pair: return "pair";
    case Role::Group: return "group";
    case Role::Retired: return "retired";
    }
    return "loner";
}

bool parseAttitude(const std::string& text, Attitude& out)
{
    const std::string t = lower(text);
    if (t == "open") out = Attitude::Open;
    else if (t == "wary") out = Attitude::Wary;
    else if (t == "hostile") out = Attitude::Hostile;
    else return false;
    return true;
}

// --- Trust ------------------------------------------------------------------

float clampTrust(float trust)
{
    return std::clamp(trust, kTrustMin, kTrustMax);
}

float trustChange(float amount, bool lonerTrait)
{
    return (amount > 0.0f && lonerTrait) ? amount * 0.5f : amount;
}

float trustForNewLife(float oldTrust, bool wasCompanion, bool retired)
{
    if (retired) return kBloodlineTrust;
    if (wasCompanion && oldTrust >= kRememberedTrust) return kRememberedStart;
    return 0.0f;
}

const char* standingWord(float trust, Attitude attitude)
{
    if (attitude == Attitude::Hostile || trust <= -30.0f) return "Do not go back.";
    if (trust >= 40.0f) return "Owes you.";
    if (attitude == Attitude::Open || trust >= kTradeWaryTrust) return "Trades.";
    return "Wary.";
}

// --- Who they are -----------------------------------------------------------

float desperation(float hunger, float thirst, float stockDays)
{
    const float hungerNeed = std::clamp(1.0f - hunger / kHungryBelow, 0.0f, 1.0f);
    const float thirstNeed = std::clamp(1.0f - thirst / kHungryBelow, 0.0f, 1.0f);
    const float shortage = std::clamp(1.0f - stockDays / kLowStockDays, 0.0f, 1.0f);
    return std::clamp(0.6f * std::max(hungerNeed, thirstNeed) + 0.4f * shortage, 0.0f, 1.0f);
}

Attitude attitude(const Personality& p, float desp, float trust, bool angry)
{
    if (angry) return Attitude::Hostile;
    if (p.aggression > 0.7f || (desp > 0.7f && p.cooperation < 0.4f)) return Attitude::Hostile;
    if (p.cooperation > 0.6f && trust >= 0.0f) return Attitude::Open;
    return Attitude::Wary;
}

bool wouldRob(float desp, float trust, bool companion)
{
    return !companion && desp > 0.8f && trust < 10.0f;
}

float fleeTiles(float aggression)
{
    return 6.0f * (1.0f - std::clamp(aggression, 0.0f, 1.0f));
}

float blowSeconds(float competence)
{
    return 1.5f - 0.6f * std::clamp(competence, 0.0f, 1.0f);
}

// --- Choosing ---------------------------------------------------------------

Scores score(const Situation& s)
{
    Scores out{};
    const float aggression = std::clamp(s.p.aggression, 0.0f, 1.0f);
    const bool weak = s.health < 0.35f;

    out[static_cast<int>(Option::Idle)] = 0.1f;

    // Run from the dead in range (more of them, more reason), or from any
    // close by when badly hurt; three or more in reach are never fought.
    float flee = 0.0f;
    if (s.deadInFleeRange > 0)
    {
        flee = 0.7f + 0.3f * (1.0f - aggression) + 0.05f * static_cast<float>(std::min(s.deadInFleeRange, 4));
    }
    if (weak && s.nearestDeadTiles < 6.0f) flee = std::max(flee, 0.98f);
    if (s.deadInFightRange >= 3) flee = std::max(flee, 0.97f);
    out[static_cast<int>(Option::Flee)] = flee;

    if (s.deadInFightRange >= 1 && s.deadInFightRange <= 2 && !weak)
    {
        out[static_cast<int>(Option::Fight)] = 0.5f + 0.4f * aggression + (s.armed ? 0.15f : 0.0f);
    }

    if (s.hasFood && s.hunger < kHungryBelow)
    {
        out[static_cast<int>(Option::Eat)] = 0.3f + 0.6f * (1.0f - s.hunger / kHungryBelow);
    }
    if (s.hasDrink && s.thirst < kHungryBelow)
    {
        out[static_cast<int>(Option::Drink)] = 0.3f + 0.6f * (1.0f - s.thirst / kHungryBelow);
    }

    if (s.sleepDebt > 50.0f || (s.night && s.sleepDebt > 25.0f))
    {
        out[static_cast<int>(Option::Sleep)] = 0.2f + 0.6f * std::min(1.0f, s.sleepDebt / 100.0f) + (s.night ? 0.2f : 0.0f);
    }

    if (s.stockDays < kLowStockDays && !s.night && s.containerKnown && !s.stayer)
    {
        out[static_cast<int>(Option::Scavenge)] = 0.35f + 0.4f * (1.0f - s.stockDays / kLowStockDays);
    }

    if (s.p.competence > 0.4f && s.hasBoards && s.homeOpen)
    {
        out[static_cast<int>(Option::Fortify)] = 0.25f + 0.3f * s.p.competence;
    }

    if (s.wounds > 0 && s.hasMedicine)
    {
        out[static_cast<int>(Option::Tend)] = 0.5f + 0.1f * static_cast<float>(std::min(s.wounds, 3));
    }

    if (s.playerWatched)
    {
        out[static_cast<int>(Option::Watch)] = 0.45f;
    }
    return out;
}

Option choose(const Situation& s)
{
    Scores scores = score(s);
    const int current = static_cast<int>(s.current);
    if (current >= 0 && current < kOptionCount && scores[static_cast<std::size_t>(current)] > 0.0f)
    {
        scores[static_cast<std::size_t>(current)] += 0.05f;
    }
    int best = 0;
    for (int i = 1; i < kOptionCount; ++i)
    {
        if (scores[static_cast<std::size_t>(i)] > scores[static_cast<std::size_t>(best)]) best = i;
    }
    return static_cast<Option>(best);
}

// --- Worth and trade --------------------------------------------------------

float worth(float value, Kind kind, const Wants& wants)
{
    float scale = 1.0f;
    if (kind == Kind::Food && wants.hungry) scale = 2.0f;
    if (kind == Kind::Drink && wants.thirsty) scale = 2.0f;
    if (kind == Kind::Medicine && wants.hurt) scale = 3.0f;
    return std::max(0.0f, value) * scale;
}

Verdict verdict(float given, float taken)
{
    if (given <= 0.0f) return Verdict::WantMore;
    if (taken <= 0.0f) return Verdict::Generous;
    const float ratio = given / taken;
    if (ratio < 0.9f) return Verdict::WantMore;
    if (ratio >= 1.5f) return Verdict::Generous;
    return Verdict::Fair;
}

const char* verdictText(Verdict v)
{
    switch (v)
    {
    case Verdict::WantMore: return "They want more";
    case Verdict::Fair: return "Fair";
    case Verdict::Generous: return "Generous";
    }
    return "Fair";
}

float verdictTrust(Verdict v)
{
    switch (v)
    {
    case Verdict::WantMore: return 0.0f;
    case Verdict::Fair: return kTradeFairTrust;
    case Verdict::Generous: return kTradeGenerousTrust;
    }
    return 0.0f;
}

float giftTrust(float w)
{
    return std::max(0.0f, w) * 2.0f;
}

bool willTrade(Attitude a, float trust)
{
    if (a == Attitude::Open) return true;
    if (a == Attitude::Wary) return trust >= kTradeWaryTrust;
    return false;
}

std::vector<int> spareCounts(const std::vector<StockLine>& stock)
{
    std::vector<int> spare(stock.size(), 0);
    float keepHunger = 2.0f * kFoodPerDay;
    float keepThirst = 2.0f * kWaterPerDay;
    // The best weapon stays with them.
    int bestWeapon = -1;
    for (std::size_t i = 0; i < stock.size(); ++i)
    {
        if (stock[i].kind == Kind::Weapon && stock[i].count > 0 &&
            (bestWeapon < 0 || stock[i].weaponScore > stock[static_cast<std::size_t>(bestWeapon)].weaponScore))
        {
            bestWeapon = static_cast<int>(i);
        }
    }
    for (std::size_t i = 0; i < stock.size(); ++i)
    {
        const StockLine& line = stock[i];
        int keep = 0;
        for (int n = 0; n < line.count; ++n)
        {
            bool needed = false;
            if (line.hunger > 0.0f && keepHunger > 0.0f)
            {
                keepHunger -= line.hunger;
                needed = true;
            }
            if (line.thirst > 0.0f && keepThirst > 0.0f)
            {
                keepThirst -= line.thirst;
                needed = true;
            }
            if (needed) ++keep;
        }
        if (static_cast<int>(i) == bestWeapon) keep = std::max(keep, 1);
        spare[i] = std::max(0, line.count - keep);
    }
    return spare;
}

float stockDays(const std::vector<StockLine>& stock)
{
    float hunger = 0.0f;
    float thirst = 0.0f;
    for (const StockLine& line : stock)
    {
        hunger += line.hunger * static_cast<float>(line.count);
        thirst += line.thirst * static_cast<float>(line.count);
    }
    return std::min(hunger / kFoodPerDay, thirst / kWaterPerDay);
}

// --- Lines ------------------------------------------------------------------

std::size_t lineIndex(const std::string& name, int day, std::uint32_t salt, std::size_t size)
{
    if (size == 0) return 0;
    std::uint32_t h = 2166136261u;
    for (char c : name)
    {
        h ^= static_cast<unsigned char>(c);
        h *= 16777619u;
    }
    h = mix32(h ^ mix32(static_cast<std::uint32_t>(day) * 0x9E3779B1u) ^ salt);
    return static_cast<std::size_t>(h % static_cast<std::uint32_t>(size));
}

// --- The town's people ------------------------------------------------------

std::vector<CastMember> castWorld(std::uint32_t seed, bool infectedSociety, const std::vector<HomeChoice>& houses,
                                  const std::vector<HomeChoice>& garages, const std::vector<std::string>& names)
{
    Dice dice(seed);
    std::vector<CastMember> cast;

    // Homes: houses shuffled by the seed, taken in turn.
    std::vector<int> freeHouses;
    for (std::size_t i = 0; i < houses.size(); ++i) freeHouses.push_back(static_cast<int>(i));
    for (std::size_t i = freeHouses.size(); i > 1; --i)
    {
        std::swap(freeHouses[i - 1], freeHouses[static_cast<std::size_t>(dice.below(static_cast<int>(i)))]);
    }
    std::size_t nextHouse = 0;
    const auto takeHouse = [&](bool big, bool awayFromStart = false) -> int
    {
        if (big || awayFromStart)
        {
            for (std::size_t k = nextHouse; k < freeHouses.size(); ++k)
            {
                const HomeChoice& h = houses[static_cast<std::size_t>(freeHouses[k])];
                if ((!big || h.big) && (!awayFromStart || !h.nearStart))
                {
                    std::swap(freeHouses[k], freeHouses[nextHouse]);
                    break;
                }
            }
        }
        if (nextHouse >= freeHouses.size()) return houses.empty() ? -1 : freeHouses[dice.below(static_cast<int>(freeHouses.size()))];
        return freeHouses[nextHouse++];
    };

    // Names: shuffled, never the same twice while the pool lasts.
    std::vector<std::string> pool = names;
    if (pool.empty()) pool.push_back("Someone");
    for (std::size_t i = pool.size(); i > 1; --i)
    {
        std::swap(pool[i - 1], pool[static_cast<std::size_t>(dice.below(static_cast<int>(i)))]);
    }
    std::size_t nextName = 0;
    const auto takeName = [&]() -> std::string
    {
        std::string name = pool[nextName % pool.size()];
        if (nextName >= pool.size()) name += " " + std::to_string(nextName / pool.size() + 1);
        ++nextName;
        return name;
    };

    const auto stockFor = [&](CastMember& m)
    {
        m.stock.emplace_back("canned_food", 2 + dice.below(3));
        m.stock.emplace_back("water_bottle", 2 + dice.below(3));
        if (dice.unit() < 0.5f) m.stock.emplace_back("bandage", 1);
        if (m.p.competence >= 0.6f)
        {
            m.stock.emplace_back("wood_planks", 2);
            m.stock.emplace_back("nails", 6);
        }
    };

    const auto person = [&](Role role, float aggLo, float aggHi, float coopLo, float coopHi, float compLo, float compHi)
    {
        CastMember m;
        m.name = takeName();
        m.role = role;
        m.p.aggression = dice.range(aggLo, aggHi);
        m.p.cooperation = dice.range(coopLo, coopHi);
        m.p.competence = dice.range(compLo, compHi);
        m.outfit = static_cast<int>(cast.size() % 3);
        return m;
    };

    // Six loners (Infected Society: fifteen less the group and pairs).
    const int loners = infectedSociety ? 9 : 6;
    for (int i = 0; i < loners; ++i)
    {
        CastMember m = person(Role::Loner, 0.1f, 0.6f, 0.2f, 0.85f, 0.3f, 0.85f);
        m.home = takeHouse(false);
        stockFor(m);
        if (dice.unit() < 0.4f) m.stock.emplace_back(dice.unit() < 0.5f ? "knife" : "pipe", 1);
        cast.push_back(m);
    }

    // Two pairs: one goes out, the other keeps the house.
    for (int pair = 0; pair < 2; ++pair)
    {
        const int home = takeHouse(false);
        const int first = static_cast<int>(cast.size());
        for (int half = 0; half < 2; ++half)
        {
            CastMember m = person(Role::Pair, 0.1f, 0.5f, 0.5f, 0.9f, 0.3f, 0.8f);
            m.home = home;
            m.partner = half == 0 ? first + 1 : first;
            m.stayer = half == 1;
            stockFor(m);
            cast.push_back(m);
        }
    }

    // The ones at the garage: two, hostile, one with a pistol. Not by the
    // start: a garage away from it, else a house away from it.
    {
        int home = -1;
        std::vector<int> away;
        for (std::size_t i = 0; i < garages.size(); ++i)
        {
            if (!garages[i].nearStart) away.push_back(static_cast<int>(i));
        }
        if (!away.empty())
        {
            home = static_cast<int>(houses.size()) + away[static_cast<std::size_t>(dice.below(static_cast<int>(away.size())))];
        }
        else
        {
            home = takeHouse(false, true);
            if (home >= 0 && houses[static_cast<std::size_t>(home)].nearStart && !garages.empty())
            {
                home = static_cast<int>(houses.size()) + dice.below(static_cast<int>(garages.size()));
            }
        }
        for (int i = 0; i < 2; ++i)
        {
            CastMember m = person(Role::Group, 0.75f, 0.95f, 0.1f, 0.3f, 0.5f, 0.8f);
            m.group = 1;
            m.home = home;
            m.stock.emplace_back("canned_food", 2 + dice.below(2));
            m.stock.emplace_back("water_bottle", 2 + dice.below(2));
            if (i == 0)
            {
                m.stock.emplace_back("pistol", 1);
                m.stock.emplace_back("pistol_ammo", 6);
            }
            else
            {
                m.stock.emplace_back("baseball_bat", 1);
            }
            cast.push_back(m);
        }
    }

    // Infected Society: a group of five who hold a big house together.
    if (infectedSociety)
    {
        const int home = takeHouse(true);
        for (int i = 0; i < 5; ++i)
        {
            CastMember m = person(Role::Group, 0.2f, 0.5f, 0.6f, 0.85f, 0.5f, 0.8f);
            m.group = 2;
            m.home = home;
            m.stayer = i >= 2;
            stockFor(m);
            if (i == 0) m.stock.emplace_back("axe", 1);
            cast.push_back(m);
        }
    }
    return cast;
}

// --- Between lives ----------------------------------------------------------

float dangerOf(float infection)
{
    return 0.5f + 1.5f * std::clamp(infection, 0.0f, 100.0f) / 100.0f;
}

float forageChance(float competence)
{
    return kForageChance + 0.4f * std::clamp(competence, 0.0f, 1.0f);
}

float dailyDeathChance(float competence, float danger)
{
    return kDailyDeathChance * (2.0f - std::clamp(competence, 0.0f, 1.0f)) * std::max(0.0f, danger);
}

BetweenLivesResult advanceBetweenLives(std::vector<LifeSurvivor>& survivors, std::vector<LifeContainer>& containers,
                                       int days, std::uint32_t seed, int firstDay)
{
    BetweenLivesResult result;
    result.eatenFood.assign(survivors.size(), 0);
    result.drunk.assign(survivors.size(), 0);
    std::uint64_t fp = 1469598103934665603ull;
    const auto fold = [&fp](std::uint64_t v)
    {
        fp ^= v;
        fp *= 1099511628211ull;
    };

    for (int d = 0; d < std::max(0, days); ++d)
    {
        const int day = firstDay + d;
        for (std::size_t i = 0; i < survivors.size(); ++i)
        {
            LifeSurvivor& s = survivors[i];
            if (!s.alive) continue;

            // Upkeep: one thing to eat a day, and one to drink while there is
            // any (they have rain and a tap besides: water never kills them).
            // With nothing in the house they forage a little (a garden, the
            // edge of town); some days they find nothing.
            bool ate = false;
            if (s.food > 0)
            {
                --s.food;
                ++result.eatenFood[i];
                ate = true;
            }
            else if (unitHash(seed, static_cast<std::uint32_t>(s.id), static_cast<std::uint32_t>(day), 0xF00Du) <
                     forageChance(s.competence))
            {
                ate = true;
            }
            if (s.drink > 0)
            {
                --s.drink;
                ++result.drunk[i];
            }
            s.hungryDays = ate ? 0 : s.hungryDays + 1;

            // Low: out to the nearest unsearched containers near home.
            if (s.food < static_cast<int>(kLowStockDays))
            {
                const int trips = s.competence > 0.6f ? 2 : 1;
                for (int t = 0; t < trips; ++t)
                {
                    int best = -1;
                    float bestDistance = 0.0f;
                    for (std::size_t c = 0; c < containers.size(); ++c)
                    {
                        const LifeContainer& box = containers[c];
                        if (box.searched || box.protectedHome) continue;
                        if (box.building == s.homeBuilding && s.homeBuilding > 0) continue; // their own
                        const glm::vec2 delta(box.tile - s.home);
                        const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                        if (distance > kScavengeRadiusTiles) continue;
                        if (best < 0 || distance < bestDistance)
                        {
                            best = static_cast<int>(c);
                            bestDistance = distance;
                        }
                    }
                    if (best < 0) break;
                    LifeContainer& box = containers[static_cast<std::size_t>(best)];
                    box.searched = true;
                    s.food += box.food;
                    s.drink += box.drink;
                    box.food = 0;
                    box.drink = 0;
                    box.medicine = 0;
                    result.loots.push_back(LootEvent{static_cast<int>(i), best, day});
                    fold(static_cast<std::uint64_t>(i) * 1000003ull + static_cast<std::uint64_t>(best));
                }
            }

            // Days on end with nothing: they starve. Otherwise the dead may come.
            bool dies = false;
            bool starved = false;
            if (s.hungryDays >= kStarveDays && s.food == 0)
            {
                dies = true;
                starved = true;
            }
            else if (unitHash(seed, static_cast<std::uint32_t>(s.id), static_cast<std::uint32_t>(day), 0xDEADu) <
                     dailyDeathChance(s.competence, s.danger))
            {
                dies = true;
            }
            if (dies)
            {
                s.alive = false;
                s.deathDay = day;
                result.deaths.push_back(DeathEvent{static_cast<int>(i), day, starved});
                fold(0xD00Dull + static_cast<std::uint64_t>(i) * 31ull + static_cast<std::uint64_t>(day));
            }
        }
    }
    for (const LifeSurvivor& s : survivors)
    {
        fold(static_cast<std::uint64_t>(s.food) * 7ull + static_cast<std::uint64_t>(s.drink) * 13ull + (s.alive ? 1ull : 0ull));
    }
    result.fingerprint = fp;
    return result;
}

FarHour farHour(bool day, float stock, bool outToday, float danger, std::uint32_t seed, int survivorId, int absoluteHour)
{
    FarHour out;
    if (!day || outToday || stock >= kLowStockDays) return out;
    out.scavenge = true;
    const auto id = static_cast<std::uint32_t>(survivorId);
    const auto hour = static_cast<std::uint32_t>(absoluteHour);
    out.wound = unitHash(seed, id, hour, 0x3007u) < 0.02f * danger;
    out.dies = unitHash(seed, id, hour, 0x3008u) < 0.005f;
    return out;
}
} // namespace survivorrules
