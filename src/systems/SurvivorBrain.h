#pragma once

#include <cstdint>

#include <glm/vec2.hpp>

#include "components/SurvivorAI.h"
#include "ecs/SpatialGrid.h"
#include "ecs/World.h"
#include "map/TileMap.h"
#include "noise/NoiseModel.h"

// What the game does for a survivor (core/GameSurvivors.cpp): the brain
// decides and walks; doors, containers, boards, blows and stock are the
// game's.
class SurvivorHooksBase
{
public:
    virtual ~SurvivorHooksBase() = default;

    // Stock as the rules read it.
    struct Stock
    {
        float days = 0.0f;
        bool food = false;
        bool drink = false;
        bool medicine = false;
        bool boards = false; // a plank and nails
        bool weapon = false;
        bool gun = false;    // a pistol with rounds
    };
    virtual Stock stock(Entity survivor) = 0;
    // Eat or drink one thing from stock (or the pack of a companion). True if there was one.
    virtual bool consume(Entity survivor, bool drink) = 0;
    // A bandage on the worst wound. True if there was one.
    virtual bool tend(Entity survivor) = 0;

    // The nearest unsearched container within reach of home, not in anyone's
    // house; kInvalidEntity when none. `tile` gets where to stand.
    virtual Entity findContainer(Entity survivor, glm::ivec2& standAt) = 0;
    // Takes food, water, medicine and a better weapon from it into stock.
    virtual void search(Entity survivor, Entity container) = 0;
    // Home with what was found.
    virtual void store(Entity survivor) = 0;
    // A door of home without boards, and the floor inside it to stand on;
    // false when every door is boarded.
    virtual bool openWayIn(int building, glm::ivec2& door, glm::ivec2& standAt) = 0;
    virtual void board(Entity survivor, const glm::ivec2& door) = 0;
    // Shut every door of home.
    virtual void shutHome(int building) = 0;
    // Where to lie down at home.
    virtual glm::ivec2 bedTile(int building, const glm::ivec2& near) = 0;
    // A random floor tile of home.
    virtual glm::ivec2 roomTile(int building, std::uint32_t roll) = 0;

    // A closed door on the way: open it if they can. False when it is boarded or locked.
    virtual bool openDoor(Entity survivor, const glm::ivec2& door) = 0;
    virtual void closeDoor(const glm::ivec2& door) = 0;
    virtual bool tileBurning(const glm::ivec2& tile) = 0;

    // Blows and shots: on the dead, or on the player.
    virtual void blowAt(Entity survivor, Entity target) = 0;
    virtual void shootAt(Entity survivor, Entity target) = 0;
    // A blood drop where a bleeding survivor stands.
    virtual void bleed(Entity survivor, const glm::vec2& where) = 0;
    // A far-off survivor's hour out: empties one container near home.
    virtual void farScavenge(Entity survivor) = 0;
};

// What one tick knows.
struct SurvivorTick
{
    const TileMap* map = nullptr;
    NoiseModel* noise = nullptr;
    const SpatialGrid* grid = nullptr; // the dead, by position (ZombieAISystem)
    float dt = 0.0f;
    int day = 1;
    double hours = 12.0;
    bool night = false;
    Entity player = kInvalidEntity;
    glm::vec2 playerCentre{0.0f};
    bool playerValid = false;
    bool playerCrouched = false;
    bool playerAsleep = false;
    std::uint32_t seed = 0;
};

// The utility AI of the other people in town (overhaul phase 8). Near the
// player (60 tiles) they think every two seconds (world/SurvivorRules
// scores the options) and walk; further off they live an hour at a time.
// The game's overrides (following you, waiting, robbing you, going for you,
// going home) are states it sets; the brain walks them.
class SurvivorBrain
{
public:
    void update(World& world, const SurvivorTick& tick, SurvivorHooksBase& hooks);

    static constexpr float kWalkSpeed = 88.0f;
    static constexpr float kRunSpeed = 150.0f;
    static constexpr float kReach = 30.0f;     // a blow lands within this (px)
    static constexpr float kGunTiles = 7.0f;   // a gun is used within this
    static constexpr float kSearchSeconds = 4.0f;
    static constexpr float kBoardSeconds = 5.0f;
    static constexpr float kEatSeconds = 2.0f;
    static constexpr float kMendPerHour = 1.0f;       // health a game hour, fed and awake
    static constexpr float kMendAsleepPerHour = 3.0f; // asleep (the player's rate)

    // Walk toward `goal` (a tile), finding a path as needed. True on arrival.
    static bool walkTo(SurvivorAI& ai, const glm::vec2& centre, glm::vec2& velocity, const glm::ivec2& goal,
                       float speed, const SurvivorTick& tick, SurvivorHooksBase& hooks, Entity self);
    [[nodiscard]] static bool isOverride(SurvivorState state);

private:
    void near(World& world, Entity e, SurvivorAI& ai, const SurvivorTick& tick, SurvivorHooksBase& hooks);
    void far(World& world, Entity e, SurvivorAI& ai, const SurvivorTick& tick, SurvivorHooksBase& hooks);
    void think(World& world, Entity e, SurvivorAI& ai, const glm::vec2& centre, const SurvivorTick& tick,
               SurvivorHooksBase& hooks);
    void act(World& world, Entity e, SurvivorAI& ai, const glm::vec2& centre, glm::vec2& velocity,
             const SurvivorTick& tick, SurvivorHooksBase& hooks);
    // The nearest of the dead within `tiles` (and how many there are).
    static Entity nearestDead(const SurvivorTick& tick, const glm::vec2& centre, float tiles, int* count,
                              float* distanceTiles);
    void fight(World& world, Entity e, SurvivorAI& ai, const glm::vec2& centre, glm::vec2& velocity,
               Entity target, const SurvivorTick& tick, SurvivorHooksBase& hooks);
};
