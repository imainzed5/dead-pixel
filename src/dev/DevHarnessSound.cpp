// Harness commands for sound and mood (overhaul phase 12): sounds, sound,
// fx, tone, listen, and the expect sound|layer|music|duck|listening|fx|idle
// lines. Harness runs are silent (no audio device), but the mixer still
// decides everything it would play, and these read that. See DevHarness.h.
#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "core/Game.h"
#include "core/GameShared.h"

namespace
{
std::string lowered(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// A layer by its key, with or without the "loop_" in front.
LayerId layerNamed(const std::string& name)
{
    LayerId id = layerByKey(name.c_str());
    if (id == LayerId::Count) id = layerByKey(("loop_" + name).c_str());
    return id;
}

std::string fixed(float value, int places = 2)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(places) << value;
    return out.str();
}
} // namespace

bool DevHarness::soundCommand(Game& game, const Command& command)
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
    AudioManager& audio = game.mAudioManager;

    if (op == "sounds")
    {
        // What was played in the last second, the layers that are up, the voices.
        std::cout << "[harness] tick " << mTick << ": sounds (last 1 s):";
        const auto played = audio.recent(1.0);
        if (played.empty()) std::cout << " none";
        for (const AudioManager::Played& p : played)
        {
            std::cout << ' ' << soundKey(p.id) << '@' << fixed(p.volume);
            if (p.positional) std::cout << " d=" << fixed(p.distanceTiles, 1);
            if (p.muffled) std::cout << " muffled";
            if (p.far) std::cout << " far";
            if (p.dropped) std::cout << " dropped";
            std::cout << ';';
        }
        std::cout << "\n[harness]   layers:";
        bool any = false;
        for (int i = 0; i < kLayerCount; ++i)
        {
            const LayerId id = static_cast<LayerId>(i);
            const float now = audio.layerVolume(id);
            const float target = audio.layerTarget(id);
            if (now < 0.005f && target < 0.005f) continue;
            any = true;
            std::cout << ' ' << layerKey(id) << ' ' << fixed(now) << "->" << fixed(target);
        }
        if (!any) std::cout << " none";
        std::cout << "\n[harness]   voices " << audio.activeVoices() << ", duck " << fixed(audio.duck()) << ", music starts "
                  << game.mMusic.starts() << (game.mListening ? ", listening" : "")
                  << (audio.deviceOpen() ? ", device open" : ", no device") << '\n';
        return true;
    }
    if (op == "sound")
    {
        // sound mark: from now, for expect sound. sound KEY [DX DY]: play it
        // at the player, or DX, DY tiles off (through the real mixer: distance,
        // walls, the far tail).
        if (!need(1)) return true;
        if (lowered(a[1]) == "mark")
        {
            mSoundMark = audio.clock();
            std::cout << "[harness] tick " << mTick << ": sound mark\n";
            return true;
        }
        const SoundId id = soundByKey(lowered(a[1]).c_str());
        if (id == SoundId::None)
        {
            fail(command, "unknown sound '" + a[1] + "'");
            return true;
        }
        glm::vec2 at = game.playerWorldCenter();
        const float tilePx = static_cast<float>(std::max(1, game.mTileMap.tileWidth()));
        if (a.size() >= 4 && lowered(a[2]) == "building")
        {
            // sound KEY building N: inside the Nth nearest building that is
            // not the one you stand in (N = 2: two buildings away).
            const glm::ivec2 me = gameshared::worldToTile(game.mTileMap, at);
            const int mine = game.mTileMap.inBounds(me.x, me.y) ? game.mTileMap.getBuildingId(me.x, me.y) : 0;
            std::vector<std::pair<float, glm::vec2>> others;
            const auto& buildings = game.mMapGenerator.layoutData().buildings;
            for (std::size_t i = 0; i < buildings.size(); ++i)
            {
                if (static_cast<int>(i) + 1 == mine) continue;
                const auto& b = buildings[i];
                const glm::vec2 c((static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f) * tilePx,
                                  (static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f) * tilePx);
                others.emplace_back(glm::length(c - at), c);
            }
            std::sort(others.begin(), others.end(), [](const auto& l, const auto& r) { return l.first < r.first; });
            const int n = std::max(1, std::stoi(a[3]));
            if (others.size() < static_cast<std::size_t>(n))
            {
                fail(command, "no building " + a[3] + " away");
                return true;
            }
            at = others[static_cast<std::size_t>(n - 1)].second;
            std::cout << "[harness] tick " << mTick << ": " << a[1] << " in a building "
                      << fixed(others[static_cast<std::size_t>(n - 1)].first / tilePx, 1) << " tiles off\n";
        }
        else if (a.size() >= 4)
        {
            at += glm::vec2(std::stof(a[2]), std::stof(a[3])) * tilePx;
        }
        game.playAt(id, at);
        return true;
    }
    if (op == "fx")
    {
        // fx on|reduced|off: the screen effects (off for harness runs unless asked).
        if (!need(1)) return true;
        const std::string level = lowered(a[1]);
        if (level == "on" || level == "full") game.mScreenEffects = 1.0f;
        else if (level == "reduced") game.mScreenEffects = 0.5f;
        else if (level == "off") game.mScreenEffects = 0.0f;
        else
        {
            fail(command, "fx on|reduced|off");
            return true;
        }
        // The pass reads this tick's state.
        game.updateMood(0.0f);
        std::cout << "[harness] tick " << mTick << ": fx " << level << '\n';
        return true;
    }
    if (op == "tone")
    {
        // tone hopeless|grim|brutal: the tone (for this run's music, and the next newrun).
        if (!need(1)) return true;
        const std::string name = lowered(a[1]);
        int index = -1;
        if (name == "hopeless") index = 0;
        else if (name == "grim") index = 1;
        else if (name == "brutal") index = 2;
        if (index < 0)
        {
            fail(command, "tone hopeless|grim|brutal");
            return true;
        }
        game.mTitleToneIndex = index;
        game.mMusic.reset();
        std::cout << "[harness] tick " << mTick << ": tone " << name << '\n';
        return true;
    }
    if (op == "music" && a.size() > 1 && lowered(a[1]) == "reset")
    {
        game.mMusic.reset();
        return true;
    }

    if (op != "expect" || a.size() < 2) return false;
    const std::string what = lowered(a[1]);
    if (what == "sound" || what == "nosound")
    {
        // expect sound KEY [muffled|far|plain] [N]: played since `sound mark`
        // (N times at least, default 1). expect nosound KEY: not once.
        if (!need(2)) return true;
        const SoundId id = soundByKey(lowered(a[2]).c_str());
        if (id == SoundId::None)
        {
            fail(command, "unknown sound '" + a[2] + "'");
            return true;
        }
        std::string flag;
        int least = 1;
        for (std::size_t i = 3; i < a.size(); ++i)
        {
            const std::string arg = lowered(a[i]);
            if (arg == "muffled" || arg == "far" || arg == "plain") flag = arg;
            else least = std::stoi(arg);
        }
        int count = 0;
        int any = 0;
        for (const AudioManager::Played& p : audio.recent(audio.clock() - mSoundMark + 1e-6))
        {
            if (p.id != id || p.dropped) continue;
            ++any;
            if (flag == "muffled" && !p.muffled) continue;
            if (flag == "far" && !p.far) continue;
            if (flag == "plain" && (p.muffled || p.far)) continue;
            ++count;
        }
        if (what == "nosound")
        {
            if (any > 0) fail(command, a[2] + " played " + std::to_string(any) + " time(s)");
            else std::cout << "[harness] tick " << mTick << ": ok, no " << a[2] << '\n';
            return true;
        }
        if (count < least)
        {
            fail(command, "expected " + a[2] + (flag.empty() ? "" : " " + flag) + " x" + std::to_string(least) + ", heard " +
                              std::to_string(count) + " (" + std::to_string(any) + " in all)");
        }
        else
        {
            std::cout << "[harness] tick " << mTick << ": ok, " << a[2] << (flag.empty() ? "" : " " + flag) << " x" << count
                      << '\n';
        }
        return true;
    }
    if (what == "layer")
    {
        // expect layer NAME above|below V: where its fade is now.
        if (!need(4)) return true;
        const LayerId id = layerNamed(lowered(a[2]));
        if (id == LayerId::Count)
        {
            fail(command, "unknown layer '" + a[2] + "'");
            return true;
        }
        const float now = audio.layerVolume(id);
        const float v = std::stof(a[4]);
        const bool above = lowered(a[3]) == "above";
        if (above ? now > v : now < v)
        {
            std::cout << "[harness] tick " << mTick << ": ok, " << layerKey(id) << ' ' << fixed(now) << '\n';
        }
        else
        {
            fail(command, std::string(layerKey(id)) + " at " + fixed(now) + ", not " + a[3] + ' ' + a[4]);
        }
        return true;
    }
    if (what == "music")
    {
        // expect music starts N | expect music none: pads begun this run.
        if (!need(2)) return true;
        const int starts = game.mMusic.starts();
        const std::string arg = lowered(a[2]);
        const int want = arg == "none" ? 0 : (a.size() > 3 ? std::stoi(a[3]) : 1);
        if (starts == want) std::cout << "[harness] tick " << mTick << ": ok, music starts " << starts << '\n';
        else fail(command, "music started " + std::to_string(starts) + " time(s), not " + std::to_string(want));
        return true;
    }
    if (what == "duck")
    {
        // expect duck below V | expect duck none.
        if (!need(2)) return true;
        const float duck = audio.duck();
        const bool ok = lowered(a[2]) == "none" ? duck > 0.99f : (a.size() > 3 && duck < std::stof(a[3]));
        if (ok) std::cout << "[harness] tick " << mTick << ": ok, duck " << fixed(duck) << '\n';
        else fail(command, "duck is " + fixed(duck));
        return true;
    }
    if (what == "listening")
    {
        // expect listening [no].
        const bool want = !(a.size() > 2 && lowered(a[2]) == "no");
        if (game.mListening == want) std::cout << "[harness] tick " << mTick << ": ok, listening " << want << '\n';
        else fail(command, std::string("listening is ") + (game.mListening ? "on" : "off"));
        return true;
    }
    if (what == "fx")
    {
        // expect fx neutral|active: the mood pass would (not) run this frame.
        if (!need(2)) return true;
        const bool neutral = game.mMoodParams.neutral();
        const bool wantNeutral = lowered(a[2]) == "neutral";
        if (neutral == wantNeutral) std::cout << "[harness] tick " << mTick << ": ok, fx " << a[2] << '\n';
        else fail(command, std::string("the mood pass is ") + (neutral ? "neutral" : "active"));
        return true;
    }
    if (what == "idle")
    {
        // expect idle low|cold|knee|none: the idle the character falls into.
        if (!need(2)) return true;
        const std::string idle = game.moodIdle();
        const std::string want = lowered(a[2]) == "none" ? "" : lowered(a[2]);
        if (idle == want) std::cout << "[harness] tick " << mTick << ": ok, idle " << a[2] << '\n';
        else fail(command, "idle is '" + idle + "'");
        return true;
    }
    return false;
}
