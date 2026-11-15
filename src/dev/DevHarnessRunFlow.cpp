// Harness commands for the run flow (overhaul phase 13): the window's size
// and the interface's, the settings file, teaching, day length, the demo,
// the extra checkpoints and the standing aims. See DevHarness.h.
#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "core/Game.h"
#include "components/Corpse.h"
#include "core/GameShared.h"
#include "world/FirstDays.h"

namespace
{
std::string lowered(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool onOff(const std::string& word, bool& out)
{
    const std::string w = lowered(word);
    if (w == "on" || w == "true" || w == "1" || w == "toggle" || w == "once") out = true;
    else if (w == "off" || w == "false" || w == "0" || w == "hold") out = false;
    else return false;
    return true;
}

// An action by its settings name ("shove").
int actionNamed(const std::string& name)
{
    for (int i = 0; i < kActionCount; ++i)
    {
        if (lowered(name) == bindings::actionKey(static_cast<Action>(i))) return i;
    }
    return -1;
}
} // namespace

bool DevHarness::runFlowCommand(Game& game, const Command& command)
{
    const auto& a = command.args;
    const std::string op = lowered(a[0]);
    const auto need = [&](std::size_t count)
    {
        if (a.size() < count + 1)
        {
            fail(command, "'" + op + "' needs " + std::to_string(count) + " argument(s)");
            return false;
        }
        return true;
    };

    if (op == "corpses" && a.size() > 2 && lowered(a[1]) == "age")
    {
        // corpses age S: every body on the map has lain there S seconds (flies after a day).
        const float age = std::stof(a[2]);
        int count = 0;
        game.mWorld.forEach<Corpse>(
            [&](Entity, Corpse& c)
            {
                c.age = age;
                ++count;
            });
        std::cout << "[harness] tick " << mTick << ": " << count << " bodies aged " << age << " s\n";
        return true;
    }
    if (op == "goto" && a.size() > 1 && lowered(a[1]) == "clock")
    {
        // goto clock: just in front of the town clock's wall (chosen at
        // the first frame drawn with screen effects on).
        if (game.mClockBuilding < 0)
        {
            fail(command, "no town clock (screen effects off, or no building for it)");
            return true;
        }
        const auto& r = game.mMapGenerator.layoutData().buildings[static_cast<std::size_t>(game.mClockBuilding)];
        const int x = r.x + r.w / 2;
        for (int y = r.y + r.h + 1; y < r.y + r.h + 8; ++y)
        {
            if (game.mTileMap.isSolid(x, y)) continue;
            execute(game, Command{command.line, {"teleport", std::to_string(x), std::to_string(y)}});
            return true;
        }
        fail(command, "no ground in front of the clock");
        return true;
    }
    if (op == "screen")
    {
        // screen W H: the window's pixels (shots are taken at this size).
        if (!need(2)) return true;
        const int w = std::clamp(std::stoi(a[1]), 320, 3840);
        const int h = std::clamp(std::stoi(a[2]), 240, 2160);
        if (game.mWindow != nullptr) SDL_SetWindowSize(game.mWindow, w, h);
        game.onPixelSize(w, h);
        std::cout << "[harness] tick " << mTick << ": screen " << w << "x" << h << " interface "
                  << game.mWindowWidth << "x" << game.mWindowHeight << " at " << game.mUiScale << "x\n";
        return true;
    }
    if (op == "uisize")
    {
        // uisize auto|1|1.5|2: the interface size setting.
        if (!need(1)) return true;
        const std::string v = lowered(a[1]);
        settings::UiSize size = settings::UiSize::Auto;
        if (v == "1") size = settings::UiSize::X1;
        else if (v == "1.5") size = settings::UiSize::X1_5;
        else if (v == "2") size = settings::UiSize::X2;
        else if (v != "auto")
        {
            fail(command, "uisize auto|1|1.5|2");
            return true;
        }
        game.mSettings.uiSize = size;
        game.onPixelSize(game.mPixelWidth, game.mPixelHeight);
        std::cout << "[harness] tick " << mTick << ": interface " << game.mWindowWidth << "x" << game.mWindowHeight
                  << " at " << game.mUiScale << "x\n";
        return true;
    }
    if (op == "settings")
    {
        // settings reload|save|reset|open|close | settings NAME VALUE
        if (!need(1)) return true;
        const std::string what = lowered(a[1]);
        if (what == "reload")
        {
            game.mSettings = settings::load(game.mSettingsPath);
            game.applySettings();
        }
        else if (what == "save")
        {
            game.saveSettings();
        }
        else if (what == "reset")
        {
            game.mSettings = settings::Settings{};
            game.applySettings();
        }
        else if (what == "open")
        {
            game.openSettings();
        }
        else if (what == "close")
        {
            game.mShowSettings = false;
        }
        else
        {
            if (!need(2)) return true;
            settings::Settings& s = game.mSettings;
            bool flag = false;
            const std::string value = lowered(a[2]);
            if (what == "master" || what == "effects" || what == "ambience" || what == "music" || what == "shake")
            {
                const int level = std::clamp(std::stoi(value), 0, settings::kLevelMax);
                (what == "master" ? s.master : what == "effects" ? s.effects : what == "ambience" ? s.ambience
                 : what == "music" ? s.music : s.shake) = level;
            }
            else if ((what == "teaching" || what == "crouch" || what == "run" || what == "pressonce" || what == "autopause") &&
                     onOff(value, flag))
            {
                (what == "teaching" ? s.teaching : what == "crouch" ? s.crouchToggle : what == "run" ? s.runToggle
                 : what == "pressonce" ? s.pressOnce : s.autoPause) = flag;
            }
            else
            {
                fail(command, "settings NAME VALUE: master|effects|ambience|music|shake 0-10, "
                              "teaching|crouch|run|pressonce|autopause on|off");
                return true;
            }
            game.applySettings();
        }
        std::cout << "[harness] tick " << mTick << ": settings " << what << " (" << game.mSettingsPath << ")\n";
        return true;
    }
    if (op == "teach")
    {
        // teach on|off: teaching prompts and the look around (off for scripts unless asked).
        bool on = false;
        if (!need(1) || !onOff(a[1], on)) return true;
        game.mTeachAllowed = on;
        if (on && game.mSaveManager.runNumber() <= 1) game.mLookAroundFirstLife = true;
        std::cout << "[harness] tick " << mTick << ": teach " << (on ? "on" : "off") << '\n';
        return true;
    }
    if (op == "autosave")
    {
        // autosave on|off: checkpoints at dawn and on coming home.
        bool on = false;
        if (!need(1) || !onOff(a[1], on)) return true;
        game.mAutosaveAllowed = on;
        return true;
    }
    if (op == "daylength")
    {
        // daylength 24|36|48: real minutes in a game day, this world (and the next newrun).
        if (!need(1)) return true;
        const int minutes = daylength::clampMinutes(std::stoi(a[1]));
        game.mDayScale = daylength::scale(minutes);
        RunPresentationSettings presentation = game.mSaveManager.presentationSettings();
        presentation.dayMinutes = minutes;
        game.mSaveManager.setPresentationSettings(presentation);
        std::cout << "[harness] tick " << mTick << ": day length " << minutes << " min (scale " << game.mDayScale << ")\n";
        return true;
    }
    if (op == "demo")
    {
        bool on = false;
        if (!need(1) || !onOff(a[1], on)) return true;
        game.mDemo = on;
        return true;
    }
    if (op == "day" && a.size() > 1 && lowered(a[1]) == "horde")
    {
        // day horde [next]: the first day from 4 (or after today) whose night
        // brings a horde (this seed and mode).
        const bool next = a.size() > 2 && lowered(a[2]) == "next";
        for (int d = next ? game.mCurrentDay + 1 : aims::kFirstAimDay; d < 200; ++d)
        {
            if (!game.isHordeNight(d)) continue;
            game.mCurrentDay = d;
            std::cout << "[harness] tick " << mTick << ": day " << d << " (a horde night)\n";
            return true;
        }
        fail(command, "no horde night before day 200");
        return true;
    }
    if (op == "home" && a.size() > 1 && lowered(a[1]) == "sealed")
    {
        // home sealed: every door of the home boarded three times, every window twice.
        for (const glm::ivec2& tile : game.mFirstDays.homeDoors)
        {
            game.mTileMap.setDoorState(tile.x, tile.y, DoorState::Closed);
            while (game.mDoors.boards(tile) < Doors::kMaxBoards && game.mDoors.addBoard(game.mTileMap, tile)) {}
        }
        for (const Windows::Window& window : game.mWindows.all())
        {
            const glm::ivec2 t = window.spot.tile;
            bool home = false;
            for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
            {
                const glm::ivec2 n = t + side;
                if (game.mTileMap.inBounds(n.x, n.y) && game.mTileMap.getBuildingId(n.x, n.y) == game.mFirstDays.homeBuilding) home = true;
            }
            if (home) game.mWindows.restore(game.mTileMap, t, true, 2.0f * Windows::kBoardHp, false);
        }
        int ways = 0;
        int boarded = 0;
        for (const Game::WayIn& way : game.waysIn(game.mFirstDays.homeBuilding))
        {
            ++ways;
            if (way.boarded) ++boarded;
        }
        std::cout << "[harness] tick " << mTick << ": home sealed, " << boarded << " of " << ways << " ways boarded\n";
        return true;
    }
    if (op == "aim")
    {
        // aim: print the standing aim as the panel shows it | aim choose: choose again now.
        if (a.size() > 1 && lowered(a[1]) == "choose") game.chooseAim();
        game.mAimRefresh = 0.0f;
        game.updateAims(0.0f);
        const aims::Aim aim = game.currentAim();
        std::cout << "[harness] tick " << mTick << ": aim day " << game.mCurrentDay << " " << aims::key(game.mAimKind)
                  << " '" << aim.title << "' " << aim.detail << " rewarded=" << game.mAimsRewarded << '\n';
        for (const aims::Step& step : aim.steps)
        {
            std::cout << "[harness]   [" << (step.done ? 'x' : ' ') << "] " << step.text << '\n';
        }
        return true;
    }

    if (op != "expect" || a.size() < 2) return false;
    const std::string what = lowered(a[1]);
    if (what == "aim")
    {
        // expect aim KEY|none [no]: the standing aim's kind (water, house, tonight, ...),
        // or with "no", anything but that.
        if (!need(2)) return true;
        game.mAimRefresh = 0.0f;
        game.updateAims(0.0f);
        const std::string have = game.mAimKind == aims::Kind::None ? "none" : aims::key(game.mAimKind);
        const bool notThis = a.size() > 3 && lowered(a[3]) == "no";
        if ((have == lowered(a[2])) == notThis) fail(command, "expected aim " + (notThis ? "other than " : std::string{}) + a[2] + ", it is " + have);
        else std::cout << "[harness] tick " << mTick << ": aim " << have << " '" << game.currentAim().title << "'\n";
        return true;
    }
    if (what == "aimdone")
    {
        // expect aimdone [no]: this aim's "Done." was given.
        const bool want = !(a.size() > 2 && lowered(a[2]) == "no");
        const bool have = (game.mAimsRewarded & (1 << static_cast<int>(game.mAimKind))) != 0;
        if (have != want) fail(command, std::string("expected the aim ") + (want ? "done" : "not done"));
        else std::cout << "[harness] tick " << mTick << ": aim done " << (have ? "yes" : "no") << '\n';
        return true;
    }
    if (what == "binding")
    {
        // expect binding ACTION KEY: the action's first key, by its label ("F", "Space").
        if (!need(3)) return true;
        const int action = actionNamed(a[2]);
        if (action < 0)
        {
            fail(command, "unknown action '" + a[2] + "'");
            return true;
        }
        const std::string have = bindings::label(game.mInput.bindings(), static_cast<Action>(action));
        if (lowered(have) != lowered(a[3])) fail(command, "expected " + a[2] + " on " + a[3] + ", it is on " + have);
        else std::cout << "[harness] tick " << mTick << ": " << a[2] << " is " << have << '\n';
        return true;
    }
    if (what == "teach")
    {
        // expect teach ID|none: the teaching prompt on the line now.
        if (!need(2)) return true;
        const std::string have = game.mTeachLine.empty() ? "none" : game.mTeachId;
        if (have != lowered(a[2])) fail(command, "expected teach " + a[2] + ", it is " + have);
        else std::cout << "[harness] tick " << mTick << ": teach " << have << " '" << game.mTeachLine << "'\n";
        return true;
    }
    if (what == "taught" || what == "seen")
    {
        // expect taught ID [no]: used (never shown again); expect seen ID [no]: shown once.
        if (!need(2)) return true;
        const bool want = !(a.size() > 3 && lowered(a[3]) == "no");
        bool have = game.wasTaught(lowered(a[2]));
        if (what == "seen")
        {
            const auto slot = game.mSettings.taught.find(game.mActiveSaveSlot);
            have = slot != game.mSettings.taught.end() && slot->second.count(lowered(a[2])) > 0;
        }
        if (have != want) fail(command, "expected " + a[2] + (want ? " " : " not ") + what);
        else std::cout << "[harness] tick " << mTick << ": " << a[2] << (have ? " " : " not ") << what << '\n';
        return true;
    }
    if (what == "step")
    {
        // expect step TEXT... [done|open]: a line of the objective panel (the first days' plan).
        if (!need(2)) return true;
        std::string text;
        std::string state;
        for (std::size_t i = 2; i < a.size(); ++i)
        {
            const std::string w = lowered(a[i]);
            if (i + 1 == a.size() && (w == "done" || w == "open"))
            {
                state = w;
                break;
            }
            text += (text.empty() ? "" : " ") + a[i];
        }
        const firstdays::Plan plan = firstdays::planFor(game.mCurrentDay, game.mGameHours, game.firstDaysFacts());
        const firstdays::Step* found = nullptr;
        for (const firstdays::Step& step : plan.steps)
        {
            if (lowered(step.text).find(lowered(text)) != std::string::npos) found = &step;
        }
        const bool wantNone = lowered(text) == "none";
        if (state.empty() && found == nullptr && !wantNone) fail(command, "no step '" + text + "' in '" + plan.title + "'");
        else if (!state.empty() && (found == nullptr || found->done != (state == "done")))
            fail(command, "step '" + text + "' is not " + state);
        else std::cout << "[harness] tick " << mTick << ": step '" << text << "' " << (found ? (found->done ? "done" : "open") : "absent") << '\n';
        return true;
    }
    if (what == "nostep")
    {
        // expect nostep TEXT...: no such line in the plan.
        std::string text;
        for (std::size_t i = 2; i < a.size(); ++i) text += (i > 2 ? " " : "") + a[i];
        const firstdays::Plan plan = firstdays::planFor(game.mCurrentDay, game.mGameHours, game.firstDaysFacts());
        for (const firstdays::Step& step : plan.steps)
        {
            if (lowered(step.text).find(lowered(text)) != std::string::npos)
            {
                fail(command, "step '" + step.text + "' is there");
                return true;
            }
        }
        std::cout << "[harness] tick " << mTick << ": no step '" << text << "'\n";
        return true;
    }
    if (what == "hint")
    {
        // expect hint TEXT...|none: the run-end screen's line.
        std::string text;
        for (std::size_t i = 2; i < a.size(); ++i) text += (i > 2 ? " " : "") + a[i];
        const bool none = lowered(text) == "none";
        if (none ? !game.mDeathHint.empty() : lowered(game.mDeathHint).find(lowered(text)) == std::string::npos)
            fail(command, "expected hint '" + text + "', it is '" + game.mDeathHint + "'");
        else std::cout << "[harness] tick " << mTick << ": hint '" << game.mDeathHint << "'\n";
        return true;
    }
    if (what == "retired")
    {
        if (game.mRunState != Game::RunState::Retired) fail(command, "expected a retirement");
        else std::cout << "[harness] tick " << mTick << ": retired: " << game.mDeathCause << " / " << game.mRunEndLegacyLine << '\n';
        return true;
    }
    if (what == "daycard")
    {
        // expect daycard TEXT...: the dawn card's line holds TEXT.
        std::string text;
        for (std::size_t i = 2; i < a.size(); ++i) text += (i > 2 ? " " : "") + a[i];
        const std::string have = game.dawnCard().line;
        if (lowered(have).find(lowered(text)) == std::string::npos) fail(command, "expected card '" + text + "', it is '" + have + "'");
        else std::cout << "[harness] tick " << mTick << ": card '" << have << "'\n";
        return true;
    }
    if (what == "hour")
    {
        // expect hour LO HI: the clock's hour of the day.
        if (!need(3)) return true;
        const double lo = std::stod(a[2]);
        const double hi = std::stod(a[3]);
        if (game.mGameHours < lo || game.mGameHours > hi)
            fail(command, "expected the hour in " + a[2] + "-" + a[3] + ", it is " + std::to_string(game.mGameHours));
        else std::cout << "[harness] tick " << mTick << ": hour " << game.mGameHours << '\n';
        return true;
    }
    if (what == "level")
    {
        // expect level master|effects|ambience|music|shake N: a 0-10 setting.
        if (!need(3)) return true;
        const settings::Settings& s = game.mSettings;
        const std::string name = lowered(a[2]);
        const int have = name == "master" ? s.master : name == "effects" ? s.effects : name == "ambience" ? s.ambience
                         : name == "music" ? s.music : name == "shake" ? s.shake : -1;
        if (have != std::stoi(a[3])) fail(command, "expected " + name + " " + a[3] + ", it is " + std::to_string(have));
        else std::cout << "[harness] tick " << mTick << ": " << name << " " << have << '\n';
        return true;
    }
    if (what == "uiscale")
    {
        if (!need(2)) return true;
        if (std::abs(game.mUiScale - std::stof(a[2])) > 0.01f) fail(command, "expected interface " + a[2] + "x, it is " + std::to_string(game.mUiScale));
        else std::cout << "[harness] tick " << mTick << ": interface " << game.mUiScale << "x\n";
        return true;
    }
    if (what == "settings")
    {
        // expect settings open|closed
        if (!need(2)) return true;
        const bool want = lowered(a[2]) == "open";
        if (game.mShowSettings != want) fail(command, std::string("expected settings ") + (want ? "open" : "closed"));
        else std::cout << "[harness] tick " << mTick << ": settings " << (want ? "open" : "closed") << " note '" << game.mSettingsNote << "'\n";
        return true;
    }
    if (what == "note")
    {
        // expect note TEXT...: the settings screen's last note.
        std::string text;
        for (std::size_t i = 2; i < a.size(); ++i) text += (i > 2 ? " " : "") + a[i];
        if (lowered(game.mSettingsNote).find(lowered(text)) == std::string::npos)
            fail(command, "expected note '" + text + "', it is '" + game.mSettingsNote + "'");
        else std::cout << "[harness] tick " << mTick << ": note '" << game.mSettingsNote << "'\n";
        return true;
    }
    return false;
}
