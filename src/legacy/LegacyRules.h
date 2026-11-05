#pragma once

// Rules for what the world keeps of past characters: who comes back, and how a
// new survivor hears about what was left behind. No engine dependencies, so the
// rules are unit-tested directly.

#include <string>
#include <vector>

#include <glm/vec2.hpp>

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
    Survivor  // retired, stayed on in town as a survivor
};

struct Lead
{
    LeadKind kind = LeadKind::Grave;
    std::string name;
    glm::vec2 world{0.0f};
    int runIndex = 0;
    int graveIndex = -1; // index into the grave records, -1 for survivors
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

} // namespace legacy
