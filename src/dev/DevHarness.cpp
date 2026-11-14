#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "components/Animation.h"
#include "components/Bleeding.h"
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
#include "world/BuildingKinds.h"
#include "world/Forage.h"
#include "world/Garden.h"
#include "world/Power.h"
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
    case 5: return "creating";
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
    beforeTickInner(game);
    // `perf`: the game's tick is timed from here to afterTick.
    if (mPerfTicksLeft > 0) mPerfStart = std::chrono::steady_clock::now();
}

void DevHarness::beforeTickInner(Game& game)
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
    if (mPerfTicksLeft > 0)
    {
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - mPerfStart).count();
        mPerfTotalMs += ms;
        mPerfWorstMs = std::max(mPerfWorstMs, ms);
        ++mPerfTicks;
        if (--mPerfTicksLeft == 0)
        {
            int alive = 0;
            game.mWorld.forEach<ZombieAI>([&](Entity, ZombieAI&) { ++alive; });
            std::cout << "[harness] tick " << mTick << ": perf over " << mPerfTicks << " ticks with " << alive
                      << " of the dead: average " << (mPerfTotalMs / static_cast<double>(std::max(1, mPerfTicks)))
                      << " ms, worst " << mPerfWorstMs << " ms\n";
        }
    }
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
        // newrun: a fresh run with the seed's quick pick of traits.
        // newrun tough,clumsy: with exactly these traits.
        // newrun create: through the character screen, as the menus do.
        game.mSaveManager.deleteCheckpoint();
        game.mIgnoreCheckpointRestoreOnce = true;
        game.syncPresentationSettingsToSave();
        const bool create = a.size() > 1 && lower(a[1]) == "create";
        game.mCreateCharacterOnStart = create;
        game.startNewRun();
        mWasAlive = true;
        if (a.size() > 1 && !create)
        {
            Traits traits;
            std::string list = a[1];
            for (std::size_t i = 2; i < a.size(); ++i) list += "," + a[i];
            std::size_t start = 0;
            while (start <= list.size())
            {
                std::size_t end = list.find(',', start);
                if (end == std::string::npos) end = list.size();
                const std::string name = list.substr(start, end - start);
                start = end + 1;
                if (name.empty()) continue;
                if (const auto good = traitrules::parsePositive(name))
                {
                    traitrules::toggle(traits, *good);
                }
                else if (const auto bad = traitrules::parseNegative(name))
                {
                    traitrules::toggle(traits, *bad);
                }
                else if (lower(name) != "none")
                {
                    fail(command, "unknown trait '" + name + "'");
                }
            }
            game.applyCharacter(traits, true);
        }
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
        // zombie outside R [options]: on open ground about R tiles from the
        // player (for hearing through walls).
        if (lower(a[1]) == "outside")
        {
            if (!need(2)) return;
            const float r = std::stof(a[2]);
            const glm::ivec2 me = gameshared::worldToTile(game.mTileMap, playerCentre);
            glm::ivec2 found(-1);
            float bestError = 1.0e9f;
            const int reach = static_cast<int>(r) + 2;
            for (int dy = -reach; dy <= reach; ++dy)
            {
                for (int dx = -reach; dx <= reach; ++dx)
                {
                    const glm::ivec2 t = me + glm::ivec2(dx, dy);
                    if (!game.mTileMap.inBounds(t.x, t.y) || game.mTileMap.isSolid(t.x, t.y) ||
                        game.mTileMap.getBuildingId(t.x, t.y) > 0)
                    {
                        continue;
                    }
                    const float error = std::abs(std::hypot(static_cast<float>(dx), static_cast<float>(dy)) - r);
                    if (error < bestError)
                    {
                        bestError = error;
                        found = t;
                    }
                }
            }
            if (found.x < 0 || bestError > 1.0f)
            {
                fail(command, "no open ground " + a[2] + " tiles away");
                return;
            }
            std::vector<std::string> args = {"zombie", std::to_string(found.x - me.x), std::to_string(found.y - me.y)};
            for (std::size_t k = 3; k < a.size(); ++k) args.push_back(a[k]);
            execute(game, Command{command.line, args});
            return;
        }
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
        // A kind (shambler, runner, brute, crawler, screamer: its own health,
        // speed and senses) and "dormant" (a Crawler lying in wait).
        bool kindGiven = false;
        ZombieKind kind = ZombieKind::Shambler;
        bool dormant = false;
        bool still = false; // stands where it is put (it still looks and listens)
        bool hpGiven = false;
        for (std::size_t k = atMark ? 2 : 3; k < a.size(); ++k)
        {
            const std::string opt = lower(a[k]);
            ZombieKind parsed;
            if (zombiekinds::parseKind(opt, parsed))
            {
                kind = parsed;
                kindGiven = true;
            }
            else if (opt == "dormant") dormant = idle = true;
            else if (opt == "still") still = idle = true;
            else if (opt == "idle") idle = true;
            else if (opt == "down") down = idle = true;
            else if (opt == "lunging") lunging = idle = true;
            else if (opt == "close") close = true;
            else if (opt == "exact") exact = !atMark;
            else if (opt == "away") away = idle = true;
            else if (opt == "hp" && k + 1 < a.size())
            {
                hp = std::stof(a[++k]);
                hpGiven = true;
            }
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
        if (kindGiven)
        {
            // The middle of the kind's ranges, the same every run.
            const zombiekinds::Rolled rolled = zombiekinds::roll(zombiekinds::activeTable(), kind, 0.5f, 0.5f);
            params.kind = kind;
            params.health = hpGiven ? hp : rolled.health;
            params.baseMoveSpeed = rolled.speed;
            params.dormant = dormant;
        }
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
        if (!kindGiven && zombie != kInvalidEntity && game.mWorld.hasComponent<Health>(zombie) && hp < 60.0f)
        {
            // Hurt: below the usual 60, so a shove can put it over.
            game.mWorld.getComponent<Health>(zombie).maximum = 60.0f;
        }
        if (still && zombie != kInvalidEntity && game.mWorld.hasComponent<ZombieAI>(zombie))
        {
            game.mWorld.getComponent<ZombieAI>(zombie).idleTimer = 1.0e6f;
        }
        if (!idle && zombie != kInvalidEntity && game.mWorld.hasComponent<ZombieAI>(zombie))
        {
            ZombieAISystem::sendTo(game.mWorld.getComponent<ZombieAI>(zombie), playerCentre, false);
        }
    }
    else if (op == "infection")
    {
        // infection N [all]: set the infection of the player's district (or
        // every district) and show its ruin at once, as at a dawn.
        if (!need(1)) return;
        const float value = std::stof(a[1]);
        const bool all = a.size() > 2 && lower(a[2]) == "all";
        const glm::vec2 here = game.playerWorldCenter();
        const int mine = game.mTileMap.getDistrictId(static_cast<int>(here.x) / 32, static_cast<int>(here.y) / 32);
        for (const DistrictInfection& d : game.mDistrictInfectionSystem.districts())
        {
            if (all || d.districtId == mine) game.mDistrictInfectionSystem.setInfection(d.districtId, value);
        }
        game.refreshRuinLevels();
        game.mHudDistrictId = -1; // the toast again, with the new word
        std::cout << "[harness] tick " << mTick << ": district " << mine << " ruin " << game.ruinLevel(mine) << " ("
                  << Game::ruinWord(game.ruinLevel(mine)) << "), " << game.mSceneryView.ruin().size() << " ruin marks" << std::endl;
    }
    else if (op == "mode")
    {
        // mode NAME: the threat mode of the next newrun (swarm, stalker,
        // mutating, society, unknown, hybrid).
        if (!need(1)) return;
        static const char* kModes[] = {"swarm", "stalker", "mutating", "society", "unknown", "hybrid"};
        const std::string name = lower(a[1]);
        int index = -1;
        for (int i = 0; i < 6; ++i)
        {
            if (name == kModes[i]) index = i;
        }
        if (index < 0)
        {
            fail(command, "unknown mode '" + name + "'");
            return;
        }
        game.mTitleModeIndex = index;
        std::cout << "[harness] tick " << mTick << ": mode " << name << '\n';
    }
    else if (op == "horde")
    {
        // horde: send tonight's horde now (whatever the night).
        game.startHorde();
        std::cout << "[harness] tick " << mTick << ": horde of " << game.mHorde.members << " set out ("
                  << game.mHorde.route.size() << " waypoints)\n";
    }
    else if (op == "trail")
    {
        // trail: print the blood markers on the ground.
        const auto& markers = game.mBloodTrail.markers();
        std::cout << "[harness] tick " << mTick << ": " << markers.size() << " blood markers\n";
    }
    else if (op == "noise")
    {
        // noise TIER [DX DY]: a noise (whisper, soft, medium, loud,
        // explosive) at the player, or DX, DY tiles from them.
        if (!need(1)) return;
        const std::string tierName = lower(a[1]);
        NoiseTier tier = NoiseTier::None;
        if (tierName == "whisper") tier = NoiseTier::Whisper;
        else if (tierName == "soft") tier = NoiseTier::Soft;
        else if (tierName == "medium") tier = NoiseTier::Medium;
        else if (tierName == "loud") tier = NoiseTier::Loud;
        else if (tierName == "explosive") tier = NoiseTier::Explosive;
        if (tier == NoiseTier::None)
        {
            fail(command, "unknown tier '" + tierName + "'");
            return;
        }
        glm::vec2 at = game.playerWorldCenter();
        if (a.size() >= 4) at += glm::vec2(std::stof(a[2]), std::stof(a[3])) * static_cast<float>(game.mTileMap.tileWidth());
        game.mNoiseModel.emitNoise(at, tier, 1.0f, game.mPlayerEntity);
    }
    else if (op == "perf")
    {
        // perf N: N of the dead around the player (5 to 40 tiles), then the
        // next 600 ticks are timed; average and worst are printed.
        if (!need(1)) return;
        const int count = std::max(0, std::stoi(a[1]));
        const glm::vec2 player = game.playerWorldCenter();
        const float tile = static_cast<float>(std::max(1, game.mTileMap.tileWidth()));
        std::mt19937 rng(1234u);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        int placed = 0;
        for (int attempt = 0; placed < count && attempt < count * 50; ++attempt)
        {
            const float angle = unit(rng) * 6.2831853f;
            const float distance = (5.0f + unit(rng) * 35.0f) * tile;
            const glm::vec2 at = player + glm::vec2(std::cos(angle), std::sin(angle)) * distance;
            const glm::ivec2 t = gameshared::worldToTile(game.mTileMap, at);
            if (!game.mTileMap.inBounds(t.x, t.y) || game.mTileMap.isSolid(t.x, t.y)) continue;
            const glm::vec2 topLeft = (glm::vec2(t) + glm::vec2(0.5f)) * tile - glm::vec2(16.0f);
            EntityFactory::spawnZombie(game.mWorld, game.mSpriteSheet,
                                       game.zombieParams(game.rollKind(rng, t), topLeft, rng));
            ++placed;
        }
        int alive = 0;
        game.mWorld.forEach<ZombieAI>([&](Entity, ZombieAI&) { ++alive; });
        std::cout << "[harness] tick " << mTick << ": perf: " << placed << " placed, " << alive << " of the dead\n";
        mPerfTicksLeft = 600;
        mPerfTotalMs = 0.0;
        mPerfWorstMs = 0.0;
        mPerfTicks = 0;
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
        if (kind == "kind")
        {
            // goto kind NAME [inside] [N]: the ground N tiles (default 1) in
            // front of the street door of the nearest building of that kind
            // (pharmacy, police...), or the floor just inside it.
            if (!need(2)) return;
            BuildingKind want = BuildingKind::House;
            if (!buildingkinds::fromName(a[2], want))
            {
                fail(command, "unknown building kind '" + a[2] + "'");
                return;
            }
            const bool inside = std::find(a.begin(), a.end(), "inside") != a.end();
            int out = 1;
            if (a.size() > 3 && std::isdigit(static_cast<unsigned char>(a.back()[0]))) out = std::max(1, std::stoi(a.back()));
            const LayoutData& layout = game.mMapGenerator.layoutData();
            const glm::vec2 from = glm::vec2(game.playerTile());
            glm::ivec2 best(-1);
            float bestD = 0.0f;
            int bestBuilding = 0;
            for (std::size_t i = 0; i < layout.buildings.size(); ++i)
            {
                if (buildingkinds::kindOf(layout.buildings[i]) != want) continue;
                const glm::ivec2 door = buildingkinds::streetDoor(layout, static_cast<int>(i) + 1);
                if (door.x < 0) continue;
                const float d = glm::length(glm::vec2(door) - from);
                if (best.x < 0 || d < bestD)
                {
                    best = door;
                    bestD = d;
                    bestBuilding = static_cast<int>(i) + 1;
                }
            }
            if (best.x < 0)
            {
                fail(command, std::string("the town has no ") + buildingkinds::name(want));
                return;
            }
            glm::ivec2 tile = best + glm::ivec2(0, inside ? -1 : 1);
            if (inside && game.mTileMap.isSolid(tile.x, tile.y)) tile = best + glm::ivec2(1, -1);
            for (int i = 1; !inside && i < out; ++i)
            {
                const glm::ivec2 next = tile + glm::ivec2(0, 1);
                if (game.mTileMap.isSolid(next.x, next.y)) break;
                tile = next;
            }
            std::cout << "[harness] goto kind: " << buildingkinds::name(want) << " (building " << bestBuilding << "), door "
                      << best.x << "," << best.y << (game.doorLocked(best) ? " locked" : " unlocked") << '\n';
            execute(game, Command{command.line, {"teleport", std::to_string(tile.x), std::to_string(tile.y)}});
            return;
        }
        if (kind == "wild")
        {
            // goto wild: the nearest open grass at least 15 tiles from any
            // building (where a snare may be set).
            const TileMap& map = game.mTileMap;
            const int w = map.width();
            const int h = map.height();
            std::vector<int> dist(static_cast<std::size_t>(w * h), -1);
            std::vector<glm::ivec2> queue;
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    if (map.getBuildingId(x, y) <= 0) continue;
                    dist[static_cast<std::size_t>(y * w + x)] = 0;
                    queue.emplace_back(x, y);
                }
            }
            for (std::size_t i = 0; i < queue.size(); ++i)
            {
                const glm::ivec2 t = queue[i];
                const int d = dist[static_cast<std::size_t>(t.y * w + t.x)];
                for (const glm::ivec2& step : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
                {
                    const glm::ivec2 n = t + step;
                    if (n.x < 0 || n.y < 0 || n.x >= w || n.y >= h || dist[static_cast<std::size_t>(n.y * w + n.x)] >= 0) continue;
                    dist[static_cast<std::size_t>(n.y * w + n.x)] = d + 1;
                    queue.push_back(n);
                }
            }
            const glm::ivec2 from = game.playerTile();
            const std::vector<glm::ivec2> occupied = game.occupiedTiles();
            glm::ivec2 best(-1);
            int bestD = 0;
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    // Steps overstate the distance: check the circle as placeSnare does.
                    if (dist[static_cast<std::size_t>(y * w + x)] < 15) continue;
                    if (map.isSolid(x, y) || map.isIndoors(x, y) || map.getSurfaceType(x, y) != SurfaceType::Grass) continue;
                    bool clear = true;
                    for (int dy = -15; dy <= 15 && clear; ++dy)
                    {
                        for (int dx = -15; dx <= 15 && clear; ++dx)
                        {
                            if (dx * dx + dy * dy > 225 || !map.inBounds(x + dx, y + dy)) continue;
                            if (map.getBuildingId(x + dx, y + dy) > 0) clear = false;
                        }
                    }
                    if (!clear) continue;
                    if (std::find(occupied.begin(), occupied.end(), glm::ivec2(x, y)) != occupied.end()) continue;
                    const int d = (x - from.x) * (x - from.x) + (y - from.y) * (y - from.y);
                    if (best.x < 0 || d < bestD)
                    {
                        best = {x, y};
                        bestD = d;
                    }
                }
            }
            if (best.x < 0)
            {
                fail(command, "no open grass far from the buildings");
                return;
            }
            execute(game, Command{command.line, {"teleport", std::to_string(best.x), std::to_string(best.y)}});
            return;
        }
        if (kind == "forage")
        {
            // goto forage [bush|tree|tuft]: beside the nearest such spot.
            const std::string which = a.size() > 2 ? lower(a[2]) : std::string("any");
            const forage::Spot want = which == "bush" ? forage::Spot::Bush : which == "tree" ? forage::Spot::TreeBase
                                    : which == "tuft" ? forage::Spot::Tuft : forage::Spot::None;
            const glm::vec2 from = glm::vec2(game.playerTile());
            const std::pair<glm::ivec2, forage::Spot>* best = nullptr;
            float bestD = 0.0f;
            for (const auto& spot : game.mForageSpots)
            {
                if (want != forage::Spot::None && spot.second != want) continue;
                const float d = glm::length(glm::vec2(spot.first) - from);
                if (best == nullptr || d < bestD)
                {
                    best = &spot;
                    bestD = d;
                }
            }
            if (best == nullptr)
            {
                fail(command, "no forage spot");
                return;
            }
            for (const glm::ivec2& side : {glm::ivec2(0, 1), glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, -1)})
            {
                const glm::ivec2 tile = best->first + side;
                if (game.mTileMap.isSolid(tile.x, tile.y)) continue;
                std::cout << "[harness] goto forage: spot " << best->first.x << "," << best->first.y << '\n';
                execute(game, Command{command.line, {"teleport", std::to_string(tile.x), std::to_string(tile.y)}});
                return;
            }
            fail(command, "no free tile beside the forage spot");
            return;
        }
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
        if (kind == "pavement")
        {
            // goto pavement: the nearest open outdoor spot with ordinary ground
            // (no grass, gravel or road) three tiles either side and clear
            // ground five tiles south.
            if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
            {
                fail(command, "no player");
                return;
            }
            const Transform& pt = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
            const glm::ivec2 from(static_cast<int>(pt.x + 16.0f) / 32, static_cast<int>(pt.y + 16.0f) / 32);
            const TileMap& map = game.mTileMap;
            auto plain = [&](int x, int y)
            {
                return !map.isSolid(x, y) && map.getBuildingId(x, y) == 0 &&
                       map.getSurfaceType(x, y) == SurfaceType::Default;
            };
            glm::ivec2 best(-1);
            int bestD = 0;
            for (int y = 1; y < map.height() - 6; ++y)
            {
                for (int x = 4; x < map.width() - 4; ++x)
                {
                    bool ok = !map.isSolid(x, y + 5) && map.getBuildingId(x, y + 5) == 0;
                    for (int dx = -3; ok && dx <= 3; ++dx) ok = plain(x + dx, y);
                    if (!ok) continue;
                    const int d = (x - from.x) * (x - from.x) + (y - from.y) * (y - from.y);
                    if (best.x < 0 || d < bestD)
                    {
                        best = {x, y};
                        bestD = d;
                    }
                }
            }
            if (best.x < 0)
            {
                fail(command, "no pavement");
                return;
            }
            execute(game, Command{command.line, {"teleport", std::to_string(best.x), std::to_string(best.y)}});
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
    else if (op == "recipe")
    {
        // recipe NAME...: with the Make panel open, put the cursor on that
        // recipe (in the tab showing) and press Enter.
        if (!need(1)) return;
        std::string name;
        for (std::size_t i = 1; i < a.size(); ++i) name += (i > 1 ? " " : "") + lower(a[i]);
        if (!game.mShowCrafting || game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
        {
            fail(command, "the Make panel is not open");
            return;
        }
        const auto visible = gameshared::buildVisibleRecipeList(game.mCraftingSystem, game.mSaveManager,
                                                                game.mWorld.getComponent<Inventory>(game.mPlayerEntity),
                                                                game.mSessionDiscoveredRecipes, game.mCraftingCategory);
        int found = -1;
        for (std::size_t i = 0; i < visible.size(); ++i)
        {
            if (lower(game.mCraftingSystem.recipes()[visible[i]].label) == name) found = static_cast<int>(i);
        }
        if (found < 0)
        {
            fail(command, "no recipe '" + name + "' in this tab");
            return;
        }
        game.mCraftingRecipeCursor = found;
        game.mInput.setKeyState(SDL_SCANCODE_RETURN, true);
        mPendingReleases.push_back({mTick + 1, false, SDL_SCANCODE_RETURN, 0});
    }
    else if (op == "power")
    {
        // power on|off|auto: force the town's power (phase 6).
        // power soon: back to the clock, six game minutes before it fails.
        if (!need(1)) return;
        const std::string state = lower(a[1]);
        game.mPowerForced = state == "on" ? 1 : state == "off" ? 0 : -1;
        if (state == "soon")
        {
            const double at = power::failHour(game.mSaveManager.currentRunSeed()) - 0.1;
            game.mCurrentDay = static_cast<int>(at / 24.0);
            game.mGameHours = at - static_cast<double>(game.mCurrentDay) * 24.0;
        }
        std::cout << "[harness] power " << state << " (now " << (game.powerOn() ? "on" : "off") << ", fails at hour "
                  << power::failHour(game.mSaveManager.currentRunSeed()) << ")\n";
    }
    else if (op == "grow")
    {
        // grow DAYS [dry]: every garden bed lives DAYS days, watered each day
        // (or not, with dry).
        if (!need(1)) return;
        const int days = std::max(0, std::stoi(a[1]));
        const bool dry = a.size() > 2 && lower(a[2]) == "dry";
        int beds = 0;
        game.mWorld.forEach<Structure>(
            [&](Entity, Structure& s)
            {
                if (s.type != StructureType::GardenBed) return;
                garden::Bed bed;
                bed.growHours = s.growHours;
                bed.sinceWaterHours = s.sinceWaterHours;
                bed.regrowing = s.regrowing;
                bed.dead = s.dead;
                for (int d = 0; d < days; ++d)
                {
                    if (!dry) garden::water(bed);
                    for (int h = 0; h < 24; ++h) garden::tick(bed, 1.0f, false, 0);
                }
                s.growHours = bed.growHours;
                s.sinceWaterHours = bed.sinceWaterHours;
                s.regrowing = bed.regrowing;
                s.dead = bed.dead;
                ++beds;
                std::cout << "[harness] grow: bed stage " << garden::stage(bed) << (bed.dead ? " dead" : "")
                          << (garden::ripe(bed) ? " ripe" : "") << '\n';
            });
        if (beds == 0) fail(command, "no garden beds");
    }
    else if (op == "locks")
    {
        // locks: list the locked doors and the alarms (to the log).
        // locks arm: the building whose locked door is nearest gets an alarm.
        const LayoutData& layout = game.mMapGenerator.layoutData();
        if (a.size() > 1 && lower(a[1]) == "arm")
        {
            const glm::vec2 me = glm::vec2(game.playerTile());
            int building = 0;
            float bestD = 0.0f;
            for (const auto& lock : game.mLocks)
            {
                const float d = glm::length(glm::vec2(lock.tile) - me);
                if (building == 0 || d < bestD)
                {
                    building = lock.building;
                    bestD = d;
                }
            }
            if (building == 0)
            {
                fail(command, "no locked doors");
                return;
            }
            for (auto& lock : game.mLocks)
            {
                if (lock.building == building) lock.alarm = true;
            }
            std::erase(game.mAlarmsSpent, building);
            game.addAlarmBox(building);
            std::cout << "[harness] locks arm: building " << building << '\n';
        }
        int alarms = 0;
        for (const auto& lock : game.mLocks)
        {
            const auto kind = buildingkinds::kindOf(layout.buildings[static_cast<std::size_t>(lock.building - 1)]);
            alarms += lock.alarm ? 1 : 0;
            std::cout << "[harness] lock: " << lock.tile.x << "," << lock.tile.y << " " << buildingkinds::name(kind)
                      << (lock.alarm ? " alarm" : "") << (lock.edge ? "" : " (inner)") << '\n';
        }
        std::cout << "[harness] locks: " << game.mLocks.size() << " locked doors, " << alarms << " with alarms, "
                  << game.mAlarms.size() << " ringing\n";
    }
    else if (op == "fridge")
    {
        // fridge mark: note the condition of a food in the nearest fridge.
        const glm::vec2 here = game.playerWorldCenter();
        Entity best = kInvalidEntity;
        float bestD = 0.0f;
        glm::ivec2 bestTile(-1);
        for (const FurniturePiece& piece : game.mFurniture)
        {
            if (piece.kind != FurnitureKind::Fridge) continue;
            const float d = glm::length(glm::vec2(piece.tile) * 32.0f + glm::vec2(16.0f) - here);
            if (best != kInvalidEntity && d >= bestD) continue;
            game.mWorld.forEach<Interactable, Inventory, Transform>(
                [&](Entity e, Interactable&, Inventory&, Transform& t)
                {
                    if (static_cast<int>(t.x) / 32 == piece.tile.x && static_cast<int>(t.y) / 32 == piece.tile.y)
                    {
                        best = e;
                        bestD = d;
                        bestTile = piece.tile;
                    }
                });
        }
        if (best == kInvalidEntity)
        {
            fail(command, "no fridge");
            return;
        }
        Inventory& inv = game.mWorld.getComponent<Inventory>(best);
        InventorySlot* food = nullptr;
        for (InventorySlot& slot : inv.slots)
        {
            const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? game.mItemDatabase.find(slot.itemId) : nullptr;
            if (item != nullptr && item->spoilHours > 0.0f)
            {
                food = &slot;
                break;
            }
        }
        if (food == nullptr)
        {
            // Stock it: a fresh apple, so there is something to watch.
            const ItemDef* apple = game.mItemDatabase.findByName("Apple");
            if (apple == nullptr || InventorySystem::addItem(inv, game.mItemDatabase, apple->id, 1).added < 1)
            {
                fail(command, "the fridge holds no food and takes none");
                return;
            }
            for (InventorySlot& slot : inv.slots)
            {
                if (slot.itemId == apple->id) food = &slot;
            }
            if (food == nullptr) return;
            food->condition = 1.0f;
        }
        mFridgeItem = food->itemId;
        mFridgeCondition = food->condition;
        mFridgeHour = game.absoluteHour();
        mFridgeTile = bestTile;
        std::cout << "[harness] fridge mark: " << game.mItemDatabase.find(food->itemId)->name << " at " << food->condition
                  << " (power " << (game.powerOn() ? "on" : "off") << ")\n";
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
        game.mDevGodMode = mGodMode;
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
    else if (op == "set" && a.size() > 1 && lower(a[1]) == "skill")
    {
        // set skill NAME LEVEL (phase 5).
        if (!need(3)) return;
        const auto skill = skillrules::parse(a[2]);
        const Entity player = game.mPlayerEntity;
        if (!skill || player == kInvalidEntity || !game.mWorld.hasComponent<Skills>(player))
        {
            fail(command, "cannot set skill '" + a[2] + "'");
            return;
        }
        skillrules::setLevel(game.mWorld.getComponent<Skills>(player), *skill, std::stoi(a[3]), game.absoluteHour());
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
            else if (stat == "sleepdebt" || stat == "debt") { needs.sleepDebt = value; applied = true; }
            else if (stat == "illness") { needs.illness = value; applied = true; }
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
        if (stat == "rebleeds")
        {
            game.mRebleeds = static_cast<int>(value);
            applied = true;
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
                wounds.active.push_back(woundrules::make(WoundType::Cut, BodyPart::Torso, WoundDepth::Shallow));
            }
            if (world.hasComponent<Health>(player))
            {
                gameshared::syncHealthWithWounds(world.getComponent<Health>(player), wounds);
            }
            applied = true;
        }
        if (stat == "bleeding")
        {
            // set bleeding SECONDS: bleeding for that long (0 stops it).
            if (world.hasComponent<Bleeding>(player)) world.removeComponent<Bleeding>(player);
            if (value > 0.0f)
            {
                Bleeding bleeding{};
                bleeding.bleedRate = 0.2f;
                bleeding.remainingSeconds = value;
                world.addComponent<Bleeding>(player, bleeding);
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
        // trait NAME: add one trait to the player (health and carry follow).
        // trait plain: no bad traits (a Clumsy player's steps are louder).
        if (!need(1)) return;
        const Entity player = game.mPlayerEntity;
        const Traits* own = game.playerTraits();
        if (player == kInvalidEntity || own == nullptr)
        {
            fail(command, "no player");
            return;
        }
        Traits traits = *own;
        if (lower(a[1]) == "plain")
        {
            traits.negatives.clear();
        }
        else if (const auto good = traitrules::parsePositive(a[1]))
        {
            if (!traits.has(*good)) traitrules::toggle(traits, *good);
        }
        else if (const auto bad = traitrules::parseNegative(a[1]))
        {
            if (!traits.has(*bad)) traitrules::toggle(traits, *bad);
        }
        else
        {
            fail(command, "cannot give trait '" + a[1] + "'");
            return;
        }
        // Skills are kept; health is refilled to the new maximum.
        Skills skills = game.mWorld.hasComponent<Skills>(player) ? game.mWorld.getComponent<Skills>(player) : Skills{};
        game.applyCharacter(traits, true);
        game.mWorld.getComponent<Skills>(player) = skills;
    }
    else if (op == "wound")
    {
        // wound TYPE PART [deep] [dirty]: give the player a wound (phase 5).
        // TYPE cut|bite|bruise|fracture; PART head|torso|larm|rarm|lleg|rleg.
        if (!need(2)) return;
        const Entity player = game.mPlayerEntity;
        if (player == kInvalidEntity)
        {
            fail(command, "no player");
            return;
        }
        const std::string type = lower(a[1]);
        const std::string part = lower(a[2]);
        WoundType woundType = WoundType::Cut;
        if (type == "bite") woundType = WoundType::Bite;
        else if (type == "bruise") woundType = WoundType::Bruise;
        else if (type == "fracture" || type == "broken") woundType = WoundType::Fracture;
        else if (type != "cut")
        {
            fail(command, "unknown wound '" + type + "'");
            return;
        }
        BodyPart bodyPart = BodyPart::Torso;
        if (part == "head") bodyPart = BodyPart::Head;
        else if (part == "larm" || part == "leftarm" || part == "arm") bodyPart = BodyPart::LeftArm;
        else if (part == "rarm" || part == "rightarm") bodyPart = BodyPart::RightArm;
        else if (part == "lleg" || part == "leftleg" || part == "leg") bodyPart = BodyPart::LeftLeg;
        else if (part == "rleg" || part == "rightleg") bodyPart = BodyPart::RightLeg;
        else if (part != "torso")
        {
            fail(command, "unknown part '" + part + "'");
            return;
        }
        bool deep = false;
        bool dirty = false;
        for (std::size_t i = 3; i < a.size(); ++i)
        {
            deep = deep || lower(a[i]) == "deep";
            dirty = dirty || lower(a[i]) == "dirty";
        }
        World& world = game.mWorld;
        if (!world.hasComponent<Wounds>(player)) world.addComponent<Wounds>(player, Wounds{});
        Wounds& wounds = world.getComponent<Wounds>(player);
        wounds.active.push_back(woundrules::make(woundType, bodyPart, deep ? WoundDepth::Deep : WoundDepth::Shallow, dirty));
        if (world.hasComponent<Health>(player)) gameshared::syncHealthWithWounds(world.getComponent<Health>(player), wounds);
    }
    else if (op == "treat")
    {
        // treat ITEM [WOUND]: F with this item from the pack, as the BODY
        // tab does (WOUND: its index in the list; the worst it helps if left out).
        if (!need(1)) return;
        const ItemDef* item = game.mItemDatabase.findByName(a[1]);
        if (item == nullptr) item = game.mItemDatabase.findByKey(lower(a[1]));
        const Entity player = game.mPlayerEntity;
        if (item == nullptr || player == kInvalidEntity || !game.mWorld.hasComponent<Inventory>(player))
        {
            fail(command, "cannot treat with '" + a[1] + "'");
            return;
        }
        const Inventory& inv = game.mWorld.getComponent<Inventory>(player);
        int slot = -1;
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            if (inv.slots[static_cast<std::size_t>(i)].itemId == item->id && inv.slots[static_cast<std::size_t>(i)].count > 0) slot = i;
        }
        const int wound = a.size() > 2 ? std::stoi(a[2]) : -1;
        if (slot < 0 || !game.treatWithItem(slot, wound)) fail(command, "nothing to treat with " + a[1]);
    }
    else if (op == "use")
    {
        // use ITEM: F with this item from the pack (a book, a keepsake, coffee).
        if (!need(1)) return;
        const ItemDef* item = game.mItemDatabase.findByName(a[1]);
        if (item == nullptr) item = game.mItemDatabase.findByKey(lower(a[1]));
        const Entity player = game.mPlayerEntity;
        if (item == nullptr || player == kInvalidEntity || !game.mWorld.hasComponent<Inventory>(player))
        {
            fail(command, "cannot use '" + a[1] + "'");
            return;
        }
        Inventory& inv = game.mWorld.getComponent<Inventory>(player);
        int slot = -1;
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            if (inv.slots[static_cast<std::size_t>(i)].itemId == item->id && inv.slots[static_cast<std::size_t>(i)].count > 0) slot = i;
        }
        if (slot < 0)
        {
            fail(command, "no " + a[1] + " in the pack");
            return;
        }
        // Put it on the belt and press F, as a player would.
        if (slot >= 8) std::swap(inv.slots[static_cast<std::size_t>(slot)], inv.slots[static_cast<std::size_t>(inv.activeSlot)]);
        else inv.activeSlot = slot;
        game.mShowInventory = false;
        game.mInput.setKeyState(SDL_SCANCODE_F, true);
        mPendingReleases.push_back({mTick + 1, false, SDL_SCANCODE_F, 0});
    }
    else if (op == "tune")
    {
        if (!need(2)) return;
        Tuning::set(a[1], std::stof(a[2]));
        std::cout << "[harness] tune " << a[1] << " = " << Tuning::get(a[1], 0.0f) << '\n';
    }
    else if (op == "mark")
    {
        if (a.size() > 1 && lower(a[1]) == "sim")
        {
            // mark sim: remember the zombie count and noise events (expect sim).
            mSimZombies = 0;
            game.mWorld.forEach<ZombieAI, Health>([&](Entity, ZombieAI&, Health& h) { if (h.current > 0.0f) ++mSimZombies; });
            mSimNoises = game.mNoiseModel.emittedTotal();
            return;
        }
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
        {
            fail(command, "no player");
            return;
        }
        const Transform& t = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        mMark = glm::ivec2(static_cast<int>(t.x + 16.0f) / 32, static_cast<int>(t.y + 16.0f) / 32);
        mMarkWorld = glm::vec2(t.x + 16.0f, t.y + 16.0f);
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
        if (want == "message" || want == "prompt")
        {
            // expect message|prompt TEXT...: the line on screen holds TEXT.
            if (!need(2)) return;
            std::string text;
            for (std::size_t i = 2; i < a.size(); ++i) text += (i > 2 ? " " : "") + a[i];
            const std::string have = want == "message" ? (game.mInteractionMessageTimer > 0.0f ? game.mInteractionMessage : std::string{})
                                                       : game.mProximityPrompt;
            if (lower(have).find(lower(text)) == std::string::npos) fail(command, "expected " + want + " '" + text + "', it is '" + have + "'");
            else std::cout << "[harness] tick " << mTick << ": " << want << " '" << have << "'\n";
            return;
        }
        if (want == "power")
        {
            if (!need(2)) return;
            const bool on = game.powerOn();
            if (on != (lower(a[2]) == "on")) fail(command, std::string("expected power ") + a[2] + ", it is " + (on ? "on" : "off"));
            return;
        }
        if (want == "alarm")
        {
            // expect alarm on|off: one is ringing, or none.
            if (!need(2)) return;
            const bool ringing = !game.mAlarms.empty();
            if (ringing != (lower(a[2]) == "on")) fail(command, std::string("expected alarm ") + a[2] + ", it is " + (ringing ? "ringing" : "quiet"));
            else std::cout << "[harness] tick " << mTick << ": alarm " << (ringing ? "ringing" : "quiet") << '\n';
            return;
        }
        if (want == "locked" || want == "unlocked")
        {
            // expect locked|unlocked: the door nearest the player.
            const Doors::Door* door = game.mDoors.nearest(game.mTileMap, game.playerWorldCenter(), 1.0e9f);
            if (door == nullptr)
            {
                fail(command, "no doors");
                return;
            }
            const bool locked = game.doorLocked(door->tile);
            if (locked != (want == "locked")) fail(command, std::string("expected the door ") + want + ", it is " + (locked ? "locked" : "unlocked"));
            return;
        }
        if (want == "garden")
        {
            // expect garden STAGE|dead|ripe: the nearest garden bed.
            if (!need(2)) return;
            const glm::vec2 here = game.playerWorldCenter();
            const Structure* bestBed = nullptr;
            float bestD = 0.0f;
            game.mWorld.forEach<Structure, Transform>(
                [&](Entity, Structure& s, Transform& t)
                {
                    if (s.type != StructureType::GardenBed) return;
                    const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here);
                    if (bestBed == nullptr || d < bestD)
                    {
                        bestBed = &s;
                        bestD = d;
                    }
                });
            if (bestBed == nullptr)
            {
                fail(command, "no garden bed");
                return;
            }
            garden::Bed bed;
            bed.growHours = bestBed->growHours;
            bed.sinceWaterHours = bestBed->sinceWaterHours;
            bed.regrowing = bestBed->regrowing;
            bed.dead = bestBed->dead;
            const std::string have = bed.dead ? "dead" : garden::ripe(bed) ? "ripe" : std::to_string(garden::stage(bed));
            const std::string w = lower(a[2]);
            const bool ok = w == have || (w != "dead" && w != "ripe" && !bed.dead && std::to_string(garden::stage(bed)) == w);
            if (!ok) fail(command, "expected garden " + w + ", it is " + have);
            else std::cout << "[harness] tick " << mTick << ": garden " << have << '\n';
            return;
        }
        if (want == "fridgerate")
        {
            // expect fridgerate LO HI: since "fridge mark", the food spoiled at
            // LO..HI times its normal speed (by condition per game hour).
            if (!need(3)) return;
            if (mFridgeItem < 0)
            {
                fail(command, "no fridge mark");
                return;
            }
            float now = -1.0f;
            game.mWorld.forEach<Interactable, Inventory, Transform>(
                [&](Entity, Interactable&, Inventory& inv, Transform& t)
                {
                    if (static_cast<int>(t.x) / 32 != mFridgeTile.x || static_cast<int>(t.y) / 32 != mFridgeTile.y) return;
                    for (const InventorySlot& slot : inv.slots)
                    {
                        if (slot.itemId == mFridgeItem && slot.count > 0) now = std::max(now, slot.condition);
                    }
                });
            const ItemDef* item = game.mItemDatabase.find(mFridgeItem);
            const double hours = game.absoluteHour() - mFridgeHour;
            if (now < 0.0f || item == nullptr || hours <= 0.01)
            {
                fail(command, "the marked food is gone or no time passed");
                return;
            }
            const double normal = hours / static_cast<double>(item->spoilHours);
            const double rate = (static_cast<double>(mFridgeCondition) - static_cast<double>(now)) / normal;
            std::cout << "[harness] tick " << mTick << ": fridge food " << mFridgeCondition << " -> " << now << " over " << hours
                      << " h: " << rate << "x normal\n";
            if (rate < std::stod(a[2]) || rate > std::stod(a[3])) fail(command, "spoiled at " + std::to_string(rate) + "x normal");
            return;
        }
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
        if (want == "door")
        {
            // expect door boarded N: the nearest door has exactly N boards.
            if (!need(3) || lower(a[2]) != "boarded")
            {
                fail(command, "expect door boarded N");
                return;
            }
            const Doors::Door* door = game.mDoors.nearest(game.mTileMap, game.playerWorldCenter(), 1.0e9f);
            const int boards = door != nullptr ? game.mDoors.boards(door->tile) : -1;
            if (boards != std::stoi(a[3])) fail(command, "expected " + a[3] + " boards, there are " + std::to_string(boards));
            else std::cout << "[harness] tick " << mTick << ": door boards " << boards << '\n';
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
            const bool ok = mood == "coming" ? busy > 0 : mood == "all" ? near > 0 && busy == near : mood == "calm" ? busy == 0 : mood == "inside" ? inside > 0 : mood == "outside" ? inside == 0 : false;
            const std::string have = std::to_string(near) + " near, " + std::to_string(busy) + " after something, " +
                                     std::to_string(inside) + " inside";
            if (!ok) fail(command, "expected zombies " + mood + ": " + have);
            else std::cout << "[harness] tick " << mTick << ": zombies " << have << '\n';
            return;
        }
        if (want == "kind")
        {
            // expect kind KIND MOOD R: of the dead of KIND (or "any") within
            // R tiles: "near" some, "coming" some after something, "calm"
            // none after anything, "dormant" some lying in wait, "awake" none
            // lying in wait, "unmoved" one the player struck and did not move,
            // "far" none at all.
            if (!need(4)) return;
            const std::string kindName = lower(a[2]);
            ZombieKind kind = ZombieKind::Shambler;
            const bool any = kindName == "any";
            if (!any && !zombiekinds::parseKind(kindName, kind))
            {
                fail(command, "unknown kind '" + kindName + "'");
                return;
            }
            const std::string mood = lower(a[3]);
            const float range = std::stof(a[4]) * 32.0f;
            const glm::vec2 here = game.playerWorldCenter();
            int near = 0;
            int busy = 0;
            int dormant = 0;
            int unmoved = 0;
            float nearest = 1.0e9f;
            game.mWorld.forEach<ZombieAI, Transform, Health>(
                [&](Entity, ZombieAI& ai, Transform& t, Health& h)
                {
                    if (h.current <= 0.0f || (!any && ai.kind != kind)) return;
                    const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here);
                    nearest = std::min(nearest, d / 32.0f);
                    if (d > range) return;
                    ++near;
                    if (ai.state == ZombieState::Investigate || ai.state == ZombieState::Chase ||
                        ai.state == ZombieState::Attack)
                    {
                        ++busy;
                    }
                    if (ai.dormant) ++dormant;
                    if (ai.lastDamagedBy == game.mPlayerEntity && glm::length(ai.knock) <= 1.0f) ++unmoved;
                });
            bool ok = false;
            if (mood == "near") ok = near > 0;
            else if (mood == "coming") ok = busy > 0;
            else if (mood == "calm") ok = near > 0 && busy == 0;
            else if (mood == "dormant") ok = dormant > 0;
            else if (mood == "awake") ok = near > 0 && dormant == 0;
            else if (mood == "unmoved") ok = unmoved > 0;
            else if (mood == "far") ok = near == 0;
            const std::string have = std::to_string(near) + " near (nearest " + std::to_string(static_cast<int>(nearest)) +
                                     " tiles), " + std::to_string(busy) + " after something, " + std::to_string(dormant) +
                                     " dormant, " + std::to_string(unmoved) + " struck and unmoved";
            if (!ok) fail(command, "expected " + kindName + " " + mood + ": " + have);
            else std::cout << "[harness] tick " << mTick << ": " << kindName << " " << mood << ": " << have << std::endl;
            return;
        }
        if (want == "trail")
        {
            // expect trail N: at least N blood markers on the ground ("none": none).
            if (!need(2)) return;
            const int have = static_cast<int>(game.mBloodTrail.markers().size());
            const bool ok = lower(a[2]) == "none" ? have == 0 : have >= std::stoi(a[2]);
            if (!ok) fail(command, "expected trail " + a[2] + ", there are " + std::to_string(have) + " markers");
            else std::cout << "[harness] tick " << mTick << ": " << have << " blood markers" << std::endl;
            return;
        }
        if (want == "perf")
        {
            // expect perf AVG WORST: the last `perf` run's ticks, in ms.
            if (!need(3)) return;
            const double average = mPerfTotalMs / static_cast<double>(std::max(1, mPerfTicks));
            if (mPerfTicks == 0 || mPerfTicksLeft > 0) fail(command, "no finished perf run");
            else if (average >= std::stod(a[2]) || mPerfWorstMs >= std::stod(a[3]))
                fail(command, "average " + std::to_string(average) + " ms, worst " + std::to_string(mPerfWorstMs) + " ms");
            return;
        }
        if (want == "horde")
        {
            // expect horde on|off: a horde is crossing the town, or not.
            if (!need(2)) return;
            const bool on = lower(a[2]) == "on";
            if (game.mHorde.active != on) fail(command, std::string("expected horde ") + a[2]);
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
        if (want == "condition" || want == "nocondition")
        {
            // expect condition NAME...: the HUD's conditions list shows it (phase 5).
            if (!need(2)) return;
            game.buildHudState();
            std::string name = a[2];
            for (std::size_t i = 3; i < a.size(); ++i) name += " " + a[i];
            bool found = false;
            std::string shown;
            for (const HudStatus& status : game.mHudState.conditions)
            {
                shown += (shown.empty() ? "" : ", ") + status.label;
                if (lower(status.label) == lower(name)) found = true;
            }
            if (found != (want == "condition")) fail(command, "conditions are: " + shown);
            else std::cout << "[harness] tick " << mTick << ": conditions " << shown << '\n';
            return;
        }
        if (want == "skill")
        {
            // expect skill NAME LEVEL: at least this level.
            if (!need(3)) return;
            const auto skill = skillrules::parse(a[2]);
            const int have = skill ? game.skillLevel(*skill) : -1;
            if (!skill || have < std::stoi(a[3])) fail(command, "expected " + a[2] + " " + a[3] + ", it is " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << a[2] << " " << have << '\n';
            return;
        }
        if (want == "carry" || want == "health")
        {
            // expect carry KG / expect health N: the limit and current health.
            if (!need(2)) return;
            const float wantValue = std::stof(a[2]);
            float have = -1.0f;
            if (want == "carry") have = game.playerCarryLimit();
            else if (game.mPlayerEntity != kInvalidEntity && game.mWorld.hasComponent<Health>(game.mPlayerEntity))
                have = game.mWorld.getComponent<Health>(game.mPlayerEntity).current;
            if (std::abs(have - wantValue) > 0.51f) fail(command, "expected " + want + " " + a[2] + ", it is " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << want << " " << have << '\n';
            return;
        }
        if (want == "need")
        {
            // expect need illness|hunger|thirst below|above V (phase 6).
            if (!need(4)) return;
            const Entity player = game.mPlayerEntity;
            if (player == kInvalidEntity || !game.mWorld.hasComponent<Needs>(player)) { fail(command, "no needs"); return; }
            const Needs& needs = game.mWorld.getComponent<Needs>(player);
            const std::string which = lower(a[2]);
            const float have = which == "illness" ? needs.illness : which == "hunger" ? needs.hunger : needs.thirst;
            const float bound = std::stof(a[4]);
            const bool ok = lower(a[3]) == "below" ? have < bound : have > bound;
            if (!ok) fail(command, "expected " + a[2] + " " + a[3] + " " + a[4] + ", it is " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << a[2] << " " << have << '\n';
            return;
        }
        if (want == "mind")
        {
            // expect mind panic|depression below|above V
            if (!need(4)) return;
            const Entity player = game.mPlayerEntity;
            if (player == kInvalidEntity || !game.mWorld.hasComponent<MentalState>(player)) { fail(command, "no mind"); return; }
            const MentalState& mental = game.mWorld.getComponent<MentalState>(player);
            const float have = lower(a[2]) == "panic" ? mental.panic : mental.depression;
            const float bound = std::stof(a[4]);
            const bool ok = lower(a[3]) == "below" ? have < bound : have > bound;
            if (!ok) fail(command, "expected " + a[2] + " " + a[3] + " " + a[4] + ", it is " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << a[2] << " " << have << '\n';
            return;
        }
        if (want == "signs")
        {
            // expect signs N: at least N false silhouettes shown so far.
            if (!need(2)) return;
            const int have = game.mHallucinations.silhouettesShown();
            if (have < std::stoi(a[2])) fail(command, "expected " + a[2] + " silhouettes, saw " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << have << " silhouettes, " << game.mHallucinations.signsShown() << " signs" << '\n';
            return;
        }
        if (want == "sim")
        {
            // expect sim: the zombie count and noise events match the last
            // "mark sim" (a false sign changes neither).
            int zombies = 0;
            game.mWorld.forEach<ZombieAI, Health>([&](Entity, ZombieAI&, Health& h) { if (h.current > 0.0f) ++zombies; });
            const std::uint64_t noises = game.mNoiseModel.emittedTotal();
            if (zombies != mSimZombies || noises != mSimNoises)
            {
                fail(command, "the simulation moved: zombies " + std::to_string(mSimZombies) + " -> " + std::to_string(zombies) +
                                  ", noises " + std::to_string(mSimNoises) + " -> " + std::to_string(noises));
            }
            else
            {
                std::cout << "[harness] tick " << mTick << ": simulation unchanged (" << zombies << " zombies, " << noises
                          << " noises)" << '\n';
            }
            return;
        }
        if (want == "moved")
        {
            // expect moved MIN MAX: tiles walked since "mark" (straight line).
            if (!need(3)) return;
            const Transform& t = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
            const glm::vec2 now(t.x + 16.0f, t.y + 16.0f);
            const float tiles = glm::length(now - mMarkWorld) / 32.0f;
            if (tiles < std::stof(a[2]) || tiles > std::stof(a[3]))
                fail(command, "moved " + std::to_string(tiles) + " tiles, expected " + a[2] + " to " + a[3]);
            else std::cout << "[harness] tick " << mTick << ": moved " << tiles << " tiles" << '\n';
            return;
        }
        if (want == "rebleeds")
        {
            // expect rebleeds MIN MAX: deep wounds opened again since "set rebleeds 0".
            if (!need(3)) return;
            const int have = game.mRebleeds;
            if (have < std::stoi(a[2]) || have > std::stoi(a[3]))
                fail(command, "expected " + a[2] + " to " + a[3] + " rebleeds, there were " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << have << " rebleeds" << '\n';
            return;
        }
        if (want == "action")
        {
            // expect action none|busy: a held action (boarding, treating) is or is not under way.
            if (!need(2)) return;
            const bool busy = game.mBaseAction.kind != Game::BaseAction::Kind::None;
            if (busy != (lower(a[2]) == "busy")) fail(command, std::string("expected ") + a[2] + ", " + (busy ? "busy: " + game.mBaseAction.label : "idle"));
            else std::cout << "[harness] tick " << mTick << ": action " << (busy ? game.mBaseAction.label : "none") << '\n';
            return;
        }
        if (want == "wounds")
        {
            // expect wounds N [bleeding|dressed|stitched|splinted|dirty|undressed]: N wounds (with that).
            if (!need(2)) return;
            const Entity player = game.mPlayerEntity;
            const std::vector<Wound> wounds = player != kInvalidEntity && game.mWorld.hasComponent<Wounds>(player)
                                                  ? game.mWorld.getComponent<Wounds>(player).active
                                                  : std::vector<Wound>{};
            const std::string with = a.size() > 3 ? lower(a[3]) : "";
            int have = 0;
            if (with == "bleeding") have = game.mWorld.hasComponent<Bleeding>(player) ? 1 : 0;
            for (const Wound& w : wounds)
            {
                if (with.empty() || (with == "dressed" && w.dressed) || (with == "undressed" && !w.dressed) ||
                    (with == "stitched" && w.stitched) || (with == "splinted" && w.splinted) ||
                    (with == "dirty" && w.contaminated))
                {
                    ++have;
                }
            }
            if (have != std::stoi(a[2])) fail(command, "expected " + a[2] + " " + with + " wounds, there are " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << have << " " << with << " wounds" << '\n';
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
