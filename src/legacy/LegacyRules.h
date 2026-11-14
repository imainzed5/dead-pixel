#pragma once

// Rules for what the world keeps of past characters: who comes back, and how a
// new survivor hears about what was left behind. No engine dependencies, so the
// rules are unit-tested directly.

#include <array>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "components/Skills.h"
#include "components/Traits.h"

#include "save/SaveManager.h"

namespace legacy
{

// A character comes back if the dead took them (killed by zombies) or a bite
// had already turned (infected wound). Starving, thirst, bleeding out from
// other wounds and fire leave a grave.
[[nodiscard]] bool shouldRise(const std::string& cause, bool infectedBite);

// Direction from one world point to another as the player sees it on screen
// (N is up): one of N, NE, E, SE, S, SW, W, NW.
[[nodiscard]] const char* screenDirection(const glm::vec2& fromWorld, const glm::vec2& toWorld);

// Whole metres between two world points; one tile is one metre.
[[nodiscard]] int metresBetween(const glm::vec2& a, const glm::vec2& b, float tileSize);

enum class LeadKind
{
    Risen,    // walks again near where they fell, carrying their pack
    Pack,     // grave with their pack still beside it
    Grave,    // grave, nothing left to take
    Survivor, // retired, stayed on in town as a survivor
    Base      // a house an earlier life held (boarded, or with a station)
};

struct Lead
{
    LeadKind kind = LeadKind::Grave;
    std::string name;
    glm::vec2 world{0.0f};
    int runIndex = 0;
    int graveIndex = -1; // index into the grave records, -1 for survivors
    bool buried = false; // a stone in the cemetery (phase 10)
};

// Everything a new run inherits, most pressing first (risen, packs, graves,
// survivors), most recent run first within each kind.
[[nodiscard]] std::vector<Lead> collectLeads(const std::vector<GraveRecord>& graves,
                                             const std::vector<RetirementRecord>& retirements);

// One line for the run-start card and the notebook, e.g.
// "Hayden Stone walks again - 45 m NE".
[[nodiscard]] std::string describeLead(const Lead& lead, const glm::vec2& playerWorld, float tileSize);

// Headline for the run-start card, e.g. "Run 4 - three came before you".
[[nodiscard]] std::string runHeadline(int runNumber, int previousCharacters);

// --- Phase 10: what a bloodline learns ---------------------------------------

// Everything the bloodline has been through, as the save keeps it.
struct LegacyState
{
    std::vector<GraveRecord> graves;
    std::vector<RetirementRecord> retirements;
    std::vector<CharacterHistoryRecord> history;
};

constexpr int kScavengerDeaths = 3;   // lives ended by hunger or thirst
constexpr int kFirewiseTiles = 30;    // tiles burned in one life
constexpr int kVigilantGraves = 4;
constexpr int kGravediggerRests = 3;  // risen laid to rest
constexpr float kPeacefulDepression = 50.0f; // a chosen retirement below this hardens
constexpr std::size_t kMaxBloodlines = 2;
// What the traits do.
constexpr float kVigilantPanicDecay = 1.15f;  // panic falls 15 % faster
constexpr float kVigilantWakeSeconds = 1.0f;  // woken a second sooner
constexpr float kVigilantWalkerTiles = 1.5f;  // how far one of them comes in that second
constexpr float kScavengerSearch = 0.9f;      // searching takes 10 % less
constexpr float kLaidToRestRelief = 5.0f;     // depression off for laying one of the line to rest
constexpr float kGravediggerRestScale = 2.0f;
constexpr float kGravediggerMarkTiles = 20.0f; // the risen are marked on the map this near

// The bloodline traits earned, most recently earned first, at most two.
[[nodiscard]] std::vector<BloodlineTrait> bloodlineFor(const LegacyState& state);

// Hunger or thirst in a cause of death; fire.
[[nodiscard]] bool hungerDeath(const std::string& cause);
[[nodiscard]] bool fireDeath(const std::string& cause);

// The history record of the life a grave belongs to (nullptr: a companion's,
// or an older save's without one).
[[nodiscard]] const CharacterHistoryRecord* lifeOf(const LegacyState& state, const GraveRecord& grave);
// A grave's journal counts as read once a later life read it, or set its
// marker in the cemetery.
[[nodiscard]] bool journalRead(const LegacyState& state, const GraveRecord& grave);

constexpr int kEchoDivisor = 3;
constexpr int kEchoCap = 2;
// Skill echoes: each skill starts at floor(sum / 3), at most 2, where every
// grave of the bloodline adds its level in that skill, whole if its journal
// was read, half if not.
[[nodiscard]] std::array<int, kSkillCount> skillEchoes(const LegacyState& state);
// "Carpentry runs in the family (1)."
[[nodiscard]] std::string echoLine(const char* skillName, int level);

// --- The cemetery -------------------------------------------------------------

// The tile a stone stands on: burial `order` (0 first), inside the fence.
// Rows two tiles apart, then the gaps between them.
[[nodiscard]] glm::ivec2 cemeterySlot(const glm::ivec2& fenceMin, const glm::ivec2& fenceMax, int order);
[[nodiscard]] int cemeterySlots(const glm::ivec2& fenceMin, const glm::ivec2& fenceMax);
constexpr int kOldStones = 3; // the first slots hold the town's old stones

// --- Staying for good ---------------------------------------------------------

constexpr int kStayFromDay = 10;
constexpr float kStayNeeds = 50.0f;
struct StayCheck
{
    int day = 1;
    bool atBase = false;   // in a house with three ways in closed and a bed
    float hunger = 0.0f;
    float thirst = 0.0f;
    float health = 0.0f;
    bool infected = false; // an infected wound
};
// Why staying is not offered yet (empty: it is).
[[nodiscard]] std::string stayBlocker(const StayCheck& check);

} // namespace legacy
