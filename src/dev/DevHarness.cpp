#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "components/Health.h"
#include "components/Inventory.h"
#include "components/LegacyLink.h"
#include "components/MentalState.h"
#include "components/Needs.h"
#include "components/Stamina.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "core/Game.h"
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
                    "inventory_items,z_investigate,z_chase,z_attack,z_migrating,indoors,pop_target\n";
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

    while (mWaitRemaining == 0 && mPc < mCommands.size() && !mFinished)
    {
        execute(game, mCommands[mPc++]);
    }

    if (mWaitRemaining > 0)
    {
        --mWaitRemaining;
    }
    else if (mPc >= mCommands.size())
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
    else if (op == "zombie")
    {
        // zombie DX DY: a zombie DX, DY tiles from the player, heading for them.
        if (!need(2)) return;
        if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
        {
            fail(command, "no player");
            return;
        }
        const int tileW = std::max(1, game.mTileMap.tileWidth());
        const int tileH = std::max(1, game.mTileMap.tileHeight());
        const Transform& pt = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        const glm::vec2 playerCentre(pt.x + 16.0f, pt.y + 16.0f);
        const int tx = static_cast<int>(playerCentre.x / static_cast<float>(tileW)) + std::stoi(a[1]);
        const int ty = static_cast<int>(playerCentre.y / static_cast<float>(tileH)) + std::stoi(a[2]);
        if (game.mTileMap.isSolid(tx, ty))
        {
            fail(command, "tile " + std::to_string(tx) + "," + std::to_string(ty) + " is solid");
            return;
        }
        EntityFactory::ZombieParams params;
        params.topLeftWorld = glm::vec2(static_cast<float>(tx * tileW), static_cast<float>(ty * tileH));
        params.health = 60.0f;
        params.baseMoveSpeed = 112.0f;
        params.spawnDay = game.mCurrentDay;
        const Entity zombie = EntityFactory::spawnZombie(game.mWorld, game.mSpriteSheet, params);
        if (zombie != kInvalidEntity && game.mWorld.hasComponent<ZombieAI>(zombie))
        {
            ZombieAISystem::sendTo(game.mWorld.getComponent<ZombieAI>(zombie), playerCentre, false);
        }
    }
    else if (op == "doors")
    {
        std::cout << "[harness] tick " << mTick << ": doors closed=" << game.mDoors.count(game.mTileMap, DoorState::Closed)
                  << " open=" << game.mDoors.count(game.mTileMap, DoorState::Open)
                  << " broken=" << game.mDoors.count(game.mTileMap, DoorState::Broken) << '\n';
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
        if (!need(1)) return;
        const std::string kind = lower(a[1]);
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
            const glm::ivec2 sides[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const glm::ivec2& side : sides)
            {
                const glm::ivec2 tile = door->tile + side;
                if (!game.mTileMap.isSolid(tile.x, tile.y) && game.mTileMap.getBuildingId(tile.x, tile.y) > 0)
                {
                    execute(game, Command{command.line, {"teleport", std::to_string(tile.x), std::to_string(tile.y)}});
                    return;
                }
            }
            fail(command, "no floor inside the nearest door");
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
        if (stat == "stamina" && world.hasComponent<Stamina>(player))
        {
            world.getComponent<Stamina>(player).current = value;
            applied = true;
        }
        if (!applied) fail(command, "unknown stat '" + stat + "'");
    }
    else if (op == "metrics")
    {
        if (!need(1)) return;
        mMetricsInterval = std::max(0, std::stoi(a[1]));
        if (mMetrics && mMetricsInterval > 0) writeMetricsRow(game);
    }
    else if (op == "shot")
    {
        if (need(1) && !capture(game, a[1])) fail(command, "screenshot failed");
    }
    else if (op == "expect")
    {
        if (!need(1)) return;
        const std::string want = lower(a[1]);
        const std::string have = runStateName(static_cast<int>(game.mRunState));
        bool ok = false;
        if (want == "alive") ok = game.mRunState == Game::RunState::Playing || game.mRunState == Game::RunState::Paused;
        else if (want == "dead") ok = game.mRunState == Game::RunState::Dead;
        else ok = want == have;
        if (!ok) fail(command, "expected " + want + ", state is " + have);
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
             << game.mThreatDirector.populationTarget() << '\n';
    mMetrics.flush();
}
