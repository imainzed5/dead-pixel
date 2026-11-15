#include "world/TraitRules.h"

#include <cctype>
#include <vector>

namespace traitrules
{
namespace
{
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

std::uint32_t mix(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
} // namespace

const std::array<PositiveTrait, 10>& allPositive()
{
    static const std::array<PositiveTrait, 10> list = {
        PositiveTrait::Tough, PositiveTrait::Quick, PositiveTrait::LightSleeper, PositiveTrait::Handy,
        PositiveTrait::Brawler, PositiveTrait::Nurse, PositiveTrait::Outdoorsy, PositiveTrait::Steady,
        PositiveTrait::StrongBack, PositiveTrait::NightOwl};
    return list;
}

const std::array<NegativeTrait, 10>& allNegative()
{
    static const std::array<NegativeTrait, 10> list = {
        NegativeTrait::Clumsy, NegativeTrait::Hungry, NegativeTrait::Nervous, NegativeTrait::Frail,
        NegativeTrait::HeavySleeper, NegativeTrait::ThinSkinned, NegativeTrait::Squeamish,
        NegativeTrait::SlowHealer, NegativeTrait::Loner, NegativeTrait::WeakStomach};
    return list;
}

int cost(PositiveTrait trait)
{
    switch (trait)
    {
    case PositiveTrait::Tough: return 3;
    case PositiveTrait::Quick: return 3;
    case PositiveTrait::LightSleeper: return 1;
    case PositiveTrait::Handy: return 2;
    case PositiveTrait::Brawler: return 2;
    case PositiveTrait::Nurse: return 2;
    case PositiveTrait::Outdoorsy: return 2;
    case PositiveTrait::Steady: return 2;
    case PositiveTrait::StrongBack: return 2;
    case PositiveTrait::NightOwl: return 1;
    case PositiveTrait::None: break;
    }
    return 0;
}

int gain(NegativeTrait trait)
{
    switch (trait)
    {
    case NegativeTrait::Clumsy: return 2;
    case NegativeTrait::Hungry: return 2;
    case NegativeTrait::Nervous: return 2;
    case NegativeTrait::Frail: return 3;
    case NegativeTrait::HeavySleeper: return 1;
    case NegativeTrait::ThinSkinned: return 2;
    case NegativeTrait::Squeamish: return 1;
    case NegativeTrait::SlowHealer: return 2;
    case NegativeTrait::Loner: return 1;
    case NegativeTrait::WeakStomach: return 1;
    case NegativeTrait::None: break;
    }
    return 0;
}

const char* effect(PositiveTrait trait)
{
    switch (trait)
    {
    case PositiveTrait::Tough: return "+20 health";
    case PositiveTrait::Quick: return "Moves 15% faster";
    case PositiveTrait::LightSleeper: return "Wakes to trouble further off";
    case PositiveTrait::Handy: return "Carpentry starts at 2";
    case PositiveTrait::Brawler: return "Fighting starts at 2";
    case PositiveTrait::Nurse: return "Medicine starts at 2";
    case PositiveTrait::Outdoorsy: return "Land skill 2; rain chills half";
    case PositiveTrait::Steady: return "Panics 25% slower";
    case PositiveTrait::StrongBack: return "Carries 6 kg more";
    case PositiveTrait::NightOwl: return "Sees further at night, tires slower";
    case PositiveTrait::None: break;
    }
    return "";
}

const char* effect(NegativeTrait trait)
{
    switch (trait)
    {
    case NegativeTrait::Clumsy: return "Footsteps are louder";
    case NegativeTrait::Hungry: return "Gets hungry 50% faster";
    case NegativeTrait::Nervous: return "Panics 30% faster";
    case NegativeTrait::Frail: return "-20 health";
    case NegativeTrait::HeavySleeper: return "Sleeps through more";
    case NegativeTrait::ThinSkinned: return "Cold and heat bite harder";
    case NegativeTrait::Squeamish: return "Each kill shakes them";
    case NegativeTrait::SlowHealer: return "Wounds take 50% longer";
    case NegativeTrait::Loner: return "Company helps half as much";
    case NegativeTrait::WeakStomach: return "Bad food makes them sicker";
    case NegativeTrait::None: break;
    }
    return "";
}

const char* key(PositiveTrait trait)
{
    switch (trait)
    {
    case PositiveTrait::Tough: return "tough";
    case PositiveTrait::Quick: return "quick";
    case PositiveTrait::LightSleeper: return "light_sleeper";
    case PositiveTrait::Handy: return "handy";
    case PositiveTrait::Brawler: return "brawler";
    case PositiveTrait::Nurse: return "nurse";
    case PositiveTrait::Outdoorsy: return "outdoorsy";
    case PositiveTrait::Steady: return "steady";
    case PositiveTrait::StrongBack: return "strong_back";
    case PositiveTrait::NightOwl: return "night_owl";
    case PositiveTrait::None: break;
    }
    return "";
}

const char* key(NegativeTrait trait)
{
    switch (trait)
    {
    case NegativeTrait::Clumsy: return "clumsy";
    case NegativeTrait::Hungry: return "hungry";
    case NegativeTrait::Nervous: return "nervous";
    case NegativeTrait::Frail: return "frail";
    case NegativeTrait::HeavySleeper: return "heavy_sleeper";
    case NegativeTrait::ThinSkinned: return "thin_skinned";
    case NegativeTrait::Squeamish: return "squeamish";
    case NegativeTrait::SlowHealer: return "slow_healer";
    case NegativeTrait::Loner: return "loner";
    case NegativeTrait::WeakStomach: return "weak_stomach";
    case NegativeTrait::None: break;
    }
    return "";
}

std::optional<PositiveTrait> parsePositive(const std::string& text)
{
    const std::string want = normalise(text);
    for (PositiveTrait trait : allPositive())
    {
        if (normalise(key(trait)) == want || normalise(traitName(trait)) == want) return trait;
    }
    return std::nullopt;
}

std::optional<NegativeTrait> parseNegative(const std::string& text)
{
    const std::string want = normalise(text);
    for (NegativeTrait trait : allNegative())
    {
        if (normalise(key(trait)) == want || normalise(traitName(trait)) == want) return trait;
    }
    return std::nullopt;
}

int points(const Traits& traits)
{
    int total = kStartingPoints;
    for (NegativeTrait trait : traits.negatives) total += gain(trait);
    for (PositiveTrait trait : traits.positives) total -= cost(trait);
    return total;
}

bool canBegin(const Traits& traits)
{
    return traits.positives.size() <= Traits::kMaxEach && traits.negatives.size() <= Traits::kMaxEach &&
           points(traits) >= 0;
}

namespace
{
template <typename T>
bool toggleIn(std::vector<T>& list, T trait)
{
    const auto it = std::find(list.begin(), list.end(), trait);
    if (it != list.end())
    {
        list.erase(it);
        return true;
    }
    if (list.size() >= Traits::kMaxEach) return false;
    list.push_back(trait);
    return true;
}
} // namespace

bool toggle(Traits& traits, PositiveTrait trait) { return toggleIn(traits.positives, trait); }
bool toggle(Traits& traits, NegativeTrait trait) { return toggleIn(traits.negatives, trait); }

float maxHealth(const Traits& traits)
{
    float health = kBaseHealth;
    if (traits.has(PositiveTrait::Tough)) health += 20.0f;
    if (traits.has(NegativeTrait::Frail)) health -= 20.0f;
    return health;
}

float carryLimitKg(const Traits& traits, int fitnessLevel)
{
    float limit = kBaseCarryKg + 1.5f * static_cast<float>(std::max(0, fitnessLevel));
    if (traits.has(PositiveTrait::StrongBack)) limit += 6.0f;
    return limit;
}

std::array<int, kSkillCount> startingLevels(const Traits& traits)
{
    std::array<int, kSkillCount> levels{};
    if (traits.has(PositiveTrait::Handy)) levels[static_cast<std::size_t>(Skill::Carpentry)] = 2;
    if (traits.has(PositiveTrait::Brawler)) levels[static_cast<std::size_t>(Skill::Fighting)] = 2;
    if (traits.has(PositiveTrait::Nurse)) levels[static_cast<std::size_t>(Skill::Medicine)] = 2;
    if (traits.has(PositiveTrait::Outdoorsy)) levels[static_cast<std::size_t>(Skill::LivingOffTheLand)] = 2;
    return levels;
}

Traits quickStart(std::uint32_t seed)
{
    Traits traits;
    const std::uint32_t a = mix(seed ^ 0xDEADBEEFu);
    const NegativeTrait bad = allNegative()[a % allNegative().size()];
    traits.negatives.push_back(bad);
    std::vector<PositiveTrait> affordable;
    for (PositiveTrait trait : allPositive())
    {
        if (cost(trait) <= gain(bad)) affordable.push_back(trait);
    }
    const std::uint32_t b = mix(a + 0x9E3779B9u);
    traits.positives.push_back(affordable[b % affordable.size()]);
    return traits;
}

Traits legacyPick(std::uint32_t seed)
{
    const std::uint32_t traitSeed = seed ^ 0xDEADBEEFu;
    Traits traits;
    traits.positives.push_back(static_cast<PositiveTrait>(1 + traitSeed % 3));
    traits.negatives.push_back(static_cast<NegativeTrait>(1 + (traitSeed / 3) % 3));
    return traits;
}

float moveScale(const Traits& traits) { return traits.has(PositiveTrait::Quick) ? 1.15f : 1.0f; }

float panicGainScale(const Traits& traits)
{
    float scale = 1.0f;
    if (traits.has(PositiveTrait::Steady)) scale *= 0.75f;
    if (traits.has(NegativeTrait::Nervous)) scale *= 1.3f;
    return scale;
}

float hungerScale(const Traits& traits) { return traits.has(NegativeTrait::Hungry) ? 1.5f : 1.0f; }

float wakeRangeScale(const Traits& traits)
{
    float scale = 1.0f;
    if (traits.has(PositiveTrait::LightSleeper)) scale *= 1.5f;
    if (traits.has(NegativeTrait::HeavySleeper)) scale *= 0.6f;
    return scale;
}

float sleepDebtGainScale(const Traits& traits) { return traits.has(PositiveTrait::NightOwl) ? 0.9f : 1.0f; }
float darkSightScale(const Traits& traits) { return traits.has(PositiveTrait::NightOwl) ? 1.2f : 1.0f; }
float temperatureBiteScale(const Traits& traits) { return traits.has(NegativeTrait::ThinSkinned) ? 1.3f : 1.0f; }
float rainChillScale(const Traits& traits) { return traits.has(PositiveTrait::Outdoorsy) ? 0.5f : 1.0f; }
float healTimeScale(const Traits& traits) { return traits.has(NegativeTrait::SlowHealer) ? 1.5f : 1.0f; }
float companyScale(const Traits& traits) { return traits.has(NegativeTrait::Loner) ? 0.5f : 1.0f; }
float badFoodIllnessScale(const Traits& traits) { return traits.has(NegativeTrait::WeakStomach) ? 2.0f : 1.0f; }
float killPanic(const Traits& traits) { return traits.has(NegativeTrait::Squeamish) ? 2.0f : 0.0f; }
float desensitisationScale(const Traits& traits) { return traits.has(NegativeTrait::Squeamish) ? 0.5f : 1.0f; }
int footstepTierBonus(const Traits& traits) { return traits.has(NegativeTrait::Clumsy) ? 1 : 0; }
} // namespace traitrules
