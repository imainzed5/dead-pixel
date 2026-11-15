// Harness commands for the way out (overhaul phase 9): campaign, gate,
// goto gate|box|generator|sergeant|marker|fuse|clue, learn, clues, and
// their expect lines. See DevHarness.h for the list.
#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "components/Health.h"
#include "components/Interactable.h"
#include "components/Inventory.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "core/Game.h"
#include "core/GameShared.h"
#include "systems/InventorySystem.h"
#include "world/Storage.h"

namespace
{
std::string lowered(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

const char* keyName(gate::KeyPlace key)
{
    switch (key)
    {
    case gate::KeyPlace::Sergeant: return "sergeant";
    case gate::KeyPlace::Loose: return "loose";
    case gate::KeyPlace::Turned: return "turned";
    }
    return "?";
}

// A free tile beside `tile` (the nearest ring first), or (-1, -1).
glm::ivec2 besideTile(const TileMap& map, const glm::ivec2& tile, const glm::ivec2& preferAway = glm::ivec2(0))
{
    if (preferAway != glm::ivec2(0))
    {
        const glm::ivec2 to = tile + preferAway;
        if (map.inBounds(to.x, to.y) && !map.isSolid(to.x, to.y) && map.doorState(to.x, to.y) == DoorState::None) return to;
    }
    // Straight beside it first, then the corners and further out.
    for (const glm::ivec2 side : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
    {
        const glm::ivec2 to = tile + side;
        if (map.inBounds(to.x, to.y) && !map.isSolid(to.x, to.y) && map.doorState(to.x, to.y) == DoorState::None) return to;
    }
    for (int r = 1; r <= 3; ++r)
    {
        for (int dy = -r; dy <= r; ++dy)
        {
            for (int dx = -r; dx <= r; ++dx)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                const glm::ivec2 to = tile + glm::ivec2(dx, dy);
                if (!map.inBounds(to.x, to.y) || map.isSolid(to.x, to.y) || map.doorState(to.x, to.y) != DoorState::None) continue;
                return to;
            }
        }
    }
    return glm::ivec2(-1);
}
} // namespace

bool DevHarness::gateCommand(Game& game, const Command& command)
{
    const auto& a = command.args;
    const std::string& op = a[0];
    const LayoutData::GateSite& site = game.mMapGenerator.layoutData().gate;
    const auto teleportTo = [&](const glm::ivec2& to, const std::string& what)
    {
        if (to.x < 0)
        {
            fail(command, "no room beside " + what);
            return;
        }
        execute(game, Command{command.line, {"teleport", std::to_string(to.x), std::to_string(to.y)}});
        std::cout << "[harness] goto " << what << ": " << to.x << "," << to.y << '\n';
    };

    if (op == "campaign")
    {
        // campaign on|off: the way out (on) or endless for the next newrun.
        const bool on = a.size() < 2 || lowered(a[1]) != "off";
        game.mTitleGameModeIndex = on ? 0 : 1;
        game.syncPresentationSettingsToSave();
        std::cout << "[harness] tick " << mTick << ": " << (on ? "the way out" : "endless") << '\n';
        return true;
    }
    if (op == "gate")
    {
        if (!site.placed)
        {
            fail(command, "no gate in this town");
            return true;
        }
        gate::State& g = game.mGate;
        const std::string what = a.size() > 1 ? lowered(a[1]) : std::string();
        if (what == "fuse") g.fuseFitted = true;
        else if (what == "fuel" && a.size() > 2) g.fuel = std::clamp(std::stoi(a[2]), 0, gate::kFuelNeeded);
        else if (what == "repair" && a.size() > 2) g.repairs = std::clamp(std::stoi(a[2]), 0, gate::kRepairSessions);
        else if (what == "freed") g.freed = true;
        else if (what == "key")
        {
            // The key in the pack, as if taken from the Sergeant.
            std::vector<Entity> sergeant;
            game.mWorld.forEach<ZombieAI>([&](Entity e, ZombieAI& ai) { if (ai.gateRole == 1) sergeant.push_back(e); });
            for (Entity e : sergeant) game.mWorld.destroyEntity(e);
            g.key = gate::KeyPlace::Loose;
            InventorySystem::addItem(game.mWorld.getComponent<Inventory>(game.mPlayerEntity), game.mItemDatabase, game.mItems.gateKey, 1);
        }
        else if (what == "open")
        {
            g.open = true;
            g.key = gate::KeyPlace::Turned;
            for (const glm::ivec2& t : site.gate) game.mTileMap.setBlocked(t.x, t.y, false);
        }
        else if (what == "reset")
        {
            g = gate::State{};
            g.noteHome = game.mSaveManager.gateState().noteHome;
            for (const glm::ivec2& t : site.gate) game.mTileMap.setBlocked(t.x, t.y, true);
        }
        else if (!what.empty())
        {
            fail(command, "gate fuse|fuel N|repair N|freed|key|open|reset");
            return true;
        }
        if (!what.empty()) game.saveGate();
        std::cout << "[harness] tick " << mTick << ": gate " << gate::sideName(site.side) << " at " << site.gate[1].x << ","
                  << site.gate[1].y << ": fuse " << (g.fuseFitted ? "fitted" : "blown") << ", fuel " << g.fuel << ", repairs "
                  << g.repairs << (g.freed ? " (freed)" : "") << ", key " << keyName(g.key) << (g.open ? ", open" : ", shut")
                  << "; stand " << (game.mStand.running ? "running" : "stopped") << " " << static_cast<int>(game.mStand.opened)
                  << " s, grind " << static_cast<int>(game.mStand.grind) << " s, waves " << game.mStand.waves << "; "
                  << g.contributors.size() << " deeds, " << g.leavers.size() << " left\n";
        return true;
    }
    if (op == "goto" && a.size() > 1)
    {
        const std::string what = lowered(a[1]);
        if (what != "gate" && what != "box" && what != "generator" && what != "sergeant" && what != "marker" && what != "fuse" &&
            what != "clue" && what != "through" && what != "key")
        {
            return false;
        }
        if (what == "key")
        {
            // goto key: beside whatever holds the gate key (a pile, a body, a grave's pack).
            glm::ivec2 at(-1);
            game.mWorld.forEach<Interactable, Inventory, Transform>(
                [&](Entity e, Interactable& inter, Inventory& inv, Transform& t)
                {
                    if (inter.type != InteractableType::Container || e == game.mPlayerEntity) return;
                    if (InventorySystem::countItem(inv, game.mItems.gateKey) > 0)
                        at = gameshared::worldToTile(game.mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
                });
            if (at.x < 0)
            {
                fail(command, "nothing in town holds the gate key");
                return true;
            }
            teleportTo(besideTile(game.mTileMap, at), "the gate key");
            return true;
        }
        if (!site.placed)
        {
            fail(command, "no gate in this town");
            return true;
        }
        if (what == "box" || what == "gate")
        {
            // Beside the box, on the side away from the wall.
            const glm::ivec2 inward = gate::insideGate(game.mMapGenerator.layoutData()) - site.gate[1];
            const glm::ivec2 step(inward.x == 0 ? 0 : (inward.x > 0 ? 1 : -1), inward.y == 0 ? 0 : (inward.y > 0 ? 1 : -1));
            teleportTo(besideTile(game.mTileMap, site.box, step), "the gate box");
            return true;
        }
        if (what == "through")
        {
            // goto through: onto the middle gate tile (the way out, once open).
            execute(game, Command{command.line, {"teleport", std::to_string(site.gate[1].x), std::to_string(site.gate[1].y)}});
            std::cout << "[harness] goto through: " << site.gate[1].x << "," << site.gate[1].y << '\n';
            return true;
        }
        if (what == "generator")
        {
            teleportTo(besideTile(game.mTileMap, site.generator), "the generator");
            return true;
        }
        if (what == "marker")
        {
            const glm::ivec2 marker = gate::insideGate(game.mMapGenerator.layoutData());
            const glm::ivec2 inward = marker - site.gate[1];
            teleportTo(besideTile(game.mTileMap, marker + inward), "the marker");
            return true;
        }
        if (what == "fuse")
        {
            if (game.mFuseCabinet.x < 0)
            {
                fail(command, "no fuse cabinet");
                return true;
            }
            teleportTo(besideTile(game.mTileMap, game.mFuseCabinet), "the fuse cabinet");
            return true;
        }
        if (what == "sergeant")
        {
            glm::ivec2 at(-1);
            game.mWorld.forEach<ZombieAI, Transform, Health>(
                [&](Entity, ZombieAI& ai, Transform& t, Health& h)
                {
                    if (ai.gateRole == 1 && h.current > 0.0f) at = gameshared::worldToTile(game.mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
                });
            if (at.x < 0)
            {
                fail(command, "no Sergeant walking");
                return true;
            }
            teleportTo(besideTile(game.mTileMap, at), "the Sergeant");
            return true;
        }
        // goto clue N|SOURCE: beside readable N (0 first) or the first of a source.
        if (a.size() < 3)
        {
            fail(command, "goto clue N|SOURCE");
            return true;
        }
        int index = -1;
        if (std::isdigit(static_cast<unsigned char>(a[2][0])) != 0)
        {
            index = std::stoi(a[2]);
        }
        else if (lowered(a[2]) == "home")
        {
            // The note in the first life's home.
            for (std::size_t i = 0; i < game.mReadables.size(); ++i)
            {
                if (game.mReadables[i].source == clues::Source::FridgeNote && game.mReadables[i].building == game.mGate.noteHome)
                {
                    index = static_cast<int>(i);
                }
            }
        }
        else
        {
            for (std::size_t i = 0; i < game.mReadables.size(); ++i)
            {
                if (lowered(clues::sourceKey(game.mReadables[i].source)) == lowered(a[2]))
                {
                    index = static_cast<int>(i);
                    break;
                }
            }
        }
        if (index < 0 || index >= static_cast<int>(game.mReadables.size()))
        {
            fail(command, "no readable '" + a[2] + "' (" + std::to_string(game.mReadables.size()) + " in town)");
            return true;
        }
        const clues::Readable& r = game.mReadables[static_cast<std::size_t>(index)];
        teleportTo(besideTile(game.mTileMap, r.tile), clues::sourceKey(r.source));
        return true;
    }
    if (op == "take")
    {
        // take ITEM NAME: from the open container into the pack (the whole stack).
        std::string name;
        for (std::size_t i = 1; i < a.size(); ++i) name += (i > 1 ? " " : "") + a[i];
        const ItemDef* item = game.mItemDatabase.findByName(name);
        if (item == nullptr)
        {
            fail(command, "no item '" + name + "'");
            return true;
        }
        const bool openHasIt = game.mContainerOpen && game.mOpenContainerEntity != kInvalidEntity &&
                               InventorySystem::countItem(game.mWorld.getComponent<Inventory>(game.mOpenContainerEntity), item->id) > 0;
        if (!openHasIt)
        {
            // The nearest container within 2.5 tiles that holds it (a pile, a body, a pack).
            const glm::vec2 here = game.playerWorldCenter();
            Entity best = kInvalidEntity;
            float bestD = 2.5f * static_cast<float>(game.mTileMap.tileWidth());
            game.mWorld.forEach<Interactable, Inventory, Transform>(
                [&](Entity e, Interactable& inter, Inventory& inv, Transform& t)
                {
                    if (inter.type != InteractableType::Container || e == game.mPlayerEntity) return;
                    if (InventorySystem::countItem(inv, item->id) <= 0) return;
                    const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here);
                    if (d < bestD)
                    {
                        best = e;
                        bestD = d;
                    }
                });
            if (best == kInvalidEntity)
            {
                fail(command, "no container near holds " + name);
                return true;
            }
            game.openContainer(best);
        }
        if (!game.mContainerOpen || game.mOpenContainerEntity == kInvalidEntity)
        {
            fail(command, "the container did not open");
            return true;
        }
        const Inventory& inv = game.mWorld.getComponent<Inventory>(game.mOpenContainerEntity);
        for (int row = 0;; ++row)
        {
            const int slot = storage::nthFilledSlot(inv, row);
            if (slot < 0) break;
            if (inv.slots[static_cast<std::size_t>(slot)].itemId != item->id) continue;
            const int moved = game.containerMove(false, row, true);
            std::cout << "[harness] tick " << mTick << ": took " << moved << " " << item->name << '\n';
            if (moved <= 0) fail(command, "could not take " + name);
            game.closeContainer();
            return true;
        }
        fail(command, "the container holds no " + name);
        return true;
    }
    if (op == "titlescreen")
    {
        // titlescreen: back to the title (its card says what the town remembers).
        game.returnToTitleScreen();
        return true;
    }
    if (op == "learn")
    {
        if (a.size() < 2)
        {
            fail(command, "learn N|all");
            return true;
        }
        const clues::Mask mask = lowered(a[1]) == "all" ? clues::all() : clues::bit(std::stoi(a[1]));
        game.learnLeads(mask, "the harness");
        return true;
    }
    if (op == "clues")
    {
        std::cout << "[harness] tick " << mTick << ": leads";
        for (int lead = 1; lead <= clues::kLeadCount; ++lead)
        {
            if (clues::knows(game.mLeads, lead)) std::cout << ' ' << lead;
        }
        std::cout << " (" << clues::count(game.mLeads) << "); seen " << (game.mGateSeen.valid ? "yes" : "no") << "; objective \""
                  << game.gateObjective() << "\"\n";
        for (std::size_t i = 0; i < game.mReadables.size(); ++i)
        {
            const clues::Readable& r = game.mReadables[i];
            std::cout << "[harness]   " << i << ": " << clues::sourceKey(r.source) << " at " << r.tile.x << "," << r.tile.y
                      << " (building " << r.building << ")\n";
        }
        return true;
    }
    if (op != "expect" || a.size() < 2) return false;

    const std::string want = lowered(a[1]);
    if (want == "leads" || want == "lead")
    {
        if (a.size() < 3)
        {
            fail(command, "expect leads N | expect lead N");
            return true;
        }
        const int n = std::stoi(a[2]);
        const bool ok = want == "leads" ? clues::count(game.mLeads) >= n : clues::knows(game.mLeads, n);
        if (!ok) fail(command, "expected " + want + " " + a[2] + ", " + std::to_string(clues::count(game.mLeads)) + " known");
        else std::cout << "[harness] tick " << mTick << ": " << want << " " << a[2] << " (" << clues::count(game.mLeads) << " known)\n";
        return true;
    }
    if (want == "objective")
    {
        std::string text;
        for (std::size_t i = 2; i < a.size(); ++i) text += (i > 2 ? " " : "") + a[i];
        const std::string have = game.gateObjective();
        if (lowered(have).find(lowered(text)) == std::string::npos) fail(command, "expected objective '" + text + "', it is '" + have + "'");
        else std::cout << "[harness] tick " << mTick << ": objective '" << have << "'\n";
        return true;
    }
    if (want == "readables")
    {
        const int n = a.size() > 2 ? std::stoi(a[2]) : 1;
        if (static_cast<int>(game.mReadables.size()) < n)
            fail(command, "expected " + std::to_string(n) + " readables, " + std::to_string(game.mReadables.size()) + " in town");
        return true;
    }
    if (want == "sergeant")
    {
        bool alive = false;
        game.mWorld.forEach<ZombieAI, Health>([&](Entity, ZombieAI& ai, Health& h) { if (ai.gateRole == 1 && h.current > 0.0f) alive = true; });
        const bool wantAlive = a.size() < 3 || lowered(a[2]) != "dead";
        if (alive != wantAlive) fail(command, std::string("expected the Sergeant ") + (wantAlive ? "alive" : "dead"));
        return true;
    }
    if (want == "state")
    {
        const bool out = game.mRunState == Game::RunState::Out;
        if (a.size() < 3 || lowered(a[2]) != "out") return false;
        if (!out) fail(command, "expected state out");
        else std::cout << "[harness] tick " << mTick << ": out (" << game.mDeathCause << ")\n";
        return true;
    }
    if (want != "gate") return false;
    if (a.size() < 3)
    {
        fail(command, "expect gate open|shut|fuse|freed|running|stage N|fuel N|key PLACE");
        return true;
    }
    const gate::State& g = game.mGate;
    const std::string w = lowered(a[2]);
    bool ok = false;
    std::string have;
    if (w == "open" || w == "shut")
    {
        ok = g.open == (w == "open");
        have = g.open ? "open" : "shut";
    }
    else if (w == "fuse")
    {
        ok = g.fuseFitted;
        have = g.fuseFitted ? "fitted" : "blown";
    }
    else if (w == "freed")
    {
        ok = g.freed;
        have = g.freed ? "freed" : "seized, " + std::to_string(g.repairs) + " sessions";
    }
    else if (w == "running" || w == "stopped")
    {
        ok = game.mStand.running == (w == "running");
        have = game.mStand.running ? "running" : "stopped";
    }
    else if ((w == "stage" || w == "fuel" || w == "repairs") && a.size() > 3)
    {
        const int n = std::stoi(a[3]);
        const int value = w == "stage" ? (g.open ? 3 : gate::stage(game.mStand.opened)) : w == "fuel" ? g.fuel : g.repairs;
        ok = value == n;
        have = w + " " + std::to_string(value);
    }
    else if (w == "key" && a.size() > 3)
    {
        have = keyName(g.key);
        ok = lowered(a[3]) == have;
    }
    else
    {
        fail(command, "unknown gate condition '" + w + "'");
        return true;
    }
    if (!ok) fail(command, "expected gate " + w + (a.size() > 3 ? " " + a[3] : "") + ", it is " + have);
    else std::cout << "[harness] tick " << mTick << ": gate " << have << '\n';
    return true;
}
