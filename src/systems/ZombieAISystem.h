#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

#include <glm/vec2.hpp>

#include "ai/FlowField.h"
#include "components/Velocity.h"
#include "components/Weather.h"
#include "components/ZombieAI.h"
#include "ecs/SpatialGrid.h"
#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"
#include "world/Senses.h"

class DistrictInfectionSystem;
namespace bloodtrail
{
class Trail;
}

// A zombie pressing into something between it and where it is going: a
// closed door, a window, furniture across a doorway. The game decides what
// the blow does (batter, break glass, climb through).
struct BarrierBlow
{
    glm::ivec2 tile{0};
    Entity zombie = kInvalidEntity;
};

// A blow that reached the player: which way it threw them, and what it
// does beyond the damage (a Brute puts you down, a Crawler hobbles you).
struct PlayerBlow
{
    glm::vec2 direction{0.0f};
    float knockdownSeconds = 0.0f;
    float hobbleSeconds = 0.0f;
    ZombieKind kind = ZombieKind::Shambler; // who struck (for the cause of death)
    bool follower = false;                  // the Stalker's follower
};

// Things the dead did this tick that the game shows or plays.
struct ZombieEvent
{
    enum class Type
    {
        Scream,      // a Screamer began to scream
        Scrape,      // a dormant Crawler is about to move
        StartRun,    // a Runner's head snapped up
        Groan,       // a chasing one groaned
        UnknownSeen  // a hidden rule of The Unknown showed itself (`rule`)
    };
    Type type = Type::Scream;
    Entity zombie = kInvalidEntity;
    glm::vec2 position{0.0f};
    int rule = 0;
};

// What the dead know of the world this tick.
struct ZombieTick
{
    const TileMap* tileMap = nullptr;
    NoiseModel* noise = nullptr;
    Entity player = kInvalidEntity;
    float dt = 0.0f;
    double hours = 12.0;
    int day = 1;
    const DistrictInfectionSystem* infection = nullptr;
    std::vector<BarrierBlow>* blows = nullptr;
    std::vector<PlayerBlow>* playerBlows = nullptr;
    std::vector<ZombieEvent>* events = nullptr;

    // How the player shows.
    bool playerLit = false;
    bool playerCrouched = false;
    bool playerMoving = false;
    bool playerDown = false; // knocked down: anything that reaches them holds on

    // The weather, as the rules read it (world/WeatherRules).
    bool raining = false;        // rain, heavy rain or a storm
    float hearingScale = 1.0f;   // how far noises carry
    float visibility = 1.0f;     // the dead's sight
    float weatherSpeed = 1.0f;   // a blizzard slows them
    float nightSpeed = 1.0f;     // summer nights quicken them
    float snowCover = 0.0f;      // snow on the ground outdoors slows them
    glm::vec2 windTo{0.0f};
    float windStrength = 0.0f;

    // The threat mode.
    float speedScale = 1.0f;
    float giveUpSeconds = 90.0f;
    bool awarenessDrains = true;
    float clumpTiles = 6.0f;
    // The Unknown's rules in play.
    bool dormantByDay = false;
    bool blindHearing = false;
    bool rainCalm = false;
    bool frenzyAfterRain = false; // rain has just stopped
    bool ignoreStill = false;

    const bloodtrail::Trail* trail = nullptr;
    // The building a tile lies in and whether its shell is shut.
    std::function<senses::Shell(const glm::ivec2&)> shellAt;
};

class ZombieAISystem
{
public:
    void update(World& world, const ZombieTick& tick);

    // Send a zombie toward a world position (used by the threat director).
    // Long trips follow an A* path; `migrating` zombies shamble rather than rush.
    static void sendTo(ZombieAI& zombie, const glm::vec2& targetWorld, bool migrating);

    // Uniform [0, 1) from the zombie's own RNG stream.
    [[nodiscard]] static float nextRandom(ZombieAI& zombie, Entity entity);

    // Tiles one can see through a window (either way).
    static constexpr float kWindowSightTiles = 6.0f;
    [[nodiscard]] static bool hasLineOfSight(const TileMap& tileMap, const glm::vec2& worldStart, const glm::vec2& worldEnd);

