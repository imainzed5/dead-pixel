#pragma once

#include <GL/glew.h>
#include <SDL.h>

#include <glm/vec2.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "dev/Autoplayer.h"
#include "dev/LaunchOptions.h"
#include "ecs/Types.h"
#include "world/SurvivorRules.h"

class Game;

// Scripted autoplay for development: drives input deterministically, one
// simulation tick per loop iteration, captures offscreen screenshots and writes
// metrics. Used to look at the game without a human at the keyboard and as a
// smoke test (script `expect` lines set the exit code).
//
// Script format: one command per line, `#` starts a comment.
//   wait N | waits SECONDS        let N ticks (or seconds) of simulation pass
//   hold KEY TICKS                hold a key (non-blocking; combine for diagonals)
//   tap KEY                       press and release a key
//   key KEY down|up               raw key state
//   mouse X Y                     move the mouse (screen pixels)
//   click left|right              click a mouse button
//   button left|right down|up     raw mouse button state
//   newrun                        start a fresh run from the title screen
//   hours H                       set the time of day (0-24)
//   teleport TX TY                move the player to a tile and snap the camera
//   zoom Z                        set camera zoom
//   god on|off                    keep the player's health topped up
//   give ITEM NAME [COUNT]        add items to the player's inventory
//   set STAT VALUE                hp|stamina|hunger|thirst|sleepdebt|fatigue|temp|illness|panic|depression|wounds
//   trait tough                   give the player the Tough trait (120 max health)
//   tune KEY VALUE                override a tuning value (assets/data/tuning.json) for the run
//   stash ITEM NAME [COUNT]       move items from the pack into the open container
//   open                          open the nearest container in reach
//   container                     print what the open container holds
//   mark | goto mark              remember the player's tile / go back to it
//   weather mark|roll             remember the condition / let the weather roll a new one now
//   bot PROFILE [DAYS]            careful|normal|bold: the bot plays DAYS days (default 7)
//   metrics TICKS                 metrics row every TICKS ticks (0 = off)
//   shot NAME                     save shots/NAME.png
//   expect alive|dead|playing|title
//   expect maxhp VALUE            the player's maximum health
//   expect weather same           the condition is the one `weather mark` saw
//   expect container ITEM NAME COUNT   the open container holds at least COUNT
//   survivor DX DY [open|wary|hostile] [low|empty|hungry|planks|bandages|armed|gun] [trust N]
//                                 a survivor DX, DY tiles off, living in the building there
//   goto survivor [NAME]          beside the survivor (the last made, or by name)
//   goto open N                   open ground outdoors about N tiles away
//   set trust N                   that survivor's trust in you
//   survivors                     print everyone: state, trust, stock, home
//   talk | talk N|WORD            open the talk panel with them / pick a choice
//   trade offer|ask ITEM NAME [COUNT] | trade agree   in the trade panel
//   advance DAYS [apply]          the days between lives (dry run, or for real)
//   survivor house [doors2] [...]  in the nearest free house (doors2: any building, two doors, one to board)
//   survivor dead N [KIND] | survivor fill ITEM NAME [COUNT] | survivor crowd N
//   expect survivor STATE|alive|dead|buried|home|away|shut|boarded [door]|searched|companion
//   expect survivor stock ITEM NAME COUNT | trust above|below N | near N | farther N | kills N
//   waitfor SECONDS survivor CONDITION | waitfor SECONDS held   hold the script until it holds (FAIL after SECONDS)
//   survivors gone                nobody living in town this run (the player and the dead alone)
//   expect talk open|closed|trade, expect card TEXT, expect advance same|loots N
//   log TEXT                      print a line
//   quit                          end the run
class DevHarness
{
public:
    explicit DevHarness(LaunchOptions options);
    ~DevHarness();

    DevHarness(const DevHarness&) = delete;
    DevHarness& operator=(const DevHarness&) = delete;

    bool loadScript(std::string& error);

    void beforeTick(Game& game);
    void beforeTickInner(Game& game);
    void afterTick(Game& game);

    [[nodiscard]] bool finished() const { return mFinished; }
    [[nodiscard]] int exitCode() const { return mFailures > 0 ? 2 : 0; }
    [[nodiscard]] const LaunchOptions& options() const { return mOptions; }

private:
    int fireReach(const Game& game, int side) const;
    struct Command
    {
        int line = 0;
        std::vector<std::string> args;
    };

    struct PendingRelease
    {
        std::uint64_t tick = 0;
        bool isMouse = false;
        SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
        Uint8 button = 0;
    };

    void execute(Game& game, const Command& command);
    // DevHarnessSurvivors.cpp: true when the command was one of the survivors'.
    bool survivorCommand(Game& game, const Command& command);
    Entity survivorFocus(Game& game, const std::string& name);
    // A survivor condition (home, flee, stock ITEM N, ...): true when it holds; `what` says what was seen.
    bool survivorCondition(Game& game, const std::vector<std::string>& condition, std::string& what);
    // `waitfor SECONDS survivor CONDITION`: the script waits until it holds.
    void startWaitFor(Game& game, const Command& command);
    bool waitingFor(Game& game);
    void fail(const Command& command, const std::string& message);
    bool capture(Game& game, const std::string& name);
    void writeMetricsRow(Game& game);
    bool ensureFramebuffer(int width, int height);

    LaunchOptions mOptions;
    std::vector<Command> mCommands;
    std::size_t mPc = 0;
    std::uint64_t mTick = 0;
    std::uint64_t mWaitRemaining = 0;
    std::vector<PendingRelease> mPendingReleases;
    bool mFinished = false;
    int mFailures = 0;

    bool mGodMode = false;
    Autoplayer mBot;
    int mBotShotInterval = 0;
    bool mBotFailReported = false;
    glm::ivec2 mMark{-1, -1};
    glm::vec2 mMarkWorld{0.0f};      // "mark": where the player stood (expect moved)
    int mSimZombies = 0;             // "mark sim": the simulation, for expect sim
    std::uint64_t mSimNoises = 0;
    int mWeatherMark = -1;
    glm::ivec2 mFireOrigin{0, 0};    // "ignite": where the harness lit a fire (phase 7)
    // "fridge mark": a food's condition in the nearest fridge, and when (phase 6).
    float mFridgeCondition = -1.0f;
    double mFridgeHour = 0.0;
    int mFridgeItem = -1;
    glm::ivec2 mFridgeTile{-1, -1};
    // Survivors (phase 8): the one the last `survivor` made (or `goto
    // survivor` found), and the fingerprints `advance` printed.
    int mSurvivorFocus = -1;
    Command mWaitFor;
    std::uint64_t mWaitForLeft = 0;
    std::vector<std::uint64_t> mAdvancePrints;
    survivorrules::BetweenLivesResult mLastAdvance;
    std::uint64_t mNoiseAtLastRow = 0;
    double mClockAtLastRow = -1.0;
    int mMetricsInterval = 0;
    std::ofstream mMetrics;
    bool mWasAlive = true;

    GLuint mFramebuffer = 0;
    // `perf N`: the next ticks are timed.
    int mPerfTicksLeft = 0;
    int mPerfTicks = 0;
    double mPerfTotalMs = 0.0;
    double mPerfWorstMs = 0.0;
    std::chrono::steady_clock::time_point mPerfStart{};
    GLuint mColorTexture = 0;
    int mFramebufferWidth = 0;
    int mFramebufferHeight = 0;
};
