#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>

#include "ecs/Types.h"
#include "world/SurvivorRules.h"

// What a survivor is doing at the moment (the chosen option, or one of the
// game's overrides: following you, waiting, robbing you, going for you).
enum class SurvivorState
{
    Idle,
    Flee,
    Fight,
    Eat,
    Drink,
    Sleep,
    Scavenge,
    Fortify,
    Tend,
    Watch,
    Follow,  // a companion at your side
    Wait,    // a companion told to wait
    Rob,     // following you to take from you
    Attack,  // going for you
    GoHome,  // walking home (a companion who gave up, a robber done)
    Dead
};

// One of the other people in town (phase 8). The record in the survivors
// file is the lasting part; this is the live part while they walk. Their
// stock is an Inventory on the same entity.
struct SurvivorAI
{
    int id = 0;            // index into Game::mSurvivors
    std::string name;
    SurvivorState state = SurvivorState::Idle;
    survivorrules::Option option = survivorrules::Option::Idle;
    survivorrules::Attitude attitude = survivorrules::Attitude::Wary; // as of the last think
    float desperation = 0.0f;
    survivorrules::Personality p;
    survivorrules::Role role = survivorrules::Role::Loner;
    int outfit = 0;
    int loadout = 0;       // what they hold, for the frames: 0 none, 1 club, 2 blade, 3 gun
    bool buried = false;   // dead and in a grave (no body)
    int group = 0;
    bool stayer = false;   // keeps the house while the other half goes out
    float danger = 1.0f;   // of their district (between lives, far hours)

    int homeBuilding = 0;
    glm::ivec2 homeTile{0};

    // Needs as the player's (100 full), and the body.
    float hunger = 100.0f;
    float thirst = 100.0f;
    float sleepDebt = 0.0f;
    int wounds = 0;
    float bleedSeconds = 0.0f; // bleeding from the last blow
    float hurtFlash = 0.0f;    // the hurt frame after a blow

    bool alive = true;
    bool far = false;      // beyond kNearTiles of the player: an hour at a time
    float farHourClock = 0.0f;
    int outDay = -1;       // the day they last went out for supplies (far)

    // Thinking and walking.
    float thinkTimer = 0.0f;
    float dangerClock = 0.0f;  // the quick look round for the dead
    float actionTimer = 0.0f;  // eating, searching, boarding, tending
    float attackCooldown = 0.0f;
    float swingTimer = 0.0f;   // the swing frames
    bool shooting = false;     // the swing is a shot
    std::vector<glm::ivec2> path;
    std::size_t pathStep = 0;
    glm::ivec2 goal{-1};
    float repathTimer = 0.0f;
    bool noPath = false;            // the last path search found no way
    float stuckTimer = 0.0f;
    float noiseClock = 0.0f;
    Entity target = kInvalidEntity; // the container, the zombie
    glm::ivec2 targetTile{-1};
    glm::ivec2 openedDoor{-1};      // the door they opened last, to shut behind them
    bool carrying = false;          // home with what they found
    std::vector<std::pair<int, int>> carried; // itemId, count, on the way home
    glm::vec2 faceTo{0.0f};         // where they look while standing
    glm::ivec2 lastSearched{-1};    // the container they emptied last (the harness)
    int kills = 0;                  // the dead they put down this life

    // Toward the player.
    float playerDamage = 0.0f;  // the player's blows and shots this tick (CombatSystem)
    float sincePlayerHit = 99.0f; // seconds since the player last hurt them
    float trespassSeconds = 0.0f; // the player in their house since the warning
    std::vector<glm::ivec2> skipped; // containers they could not reach this life
    float trust = 0.0f;
    bool companion = false;
    bool waiting = false;
    bool riding = false;        // phase 11: in the player's vehicle (not saved: out at a load)
    int angryDay = -1;          // hostile for this day (trespass, a blow)
    bool warnedHome = false;    // "Out." said once
    bool warnedWeapon = false;  // "Put it down." said for this drawing
    bool avoiding = false;      // they robbed you and keep away
    bool robbed = false;
    float robSeconds = 0.0f;    // following to rob
    float ignoredHours = 0.0f;  // a companion not spoken to
    float askClock = 0.0f;      // seconds before asking for food again
    float sayTimer = 0.0f;      // a line over their head
    std::string say;
};