    // The lunge: a wind-up the player can read, then a strike that throws the
    // body forward. The animation follows both.
    static constexpr float kZombieAttackWindupSeconds = 0.55f;
    static constexpr float kZombieStrikeSeconds = 0.36f;
    static constexpr float kZombieStrikeSpeed = 80.0f;

    // Performance (phase 4): the shared flow field is rebuilt this often; at
    // most this many A* paths are found a tick (the rest wait their turn);
    // the far dead think at 6 Hz.
    static constexpr int kFlowEveryTicks = 15;
    static constexpr int kPathsPerTick = 6;
    static constexpr float kSleepTiles = 45.0f;
    static constexpr int kSleepEveryTicks = 10;

    // Zombies, survivors and bodies by position, rebuilt each tick.
    [[nodiscard]] const SpatialGrid& grid() const { return mGrid; }
    [[nodiscard]] const FlowField& flowField() const { return mFlow; }
    [[nodiscard]] std::uint64_t ticks() const { return mTick; }
    void reset();

private:
    static constexpr float kZombieAttackRange = 28.0f;
    static constexpr float kZombieAttackCooldownSeconds = 1.5f;
    static constexpr float kZombieAttackReachScale = 1.25f; // reach at resolve, relative to attack range
    static constexpr float kZombieWoundChance = 0.35f;     // per landed hit
    static constexpr float kZombieBiteShare = 0.3f;         // of wounds
    static constexpr float kBiteContaminationChance = 0.5f;
    static constexpr float kChaseRepathSeconds = 1.0f;

    // Wandering
    static constexpr float kWanderSpeedFactor = 0.45f;
    static constexpr float kMigrateSpeedFactor = 0.6f;
    static constexpr float kBoredSeconds = 30.0f;
    static constexpr float kInvestigateSpeedFactor = 0.8f;
    static constexpr float kWanderStuckSeconds = 4.0f;

    // Investigate targets farther than this (tiles) use A* instead of a straight line.
    static constexpr float kPathfindDistanceTiles = 5.0f;

    // Zombies closer than this (world px) push apart.
    static constexpr float kSeparationRadius = 14.0f;
    static constexpr float kPlayerBodyRadius = 13.0f;

    // What one zombie works with this tick (ZombieSenses/Move/Attack.cpp).
    struct Ctx
    {
        World* world = nullptr;
        const ZombieTick* tick = nullptr;
        float tileSize = 32.0f;
        bool playerValid = false;
        glm::vec2 playerCenter{0.0f};
        glm::vec2 playerFacing{0.0f};
        bool playerExhausted = false;
        bool playerImmune = false;
        bool playerHeld = false;
        bool night = false;
    };
    struct One
    {
        Entity entity = kInvalidEntity;
        ZombieAI* ai = nullptr;
        glm::vec2 center{0.0f};
        glm::ivec2 tile{0};
        float speedMultiplier = 1.0f;
        float damageMultiplier = 1.0f;
        float visionMultiplier = 1.0f;
        float dt = 0.0f;
        bool sleeping = false;
    };

    // ZombieSenses.cpp
    void sense(const Ctx& c, One& z);
    void see(const Ctx& c, One& z);
    void hear(const Ctx& c, One& z);
    void smell(const Ctx& c, One& z);
    // ZombieMove.cpp
    void wander(const Ctx& c, One& z, Velocity& v);
    void investigate(const Ctx& c, One& z, Velocity& v);
    void chase(const Ctx& c, One& z, Velocity& v);
    void keepAway(const Ctx& c, One& z, Velocity& v);
    void separate(const Ctx& c, One& z, Velocity& v);
    bool followPath(const Ctx& c, One& z, Velocity& v, const glm::vec2& goal, float speed);
    void requestPath(Entity entity, ZombieAI& ai);
    void runPathQueue(World& world, const TileMap& tileMap);
    // ZombieAttack.cpp
    void attack(const Ctx& c, One& z, Velocity& v);
    void hold(const Ctx& c, One& z, Velocity& v);
    [[nodiscard]] bool backCovered(const Ctx& c) const;

    [[nodiscard]] static glm::vec2 tileCenterWorld(const TileMap& tileMap, const glm::ivec2& tile);

    SpatialGrid mGrid;
    FlowField mFlow;
    std::deque<Entity> mPathQueue;
    std::uint64_t mTick = 0;
};
