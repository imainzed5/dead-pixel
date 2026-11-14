#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"

// The service gate (overhaul phase 9): the one way out of town. Pure rules:
// where the gate complex stands, what opening it takes, the last stand's
// schedule and who did what. The game (GameGate.cpp) holds the state and
// draws it; the save keeps State as world memory (<slot>_gate.json).
namespace gate
{
enum class Side : int
{
    East = 0,  // the wall at x = width - 1
    West = 1,  // x = 0
    South = 2, // y = height - 1
    North = 3  // y = 0
};
// "east", "west", "south", "north".
[[nodiscard]] const char* sideName(int side);

// What opening it takes.
inline constexpr int kFuelNeeded = 4;              // cans poured into the generator
inline constexpr int kRepairSessions = 3;          // held sessions that free it without Mechanics 3
inline constexpr float kRepairSessionSeconds = 60.0f;
inline constexpr int kMechanicsToFree = 3;         // frees it in one session
inline constexpr float kFitFuseSeconds = 10.0f;
inline constexpr float kPourSeconds = 6.0f;
inline constexpr float kReadSeconds = 2.0f;        // a readable clue, quietly

// The last stand.
inline constexpr float kOpenSeconds = 180.0f;      // running time until the gate is open
inline constexpr float kGrindSeconds = 30.0f;      // each restart grinds this long first
inline constexpr float kSecondsPerCan = 60.0f;     // fuel burned while it runs
inline constexpr float kNoiseSeconds = 1.0f;       // Explosive, while it runs
inline constexpr float kWaveSeconds = 20.0f;       // fresh dead at the district's edge
inline constexpr float kStandRadiusTiles = 12.0f;  // farther from the box and the motor stops
inline constexpr int kOverwhelmedInfection = 75;   // the base crowd doubles
inline constexpr int kStandDeadCap = 300;          // never more of the dead than this in town

// Seeing it.
inline constexpr float kSeenTiles = 15.0f;         // close enough to learn where it is

// The Sergeant.
inline constexpr float kSergeantHealth = 260.0f;
inline constexpr int kEscortMin = 4;
inline constexpr int kEscortMax = 6;
inline constexpr int kLoopPoints = 5;

// Where the key is: on the Sergeant until he falls, then loose (held,
// in a pack, a container, on a floor: the items carry it), then turned in
// the box once the gate is open.
enum class KeyPlace : int
{
    Sergeant = 0,
    Loose = 1,
    Turned = 2
};

// Who did what (deed keys: fuse, can, repair, freed, sergeant, started, held, left).
struct Contributor
{
    std::string name;
    std::string deed;
    int run = 0;
};

// Someone who left by the gate: their marker reads "<name> left by this
// gate on day <day>."
struct Leaver
{
    std::string name;
    int day = 1;
    int run = 0;
};

// World memory: what every life has done to the gate.
struct State
{
    bool fuseFitted = false;
    int fuel = 0;            // 0 to kFuelNeeded
    int repairs = 0;         // sessions done
    bool freed = false;      // the generator turns
    KeyPlace key = KeyPlace::Sergeant;
    glm::ivec2 keyTile{-1, -1};      // where the key last lay outdoors (or where he fell)
    bool open = false;       // for ever, once open
    int noteHome = 0;        // the first life's home (The way out): its fridge note stays
    float burned = 0.0f;     // running seconds since the last can burned
    std::vector<Contributor> contributors;
    std::vector<Leaver> leavers;
};

// Fuse fitted, the generator freed and fuel enough to start (`fuelNeeded`:
// a full tank for a first start in a life, any fuel for a restart).
[[nodiscard]] bool ready(const State& state, int fuelNeeded = kFuelNeeded);
// What the box says is missing: "Needs: fuse, fuel 1 of 4, generator." with
// "key" last when it is not in hand; "" when nothing is.
[[nodiscard]] std::string missingText(const State& state, bool keyInHand, int fuelNeeded = kFuelNeeded);

// Records a deed (repeated cans are kept, one entry each).
void credit(State& state, const std::string& name, const std::string& deed, int run);
// The names in the order they first did something.
[[nodiscard]] std::vector<std::string> contributorNames(const State& state);
// One line per name: "Ann Hale: fitted the fuse, poured 2 cans."
[[nodiscard]] std::vector<std::string> contributorLines(const State& state);

// The last stand, as this life has it (not saved across lives: the next
// life finds the gate closed again).
struct Stand
{
    bool running = false;
    bool started = false;    // started at least once this life
    float opened = 0.0f;     // seconds of opening done (0 to kOpenSeconds)
    float grind = 0.0f;      // grinding left before it moves again
    float elapsed = 0.0f;    // running seconds this life (the waves)
    int waves = 0;           // waves sent this life
    float noiseTimer = 0.0f;
};

// The dead that come at once when it starts, from the gate district's
// infection (0 to 100); doubled at kOverwhelmedInfection.
[[nodiscard]] int baseCrowd(int infection);
// The size of wave `wave` (0 first): 3, then 4, then 5...
[[nodiscard]] int waveSize(int wave);
// How far it has opened: 0 closed, 1 and 2 thirds, 3 open.
[[nodiscard]] int stage(float opened);
// Starts or restarts the motor. A restart grinds kGrindSeconds first.
void start(Stand& stand);
// One step of a running motor: it grinds, then opens; it burns fuel and
// stops when the tank is dry or the gate is open. Returns the cans burned.
int run(Stand& stand, State& state, float dtSeconds);
// Waves due by now that have not been sent (and counts them as sent).
[[nodiscard]] int wavesDue(Stand& stand);

// --- Where it stands --------------------------------------------------------

// The district farthest from the player start (where the old campaign
// objective stood).
[[nodiscard]] int farthestDistrict(const LayoutData& layout);
// Stamps the gate, the guard hut and its yard onto the layout at the end of
// the road nearest `district` (0: the farthest district). The hut is
// appended as the last building (kind GuardHut) with its own district.
// Deterministic from the layout. False (and `why`) when no road end has room.
bool stamp(LayoutData& layout, int district, std::string* why = nullptr);
// Inside the yard round the hut (no scenery, no spawns).
[[nodiscard]] bool inYard(const LayoutData::GateSite& site, const glm::ivec2& tile);
// The walkable tile just inside the gate, on the middle of the road (where a
// leaver's marker stands).
[[nodiscard]] glm::ivec2 insideGate(const LayoutData& layout);

// --- The Sergeant -------------------------------------------------------------

// The building (index + 1) whose streets he walks: seeded by the world,
// not the gate's district, not the start's, away from both.
[[nodiscard]] int sergeantBuilding(const LayoutData& layout, std::uint32_t worldSeed);
// Five road tiles round that building, in order round it.
[[nodiscard]] std::vector<glm::ivec2> sergeantLoop(const LayoutData& layout, int building);

// The dead an open gate lets in: extra spawns for `deadNear` already near
// it (population x1.5).
[[nodiscard]] int openGateExtra(int deadNear);
} // namespace gate
