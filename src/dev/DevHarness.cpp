#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "components/Animation.h"
#include "components/Combat.h"
#include "components/Fire.h"
#include "components/Corpse.h"
#include "components/Structure.h"
#include "components/Health.h"
#include "components/Interactable.h"
#include "components/Velocity.h"
#include "components/Inventory.h"
#include "components/LegacyLink.h"
#include "components/MentalState.h"
#include "components/Needs.h"
#include "components/Stamina.h"
#include "components/Traits.h"
#include "components/Wounds.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "core/Game.h"
#include "core/GameShared.h"
#include "core/Tuning.h"
#include "world/Storage.h"
#include "world/CombatRules.h"
#include "world/FurnitureUse.h"
#include "systems/BuildSystem.h"
#include "systems/InventorySystem.h"
#include "systems/ZombieAISystem.h"
#include "world/EntityFactory.h"

namespace
{
constexpr double kTicksPerSecond = 60.0;
constexpr float kSpriteSizeForHarness = 32.0f; // matches Game's entity sprite size
constexpr float kNearZombieRadiusTiles = 10.0f;

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

SDL_Scancode scancodeFromName(const std::string& rawName)
{
    const std::string name = lower(rawName);
    if (name == "enter") return SDL_SCANCODE_RETURN;
    if (name == "esc") return SDL_SCANCODE_ESCAPE;
    if (name == "lshift" || name == "shift") return SDL_SCANCODE_LSHIFT;
    if (name == "lctrl" || name == "ctrl") return SDL_SCANCODE_LCTRL;

    std::string sdlName = rawName;
    std::replace(sdlName.begin(), sdlName.end(), '_', ' ');
    return SDL_GetScancodeFromName(sdlName.c_str());
}

Uint8 mouseButtonFromName(const std::string& name)
{
    const std::string n = lower(name);
    if (n == "left") return SDL_BUTTON_LEFT;
    if (n == "right") return SDL_BUTTON_RIGHT;
    if (n == "middle") return SDL_BUTTON_MIDDLE;
    return 0;
}

// "NAME WORDS [COUNT]" from args[first..]: the name, and the count (default 1).
std::string itemArgs(const std::vector<std::string>& a, std::size_t first, int& count)
{
    std::size_t nameEnd = a.size();
    count = 1;
    if (a.size() > first + 1 &&
        std::all_of(a.back().begin(), a.back().end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
    {
        count = std::stoi(a.back());
        nameEnd = a.size() - 1;
    }
    std::string name;
    for (std::size_t i = first; i < nameEnd; ++i)
    {
        if (!name.empty()) name += ' ';
        name += a[i];
    }
    return name;
}

// A compass word as a tile step: n/s/e/w (north is -y).
bool parseDirection(const std::string& word, glm::ivec2& dir)
{
    if (word == "n" || word == "north" || word == "up") dir = {0, -1};
    else if (word == "s" || word == "south" || word == "down") dir = {0, 1};
    else if (word == "e" || word == "east" || word == "right") dir = {1, 0};
    else if (word == "w" || word == "west" || word == "left") dir = {-1, 0};
    else return false;
    return true;
}

// A station by a word of its name: campfire, workbench, crate, collector, bedroll.
const StationRecipe* recipeByWord(const std::string& word)
{
    for (const StationRecipe& recipe : BuildSystem::kRecipes)
    {
        std::string name = recipe.name;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name.find(word) != std::string::npos) return &recipe;
    }
    return nullptr;
}

std::string csvSafe(std::string text)
{
    std::replace(text.begin(), text.end(), ',', ';');
    std::replace(text.begin(), text.end(), '\n', ' ');
    return text;
}

const char* runStateName(int state)
{
    switch (state)
    {
    case 0: return "title";
    case 1: return "playing";
    case 2: return "paused";
    case 3: return "dead";
    case 4: return "retired";
    default: return "unknown";
    }
}
} // namespace

DevHarness::DevHarness(LaunchOptions options)
    : mOptions(std::move(options))
{
}

DevHarness::~DevHarness()
{
    if (mColorTexture != 0)
    {
        glDeleteTextures(1, &mColorTexture);
    }
    if (mFramebuffer != 0)
    {
        glDeleteFramebuffers(1, &mFramebuffer);
    }
}

bool DevHarness::loadScript(std::string& error)
{
    std::ifstream file(mOptions.scriptPath);
    if (!file)
    {
        error = "cannot open script: " + mOptions.scriptPath;
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line))
    {
        ++lineNumber;
        if (const auto hash = line.find('#'); hash != std::string::npos)
        {
            line.erase(hash);
        }

        std::istringstream stream(line);
        Command command;
        command.line = lineNumber;
        std::string token;
        while (stream >> token)
        {
            command.args.push_back(token);
        }
        if (!command.args.empty())
        {
            command.args[0] = lower(command.args[0]);
            mCommands.push_back(std::move(command));
        }
    }

    if (!mOptions.metricsPath.empty())
    {
        mMetrics.open(mOptions.metricsPath, std::ios::trunc);
        if (!mMetrics)
        {
            error = "cannot open metrics file: " + mOptions.metricsPath;
            return false;
        }
        mMetrics << "tick,seconds,day,hour,state,hp,hp_max,hunger,thirst,fatigue,sleep_debt,"
                    "body_temp,illness,stamina,panic,depression,zombies,zombies_near,kills,"
                    "inventory_items,z_investigate,z_chase,z_attack,z_migrating,indoors,pop_target,"
                    "cause,food,drink,medical,weapons,materials,containers_searched,boards_up,"
                    "callers_tonight,noise_per_hour,bot,at_home\n";
    }

    std::error_code ec;
    std::filesystem::create_directories(mOptions.shotDir, ec);
    return true;
}

void DevHarness::fail(const Command& command, const std::string& message)
{
    ++mFailures;
    std::cerr << "[harness] FAIL line " << command.line << ": " << message << '\n';
}

