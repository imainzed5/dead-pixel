// Harness commands for the other people in town (overhaul phase 8):
// survivor, goto survivor, set trust, survivors, talk, trade, advance, and
// their expect lines. See DevHarness.h for the list.
#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "components/Health.h"
#include "components/Inventory.h"
#include "components/SurvivorAI.h"
#include "components/Transform.h"
#include "core/Game.h"
#include "core/GameShared.h"
#include "systems/InventorySystem.h"
#include "world/BuildingKinds.h"

namespace
{
std::string lowered(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

const char* stateName(SurvivorState state)
{
    switch (state)
    {
    case SurvivorState::Idle: return "idle";
    case SurvivorState::Flee: return "flee";
    case SurvivorState::Fight: return "fight";
    case SurvivorState::Eat: return "eat";
    case SurvivorState::Drink: return "drink";
    case SurvivorState::Sleep: return "sleep";
    case SurvivorState::Scavenge: return "scavenge";
    case SurvivorState::Fortify: return "fortify";
    case SurvivorState::Tend: return "tend";
    case SurvivorState::Watch: return "watch";
    case SurvivorState::Follow: return "follow";
    case SurvivorState::Wait: return "wait";
    case SurvivorState::Rob: return "rob";
    case SurvivorState::Attack: return "attack";
    case SurvivorState::GoHome: return "gohome";
    case SurvivorState::Dead: return "dead";
    }
    return "?";
}

// "NAME WORDS [COUNT]" from args[first..].
std::string nameAndCount(const std::vector<std::string>& a, std::size_t first, int& count)
{
    std::size_t end = a.size();
    count = 1;
    if (a.size() > first + 1 && std::all_of(a.back().begin(), a.back().end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
    {
        count = std::stoi(a.back());
        end = a.size() - 1;
    }
    std::string name;
    for (std::size_t i = first; i < end; ++i) name += (name.empty() ? "" : " ") + a[i];
    return name;
}
} // namespace

Entity DevHarness::survivorFocus(Game& game, const std::string& name)
{
    Entity found = kInvalidEntity;
    float bestDistance = 0.0f;
    const glm::vec2 player = game.playerWorldCenter();
    const std::string want = lowered(name);
    game.mWorld.forEach<SurvivorAI, Transform>(
        [&](Entity e, SurvivorAI& ai, Transform& t)
        {
            if (!want.empty())
            {
                if (lowered(ai.name).rfind(want, 0) == 0 && (found == kInvalidEntity || ai.alive)) found = e;
                return;
            }
            if (mSurvivorFocus >= 0)
            {
                if (ai.id == mSurvivorFocus) found = e;
                return;
            }
            if (!ai.alive) return;
            const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - player);
            if (found == kInvalidEntity || d < bestDistance)
            {
                found = e;
                bestDistance = d;
            }
        });
    return found;
}

bool DevHarness::survivorCommand(Game& game, const Command& command)
{
    const auto& a = command.args;
    const std::string& op = a[0];
    const float tile = static_cast<float>(std::max(1, game.mTileMap.tileWidth()));

    if (op == "survivor")
    {
        // survivor DX DY [open|wary|hostile] [low|empty|hungry|planks|bandages|armed|gun] [trust N]
        // survivor crowd N: N of them on open floor 3 to 20 tiles round you (perf).
        if (a.size() > 2 && lowered(a[1]) == "crowd")
        {
            const int count = std::max(0, std::stoi(a[2]));
            const glm::ivec2 me = game.playerTile();
            int placed = 0;
            std::uint32_t roll = 1234u;
            for (int attempt = 0; placed < count && attempt < count * 60; ++attempt)
            {
                roll = roll * 1664525u + 1013904223u;
                const float angle = static_cast<float>((roll >> 8) % 6283u) / 1000.0f;
                const float r = 3.0f + static_cast<float>((roll >> 20) % 1700u) / 100.0f;
                const glm::ivec2 d(static_cast<int>(std::cos(angle) * r), static_cast<int>(std::sin(angle) * r));
                const glm::ivec2 to = me + d;
                if (!game.mTileMap.inBounds(to.x, to.y) || game.mTileMap.isSolid(to.x, to.y)) continue;
                const char* manner = placed % 3 == 0 ? "open" : placed % 3 == 1 ? "wary" : "hostile";
                execute(game, Command{command.line, {"survivor", std::to_string(d.x), std::to_string(d.y), manner,
                                                     placed % 2 == 0 ? "armed" : "low"}});
                ++placed;
            }
            std::cout << "[harness] survivor crowd: " << placed << " placed\n";
            return true;
        }
        // survivor house [doors2] [...]: living in the nearest house nobody
        // has (not yours); doors2: the nearest free building of any kind with
        // two doors, one of them one they would board (few houses have two).
        if (a.size() > 1 && lowered(a[1]) == "house")
        {
            bool twoDoors = false;
            std::vector<std::string> rest;
            for (std::size_t k = 2; k < a.size(); ++k)
            {
                if (lowered(a[k]) == "doors2") twoDoors = true;
                else rest.push_back(a[k]);
            }
            const LayoutData& layout = game.mMapGenerator.layoutData();
            const glm::ivec2 me = game.playerTile();
            glm::ivec2 best(-1);
            float bestDistance = 0.0f;
            for (std::size_t i = 0; i < layout.buildings.size(); ++i)
            {
                const int b = static_cast<int>(i) + 1;
                // Two doors: few houses have them, so any building will do.
                const bool kindOk = twoDoors || buildingkinds::kindOf(layout.buildings[i]) == BuildingKind::House;
                if (!kindOk || game.survivorLivesIn(b) ||
                    b == game.mFirstDays.homeBuilding || b == game.mSaveManager.baseRecord().building || game.buildingGutted(b))
                {
                    continue;
                }
                if (twoDoors && (game.survivorHomeDoors(b).size() < 2 || !game.survivorCouldBoard(b))) continue;
                const glm::ivec2 home = game.homeTileOf(b);
                if (home.x < 0) continue;
                const float d = glm::length(glm::vec2(home - me));
                if (best.x < 0 || d < bestDistance)
                {
                    best = home;
                    bestDistance = d;
                }
            }
            if (best.x < 0)
            {
                fail(command, "no free house");
                return true;
            }
            std::vector<std::string> args = {"survivor", std::to_string(best.x - me.x), std::to_string(best.y - me.y)};
            args.insert(args.end(), rest.begin(), rest.end());
            execute(game, Command{command.line, args});
            return true;
        }
        // survivor dead N [KIND]: N of the dead standing 2 to 4 tiles round them.
        if (a.size() > 2 && lowered(a[1]) == "dead")
        {
            const Entity e = survivorFocus(game, std::string());
            if (e == kInvalidEntity)
            {
                fail(command, "no survivor");
                return true;
            }
            const Transform& t = game.mWorld.getComponent<Transform>(e);
            const glm::ivec2 at = gameshared::worldToTile(game.mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            const glm::ivec2 me = game.playerTile();
            const int count = std::max(1, std::stoi(a[2]));
            int placed = 0;
            for (int r = 2; r <= 4 && placed < count; ++r)
            {
                for (int dy = -r; dy <= r && placed < count; ++dy)
                {
                    for (int dx = -r; dx <= r && placed < count; ++dx)
                    {
                        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                        const glm::ivec2 to = at + glm::ivec2(dx, dy);
                        if (!game.mTileMap.inBounds(to.x, to.y) || game.mTileMap.isSolid(to.x, to.y) ||
                            game.mTileMap.doorState(to.x, to.y) != DoorState::None || (to.x + to.y) % 2 != 0)
                        {
                            continue;
                        }
                        std::vector<std::string> args = {"zombie", std::to_string(to.x - me.x), std::to_string(to.y - me.y), "idle"};
                        if (a.size() > 3) args.push_back(a[3]);
                        execute(game, Command{command.line, args});
                        ++placed;
                    }
                }
            }
            std::cout << "[harness] survivor dead: " << placed << " round " << game.mWorld.getComponent<SurvivorAI>(e).name << '\n';
            return true;
        }
        // survivor fill ITEM NAME [COUNT]: into the container they will go to next.
        if (a.size() > 2 && lowered(a[1]) == "fill")
        {
            const Entity e = survivorFocus(game, std::string());
            int count = 1;
            const std::string name = nameAndCount(a, 2, count);
            const ItemDef* item = game.mItemDatabase.findByName(name);
            const Entity box = e != kInvalidEntity ? game.survivorNextContainer(e) : kInvalidEntity;
            if (box == kInvalidEntity || item == nullptr)
            {
                fail(command, "no container for them (or no item '" + name + "')");
                return true;
            }
            Inventory& inv = game.mWorld.getComponent<Inventory>(box);
            const int added = InventorySystem::addItem(inv, game.mItemDatabase, item->id, count).added;
            game.recordContainer(box);
            // Theirs to find: everyone else leaves that one alone.
            glm::ivec2 stand(-1);
            for (const auto& c : game.mSurvivorContainers)
            {
                if (c.entity == box) stand = c.stand;
            }
            game.mWorld.forEach<SurvivorAI>([&](Entity other, SurvivorAI& ai) {
                if (other == e) return;
                ai.skipped.push_back(stand);
                if (ai.target == box)
                {
                    ai.target = kInvalidEntity;
                    ai.state = SurvivorState::Idle;
                    ai.thinkTimer = 0.0f;
                }
            });
            const Transform& t = game.mWorld.getComponent<Transform>(box);
            const glm::ivec2 at = gameshared::worldToTile(game.mTileMap, glm::vec2(t.x + 1.0f, t.y + 1.0f));
            mMark = at;
            std::cout << "[harness] survivor fill: " << added << " " << item->name << " into " << game.containerName(box) << " at "
                      << at.x << "," << at.y << " (building " << game.mTileMap.getBuildingId(at.x, at.y) << ")\n";
            return true;
        }
        if (a.size() < 3)
        {
            fail(command, "'survivor' needs DX DY");
            return true;
        }
        const glm::ivec2 at = game.playerTile() + glm::ivec2(std::stoi(a[1]), std::stoi(a[2]));
        if (!game.mTileMap.inBounds(at.x, at.y) || game.mTileMap.isSolid(at.x, at.y))
        {
            fail(command, "tile " + std::to_string(at.x) + "," + std::to_string(at.y) + " is solid");
            return true;
        }
        SurvivorRecord r;
        r.id = static_cast<int>(game.mSurvivors.size());
        // A name from the pool nobody has yet.
        r.name = "Harlan";
        if (const auto names = game.mSurvivorLines.find("names"); names != game.mSurvivorLines.end())
        {
            for (const std::string& n : names->second)
            {
                const bool used = std::any_of(game.mSurvivors.begin(), game.mSurvivors.end(), [&](const SurvivorRecord& o) { return o.name == n; });
                if (!used)
                {
                    r.name = n;
                    break;
                }
            }
        }
        r.role = static_cast<int>(survivorrules::Role::Loner);
        r.aggression = 0.3f;
        r.cooperation = 0.5f;
        r.competence = 0.6f;
        r.hunger = 85.0f;
        r.thirst = 85.0f;
        r.outfit = r.id % 3;
        r.spawnDay = game.mCurrentDay;
        bool low = false;
        bool empty = false;
        bool planks = false;
        bool bandages = false;
        bool armed = false;
        bool gun = false;
        for (std::size_t k = 3; k < a.size(); ++k)
        {
            const std::string opt = lowered(a[k]);
            if (opt == "open") { r.aggression = 0.1f; r.cooperation = 0.8f; }
            else if (opt == "wary") { r.aggression = 0.35f; r.cooperation = 0.45f; }
            else if (opt == "hostile") { r.aggression = 0.85f; r.cooperation = 0.1f; }
            else if (opt == "low") low = true;
            else if (opt == "empty") empty = true;
            else if (opt == "hungry") { r.hunger = 30.0f; r.thirst = 70.0f; }
            else if (opt == "planks") planks = true;
            else if (opt == "bandages") bandages = true;
            else if (opt == "armed") armed = true;
            else if (opt == "gun") gun = true;
            else if (opt == "trust" && k + 1 < a.size()) r.trust = std::stof(a[++k]);
            else fail(command, "unknown survivor option '" + a[k] + "'");
        }
        const int building = game.mTileMap.isIndoors(at.x, at.y) ? game.mTileMap.getBuildingId(at.x, at.y) : 0;
        r.homeBuilding = building;
        r.homeX = at.x;
        r.homeY = at.y;
        r.x = static_cast<float>(at.x) * tile;
        r.y = static_cast<float>(at.y) * tile;
        const auto add = [&](const char* key, int count)
        {
            const int id = game.mItemDatabase.idOf(key);
            if (id >= 0 && count > 0) r.stock.push_back(ContainerItemRecord{id, count, 1.0f, 0});
        };
        if (!empty)
        {
            add("canned_food", low ? 1 : 5);
            add("water_bottle", low ? 1 : 6);
        }
        if (planks)
        {
            add("wood_planks", 2);
            add("nails", 6);
        }
        if (bandages) add("bandage", 3);
        if (armed) add("baseball_bat", 1);
        if (gun)
        {
            add("pistol", 1);
            add("pistol_ammo", 8);
        }
        game.mSurvivors.push_back(r);
        const Entity e = game.spawnSurvivor(game.mSurvivors.size() - 1);
        mSurvivorFocus = r.id;
        const int doors = building > 0 ? static_cast<int>(game.survivorHomeDoors(building).size()) : 0;
        std::cout << "[harness] survivor " << r.name << " (id " << r.id << ") at " << at.x << "," << at.y << ", home building "
                  << building << " (" << doors << " doors), " << survivorrules::attitudeName(game.mWorld.getComponent<SurvivorAI>(e).attitude)
                  << '\n';
        return true;
    }
    if (op == "waitfor")
    {
        startWaitFor(game, command);
        return true;
    }
    if (op == "survivors" && a.size() > 1 && lowered(a[1]) == "gone")
    {
        // survivors gone: nobody living in town for this run (their records
        // stay as they are), for scripts that test the player and the dead
        // alone: survivors draw the dead, make noise and put them down.
        if (game.mTalk.open) game.closeTalk();
        std::vector<Entity> gone;
        game.mWorld.forEach<SurvivorAI>([&](Entity e, SurvivorAI&) { gone.push_back(e); });
        for (Entity e : gone) game.mWorld.destroyEntity(e);
        mSurvivorFocus = -1;
        std::cout << "[harness] survivors gone: " << gone.size() << '\n';
        return true;
    }
    if (op == "survivors")
    {
        game.mWorld.forEach<SurvivorAI, Transform, Health>(
            [&](Entity e, SurvivorAI& ai, Transform& t, Health& h)
            {
                const glm::ivec2 at = gameshared::worldToTile(game.mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
                const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - game.playerWorldCenter()) / tile;
                std::cout << "[harness] survivor " << ai.id << " " << ai.name << ": " << (ai.alive ? stateName(ai.state) : "dead")
                          << ", " << survivorrules::roleName(ai.role) << ", " << survivorrules::attitudeName(ai.attitude) << ", trust "
                          << static_cast<int>(ai.trust) << ", hp " << static_cast<int>(h.current) << ", hunger "
                          << static_cast<int>(ai.hunger) << ", thirst " << static_cast<int>(ai.thirst) << ", stock "
                          << survivorrules::stockDays(game.survivorStockLines(e)) << " days, home " << ai.homeBuilding << ", at "
                          << at.x << "," << at.y << " (" << static_cast<int>(d) << " tiles)" << (ai.companion ? ", companion" : "")
                          << (ai.far ? ", far" : "") << '\n';
            });
        return true;
    }
    if (op == "goto" && a.size() > 2 && lowered(a[1]) == "open")
    {
        // goto open N: open ground outdoors about N tiles from where you stand
        // (a companion has to walk to you).
        const float r = std::stof(a[2]);
        const glm::ivec2 me = game.playerTile();
        const TileMap& map = game.mTileMap;
        glm::ivec2 found(-1);
        float bestError = 1.0e9f;
        const int reach = static_cast<int>(r) + 3;
        for (int dy = -reach; dy <= reach; ++dy)
        {
            for (int dx = -reach; dx <= reach; ++dx)
            {
                const glm::ivec2 to = me + glm::ivec2(dx, dy);
                bool clear = true;
                for (int oy = -1; oy <= 1 && clear; ++oy)
                {
                    for (int ox = -1; ox <= 1 && clear; ++ox)
                    {
                        const int x = to.x + ox;
                        const int y = to.y + oy;
                        clear = map.inBounds(x, y) && !map.isSolid(x, y) && map.getBuildingId(x, y) == 0;
                    }
                }
                if (!clear) continue;
                const float error = std::abs(std::hypot(static_cast<float>(dx), static_cast<float>(dy)) - r);
                if (error < bestError)
                {
                    bestError = error;
                    found = to;
                }
            }
        }
        if (found.x < 0)
        {
            fail(command, "no open ground " + a[2] + " tiles away");
            return true;
        }
        execute(game, Command{command.line, {"teleport", std::to_string(found.x), std::to_string(found.y)}});
        std::cout << "[harness] goto open: " << found.x << "," << found.y << '\n';
        return true;
    }
    if (op == "goto" && a.size() > 1 && lowered(a[1]) == "survivor")
    {
        // goto survivor [NAME]: a free tile beside them.
        const Entity e = survivorFocus(game, a.size() > 2 ? a[2] : std::string());
        if (e == kInvalidEntity)
        {
            fail(command, "no survivor");
            return true;
        }
        const SurvivorAI& ai = game.mWorld.getComponent<SurvivorAI>(e);
        mSurvivorFocus = ai.id;
        const Transform& t = game.mWorld.getComponent<Transform>(e);
        const glm::ivec2 at = gameshared::worldToTile(game.mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
        const glm::ivec2 sides[8] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        for (const glm::ivec2& side : sides)
        {
            const glm::ivec2 to = at + side;
            if (!game.mTileMap.inBounds(to.x, to.y) || game.mTileMap.isSolid(to.x, to.y) ||
                game.mTileMap.doorState(to.x, to.y) != DoorState::None)
            {
                continue;
            }
            execute(game, Command{command.line, {"teleport", std::to_string(to.x), std::to_string(to.y)}});
            std::cout << "[harness] goto survivor: " << ai.name << '\n';
            return true;
        }
        fail(command, "no room beside " + ai.name);
        return true;
    }
    if (op == "set" && a.size() > 2 && lowered(a[1]) == "shunger")
    {
        // set shunger N: that survivor's hunger (100 full).
        const Entity e = survivorFocus(game, std::string());
        if (e == kInvalidEntity)
        {
            fail(command, "no survivor");
            return true;
        }
        game.mWorld.getComponent<SurvivorAI>(e).hunger = std::stof(a[2]);
        return true;
    }
    if (op == "set" && a.size() > 2 && lowered(a[1]) == "trust")
    {
        const Entity e = survivorFocus(game, a.size() > 3 ? a[3] : std::string());
        if (e == kInvalidEntity)
        {
            fail(command, "no survivor");
            return true;
        }
        SurvivorAI& ai = game.mWorld.getComponent<SurvivorAI>(e);
        ai.trust = survivorrules::clampTrust(std::stof(a[2]));
        ai.attitude = survivorrules::attitude(ai.p, ai.desperation, ai.trust, ai.angryDay == game.mCurrentDay);
        std::cout << "[harness] " << ai.name << " trusts you " << static_cast<int>(ai.trust) << '\n';
        return true;
    }
    if (op == "talk")
    {
        // talk: open the panel with them (as E would); talk N|WORD: pick.
        if (a.size() == 1)
        {
            const Entity e = survivorFocus(game, std::string());
            if (e == kInvalidEntity)
            {
                fail(command, "no survivor");
                return true;
            }
            game.openTalk(e);
            return true;
        }
        if (!game.mTalk.open)
        {
            fail(command, "the talk panel is not open");
            return true;
        }
        if (game.mTalk.view != Game::TalkState::View::Talk) game.mTalk.view = Game::TalkState::View::Talk;
        const std::vector<std::string> choices = game.talkChoices();
        int pick = -1;
        if (std::isdigit(static_cast<unsigned char>(a[1][0])) != 0)
        {
            pick = std::stoi(a[1]) - 1;
        }
        else
        {
            for (std::size_t i = 0; i < choices.size(); ++i)
            {
                if (lowered(choices[i]).find(lowered(a[1])) != std::string::npos)
                {
                    pick = static_cast<int>(i);
                    break;
                }
            }
        }
        if (pick < 0 || pick >= static_cast<int>(choices.size()))
        {
            std::string list;
            for (const std::string& c : choices) list += (list.empty() ? "" : ", ") + c;
            fail(command, "no choice '" + a[1] + "' (" + list + ")");
            return true;
        }
        game.mTalk.cursor = pick;
        game.talkChoose(pick);
        std::cout << "[harness] talk: " << choices[static_cast<std::size_t>(pick)] << " -> \"" << game.mTalk.line << "\"\n";
        return true;
    }
    if (op == "trade")
    {
        if (!game.mTalk.open || game.mTalk.view != Game::TalkState::View::Trade || a.size() < 2)
        {
            fail(command, "the trade panel is not open");
            return true;
        }
        const std::string what = lowered(a[1]);
        if (what == "agree")
        {
            game.tradeAgree();
            const SurvivorAI& ai = game.mWorld.getComponent<SurvivorAI>(game.mTalk.who);
            std::cout << "[harness] trade agreed: \"" << game.mTalk.line << "\", trust " << static_cast<int>(ai.trust) << '\n';
            return true;
        }
        int count = 1;
        const std::string name = nameAndCount(a, 2, count);
        const ItemDef* item = game.mItemDatabase.findByName(name);
        if (item == nullptr)
        {
            fail(command, "no item '" + name + "'");
            return true;
        }
        const bool offer = what == "offer";
        const Inventory& inv = game.mWorld.getComponent<Inventory>(offer ? game.mPlayerEntity : game.mTalk.who);
        const std::vector<int> spare = game.survivorSpare(game.mTalk.who);
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            const std::size_t s = static_cast<std::size_t>(i);
            if (inv.slots[s].itemId != item->id || inv.slots[s].count <= 0) continue;
            const int most = offer ? inv.slots[s].count : spare[s];
            if (most <= 0) continue;
            (offer ? game.mTalk.offer : game.mTalk.ask)[s] = std::min(most, count);
            std::cout << "[harness] trade " << what << " " << std::min(most, count) << " " << item->name << '\n';
            return true;
        }
        fail(command, std::string(offer ? "you have no " : "they will not part with ") + name);
        return true;
    }
    if (op == "advance")
    {
        // advance DAYS [apply]: the days between lives for everyone alive.
        if (a.size() < 2)
        {
            fail(command, "'advance' needs DAYS");
            return true;
        }
        const int days = std::max(1, std::stoi(a[1]));
        const bool apply = a.size() > 2 && lowered(a[2]) == "apply";
        game.syncSurvivorRecords();
        int before = 0;
        for (const SurvivorRecord& r : game.mSurvivors)
        {
            if (!r.alive) continue;
            for (const ContainerItemRecord& item : r.stock) before += item.count;
        }
        mLastAdvance = game.advanceSurvivors(days, apply);
        mAdvancePrints.push_back(mLastAdvance.fingerprint);
        int eaten = 0;
        for (const int n : mLastAdvance.eatenFood) eaten += n;
        for (const int n : mLastAdvance.drunk) eaten += n;
        std::cout << "[harness] advance " << days << (apply ? " (applied)" : " (dry run)") << ": " << mLastAdvance.loots.size()
                  << " containers emptied, " << eaten << " things eaten and drunk, " << mLastAdvance.deaths.size()
                  << " died, stock before " << before << ", fingerprint " << std::hex << mLastAdvance.fingerprint << std::dec << '\n';
        if (apply)
        {
            // Everyone as the records now say.
            std::vector<Entity> gone;
            game.mWorld.forEach<SurvivorAI>([&](Entity e, SurvivorAI&) { gone.push_back(e); });
            for (Entity e : gone) game.mWorld.destroyEntity(e);
            int after = 0;
            for (std::size_t i = 0; i < game.mSurvivors.size(); ++i)
            {
                const SurvivorRecord& r = game.mSurvivors[i];
                if (r.alive)
                {
                    for (const ContainerItemRecord& item : r.stock) after += item.count;
                }
                if (!r.alive && r.buried) continue;
                (void)game.spawnSurvivor(i);
            }
            std::cout << "[harness] advance: stock after " << after << '\n';
        }
        return true;
    }
    if (op != "expect" || a.size() < 2) return false;

    const std::string want = lowered(a[1]);
    if (want == "card")
    {
        // expect card TEXT: the run-start card says it ($survivor: the
        // name of the last survivor the script made).
        std::string text;
        for (std::size_t i = 2; i < a.size(); ++i) text += (i > 2 ? " " : "") + a[i];
        if (const std::size_t at = text.find("$survivor");
            at != std::string::npos && mSurvivorFocus >= 0 && mSurvivorFocus < static_cast<int>(game.mSurvivors.size()))
        {
            text.replace(at, 9, game.mSurvivors[static_cast<std::size_t>(mSurvivorFocus)].name);
        }
        std::string all = game.mLegacyCardTitle;
        for (const std::string& line : game.mLegacyCardLines) all += " | " + line;
        if (lowered(all).find(lowered(text)) == std::string::npos) fail(command, "the card does not say '" + text + "': " + all);
        else std::cout << "[harness] tick " << mTick << ": card: " << all << '\n';
        return true;
    }
    if (want == "talk")
    {
        if (a.size() < 3) return true;
        const std::string w = lowered(a[2]);
        const bool ok = w == "open" ? game.mTalk.open
                      : w == "closed" ? !game.mTalk.open
                      : w == "trade" ? game.mTalk.open && game.mTalk.view == Game::TalkState::View::Trade
                                     : false;
        if (!ok) fail(command, "expected talk " + w);
        return true;
    }
    if (want == "advance")
    {
        if (a.size() < 3) return true;
        const std::string w = lowered(a[2]);
        if (w == "same")
        {
            const std::size_t n = mAdvancePrints.size();
            if (n < 2 || mAdvancePrints[n - 1] != mAdvancePrints[n - 2]) fail(command, "the last two advances differ");
            else std::cout << "[harness] tick " << mTick << ": advance same twice\n";
        }
        else if (w == "loots" && a.size() > 3)
        {
            if (static_cast<int>(mLastAdvance.loots.size()) < std::stoi(a[3]))
                fail(command, "only " + std::to_string(mLastAdvance.loots.size()) + " containers emptied");
        }
        return true;
    }
    if (want != "survivor") return false;

    // expect survivor ...: the last made (or found).
    std::string what;
    const std::vector<std::string> condition(a.begin() + 2, a.end());
    const bool ok = survivorCondition(game, condition, what);
    if (!ok) fail(command, "expected " + what);
    else std::cout << "[harness] tick " << mTick << ": " << what << '\n';
    return true;
}

bool DevHarness::survivorCondition(Game& game, const std::vector<std::string>& c, std::string& what)
{
    const Entity e = survivorFocus(game, std::string());
    if (e == kInvalidEntity && !c.empty() && mSurvivorFocus >= 0 && mSurvivorFocus < static_cast<int>(game.mSurvivors.size()))
    {
        // Buried (a companion's grave): only the record is left.
        const SurvivorRecord& r = game.mSurvivors[static_cast<std::size_t>(mSurvivorFocus)];
        const std::string w = lowered(c[0]);
        what = r.name + (r.alive ? " gone" : r.buried ? " dead and buried" : " dead");
        if (w == "dead") return !r.alive;
        if (w == "buried") return !r.alive && r.buried;
        return false;
    }
    if (e == kInvalidEntity || c.empty())
    {
        what = "a survivor";
        return false;
    }
    const float tile = static_cast<float>(std::max(1, game.mTileMap.tileWidth()));
    const SurvivorAI& ai = game.mWorld.getComponent<SurvivorAI>(e);
    const Transform& t = game.mWorld.getComponent<Transform>(e);
    const glm::vec2 centre(t.x + 16.0f, t.y + 16.0f);
    const glm::ivec2 at = gameshared::worldToTile(game.mTileMap, centre);
    const std::string w = lowered(c[0]);
    const auto result = [&](bool ok, const std::string& text)
    {
        what = ai.name + " " + text + " (" + (ai.alive ? stateName(ai.state) : "dead") + ")";
        return ok;
    };
    if (w == "alive" || w == "dead") return result(ai.alive == (w == "alive"), w);
    if (w == "buried") return result(false, "not buried");
    if (w == "home" || w == "away")
    {
        const bool home = ai.homeBuilding > 0 && game.mTileMap.getBuildingId(at.x, at.y) == ai.homeBuilding;
        return result(home == (w == "home"), w == "home" ? "at home" : "away from home");
    }
    if (w == "companion") return result(ai.companion, "travelling with you");
    if (w == "shut" || w == "boarded")
    {
        // boarded [door]: a way in boarded (a door, if asked).
        const bool doorsOnly = c.size() > 1 && lowered(c[1]) == "door";
        int open = 0;
        int boarded = 0;
        for (const glm::ivec2& d : game.survivorHomeDoors(ai.homeBuilding))
        {
            if (game.mTileMap.doorState(d.x, d.y) == DoorState::Open) ++open;
        }
        for (const Game::WayIn& way : game.waysIn(ai.homeBuilding))
        {
            if (way.window && doorsOnly) continue;
            if (way.window ? game.mWindows.boards(way.tile) > 0 : game.mDoors.boards(way.tile) > 0) ++boarded;
        }
        if (w == "shut") return result(open == 0 && ai.homeBuilding > 0, "home shut (" + std::to_string(open) + " open)");
        return result(boarded > 0, std::string("home boarded (") + std::to_string(boarded) + (doorsOnly ? " doors)" : " ways in)"));
    }
    if (w == "searched")
    {
        // The container they emptied: its record holds no food, water or medicine.
        const ContainerRecord* record =
            ai.lastSearched.x >= 0 ? game.mSaveManager.containerRecord(ai.lastSearched.x, ai.lastSearched.y) : nullptr;
        int useful = 0;
        int other = 0;
        if (record != nullptr)
        {
            for (const ContainerItemRecord& item : record->items)
            {
                const ItemDef* def = game.mItemDatabase.find(item.itemId);
                const bool keeps = def != nullptr && (def->category == ItemCategory::Food || def->category == ItemCategory::Drink ||
                                                      def->category == ItemCategory::Medical);
                (keeps ? useful : other) += item.count;
            }
        }
        return result(record != nullptr && useful == 0,
                      "searched (" + std::to_string(ai.lastSearched.x) + "," + std::to_string(ai.lastSearched.y) + "): record " +
                          (record != nullptr ? std::to_string(useful) + " food left, " + std::to_string(other) + " other" : "missing"));
    }
    if (w == "stock" && c.size() > 1)
    {
        int count = 1;
        const std::string name = nameAndCount(c, 1, count);
        const ItemDef* item = game.mItemDatabase.findByName(name);
        const int have = item != nullptr && game.mWorld.hasComponent<Inventory>(e)
                             ? InventorySystem::countItem(game.mWorld.getComponent<Inventory>(e), item->id)
                             : -1;
        return result(have >= count, "holds " + std::to_string(have) + " " + name + " (want " + std::to_string(count) + ")");
    }
    if ((w == "trust" || w == "hunger") && c.size() > 2)
    {
        const float have = w == "trust" ? ai.trust : ai.hunger;
        const float v = std::stof(c[2]);
        const bool above = lowered(c[1]) == "above";
        return result(above ? have > v : have < v, w + " " + std::to_string(static_cast<int>(have)) + " " + c[1] + " " + c[2]);
    }
    if (w == "near" && c.size() > 1)
    {
        const float d = glm::length(centre - game.playerWorldCenter()) / tile;
        return result(d <= std::stof(c[1]), "within " + c[1] + " tiles (" + std::to_string(d) + ")");
    }
    if (w == "farther" && c.size() > 1)
    {
        const float d = glm::length(centre - game.playerWorldCenter()) / tile;
        return result(d > std::stof(c[1]), "farther than " + c[1] + " tiles (" + std::to_string(d) + ")");
    }
    if (w == "kills" && c.size() > 1) return result(ai.kills >= std::stoi(c[1]), "has put down " + std::to_string(ai.kills));
    // A state: scavenge, flee, sleep, fortify, follow, wait, attack...
    return result(ai.alive && w == stateName(ai.state), w);
}

void DevHarness::startWaitFor(Game& game, const Command& command)
{
    // waitfor SECONDS survivor COND...: wait until it holds (FAIL after SECONDS).
    // waitfor SECONDS held: until the dead hold the player.
    const auto& a = command.args;
    const bool held = a.size() == 3 && lowered(a[2]) == "held";
    if (!held && (a.size() < 4 || lowered(a[2]) != "survivor"))
    {
        fail(command, "waitfor SECONDS survivor CONDITION | waitfor SECONDS held");
        return;
    }
    mWaitFor = command;
    mWaitForLeft = static_cast<std::uint64_t>(std::max(0.0, std::stod(a[1])) * 60.0);
    (void)game;
}

bool DevHarness::waitingFor(Game& game)
{
    if (mWaitFor.args.empty()) return false;
    std::string what;
    bool holds = false;
    if (mWaitFor.args.size() == 3)
    {
        holds = game.playerHeld();
        what = holds ? "player held" : "the player held (free)";
    }
    else
    {
        const std::vector<std::string> condition(mWaitFor.args.begin() + 3, mWaitFor.args.end());
        holds = survivorCondition(game, condition, what);
    }
    if (holds)
    {
        std::cout << "[harness] tick " << mTick << ": " << what << '\n';
        mWaitFor = Command{};
        return false;
    }
    if (mWaitForLeft == 0)
    {
        fail(mWaitFor, "waited for " + what);
        mWaitFor = Command{};
        return false;
    }
    --mWaitForLeft;
    return true;
}
