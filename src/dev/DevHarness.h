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
//   campaign on|off               the way out (on) or endless (off) for the next newrun
//   gate                          print the gate's state; gate fuse|freed|key|open|reset, gate fuel N, gate repair N
//   goto gate|box|generator|sergeant|marker|fuse|key | goto clue N|SOURCE|home   beside it; goto through: onto the gate
//   take ITEM NAME                into the pack from the open container (or the nearest that holds it)
//   titlescreen                   back to the title screen
//   learn N|all                   the player knows lead N; clues: print the readables and the leads
//   expect leads N | expect lead N | expect gate open|shut|fuse|freed|running|stage N|fuel N|key sergeant|loose|turned
//   expect objective TEXT | expect sergeant alive|dead | expect readables N | expect state out
//   journal | journal last        print this life's journal / the last life's, as kept (phase 10)
//   age LIVES [DAYS]              the town ages as if LIVES lives of DAYS days ended now (the save; newrun to see it)
//   aging                         print the crates' stages, the boards, district infection against creation, bones
//   goto cemetery | goto stone [NAME]   inside the cemetery's gap / in front of a stone of the line
//   bury [kill] [now]             empty the nearest dead survivor and bury them (30 s; kill: the nearest
//                                 living one dies first; now: at once)
//   world fresh|living            this save's kind: a Fresh Start forgets the town between lives
//   lineage SKILL LEVEL COUNT [read]   COUNT earlier lives who died with SKILL at LEVEL (journals read or not)
//   bloodline                     print the traits earned and the skill echoes
//   pack empty                    nothing in the player's pack
//   expect journal [last] MIN [MAX] | expect crate intact|damaged|breaking|gone | expect boardhp LO HI
//   expect infection above | expect echo SKILL N | expect bloodline NAME|none | expect stone NAME
//   expect graves N | expect remembered nothing|something | expect reader open|closed | expect marker N
//   vehicles                      print every vehicle (phase 11)
//   goto vehicle [KIND] [tailgate]   beside the nearest (sedan|pickup|bicycle), or just behind it
//   set fuel|condition|tyres N | set key in|known|none | set alarm|started on|off   (the one sat in, or the nearest)
//   drive DIR|road SECONDS [CAP] | drive brake   the keys toward n s e w ne nw se sw / along the road (waits)
//   vehicle mark|turn DEGREES|fetch KIND|park door|map R
//   expect vehicle in|out|engine|off|started|cargo|nocargo|window|ringing|silent
//   expect vehicle speed|condition|fuel|tyres above|below N | trunk N | randown|stopped N
//   expect vehicle moved N|scrapes N|lost N|same [worn]   (since vehicle mark)
//   sounds                        print the sounds of the last second, the layers up, voices, duck (phase 12)
//   sound mark | sound KEY [DX DY]   from now (expect sound) / play KEY at the player or DX, DY tiles off
//   fx on|reduced|off             the screen effects (harness runs start off)
//   tone hopeless|grim|brutal | music reset   the tone (music and stings) / forget the pads played
//   expect sound KEY [muffled|far|plain] [N] | expect nosound KEY   (since sound mark; harness runs are silent,
//                                 the mixer still decides what would play)
//   expect layer NAME above|below V | expect music starts N|none | expect duck below V|none
//   expect listening [no] | expect fx neutral|active | expect idle low|cold|knee|none
//   screen W H | uisize auto|1|1.5|2   the window's pixels (and shots) / the interface size (phase 13)
//   settings reload|save|reset|open|close | settings NAME VALUE   the settings file (scripts start from
//                                 the defaults) / master|effects|ambience|music|shake 0-10, teaching|crouch|run|
//                                 pressonce|autopause on|off
//   teach on|off | autosave on|off | demo on|off | daylength 24|36|48   (all off, and 24, for scripts)
//   aim [choose]                  print the standing aim (day 4 on) / choose it again now
//   day horde [next]              the first horde day from 4 (or after today)
//   home sealed                   the home's doors boarded three times, its windows twice
//   push any                      push a piece beside the player whichever way it goes
//   corpses age S | goto clock    every body S seconds old / in front of the town clock (fx on)
//   expect aim KEY|none [no] | expect aimdone [no] | expect binding ACTION KEY | expect teach ID|none
//   expect taught|seen ID [no] | expect step TEXT [done|open] | expect nostep TEXT | expect hint TEXT|none
//   expect retired | expect daycard TEXT | expect hour LO HI | expect uiscale X | expect settings open|closed
//   expect note TEXT | expect level master|effects|ambience|music|shake N
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
        int code = -1; // a key, by binding code
        Uint8 button = 0;
    };

    void execute(Game& game, const Command& command);
    // DevHarnessSurvivors.cpp: true when the command was one of the survivors'.
    bool survivorCommand(Game& game, const Command& command);
    // DevHarnessGate.cpp: true when the command was one of the way out's (phase 9).
    bool gateCommand(Game& game, const Command& command);
    // DevHarnessLegacy.cpp: true when the command was one of legacy's (phase 10).
    bool legacyCommand(Game& game, const Command& command);
    // DevHarnessVehicles.cpp: true when the command was one of the vehicles' (phase 11).
    bool vehicleCommand(Game& game, const Command& command);
    // DevHarnessSound.cpp: true when the command was one of sound and mood's (phase 12).
    bool soundCommand(Game& game, const Command& command);
    // DevHarnessRunFlow.cpp: true when the command was one of the run flow's (phase 13).
    bool runFlowCommand(Game& game, const Command& command);
    double mSoundMark = 0.0;         // "sound mark": the mixer's clock then
    // `drive`: the keys a driver would press, set every tick until it ends.
    void steerVehicle(Game& game);
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
    // Vehicles (phase 11): a `drive` order in progress, and `vehicle mark`.
    struct DriveOrder
    {
        std::uint64_t ticksLeft = 0;
        std::string mode;          // dir, road, brake
        glm::ivec2 axis{0, 0};     // dir: the world direction; road: the axis followed
        glm::ivec2 turnAt{-1, -1}; // road: where to take the next road
        glm::ivec2 turnTo{0, 0};
        float cap = 1.0e9f;        // px/s: above it the keys are let go
        float travelled = 0.0f;
        glm::vec2 last{0.0f};
    } mDrive;
    struct VehicleMark
    {
        bool set = false;
        glm::vec2 centre{0.0f};
        float heading = 0.0f;
        float fuel = 0.0f;
        float condition = 0.0f;
        int trunkItems = 0;
        int scrapes = 0;
    } mVehicleMark;
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