void DevHarness::beforeTick(Game& game)
{
    // Release holds whose time is up.
    for (auto it = mPendingReleases.begin(); it != mPendingReleases.end();)
    {
        if (it->tick <= mTick)
        {
            if (it->isMouse)
            {
                game.mInput.setMouseButtonState(it->button, false);
            }
            else
            {
                game.mInput.setKeyState(it->key, false);
            }
            it = mPendingReleases.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // The bot plays until its days are up or the run ends; the script waits.
    if (mBot.running())
    {
        mBot.tick(game);
        if (!mBot.stuckMessage().empty() && !mBotFailReported)
        {
            mBotFailReported = true;
            ++mFailures;
            std::cerr << "[harness] FAIL " << mBot.stuckMessage() << '\n';
        }
        if (mBot.running())
        {
            if (mBotShotInterval > 0 && mTick % static_cast<std::uint64_t>(mBotShotInterval) == 0)
            {
                capture(game, "bot_" + std::to_string(mTick));
            }
            return;
        }
        std::cout << "[harness] tick " << mTick << ": bot stopped on day " << game.mCurrentDay << " hour "
                  << static_cast<int>(game.mGameHours) << " (" << runStateName(static_cast<int>(game.mRunState))
                  << "), containers searched " << mBot.containersSearched() << '\n';
    }

    while (mWaitRemaining == 0 && mPc < mCommands.size() && !mFinished && !mBot.running())
    {
        execute(game, mCommands[mPc++]);
    }

    if (mWaitRemaining > 0)
    {
        --mWaitRemaining;
    }
    else if (mPc >= mCommands.size() && !mBot.running())
    {
        mFinished = true;
    }
}

void DevHarness::afterTick(Game& game)
{
    ++mTick;

    const bool alive = game.mRunState == Game::RunState::Playing || game.mRunState == Game::RunState::Paused;

    if (mGodMode && game.mPlayerEntity != kInvalidEntity && game.mWorld.hasComponent<Health>(game.mPlayerEntity))
    {
        Health& health = game.mWorld.getComponent<Health>(game.mPlayerEntity);
        health.current = health.maximum;
    }

    if (mMetrics && mMetricsInterval > 0 && (mTick % static_cast<std::uint64_t>(mMetricsInterval) == 0 || (mWasAlive && !alive)))
    {
        writeMetricsRow(game);
    }
    if (mWasAlive && !alive && game.mRunState != Game::RunState::TitleScreen)
    {
        std::cout << "[harness] tick " << mTick << ": run ended (" << runStateName(static_cast<int>(game.mRunState))
                  << ") cause=\"" << game.mDeathCause << "\"\n";
    }
    mWasAlive = alive;

    if (mOptions.maxTicks > 0 && mTick >= static_cast<std::uint64_t>(mOptions.maxTicks))
    {
        std::cout << "[harness] max ticks reached\n";
        mFinished = true;
    }
}

void DevHarness::execute(Game& game, const Command& command)
{
    const auto& a = command.args;
    const std::string& op = a[0];
    auto need = [&](std::size_t count) -> bool
    {
        if (a.size() < count + 1)
        {
            fail(command, "'" + op + "' needs " + std::to_string(count) + " argument(s)");
            return false;
        }
        return true;
    };

    if (op == "wait")
    {
        if (need(1)) mWaitRemaining = static_cast<std::uint64_t>(std::max(0, std::stoi(a[1])));
    }
    else if (op == "waits")
    {
        if (need(1)) mWaitRemaining = static_cast<std::uint64_t>(std::max(0.0, std::stod(a[1]) * kTicksPerSecond));
    }
    else if (op == "hold" || op == "tap")
    {
        if (!need(1)) return;
        const SDL_Scancode key = scancodeFromName(a[1]);
        if (key == SDL_SCANCODE_UNKNOWN)
        {
            fail(command, "unknown key '" + a[1] + "'");
            return;
        }
        const int ticks = op == "tap" ? 1 : (a.size() > 2 ? std::max(1, std::stoi(a[2])) : 1);
        game.mInput.setKeyState(key, true);
        mPendingReleases.push_back({mTick + static_cast<std::uint64_t>(ticks), false, key, 0});
    }
    else if (op == "key")
    {
        if (!need(2)) return;
        const SDL_Scancode key = scancodeFromName(a[1]);
        if (key == SDL_SCANCODE_UNKNOWN)
        {
            fail(command, "unknown key '" + a[1] + "'");
            return;
        }
        game.mInput.setKeyState(key, lower(a[2]) == "down");
    }
    else if (op == "mouse")
    {
        if (need(2)) game.mInput.setMousePosition(std::stoi(a[1]), std::stoi(a[2]));
    }
    else if (op == "click" || op == "button")
    {
        if (!need(1)) return;
        const Uint8 button = mouseButtonFromName(a[1]);
        if (button == 0)
        {
            fail(command, "unknown mouse button '" + a[1] + "'");
            return;
        }
        if (op == "click")
        {
            game.mInput.setMouseButtonState(button, true);
            mPendingReleases.push_back({mTick + 1, true, SDL_SCANCODE_UNKNOWN, button});
        }
        else if (need(2))
        {
            game.mInput.setMouseButtonState(button, lower(a[2]) == "down");
        }
    }
    else if (op == "newrun")
    {
        game.mSaveManager.deleteCheckpoint();
        game.mIgnoreCheckpointRestoreOnce = true;
        game.syncPresentationSettingsToSave();
        game.startNewRun();
        mWasAlive = true;
    }
    else if (op == "save")
    {
        // save: write a mid-run checkpoint, as the pause menu does.
        if (!game.saveCurrentRunCheckpoint("Run checkpoint saved."))
        {
            fail(command, "checkpoint not saved");
        }
    }
    else if (op == "resume")
    {
        // resume: rebuild the run from its checkpoint, as Continue Run does.
        game.mIgnoreCheckpointRestoreOnce = false;
        game.startNewRun();
        mWasAlive = true;
    }
    else if (op == "zombie")
    {
        // zombie DX DY: a zombie DX, DY tiles from the player, heading for them.
        // zombie mark [idle]: a zombie on the marked tile. With "idle" it
        // wanders instead of heading for the player.
        if (!need(1)) return;
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
        {
            fail(command, "no player");
            return;
        }
        const int tileW = std::max(1, game.mTileMap.tileWidth());
        const int tileH = std::max(1, game.mTileMap.tileHeight());
        const Transform& pt = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        const glm::vec2 playerCentre(pt.x + 16.0f, pt.y + 16.0f);
        const bool atMark = lower(a[1]) == "mark";
        if (atMark && mMark.x < 0)
        {
            fail(command, "no mark");
            return;
        }
        if (!atMark && !need(2)) return;
        // Options after the place: idle, down (on the floor), lunging (its
        // lunge already winding up), away (its back to the player, reeling
        // so it cannot turn), close (at your feet, 20 px from you toward the
        // place, for a stomp's 30 px), exact (DX DY may be fractions: the
        // body's centre goes there, not to a tile's), hp N (below 60 it is
        // hurt: 60 stays its maximum).
        bool idle = false;
        bool down = false; // already on the floor
        bool lunging = false;
        bool away = false;
        bool close = false;
        bool exact = false;
        float hp = 60.0f;
        for (std::size_t k = atMark ? 2 : 3; k < a.size(); ++k)
        {
            const std::string opt = lower(a[k]);
            if (opt == "idle") idle = true;
            else if (opt == "down") down = idle = true;
            else if (opt == "lunging") lunging = idle = true;
            else if (opt == "close") close = true;
            else if (opt == "exact") exact = !atMark;
            else if (opt == "away") away = idle = true;
            else if (opt == "hp" && k + 1 < a.size()) hp = std::stof(a[++k]);
        }
        const int tx = atMark ? mMark.x : static_cast<int>(playerCentre.x / static_cast<float>(tileW)) + std::stoi(a[1]);
        const int ty = atMark ? mMark.y : static_cast<int>(playerCentre.y / static_cast<float>(tileH)) + std::stoi(a[2]);
        if (game.mTileMap.isSolid(tx, ty))
        {
            fail(command, "tile " + std::to_string(tx) + "," + std::to_string(ty) + " is solid");
            return;
        }
        EntityFactory::ZombieParams params;
        params.topLeftWorld = glm::vec2(static_cast<float>(tx * tileW), static_cast<float>(ty * tileH));
        if (exact)
        {
            params.topLeftWorld = playerCentre + glm::vec2(std::stof(a[1]) * static_cast<float>(tileW),
                                                           std::stof(a[2]) * static_cast<float>(tileH)) - glm::vec2(16.0f);
        }
        if (close)
        {
            const glm::vec2 toward = glm::vec2(params.topLeftWorld) + glm::vec2(16.0f) - playerCentre;
            const glm::vec2 dir = glm::length(toward) > 0.01f ? glm::normalize(toward) : glm::vec2(1.0f, 0.0f);
            params.topLeftWorld = playerCentre + dir * 20.0f - glm::vec2(16.0f);
        }
        params.health = hp;
        params.baseMoveSpeed = 112.0f;
        params.spawnDay = game.mCurrentDay;
        const Entity zombie = EntityFactory::spawnZombie(game.mWorld, game.mSpriteSheet, params);
        if (down && zombie != kInvalidEntity && game.mWorld.hasComponent<ZombieAI>(zombie))
        {
            ZombieBlow::knockDown(game.mWorld.getComponent<ZombieAI>(zombie), combatrules::kDownSeconds);
        }
        if (lunging && zombie != kInvalidEntity && game.mWorld.hasComponent<ZombieAI>(zombie))
        {
            ZombieAI& ai = game.mWorld.getComponent<ZombieAI>(zombie);
            ai.state = ZombieState::Attack;
            ai.attackWindup = ZombieAISystem::kZombieAttackWindupSeconds;
        }
        if (away && zombie != kInvalidEntity && game.mWorld.hasComponent<Animation>(zombie) &&
            game.mWorld.hasComponent<ZombieAI>(zombie))
        {
            // The dead see all round and turn at once, so a back can only be
            // staged: drawn facing away, and reeling so it does not turn.
            const glm::vec2 off = params.topLeftWorld + glm::vec2(16.0f) - playerCentre;
            const glm::vec2 want = glm::length(off) > 0.01f ? glm::normalize(off) : glm::vec2(1.0f, 0.0f);
            int best = 0;
            for (int d = 1; d < 8; ++d)
            {
                if (glm::dot(combatrules::dirToWorld(d), want) > glm::dot(combatrules::dirToWorld(best), want)) best = d;
            }
            game.mWorld.getComponent<Animation>(zombie).dir = best;
            game.mWorld.getComponent<ZombieAI>(zombie).stagger = 3.0f;
        }
        if (zombie != kInvalidEntity && game.mWorld.hasComponent<Health>(zombie) && hp < 60.0f)
        {
            // Hurt: below the usual 60, so a shove can put it over.
            game.mWorld.getComponent<Health>(zombie).maximum = 60.0f;
        }
        if (!idle && zombie != kInvalidEntity && game.mWorld.hasComponent<ZombieAI>(zombie))
        {
            ZombieAISystem::sendTo(game.mWorld.getComponent<ZombieAI>(zombie), playerCentre, false);
        }
    }
    else if (op == "shove")
    {
        // shove: Space for one tick (a shove, or a struggle tap when held).
        game.mInput.setKeyState(SDL_SCANCODE_SPACE, true);
        mPendingReleases.push_back({mTick + 1, false, SDL_SCANCODE_SPACE, 0});
    }
    else if (op == "select")
    {
        // select ITEM NAME: make the pack slot holding it the active one, and
        // take it in hand if it is a weapon.
        if (!need(1)) return;
        std::string name;
        for (std::size_t i = 1; i < a.size(); ++i) name += (name.empty() ? "" : " ") + a[i];
        const ItemDef* item = game.mItemDatabase.findByName(name);
        if (item == nullptr || game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
        {
            fail(command, "cannot select '" + name + "'");
            return;
        }
        Inventory& inv = game.mWorld.getComponent<Inventory>(game.mPlayerEntity);
        int slot = -1;
        for (int i = 0; i < Inventory::kMaxSlots && slot < 0; ++i)
        {
            if (inv.slots[static_cast<std::size_t>(i)].itemId == item->id && inv.slots[static_cast<std::size_t>(i)].count > 0) slot = i;
        }
        if (slot < 0)
        {
            fail(command, "no '" + name + "' in the pack");
            return;
        }
        inv.activeSlot = slot;
        if (item->category == ItemCategory::Weapon) game.equipWeaponFromSlot(slot);
    }
    else if (op == "reload")
    {
        // reload: R for one tick.
        game.mInput.setKeyState(SDL_SCANCODE_R, true);
        mPendingReleases.push_back({mTick + 1, false, SDL_SCANCODE_R, 0});
    }
    else if (op == "throw")
    {
        // throw DX DY [ITEM NAME]: throw the selected item (or the named one,
        // selected first) at the spot DX, DY tiles from the player.
        if (!need(2)) return;
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
        {
            fail(command, "no player");
            return;
        }
        Inventory& inv = game.mWorld.getComponent<Inventory>(game.mPlayerEntity);
        if (a.size() > 3)
        {
            std::string name;
            for (std::size_t i = 3; i < a.size(); ++i) name += (name.empty() ? "" : " ") + a[i];
            const ItemDef* item = game.mItemDatabase.findByName(name);
            int slot = -1;
            for (int i = 0; item != nullptr && i < Inventory::kMaxSlots && slot < 0; ++i)
            {
                if (inv.slots[static_cast<std::size_t>(i)].itemId == item->id && inv.slots[static_cast<std::size_t>(i)].count > 0) slot = i;
            }
            if (slot < 0)
            {
                fail(command, "no '" + name + "' in the pack");
                return;
            }
            inv.activeSlot = slot;
        }
        const float tile = static_cast<float>(game.mTileMap.tileWidth());
        const glm::vec2 target = game.playerWorldCenter() + glm::vec2(std::stof(a[1]), std::stof(a[2])) * tile;
        if (!game.throwSelected(target)) fail(command, "nothing throwable selected");
    }
    else if (op == "plan")
    {
        // plan: the first days' plan as the objective panel shows it, the
        // taps, and the night's callers still about.
        const firstdays::Plan plan = firstdays::planFor(game.mCurrentDay, game.mGameHours, game.firstDaysFacts());
        int callers = 0;
        game.mWorld.forEach<ZombieAI>([&](Entity, ZombieAI& ai) { if (ai.nightCaller) ++callers; });
        std::cout << "[harness] tick " << mTick << ": plan day " << game.mCurrentDay << " hour " << game.mGameHours
                  << " '" << plan.title << "' " << firstdays::clockLine(game.mGameHours)
                  << " taps=" << (firstdays::tapsRunning(game.mCurrentDay, game.mGameHours) ? "on" : "off")
                  << " callers=" << callers << " home=" << game.mFirstDays.homeBuilding
                  << " atHome=" << (game.playerAtHome() ? 1 : 0) << '\n';
        for (const firstdays::Step& step : plan.steps)
        {
            std::cout << "[harness]   [" << (step.done ? 'x' : ' ') << "] " << step.text << '\n';
        }
        std::cout << "[harness]   home doors:";
        for (const glm::ivec2& tile : game.mFirstDays.homeDoors)
        {
            const Doors::Door* door = game.mDoors.find(tile);
            const DoorState state = game.mTileMap.doorState(tile.x, tile.y);
            std::cout << " (" << tile.x << "," << tile.y << ") "
                      << (state == DoorState::Closed ? "closed" : state == DoorState::Open ? "open" : "broken")
                      << " boards=" << game.mDoors.boards(tile) << " boardHp=" << (door != nullptr ? door->boardHp : 0.0f)
                      << " hp=" << (door != nullptr ? door->hp : 0.0f);
        }
        // Every way into the house: doors and windows, as the plan counts them.
        std::cout << "\n[harness]   ways in:";
        for (const Game::WayIn& way : game.waysIn(game.mFirstDays.homeBuilding))
        {
            std::cout << " " << (way.window ? "window" : "door") << "(" << way.tile.x << "," << way.tile.y << ") "
                      << (way.boarded ? "boarded" : way.open ? "open" : way.covered ? "curtained" : "closed")
                      << " strength=" << static_cast<int>(way.strength);
        }
        // Each caller: state (0 wander, 1 investigate, 2 chase, 3 attack),
        // tiles from home, seconds in that state.
        std::cout << "\n[harness]   callers:";
        const glm::vec2 home = (glm::vec2(game.mSpawnData.playerStart) + glm::vec2(0.5f)) * 32.0f;
        game.mWorld.forEach<ZombieAI, Transform>(
            [&](Entity, ZombieAI& ai, Transform& t)
            {
                if (!ai.nightCaller) return;
                const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - home) / 32.0f;
                std::cout << " [" << static_cast<int>(ai.state) << " " << static_cast<int>(d) << "t "
                          << static_cast<int>(ai.stateTimer) << "s]";
            });
        std::cout << '\n';
    }
    else if (op == "doors")
    {
        std::cout << "[harness] tick " << mTick << ": doors closed=" << game.mDoors.count(game.mTileMap, DoorState::Closed)
                  << " open=" << game.mDoors.count(game.mTileMap, DoorState::Open)
                  << " broken=" << game.mDoors.count(game.mTileMap, DoorState::Broken)
                  << " boarded=" << game.boardedDoorRecords().size() << '\n';
    }
    else if (op == "slay")
    {
        // slay R: zombies within R tiles drop dead, credited to the player.
        if (!need(1)) return;
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
        {
            fail(command, "no player");
            return;
        }
        const float range = std::stof(a[1]) * static_cast<float>(game.mTileMap.tileWidth());
        const Transform& pt = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        int slain = 0;
        game.mWorld.forEach<ZombieAI, Transform, Health>(
            [&](Entity, ZombieAI& ai, Transform& t, Health& h)
            {
                const float dx = t.x - pt.x;
                const float dy = t.y - pt.y;
                if (h.current > 0.0f && dx * dx + dy * dy <= range * range)
                {
                    h.current = 0.0f;
                    ai.lastDamagedBy = game.mPlayerEntity;
                    ++slain;
                }
            });
        std::cout << "[harness] slay: " << slain << " zombie(s)\n";
    }
    else if (op == "goto")
    {
        // goto risen|pack|grave: teleport beside the first such legacy entity.
        // goto door: stand just inside the door nearest the player.
        // goto furniture [KIND|any] [stocked]: stand in front of the nearest
        // piece (of a kind such as fridge or bed; with something in it).
        if (!need(1)) return;
        const std::string kind = lower(a[1]);
        if (kind == "furniture")
        {
            if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
            {
                fail(command, "no player");
                return;
            }
            const Transform& pt = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
            const glm::vec2 from((pt.x + 16.0f) / 32.0f, (pt.y + 16.0f) / 32.0f);
            const bool wantContainer = a.size() > 2 && lower(a[2]) == "container";
            const std::string want = a.size() > 2 && lower(a[2]) != "any" && !wantContainer ? "furn_" + lower(a[2])
                                                                                                : std::string{};
            const bool wantStocked = a.size() > 3 && lower(a[3]) == "stocked";
            // Pickups lying on a tile would take the E press first.
            std::vector<glm::ivec2> pickups;
            game.mWorld.forEach<Interactable, Transform>(
                [&](Entity e, Interactable&, Transform& t)
                {
                    if (!game.mWorld.hasComponent<Inventory>(e))
                    {
                        pickups.emplace_back(static_cast<int>(t.x + 16.0f) / 32, static_cast<int>(t.y + 16.0f) / 32);
                    }
                });
            std::vector<glm::ivec2> stocked;
            game.mWorld.forEach<Interactable, Inventory, Transform>(
                [&](Entity, Interactable&, Inventory& inv, Transform& t)
                {
                    for (const auto& slot : inv.slots)
                    {
                        if (slot.itemId < 0 || slot.count <= 0) continue;
                        stocked.emplace_back(static_cast<int>(t.x) / 32, static_cast<int>(t.y) / 32);
                        return;
                    }
                });
            const FurniturePiece* best = nullptr;
            float bestD = 0.0f;
            for (const FurniturePiece& piece : game.mFurniture)
            {
                if (!want.empty() && want != furniture::info(piece.kind).sprite) continue;
                if (wantContainer && (furniture::info(piece.kind).loot == FurnitureLoot::None ||
                                      furniture::info(piece.kind).prompt == nullptr || piece.kind == FurnitureKind::Bed))
                {
                    continue;
                }
                if (wantStocked && std::find(stocked.begin(), stocked.end(), piece.tile) == stocked.end()) continue;
                const glm::vec2 d = glm::vec2(piece.tile) + glm::vec2(0.5f) - from;
                if (best == nullptr || glm::dot(d, d) < bestD)
                {
                    best = &piece;
                    bestD = glm::dot(d, d);
                }
            }
            if (best == nullptr)
            {
                fail(command, "no furniture" + (want.empty() ? std::string{} : " of kind " + a[2]));
                return;
            }
            std::vector<glm::ivec2> spots;
            if (furniture::info(best->kind).oriented) spots.push_back(furniture::frontTile(*best));
            for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(0, 1), glm::ivec2(-1, 0), glm::ivec2(0, -1)})
            {
                spots.push_back(best->tile + side);
            }
            for (const glm::ivec2& tile : spots)
            {
                if (game.mTileMap.isSolid(tile.x, tile.y)) continue;
                if (std::find(pickups.begin(), pickups.end(), tile) != pickups.end()) continue;
                execute(game, Command{command.line, {"teleport", std::to_string(tile.x), std::to_string(tile.y)}});
                return;
            }
            fail(command, "no free tile beside the nearest piece");
            return;
        }
        if (kind == "window")
        {
            // goto window [home] [front|back] [inside|outside [N]]: the floor
            // inside the nearest window (of the home; in a wall facing the
            // camera, or away from it), or the ground outside it, N tiles off.
            const bool home = std::find(a.begin(), a.end(), "home") != a.end();
            const bool front = std::find(a.begin(), a.end(), "front") != a.end();
            const bool back = std::find(a.begin(), a.end(), "back") != a.end();
            auto sideAt = std::find(a.begin(), a.end(), "outside");
            const bool outside = sideAt != a.end();
            if (!outside) sideAt = std::find(a.begin(), a.end(), "inside");
            const int out = sideAt != a.end() && sideAt + 1 != a.end() ? std::max(1, std::stoi(*(sideAt + 1))) : 1;
            const glm::vec2 here = game.playerWorldCenter();
            const Windows::Window* best = nullptr;
            float bestD = 0.0f;
            for (const Windows::Window& window : game.mWindows.all())
            {
                if (home && window.spot.building != game.mFirstDays.homeBuilding) continue;
                const glm::ivec2 facing = window.spot.outside - window.spot.tile;
                const bool facesCamera = facing.x > 0 || facing.y > 0;
                if ((front && !facesCamera) || (back && facesCamera)) continue;
                const float d = glm::length(glm::vec2(window.spot.tile) * 32.0f + glm::vec2(16.0f) - here);
                if (best == nullptr || d < bestD)
                {
                    best = &window;
                    bestD = d;
                }
            }
            if (best == nullptr)
            {
                fail(command, home ? "the home has no windows" : "no windows");
                return;
            }
            glm::ivec2 tile = outside ? best->spot.outside : best->spot.inside;
            const glm::ivec2 step = outside ? best->spot.outside - best->spot.tile : best->spot.inside - best->spot.tile;
            for (int i = 1; i < out; ++i)
            {
                const glm::ivec2 next = tile + step;
                if (game.mTileMap.isSolid(next.x, next.y)) break;
                tile = next;
            }
            execute(game, Command{command.line, {"teleport", std::to_string(tile.x), std::to_string(tile.y)}});
            return;
        }
        if (kind == "base")
        {
            // goto base: beside the storage crate of the house the last lives
            // held, or the floor nearest its middle.
            const BaseRecord& base = game.mSaveManager.baseRecord();
            if (base.building <= 0)
            {
                fail(command, "no base remembered");
                return;
            }
            glm::ivec2 crate(-1);
            game.mWorld.forEach<Structure, Transform>(
                [&](Entity, Structure& s, Transform& t)
                {
                    if (s.type != StructureType::StorageCrate) return;
                    const glm::ivec2 tile = gameshared::worldToTile(game.mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
                    if (game.mTileMap.getBuildingId(tile.x, tile.y) == base.building) crate = tile;
                });
            const glm::ivec2 centre = crate.x >= 0 ? crate : gameshared::worldToTile(game.mTileMap, glm::vec2(base.x, base.y));
            const std::vector<glm::ivec2> occupied = game.occupiedTiles();
            glm::ivec2 bestTile(-1);
            int bestD = 0;
            for (int dy = -6; dy <= 6; ++dy)
            {
                for (int dx = -6; dx <= 6; ++dx)
                {
                    const glm::ivec2 tile = centre + glm::ivec2(dx, dy);
                    if (tile == centre || game.mTileMap.isSolid(tile.x, tile.y)) continue;
                    if (game.mTileMap.getBuildingId(tile.x, tile.y) != base.building) continue;
                    if (std::find(occupied.begin(), occupied.end(), tile) != occupied.end()) continue;
                    const int d = std::abs(dx) + std::abs(dy);
                    if (bestTile.x < 0 || d < bestD)
                    {
                        bestTile = tile;
                        bestD = d;
                    }
                }
            }
            if (bestTile.x < 0)
            {
                fail(command, "no floor in the base");
                return;
            }
            execute(game, Command{command.line, {"teleport", std::to_string(bestTile.x), std::to_string(bestTile.y)}});
            return;
        }
        if (kind == "mark")
        {
            if (mMark.x < 0)
            {
                fail(command, "no mark");
                return;
            }
            execute(game, Command{command.line, {"teleport", std::to_string(mMark.x), std::to_string(mMark.y)}});
            return;
        }
        if (kind == "home")
        {
            // goto home: where the run started.
            execute(game, Command{command.line, {"teleport", std::to_string(game.mSpawnData.playerStart.x),
                                                 std::to_string(game.mSpawnData.playerStart.y)}});
            return;
        }
        if (kind == "door")
        {
            if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
            {
                fail(command, "no player");
                return;
            }
            const Transform& pt = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
            const Doors::Door* door = game.mDoors.nearest(game.mTileMap, glm::vec2(pt.x + 16.0f, pt.y + 16.0f), 1.0e9f);
            if (door == nullptr)
            {
                fail(command, "no doors");
                return;
            }
            // goto door [outside]: the floor just inside the nearest door, or
            // the ground just outside it.
            const bool outside = a.size() > 2 && lower(a[2]) == "outside";
            const glm::ivec2 sides[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const glm::ivec2& side : sides)
            {
                const glm::ivec2 tile = door->tile + side;
                if (!game.mTileMap.isSolid(tile.x, tile.y) && (game.mTileMap.getBuildingId(tile.x, tile.y) > 0) != outside)
                {
                    execute(game, Command{command.line, {"teleport", std::to_string(tile.x), std::to_string(tile.y)}});
                    return;
                }
            }
            fail(command, outside ? "no ground outside the nearest door" : "no floor inside the nearest door");
            return;
        }
        LegacyLinkKind want = LegacyLinkKind::Grave;
        if (kind == "risen") want = LegacyLinkKind::Risen;
        else if (kind == "pack") want = LegacyLinkKind::Pack;
        bool found = false;
        glm::vec2 target(0.0f);
        game.mWorld.forEach<LegacyLink, Transform>(
            [&](Entity, LegacyLink& link, Transform& t)
            {
                if (!found && link.kind == want)
                {
                    target = glm::vec2(t.x, t.y);
                    found = true;
                }
            });
        if (!found)
        {
            fail(command, "no legacy " + kind + " in the world");
            return;
        }
        const int tileW = std::max(1, game.mTileMap.tileWidth());
        const int tileH = std::max(1, game.mTileMap.tileHeight());
        const int tx = static_cast<int>((target.x + 16.0f) / static_cast<float>(tileW));
        const int ty = static_cast<int>((target.y + 16.0f) / static_cast<float>(tileH));
        execute(game, Command{command.line, {"teleport", std::to_string(tx), std::to_string(ty)}});
    }
    else if (op == "kill" || op == "retire")
    {
        // End the run the way the game does: kill [cause...] / retire [cause...]
        std::string cause;
        for (std::size_t i = 1; i < a.size(); ++i)
        {
            cause += (i > 1 ? " " : "") + a[i];
        }
        if (game.mRunState != Game::RunState::Playing)
        {
            fail(command, "no run in progress");
            return;
        }
        if (op == "kill")
        {
            game.beginDeathState(cause.empty() ? "Harness" : cause);
        }
        else
        {
            game.beginRetirementState(cause);
        }
    }
    else if (op == "hours")
    {
        if (need(1)) game.mGameHours = std::clamp(std::stod(a[1]), 0.0, 23.99);
    }
    else if (op == "day")
    {
        if (need(1)) game.mCurrentDay = std::max(1, std::stoi(a[1]));
    }
    else if (op == "enter")
    {
        // enter N: teleport to the walkable floor tile nearest building N's centre.
        if (!need(1)) return;
        const auto& buildings = game.mMapGenerator.layoutData().buildings;
        const int n = std::stoi(a[1]);
        if (n < 0 || n >= static_cast<int>(buildings.size()))
        {
            fail(command, "no building " + a[1] + " (have " + std::to_string(buildings.size()) + ")");
            return;
        }
        const auto& b = buildings[static_cast<std::size_t>(n)];
        const int cx = b.x + b.w / 2;
        const int cy = b.y + b.h / 2;
        int best = -1;
        int bestX = 0;
        int bestY = 0;
        for (int y = b.y; y < b.y + b.h; ++y)
        {
            for (int x = b.x; x < b.x + b.w; ++x)
            {
                if (game.mTileMap.isSolid(x, y) || game.mTileMap.getBuildingId(x, y) != n + 1) continue;
                const int d = std::abs(x - cx) + std::abs(y - cy);
                if (best < 0 || d < best)
                {
                    best = d;
                    bestX = x;
                    bestY = y;
                }
            }
        }
        if (best < 0)
        {
            fail(command, "building " + a[1] + " has no floor");
            return;
        }
        execute(game, Command{command.line, {"teleport", std::to_string(bestX), std::to_string(bestY)}});
    }
    else if (op == "teleport")
    {
        if (!need(2)) return;
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
        {
            fail(command, "no player to teleport");
            return;
        }
        const int tx = std::stoi(a[1]);
        const int ty = std::stoi(a[2]);
        const float tileW = static_cast<float>(game.mTileMap.tileWidth());
        const float tileH = static_cast<float>(game.mTileMap.tileHeight());
        Transform& t = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        t.x = (static_cast<float>(tx) + 0.5f) * tileW - kSpriteSizeForHarness * 0.5f;
        t.y = (static_cast<float>(ty) + 0.5f) * tileH - kSpriteSizeForHarness * 0.5f;
        t.prevX = t.x;
        t.prevY = t.y;
        game.mCamera.snapTo(glm::vec2(t.x + kSpriteSizeForHarness * 0.5f, t.y + kSpriteSizeForHarness * 0.5f));
        game.mCamera.clampToBounds(game.mTileMap.pixelSize());
    }
    else if (op == "ui")
    {
        // ui off|on: hide the whole interface, for shots of the bare world.
        if (need(1)) game.mHideUi = lower(a[1]) == "off";
    }
    else if (op == "lamp")
    {
        // lamp: stand beside the nearest street lamp that still burns.
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
        {
            fail(command, "no player");
            return;
        }
        Transform& t = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        const glm::vec2 here(t.x + 16.0f, t.y + 16.0f);
        std::vector<glm::vec2> lamps;
        game.mSceneryView.lampsNear(here, 1.0e9f, lamps);
        const glm::vec2* best = nullptr;
        for (const glm::vec2& lamp : lamps)
        {
            if (SceneryView::lampState(lamp) != 1) continue;
            if (best == nullptr || glm::dot(lamp - here, lamp - here) < glm::dot(*best - here, *best - here)) best = &lamp;
        }
        if (best == nullptr)
        {
            fail(command, "no lit lamps");
            return;
        }
        t.x = best->x - 16.0f;
        t.y = best->y + 32.0f - 16.0f;
        t.prevX = t.x;
        t.prevY = t.y;
        game.mCamera.snapTo(glm::vec2(t.x + 16.0f, t.y + 16.0f));
        game.mCamera.clampToBounds(game.mTileMap.pixelSize());
    }
    else if (op == "weather")
    {
        if (need(1))
        {
            const std::string kind = lower(a[1]);
            if (kind == "mark")
            {
                mWeatherMark = static_cast<int>(game.mWeatherState.condition);
                std::cout << "[harness] tick " << mTick << ": weather " << mWeatherMark << ", "
                          << game.mWeatherSystem.hoursLeft() << " game hours left\n";
                return;
            }
            if (kind == "roll")
            {
                // Let go of any held weather: a new condition is rolled next tick.
                game.mWeatherSystem.hold(game.mWeatherState, game.mWeatherState.condition, 0.0f);
                return;
            }
            WeatherCondition condition = WeatherCondition::Clear;
            if (kind == "cloudy") condition = WeatherCondition::Cloudy;
            else if (kind == "rain") condition = WeatherCondition::Rain;
            else if (kind == "heavyrain") condition = WeatherCondition::HeavyRain;
            else if (kind == "storm") condition = WeatherCondition::Thunderstorm;
            else if (kind == "fog") condition = WeatherCondition::Fog;
            game.mWeatherSystem.hold(game.mWeatherState, condition, 100000.0f);
        }
    }
    else if (op == "zoom")
    {
        if (need(1)) game.mCamera.setZoom(std::stof(a[1]));
    }
    else if (op == "god")
    {
        if (need(1)) mGodMode = lower(a[1]) == "on";
    }
    else if (op == "give")
    {
        // give ITEM NAME [COUNT]  (names may contain spaces)
        if (!need(1)) return;
        std::size_t nameEnd = a.size();
        int count = 1;
        if (a.size() > 2 && std::all_of(a.back().begin(), a.back().end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
        {
            count = std::stoi(a.back());
            nameEnd = a.size() - 1;
        }
        std::string name;
        for (std::size_t i = 1; i < nameEnd; ++i)
        {
            if (!name.empty()) name += ' ';
            name += a[i];
        }
        const ItemDef* item = game.mItemDatabase.findByName(name);
        if (item == nullptr || game.mPlayerEntity == kInvalidEntity ||
            !game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
        {
            fail(command, "cannot give '" + name + "'");
            return;
        }
        InventorySystem::addItem(game.mWorld.getComponent<Inventory>(game.mPlayerEntity), game.mItemDatabase, item->id, count);
    }
    else if (op == "set")
    {
        // set STAT VALUE: force a player stat, for screenshots and tuning.
        if (!need(2)) return;
        const Entity player = game.mPlayerEntity;
        if (player == kInvalidEntity)
        {
            fail(command, "no player");
            return;
        }
        const std::string stat = lower(a[1]);
        const float value = std::stof(a[2]);
        World& world = game.mWorld;
        bool applied = false;
        if (world.hasComponent<Needs>(player))
        {
            Needs& needs = world.getComponent<Needs>(player);
            if (stat == "hunger") { needs.hunger = value; applied = true; }
            else if (stat == "thirst") { needs.thirst = value; applied = true; }
            else if (stat == "sleepdebt") { needs.sleepDebt = value; applied = true; }
            else if (stat == "fatigue") { needs.fatigue = value; applied = true; }
            else if (stat == "temp") { needs.bodyTemperature = value; applied = true; }
            else if (stat == "illness") { needs.illness = value; applied = true; }
        }
        if (world.hasComponent<MentalState>(player))
        {
            MentalState& mental = world.getComponent<MentalState>(player);
            if (stat == "panic") { mental.panic = value; applied = true; }
            else if (stat == "depression") { mental.depression = value; applied = true; }
        }
        if (stat == "hp" && world.hasComponent<Health>(player))
        {
            world.getComponent<Health>(player).current = value;
            applied = true;
        }
        if (stat == "wounds")
        {
            if (!world.hasComponent<Wounds>(player)) world.addComponent<Wounds>(player, Wounds{});
            Wounds& wounds = world.getComponent<Wounds>(player);
            wounds.active.clear();
            for (int i = 0; i < static_cast<int>(value); ++i)
            {
                Wound wound;
                wound.type = WoundType::Cut;
                wound.severity = 0.5f;
                wounds.active.push_back(wound);
            }
            if (world.hasComponent<Health>(player))
            {
                Health& health = world.getComponent<Health>(player);
                health.maximum = woundedMaximum(health.baseMaximum, wounds.active.size());
                health.current = std::min(health.current, health.maximum);
            }
            applied = true;
        }
        if (stat == "stamina" && world.hasComponent<Stamina>(player))
        {
            world.getComponent<Stamina>(player).current = value;
            applied = true;
        }
        if (!applied) fail(command, "unknown stat '" + stat + "'");
    }
    else if (op == "trait")
    {
        // trait tough: the player is Tough (120 maximum health).
        if (!need(1)) return;
        const Entity player = game.mPlayerEntity;
        if (lower(a[1]) != "tough" || player == kInvalidEntity || !game.mWorld.hasComponent<Health>(player))
        {
            fail(command, "cannot give trait '" + a[1] + "'");
            return;
        }
        if (!game.mWorld.hasComponent<Traits>(player)) game.mWorld.addComponent<Traits>(player, Traits{});
        game.mWorld.getComponent<Traits>(player).positive = PositiveTrait::Tough;
        Health& health = game.mWorld.getComponent<Health>(player);
        health.baseMaximum = 120.0f;
        const std::size_t wounds =
            game.mWorld.hasComponent<Wounds>(player) ? game.mWorld.getComponent<Wounds>(player).active.size() : 0;
        health.maximum = woundedMaximum(health.baseMaximum, wounds);
        health.current = health.maximum;
    }
    else if (op == "tune")
    {
        if (!need(2)) return;
        Tuning::set(a[1], std::stof(a[2]));
        std::cout << "[harness] tune " << a[1] << " = " << Tuning::get(a[1], 0.0f) << '\n';
    }
    else if (op == "mark")
    {
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
        {
            fail(command, "no player");
            return;
        }
        const Transform& t = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        mMark = glm::ivec2(static_cast<int>(t.x + 16.0f) / 32, static_cast<int>(t.y + 16.0f) / 32);
    }
    else if (op == "stash")
    {
        // stash ITEM NAME [COUNT]: from the pack into the open container.
        if (!need(1)) return;
        int count = 1;
        const std::string name = itemArgs(a, 1, count);
        const ItemDef* item = game.mItemDatabase.findByName(name);
        if (!game.mContainerOpen || game.mOpenContainerEntity == kInvalidEntity || item == nullptr ||
            game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
        {
            fail(command, "cannot stash '" + name + "' (is a container open?)");
            return;
        }
        Inventory& pack = game.mWorld.getComponent<Inventory>(game.mPlayerEntity);
        Inventory& box = game.mWorld.getComponent<Inventory>(game.mOpenContainerEntity);
        int moved = 0;
        for (int i = 0; i < Inventory::kMaxSlots && moved < count; ++i)
        {
            if (pack.slots[static_cast<std::size_t>(i)].itemId != item->id) continue;
            moved += storage::moveItem(pack, i, box, game.mItemDatabase, count - moved);
        }
        if (moved < count)
        {
            fail(command, "stashed " + std::to_string(moved) + " of " + std::to_string(count) + " " + name);
        }
        game.recordContainer(game.mOpenContainerEntity);
    }
    else if (op == "open")
    {
        // open: open the nearest container within reach, as E at it would.
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
        {
            fail(command, "no player");
            return;
        }
        const Transform& pt = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        const glm::vec2 here(pt.x + 16.0f, pt.y + 16.0f);
        Entity best = kInvalidEntity;
        float bestD = 1.6f * 32.0f;
        game.mWorld.forEach<Interactable, Inventory, Transform>(
            [&](Entity e, Interactable& inter, Inventory&, Transform& t)
            {
                if (inter.type != InteractableType::Container || e == game.mPlayerEntity) return;
                const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here);
                if (d < bestD)
                {
                    best = e;
                    bestD = d;
                }
            });
        if (best == kInvalidEntity)
        {
            fail(command, "no container within reach");
            return;
        }
        game.openContainer(best);
    }
    else if (op == "container")
    {
        if (!game.mContainerOpen || game.mOpenContainerEntity == kInvalidEntity ||
            !game.mWorld.hasComponent<Inventory>(game.mOpenContainerEntity))
        {
            std::cout << "[harness] tick " << mTick << ": no container open\n";
            return;
        }
        const Inventory& box = game.mWorld.getComponent<Inventory>(game.mOpenContainerEntity);
        std::cout << "[harness] tick " << mTick << ": " << game.containerName(game.mOpenContainerEntity) << " holds";
        for (const InventorySlot& slot : box.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            const ItemDef* def = game.mItemDatabase.find(slot.itemId);
            std::cout << " [" << (def != nullptr ? def->name : "?") << " x" << slot.count << "]";
        }
        std::cout << " (" << storage::filledSlots(box) << " of " << box.maxSlots << ")\n";
    }
    else if (op == "bot")
    {
        // bot PROFILE [DAYS]: the bot plays; the script resumes when it stops.
        if (!need(1)) return;
        Autoplayer::Profile profile = Autoplayer::Profile::Normal;
        if (!Autoplayer::parseProfile(lower(a[1]), profile))
        {
            fail(command, "unknown bot profile '" + a[1] + "' (careful, normal, bold)");
            return;
        }
        if (game.mRunState != Game::RunState::Playing)
        {
            fail(command, "no run in progress");
            return;
        }
        const int days = a.size() > 2 ? std::max(1, std::stoi(a[2])) : 7;
        mBot.start(game, profile, days);
        mBotFailReported = false;
        std::cout << "[harness] tick " << mTick << ": bot " << lower(a[1]) << " for " << days << " day(s)\n";
    }
    else if (op == "metrics")
    {
        if (!need(1)) return;
        mMetricsInterval = std::max(0, std::stoi(a[1]));
        if (mMetrics && mMetricsInterval > 0) writeMetricsRow(game);
    }
    else if (op == "botshots")
    {
        // botshots TICKS: a screenshot every TICKS ticks while the bot plays (0 stops).
        if (need(1)) mBotShotInterval = std::max(0, std::stoi(a[1]));
    }
    else if (op == "shot")
    {
        if (need(1) && !capture(game, a[1])) fail(command, "screenshot failed");
    }
    else if (op == "expect")
    {
        if (!need(1)) return;
        const std::string want = lower(a[1]);
        if (want == "maxhp")
        {
            if (!need(2)) return;
            const float wantHp = std::stof(a[2]);
            const float hasHp = game.mPlayerEntity != kInvalidEntity && game.mWorld.hasComponent<Health>(game.mPlayerEntity)
                                    ? game.mWorld.getComponent<Health>(game.mPlayerEntity).maximum
                                    : -1.0f;
            if (std::abs(hasHp - wantHp) > 0.01f)
            {
                fail(command, "expected max health " + a[2] + ", it is " + std::to_string(hasHp));
            }
            else
            {
                std::cout << "[harness] tick " << mTick << ": max health " << hasHp << '\n';
            }
            return;
        }
        if (want == "weather")
        {
            const int now = static_cast<int>(game.mWeatherState.condition);
            if (mWeatherMark < 0 || now != mWeatherMark)
            {
                fail(command, "weather changed from " + std::to_string(mWeatherMark) + " to " + std::to_string(now));
            }
            else
            {
                std::cout << "[harness] tick " << mTick << ": weather held at " << now << '\n';
            }
            return;
        }
        if (want == "container")
        {
            // expect container ITEM NAME COUNT
            if (!need(3)) return;
            int count = 1;
            const std::string name = itemArgs(a, 2, count);
            const ItemDef* item = game.mItemDatabase.findByName(name);
            int have = 0;
            if (game.mContainerOpen && game.mOpenContainerEntity != kInvalidEntity && item != nullptr &&
                game.mWorld.hasComponent<Inventory>(game.mOpenContainerEntity))
            {
                have = InventorySystem::countItem(game.mWorld.getComponent<Inventory>(game.mOpenContainerEntity), item->id);
            }
            if (have < count)
            {
                fail(command, "expected " + std::to_string(count) + " " + name + " in the open container, found " +
                                  std::to_string(have));
            }
            return;
        }
        if (want == "pack")
        {
            // expect pack ITEM NAME COUNT: at least COUNT carried.
            if (!need(3)) return;
            int count = 1;
            const std::string name = itemArgs(a, 2, count);
            const ItemDef* item = game.mItemDatabase.findByName(name);
            int have = 0;
            if (item != nullptr && game.mPlayerEntity != kInvalidEntity && game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
            {
                have = InventorySystem::countItem(game.mWorld.getComponent<Inventory>(game.mPlayerEntity), item->id);
            }
            if (have < count)
            {
                fail(command, "expected " + std::to_string(count) + " " + name + " in the pack, found " + std::to_string(have));
            }
            else
            {
                std::cout << "[harness] tick " << mTick << ": pack holds " << have << " " << name << '\n';
            }
            return;
        }
        if (want == "blocked")
        {
            // expect blocked: a piece of furniture blocks a door of the
            // building the player stands in (or, outside, the nearest door's).
            const glm::ivec2 me = game.playerTile();
            int building = game.mTileMap.getBuildingId(me.x, me.y);
            if (building <= 0)
            {
                if (const Doors::Door* door = game.mDoors.nearest(game.mTileMap, game.playerWorldCenter(), 1.0e9f))
                {
                    building = game.mTileMap.getBuildingId(door->tile.x, door->tile.y);
                }
            }
            for (const FurniturePiece& piece : game.mFurniture)
            {
                if (piece.building != building || !game.mTileMap.doorwayBlocked(piece.tile.x, piece.tile.y)) continue;
                const glm::ivec2 door = furnitureuse::blockedDoor(game.mTileMap, piece.tile);
                std::cout << "[harness] tick " << mTick << ": " << furniture::info(piece.kind).sprite << " at "
                          << piece.tile.x << "," << piece.tile.y << " blocks door " << door.x << "," << door.y
                          << " damage " << static_cast<int>(piece.damage) << " of "
                          << static_cast<int>(furnitureuse::useInfo(piece.kind).barrierHp) << '\n';
                return;
            }
            fail(command, "no furniture blocks a door of building " + std::to_string(building));
            return;
        }
        if (want == "window")
        {
            // expect window glass|broken|boarded [N]|curtained|clear: the nearest window.
            if (!need(2)) return;
            const Windows::Window* window = game.mWindows.nearest(game.mTileMap, game.playerWorldCenter(), 1.0e9f);
            if (window == nullptr)
            {
                fail(command, "no windows");
                return;
            }
            const glm::ivec2 t = window->spot.tile;
            const std::string state = lower(a[2]);
            const int boards = game.mWindows.boards(t);
            bool ok = false;
            if (state == "glass") ok = game.mTileMap.windowState(t.x, t.y) == WindowState::Intact;
            else if (state == "broken") ok = game.mTileMap.windowState(t.x, t.y) == WindowState::Broken;
            else if (state == "boarded") ok = boards >= (a.size() > 3 ? std::stoi(a[3]) : 1);
            else if (state == "curtained") ok = game.mTileMap.windowCovered(t.x, t.y);
            else if (state == "clear") ok = !game.mTileMap.windowCovered(t.x, t.y) && boards == 0;
            const std::string have =
                std::string(game.mTileMap.windowState(t.x, t.y) == WindowState::Broken ? "broken" : "glass") +
                " boards=" + std::to_string(boards) + " boardHp=" + std::to_string(static_cast<int>(window->boardHp)) +
                (game.mTileMap.windowCovered(t.x, t.y) ? " curtained" : "");
            if (!ok) fail(command, "window at " + std::to_string(t.x) + "," + std::to_string(t.y) + " is " + have);
            else std::cout << "[harness] tick " << mTick << ": window " << t.x << "," << t.y << " " << have << '\n';
            return;
        }
        if (want == "zombies")
        {
            // expect zombies coming|all|calm|inside R: within R tiles of the
            // player, some are after something (investigate, chase, attack),
            // every one is, none are, or some stand in the player's building.
            if (!need(3)) return;
            const std::string mood = lower(a[2]);
            const float range = std::stof(a[3]) * 32.0f;
            const glm::vec2 here = game.playerWorldCenter();
            const glm::ivec2 me = game.playerTile();
            const int building = game.mTileMap.getBuildingId(me.x, me.y);
            int near = 0;
            int busy = 0;
            int inside = 0;
            game.mWorld.forEach<ZombieAI, Transform, Health>(
                [&](Entity, ZombieAI& ai, Transform& t, Health& h)
                {
                    if (h.current <= 0.0f) return;
                    const glm::vec2 c(t.x + 16.0f, t.y + 16.0f);
                    if (glm::length(c - here) > range) return;
                    ++near;
                    if (ai.state == ZombieState::Investigate || ai.state == ZombieState::Chase ||
                        ai.state == ZombieState::Attack)
                    {
                        ++busy;
                    }
                    const glm::ivec2 zt = gameshared::worldToTile(game.mTileMap, c);
                    if (building > 0 && game.mTileMap.getBuildingId(zt.x, zt.y) == building) ++inside;
                });
            const bool ok = mood == "coming" ? busy > 0 : mood == "all" ? near > 0 && busy == near : mood == "calm" ? busy == 0 : mood == "inside" ? inside > 0 : false;
            const std::string have = std::to_string(near) + " near, " + std::to_string(busy) + " after something, " +
                                     std::to_string(inside) + " inside";
            if (!ok) fail(command, "expected zombies " + mood + ": " + have);
            else std::cout << "[harness] tick " << mTick << ": zombies " << have << '\n';
            return;
        }
        if (want == "held" || want == "free")
        {
            const bool held = game.playerHeld();
            if (held != (want == "held")) fail(command, std::string("expected ") + want + ", the player is " + (held ? "held" : "free"));
            else std::cout << "[harness] tick " << mTick << ": player " << want << '\n';
            return;
        }
        if (want == "down" || want == "lured")
        {
            // expect down N / expect lured N: at least N of the dead on the
            // floor, or drawn to a thrown thing or a flare.
            if (!need(2)) return;
            const int wantCount = std::stoi(a[2]);
            int have = 0;
            game.mWorld.forEach<ZombieAI, Health>(
                [&](Entity, ZombieAI& ai, Health& h)
                {
                    if (h.current <= 0.0f) return;
                    if (want == "down" ? ZombieBlow::isDown(ai) : ai.lureTimer > 0.0f) ++have;
                });
            if (have < wantCount) fail(command, "expected " + a[2] + " " + want + ", found " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << have << " " << want << '\n';
            return;
        }
        if (want == "lunging")
        {
            // expect lunging N: exactly N of the dead winding up a lunge.
            if (!need(2)) return;
            int have = 0;
            game.mWorld.forEach<ZombieAI, Health>(
                [&](Entity, ZombieAI& ai, Health& h)
                {
                    if (h.current > 0.0f && ai.attackWindup > 0.0f) ++have;
                });
            if (have != std::stoi(a[2])) fail(command, "expected " + a[2] + " lunging, found " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << have << " lunging" << '\n';
            return;
        }
        if (want == "kills")
        {
            // expect kills N: the player has killed N of the dead this run.
            if (!need(2)) return;
            if (game.mRunStats.kills != std::stoi(a[2]))
            {
                fail(command, "expected " + a[2] + " kills, the run has " + std::to_string(game.mRunStats.kills));
            }
            else std::cout << "[harness] tick " << mTick << ": kills " << game.mRunStats.kills << '\n';
            return;
        }
        if (want == "burning")
        {
            // expect burning DX DY: a fire on that tile or one beside it.
            if (!need(3)) return;
            const glm::ivec2 me = game.playerTile();
            const glm::ivec2 at(me.x + std::stoi(a[2]), me.y + std::stoi(a[3]));
            bool found = false;
            game.mWorld.forEach<Fire, Transform>(
                [&](Entity, Fire&, Transform& ft)
                {
                    const glm::ivec2 t = gameshared::worldToTile(game.mTileMap, glm::vec2(ft.x, ft.y));
                    if (std::abs(t.x - at.x) <= 1 && std::abs(t.y - at.y) <= 1) found = true;
                });
            if (!found) fail(command, "no fire at " + a[2] + "," + a[3]);
            else std::cout << "[harness] tick " << mTick << ": burning at " << a[2] << "," << a[3] << '\n';
            return;
        }
        if (want == "loaded")
        {
            // expect loaded N: the gun in hand holds N rounds.
            if (!need(2)) return;
            const int have = game.mPlayerEntity != kInvalidEntity && game.mWorld.hasComponent<Combat>(game.mPlayerEntity)
                                 ? game.mWorld.getComponent<Combat>(game.mPlayerEntity).weapon.ammo
                                 : -1;
            if (have != std::stoi(a[2])) fail(command, "expected " + a[2] + " loaded, it holds " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": loaded " << have << '\n';
            return;
        }
        if (want == "landed")
        {
            // expect landed DX DY: the last throw came down within a tile of
            // the spot DX, DY tiles from the player.
            if (!need(3)) return;
            const float tile = static_cast<float>(game.mTileMap.tileWidth());
            const glm::vec2 spot = game.playerWorldCenter() + glm::vec2(std::stof(a[2]), std::stof(a[3])) * tile;
            const float off = glm::length(game.mLastThrowLanding - spot);
            if (game.mLastThrowLanding.x < 0.0f || off > tile)
            {
                fail(command, "the last throw landed " + std::to_string(off / tile) + " tiles from the spot");
            }
            else
            {
                std::cout << "[harness] tick " << mTick << ": landed " << off / tile << " tiles from the spot" << '\n';
            }
            return;
        }
        if (want == "speed")
        {
            // expect speed V: the player moves no faster than V pixels a second.
            if (!need(2)) return;
            const float limit = std::stof(a[2]);
            float speed = -1.0f;
            if (game.mPlayerEntity != kInvalidEntity && game.mWorld.hasComponent<Velocity>(game.mPlayerEntity))
            {
                const Velocity& v = game.mWorld.getComponent<Velocity>(game.mPlayerEntity);
                speed = std::hypot(v.dx, v.dy);
            }
            if (speed < 0.0f || speed > limit)
            {
                fail(command, "expected speed at most " + a[2] + ", it is " + std::to_string(speed));
            }
            return;
        }
        const std::string have = runStateName(static_cast<int>(game.mRunState));
        bool ok = false;
        if (want == "alive") ok = game.mRunState == Game::RunState::Playing || game.mRunState == Game::RunState::Paused;
        else if (want == "dead") ok = game.mRunState == Game::RunState::Dead;
        else ok = want == have;
        if (!ok) fail(command, "expected " + want + ", state is " + have);
    }
    else if (op == "push")
    {
        // push DIR: push the piece beside the player one tile that way (n, s, e, w).
        glm::ivec2 dir(0);
        if (!need(1)) return;
        if (!parseDirection(lower(a[1]), dir))
        {
            fail(command, "unknown direction '" + a[1] + "' (n, s, e, w)");
            return;
        }
        const glm::ivec2 tile = game.playerTile() + dir;
        const std::string why = game.pushFurniture(tile, dir, true);
        if (!why.empty())
        {
            fail(command, why);
            return;
        }
        std::cout << "[harness] tick " << mTick << ": pushed the piece at " << tile.x << "," << tile.y << " to "
                  << tile.x + dir.x << "," << tile.y + dir.y << '\n';
    }
    else if (op == "haul")
    {
        // haul KIND|any [KIND...]: push the nearest movable piece (of that kind) of this
        // house, tile by tile by the pushing rules, onto the floor inside the
        // door nearest the player, who then waits outside that door.
        if (!need(1)) return;
        const glm::vec2 here = game.playerWorldCenter();
        const Doors::Door* door = game.mDoors.nearest(game.mTileMap, here, 1.0e9f);
        if (door == nullptr)
        {
            fail(command, "no doors");
            return;
        }
        glm::ivec2 inside(-1);
        glm::ivec2 outside(-1);
        for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
        {
            const glm::ivec2 t = door->tile + side;
            if (game.mTileMap.isSolid(t.x, t.y) && !game.mTileMap.hasFurniture(t.x, t.y)) continue;
            if (game.mTileMap.getBuildingId(t.x, t.y) > 0) inside = t;
            else outside = t;
        }
        if (inside.x < 0 || outside.x < 0)
        {
            fail(command, "the nearest door has no floor inside or ground outside");
            return;
        }
        const glm::ivec2 doorTile = door->tile;
        execute(game, Command{command.line, {"teleport", std::to_string(outside.x), std::to_string(outside.y)}});
        const int building = game.mTileMap.getBuildingId(inside.x, inside.y);
        // haul KIND [KIND...]: the first kind the house has wins; then the nearest.
        std::vector<glm::ivec2> candidates;
        for (std::size_t k = 1; k < a.size() && candidates.empty(); ++k)
        {
            const std::string want = lower(a[k]) == "any" ? std::string{} : "furn_" + lower(a[k]);
            for (const FurniturePiece& piece : game.mFurniture)
            {
                if (piece.building != building || !furnitureuse::movable(piece.kind)) continue;
                if (!want.empty() && want != furniture::info(piece.kind).sprite) continue;
                candidates.push_back(piece.tile);
            }
        }
        std::sort(candidates.begin(), candidates.end(), [&](const glm::ivec2& l, const glm::ivec2& r)
        {
            const glm::ivec2 dl = l - inside;
            const glm::ivec2 dr = r - inside;
            return std::abs(dl.x) + std::abs(dl.y) < std::abs(dr.x) + std::abs(dr.y);
        });
        // The doors inside the house stand open for the move.
        for (const Doors::Door& inner : game.mDoors.all())
        {
            const glm::ivec2 t = inner.tile;
            if (t == doorTile || game.mTileMap.getBuildingId(t.x, t.y) != building) continue;
            bool interior = true;
            for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
            {
                const glm::ivec2 n = t + side;
                if (!game.mTileMap.isSolid(n.x, n.y) && game.mTileMap.getBuildingId(n.x, n.y) <= 0) interior = false;
            }
            if (interior && game.mTileMap.isClosedDoor(t.x, t.y) && game.mDoors.boards(t) == 0)
            {
                game.mDoors.toggle(game.mTileMap, t);
            }
        }
        const std::vector<glm::ivec2> occupied = game.occupiedTiles();
        const int w = game.mTileMap.width();
        for (const glm::ivec2& start : candidates)
        {
            if (start == inside)
            {
                std::cout << "[harness] tick " << mTick << ": the piece already stands at the door\n";
                return;
            }
            // Breadth-first over floor a piece can slide onto.
            std::vector<int> from(static_cast<std::size_t>(w * game.mTileMap.height()), -1);
            std::vector<glm::ivec2> queue{start};
            from[static_cast<std::size_t>(start.y * w + start.x)] = start.y * w + start.x;
            bool reached = false;
            for (std::size_t q = 0; q < queue.size() && !reached; ++q)
            {
                for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
                {
                    const glm::ivec2 next = furnitureuse::pushTarget(game.mTileMap, queue[q], side, occupied);
                    if (next.x < 0 || game.mTileMap.getBuildingId(next.x, next.y) != building) continue;
                    const auto ni = static_cast<std::size_t>(next.y * w + next.x);
                    if (from[ni] >= 0) continue;
                    from[ni] = queue[q].y * w + queue[q].x;
                    queue.push_back(next);
                    if (next == inside) reached = true;
                }
            }
            if (!reached) continue;
            std::vector<glm::ivec2> path{inside};
            while (path.back() != start)
            {
                const int f = from[static_cast<std::size_t>(path.back().y * w + path.back().x)];
                path.emplace_back(f % w, f / w);
            }
            std::reverse(path.begin(), path.end());
            std::string name = "?";
            if (const FurniturePiece* piece = game.furnitureAt(start)) name = furniture::info(piece->kind).sprite;
            for (std::size_t i = 1; i < path.size(); ++i)
            {
                const glm::ivec2 d = path[i] - path[i - 1];
                const glm::ivec2 dir(d.x > 0 ? 1 : d.x < 0 ? -1 : 0, d.y > 0 ? 1 : d.y < 0 ? -1 : 0);
                const std::string why = game.pushFurniture(path[i - 1], dir, false);
                if (!why.empty())
                {
                    fail(command, why);
                    return;
                }
            }
            std::cout << "[harness] tick " << mTick << ": hauled " << name << " " << path.size() - 1
                      << " tile(s) to " << inside.x << "," << inside.y << " blocking door " << doorTile.x << ","
                      << doorTile.y << '\n';
            return;
        }
        fail(command, std::to_string(candidates.size()) + " movable piece(s) in building " + std::to_string(building) +
                          ", none can reach " + std::to_string(inside.x) + "," + std::to_string(inside.y));
    }
    else if (op == "break")
    {
        // break: the nearest piece in reach is broken up at once (as X would, finished).
        const glm::vec2 here = game.playerWorldCenter();
        glm::ivec2 best(-1);
        float bestD = 1.6f * 32.0f;
        for (const FurniturePiece& piece : game.mFurniture)
        {
            const float d = glm::length(glm::vec2(piece.tile) * 32.0f + glm::vec2(16.0f) - here);
            if (d < bestD)
            {
                best = piece.tile;
                bestD = d;
            }
        }
        if (best.x < 0)
        {
            fail(command, "no furniture within reach");
            return;
        }
        game.breakFurniture(best);
    }
    else if (op == "build")
    {
        // build KIND: start building a station on the nearest free tile in
        // reach, with the pack's materials and tools, as the wheel would.
        if (!need(1)) return;
        const StationRecipe* recipe = recipeByWord(lower(a[1]));
        if (recipe == nullptr)
        {
            fail(command, "unknown station '" + a[1] + "'");
            return;
        }
        std::string missing;
        if (!game.canAffordStation(*recipe, &missing))
        {
            fail(command, missing);
            return;
        }
        const glm::ivec2 me = game.playerTile();
        const std::vector<glm::ivec2> occupied = game.occupiedTiles();
        glm::ivec2 best(-1);
        float bestD = 0.0f;
        for (int dy = -2; dy <= 2; ++dy)
        {
            for (int dx = -2; dx <= 2; ++dx)
            {
                const glm::ivec2 tile = me + glm::ivec2(dx, dy);
                const float d = glm::length(glm::vec2(dx, dy));
                if (tile == me || d > BuildSystem::kReachTiles) continue;
                if (!BuildSystem::placeable(game.mTileMap, tile, recipe->type, occupied)) continue;
                if (best.x < 0 || d < bestD)
                {
                    best = tile;
                    bestD = d;
                }
            }
        }
        if (best.x < 0)
        {
            fail(command, std::string("nowhere in reach for a ") + recipe->name);
            return;
        }
        game.startBuild(*recipe, best);
        std::cout << "[harness] tick " << mTick << ": building a " << recipe->name << " at " << best.x << "," << best.y
                  << " (" << recipe->seconds << " s)\n";
    }
    else if (op == "light")
    {
        // light: the nearest candle, lantern or campfire within 3 tiles is lit.
        const glm::vec2 here = game.playerWorldCenter();
        bool lit = false;
        game.mWorld.forEach<Structure, Transform>(
            [&](Entity, Structure& s, Transform& t)
            {
                if (lit || (s.type != StructureType::Light && s.type != StructureType::Campfire)) return;
                if (glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here) > 3.0f * 32.0f) return;
                s.lit = true;
                s.burnHours = std::max(s.burnHours, 4.0f);
                lit = true;
            });
        if (!lit) fail(command, "nothing to light within 3 tiles");
    }
    else if (op == "windows")
    {
        // windows: how the town's windows stand, and the one nearest the player.
        int broken = 0;
        int boarded = 0;
        int covered = 0;
        const glm::vec2 here = game.playerWorldCenter();
        const Windows::Window* nearest = nullptr;
        float nearestD = 0.0f;
        for (const Windows::Window& window : game.mWindows.all())
        {
            const glm::ivec2 t = window.spot.tile;
            if (game.mTileMap.windowState(t.x, t.y) == WindowState::Broken) ++broken;
            if (game.mWindows.boards(t) > 0) ++boarded;
            if (game.mTileMap.windowCovered(t.x, t.y)) ++covered;
            const float d = glm::length(glm::vec2(t) * 32.0f + glm::vec2(16.0f) - here);
            if (nearest == nullptr || d < nearestD)
            {
                nearest = &window;
                nearestD = d;
            }
        }
        std::cout << "[harness] tick " << mTick << ": windows " << game.mWindows.all().size() << " broken=" << broken
                  << " boarded=" << boarded << " curtained=" << covered;
        if (nearest != nullptr)
        {
            const glm::ivec2 t = nearest->spot.tile;
            std::cout << " | nearest (" << t.x << "," << t.y << ") "
                      << (game.mTileMap.windowState(t.x, t.y) == WindowState::Broken ? "broken" : "glass")
                      << " boards=" << game.mWindows.boards(t) << " boardHp=" << static_cast<int>(nearest->boardHp)
                      << " glassHp=" << static_cast<int>(nearest->glassHp)
                      << (game.mTileMap.windowCovered(t.x, t.y) ? " curtained" : "");
        }
        std::cout << '\n';
    }
    else if (op == "window")
    {
        // window STATE [all]: set the nearest window (or every window of the
        // building the player stands in) to intact, broken, boarded1,
        // boarded (two), curtained or open (curtain back).
        if (!need(1)) return;
        const std::string state = lower(a[1]);
        const bool all = a.size() > 2 && lower(a[2]) == "all";
        const glm::ivec2 me = game.playerTile();
        const int building = game.mTileMap.getBuildingId(me.x, me.y);
        const glm::vec2 here = game.playerWorldCenter();
        std::vector<glm::ivec2> tiles;
        const Windows::Window* nearest = nullptr;
        float nearestD = 0.0f;
        for (const Windows::Window& window : game.mWindows.all())
        {
            if (all && window.spot.building == building) tiles.push_back(window.spot.tile);
            const float d = glm::length(glm::vec2(window.spot.tile) * 32.0f + glm::vec2(16.0f) - here);
            if (nearest == nullptr || d < nearestD)
            {
                nearest = &window;
                nearestD = d;
            }
        }
        if (!all && nearest != nullptr) tiles.push_back(nearest->spot.tile);
        if (tiles.empty())
        {
            fail(command, "no window");
            return;
        }
        for (const glm::ivec2& t : tiles)
        {
            if (state == "intact") game.mWindows.restore(game.mTileMap, t, false, 0.0f, false);
            else if (state == "broken") game.mWindows.restore(game.mTileMap, t, true, 0.0f, false);
            else if (state == "boarded1") game.mWindows.restore(game.mTileMap, t, true, Windows::kBoardHp, false);
            else if (state == "boarded") game.mWindows.restore(game.mTileMap, t, true, 2.0f * Windows::kBoardHp, false);
            else if (state == "curtained") game.mTileMap.setWindowCovered(t.x, t.y, true);
            else if (state == "open") game.mTileMap.setWindowCovered(t.x, t.y, false);
            else
            {
                fail(command, "unknown window state '" + a[1] + "'");
                return;
            }
        }
    }
    else if (op == "zombies")
    {
        // zombies: how many of the dead are walking, and their total health.
        int count = 0;
        float health = 0.0f;
        game.mWorld.forEach<ZombieAI, Health>(
            [&](Entity, ZombieAI&, Health& h)
            {
                if (h.current <= 0.0f) return;
                ++count;
                health += h.current;
            });
        int corpses = 0;
        game.mWorld.forEach<Corpse>([&](Entity, Corpse&) { ++corpses; });
        std::cout << "[harness] tick " << mTick << ": zombies " << count << " health " << static_cast<int>(health)
                  << " corpses " << corpses << " kills " << game.mRunStats.kills << '\n';
    }
    else if (op == "log")
    {
        std::cout << "[harness] tick " << mTick << ":";
        for (std::size_t i = 1; i < a.size(); ++i) std::cout << ' ' << a[i];
        std::cout << '\n';
    }
    else if (op == "quit")
    {
        mFinished = true;
    }
    else
    {
        fail(command, "unknown command '" + op + "'");
    }
}

bool DevHarness::ensureFramebuffer(int width, int height)
{
    if (mFramebuffer != 0 && width == mFramebufferWidth && height == mFramebufferHeight)
    {
        return true;
    }

    if (mFramebuffer == 0)
    {
        glGenFramebuffers(1, &mFramebuffer);
        glGenTextures(1, &mColorTexture);
    }

    glBindTexture(GL_TEXTURE_2D, mColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTexture, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    mFramebufferWidth = width;
    mFramebufferHeight = height;
    return complete;
}

bool DevHarness::capture(Game& game, const std::string& name)
{
    const int width = game.mWindowWidth;
    const int height = game.mWindowHeight;
    if (!ensureFramebuffer(width, height))
    {
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    game.mInterpolationAlpha = 1.0f;
    game.render();
    glFinish();

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // OpenGL rows run bottom-up; PNG rows run top-down. Force opaque alpha.
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
    std::vector<unsigned char> flipped(pixels.size());
    for (int y = 0; y < height; ++y)
    {
        const unsigned char* src = pixels.data() + static_cast<std::size_t>(height - 1 - y) * rowBytes;
        unsigned char* dst = flipped.data() + static_cast<std::size_t>(y) * rowBytes;
        std::copy(src, src + rowBytes, dst);
        for (std::size_t x = 3; x < rowBytes; x += 4) dst[x] = 255;
    }

    const std::string path = (std::filesystem::path(mOptions.shotDir) / (name + ".png")).string();
    const bool ok = stbi_write_png(path.c_str(), width, height, 4, flipped.data(), static_cast<int>(rowBytes)) != 0;
    if (ok)
    {
        std::cout << "[harness] tick " << mTick << ": shot " << path << '\n';
    }
    return ok;
}

void DevHarness::writeMetricsRow(Game& game)
{
    World& world = game.mWorld;
    const Entity player = game.mPlayerEntity;
    const bool hasPlayer = player != kInvalidEntity && world.hasComponent<Transform>(player);

    float hp = 0, hpMax = 0, hunger = 0, thirst = 0, fatigue = 0, sleepDebt = 0, temp = 0, illness = 0;
    float stamina = 0, panic = 0, depression = 0;
    int inventoryItems = 0;
    glm::vec2 playerPos(0.0f);

    if (hasPlayer)
    {
        const Transform& t = world.getComponent<Transform>(player);
        playerPos = glm::vec2(t.x, t.y);
        if (world.hasComponent<Health>(player))
        {
            const Health& h = world.getComponent<Health>(player);
            hp = h.current;
            hpMax = h.maximum;
        }
        if (world.hasComponent<Needs>(player))
        {
            const Needs& n = world.getComponent<Needs>(player);
            hunger = n.hunger;
            thirst = n.thirst;
            fatigue = n.fatigue;
            sleepDebt = n.sleepDebt;
            temp = n.bodyTemperature;
            illness = n.illness;
        }
        if (world.hasComponent<Stamina>(player)) stamina = world.getComponent<Stamina>(player).current;
        if (world.hasComponent<MentalState>(player))
        {
            const MentalState& m = world.getComponent<MentalState>(player);
            panic = m.panic;
            depression = m.depression;
        }
        if (world.hasComponent<Inventory>(player))
        {
            const Inventory& inv = world.getComponent<Inventory>(player);
            for (const auto& slot : inv.slots)
            {
                if (slot.itemId >= 0) inventoryItems += slot.count;
            }
        }
    }

    int zombies = 0;
    int zombiesNear = 0;
    int zInvestigate = 0;
    int zChase = 0;
    int zAttack = 0;
    int zMigrating = 0;
    const float nearRadius = kNearZombieRadiusTiles * static_cast<float>(game.mTileMap.tileWidth());
    world.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI& ai, Transform& zt)
        {
            if (world.hasComponent<Health>(e) && world.getComponent<Health>(e).current <= 0.0f) return;
            ++zombies;
            if (ai.state == ZombieState::Investigate) ++zInvestigate;
            if (ai.state == ZombieState::Chase) ++zChase;
            if (ai.state == ZombieState::Attack) ++zAttack;
            if (ai.migrating) ++zMigrating;
            if (hasPlayer && std::hypot(zt.x - playerPos.x, zt.y - playerPos.y) <= nearRadius) ++zombiesNear;
        });

    const bool indoors = hasPlayer &&
        game.mTileMap.isIndoorsAtWorld(playerPos.x + 16.0f, playerPos.y + 16.0f);

    mMetrics << mTick << ',' << static_cast<double>(mTick) / kTicksPerSecond << ',' << game.mCurrentDay << ','
             << game.mGameHours << ',' << runStateName(static_cast<int>(game.mRunState)) << ',' << hp << ',' << hpMax
             << ',' << hunger << ',' << thirst << ',' << fatigue << ',' << sleepDebt << ',' << temp << ',' << illness
             << ',' << stamina << ',' << panic << ',' << depression << ',' << zombies << ',' << zombiesNear << ','
             << game.mRunStats.kills << ',' << inventoryItems << ',' << zInvestigate << ',' << zChase << ','
             << zAttack << ',' << zMigrating << ',' << (indoors ? 1 : 0) << ','
             << game.mThreatDirector.populationTarget() << ',';

    // Items carried by category, boards on doors, tonight's callers, noise.
    int byCategory[5] = {0, 0, 0, 0, 0};
    if (hasPlayer && world.hasComponent<Inventory>(player))
    {
        for (const auto& slot : world.getComponent<Inventory>(player).slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            if (const ItemDef* def = game.mItemDatabase.find(slot.itemId))
            {
                byCategory[std::clamp(static_cast<int>(def->category), 0, 4)] += slot.count;
            }
        }
    }
    // Boards up on doors and windows alike.
    int boards = 0;
    for (const auto& door : game.mDoors.all())
    {
        boards += game.mTileMap.doorBoards(door.tile.x, door.tile.y);
    }
    for (const auto& window : game.mWindows.all())
    {
        boards += game.mWindows.boards(window.spot.tile);
    }
    bool atHome = false;
    if (hasPlayer && game.mFirstDays.homeBuilding > 0)
    {
        const glm::ivec2 tile = gameshared::worldToTile(game.mTileMap, playerPos + glm::vec2(16.0f));
        atHome = game.mTileMap.getBuildingId(tile.x, tile.y) == game.mFirstDays.homeBuilding;
    }
    int callers = 0;
    world.forEach<ZombieAI, Health>([&](Entity, ZombieAI& ai, Health& h) { if (ai.nightCaller && h.current > 0.0f) ++callers; });
    const double clock = static_cast<double>(game.mCurrentDay) * 24.0 + game.mGameHours;
    const std::uint64_t noise = game.mNoiseModel.emittedTotal();
    double noisePerHour = 0.0;
    if (mClockAtLastRow >= 0.0 && clock > mClockAtLastRow && noise >= mNoiseAtLastRow)
    {
        noisePerHour = static_cast<double>(noise - mNoiseAtLastRow) / (clock - mClockAtLastRow);
    }
    mClockAtLastRow = clock;
    mNoiseAtLastRow = noise;
    const bool ended = game.mRunState == Game::RunState::Dead || game.mRunState == Game::RunState::Retired;
    mMetrics << (ended ? csvSafe(game.mDeathCause) : std::string{}) << ',' << byCategory[0] << ',' << byCategory[1]
             << ',' << byCategory[2] << ',' << byCategory[3] << ',' << byCategory[4] << ','
             << game.mContainersOpened << ',' << boards << ',' << callers << ',' << noisePerHour << ','
             << (mBot.running() ? mBot.activity() : "") << ',' << (atHome ? 1 : 0) << '\n';
    mMetrics.flush();
}
