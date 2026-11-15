// Harness commands for legacy and the aging town (overhaul phase 10):
// journal, age, goto cemetery|stone, bury, world, lineage, bloodline, and
// their expect lines. See DevHarness.h for the list.
#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#include "components/Health.h"
#include "components/Inventory.h"
#include "components/Structure.h"
#include "components/Transform.h"
#include "core/Game.h"
#include "core/GameShared.h"
#include "systems/BuildSystem.h"

namespace
{
std::string lowered(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

const char* stageName(aging::Stage stage)
{
    switch (stage)
    {
    case aging::Stage::Intact: return "intact";
    case aging::Stage::Damaged: return "damaged";
    case aging::Stage::Breaking: return "breaking";
    case aging::Stage::Gone: return "gone";
    }
    return "?";
}

void printJournal(const std::vector<journal::Entry>& entries, const std::string& whose)
{
    std::cout << "[harness] journal of " << whose << ": " << entries.size() << " entries\n";
    for (const journal::Entry& e : entries)
    {
        std::cout << "[harness]   day " << e.day << " (" << journal::key(e.kind) << ", " << e.rank << "): " << e.text << '\n';
    }
}
} // namespace

bool DevHarness::legacyCommand(Game& game, const Command& command)
{
    const auto& a = command.args;
    const std::string& op = a[0];
    const auto teleportTo = [&](const glm::ivec2& to, const std::string& what)
    {
        execute(game, Command{command.line, {"teleport", std::to_string(to.x), std::to_string(to.y)}});
        std::cout << "[harness] goto " << what << ": " << to.x << "," << to.y << '\n';
    };
    const auto& history = game.mSaveManager.characterHistory();

    if (op == "journal")
    {
        // journal: this life's entries; journal last: the last life's, as kept.
        if (a.size() > 1 && lowered(a[1]) == "last")
        {
            if (history.empty()) fail(command, "no earlier life");
            else printJournal(history.back().journal, history.back().name + " (run " + std::to_string(history.back().runIndex) + ")");
        }
        else
        {
            printJournal(game.mJournal, game.mCurrentCharacterName);
        }
        return true;
    }
    if (op == "age")
    {
        // age LIVES [DAYS]: the town ages as if LIVES lives of DAYS days (default:
        // this one's) ended now, over what it would keep. newrun to see it.
        if (a.size() < 2)
        {
            fail(command, "age LIVES [DAYS]");
            return true;
        }
        const int lives = std::max(1, std::stoi(a[1]));
        const int days = a.size() > 2 ? std::max(1, std::stoi(a[2])) : std::max(1, game.mCurrentDay);
        game.mSaveManager.saveStructures(game.structureRecords(true));
        game.mSaveManager.setWindows(game.windowRecords());
        game.mSaveManager.setBoardedDoors(game.boardedDoorRecords());
        game.saveDistrictMemory();
        for (int i = 0; i < lives; ++i) game.ageWorldForNextLife(days, game.mSaveManager.runNumber() + i);
        game.mSaveManager.saveWorldMemory();
        game.mSaveManager.saveSurvivors(game.mSurvivors);
        return true;
    }
    if (op == "world")
    {
        // world fresh|living: this save's kind (Living World, or a Fresh Start).
        if (a.size() < 2)
        {
            fail(command, "world fresh|living");
            return true;
        }
        RunPresentationSettings settings = game.mSaveManager.presentationSettings();
        settings.livingWorld = lowered(a[1]) != "fresh";
        game.mSaveManager.setPresentationSettings(settings);
        game.mSaveManager.savePresentationSettings();
        std::cout << "[harness] tick " << mTick << ": " << (settings.livingWorld ? "living world" : "fresh start") << '\n';
        return true;
    }
    if (op == "lineage")
    {
        // lineage SKILL LEVEL COUNT [read]: COUNT earlier lives of the line who
        // died with SKILL at LEVEL (their journals read, or not). newrun after.
        if (a.size() < 4)
        {
            fail(command, "lineage SKILL LEVEL COUNT [read]");
            return true;
        }
        const auto skill = skillrules::parse(a[1]);
        if (!skill)
        {
            fail(command, "unknown skill '" + a[1] + "'");
            return true;
        }
        const int level = std::stoi(a[2]);
        const int count = std::stoi(a[3]);
        const bool read = a.size() > 4 && lowered(a[4]) == "read";
        const glm::vec2 here = game.playerWorldCenter();
        for (int i = 0; i < count; ++i)
        {
            GraveRecord grave;
            grave.name = "Kin" + std::to_string(i + 1);
            grave.runIndex = 100 + i; // well before any life this script plays
            grave.day = 5;
            grave.x = here.x + 64.0f * static_cast<float>(i + 2);
            grave.y = here.y;
            grave.cause = "Harness";
            grave.skills = {{static_cast<int>(*skill), level}};
            game.mSaveManager.addGrave(grave);
            CharacterHistoryRecord life;
            life.name = grave.name;
            life.runIndex = grave.runIndex;
            life.dayEnded = grave.day;
            life.cause = grave.cause;
            life.journalRead = read;
            game.mSaveManager.addCharacterHistoryRecord(life);
        }
        game.mSaveManager.flushLegacy();
        std::cout << "[harness] tick " << mTick << ": " << count << " lives of " << a[1] << " " << level << (read ? ", read" : "") << '\n';
        return true;
    }
    if (op == "pack" && a.size() > 1 && lowered(a[1]) == "empty")
    {
        // pack empty: nothing in the pack (a grave with no pack beside it).
        if (game.mPlayerEntity != kInvalidEntity && game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
        {
            for (InventorySlot& slot : game.mWorld.getComponent<Inventory>(game.mPlayerEntity).slots) slot = InventorySlot{};
        }
        return true;
    }
    if (op == "bloodline")
    {
        // bloodline: what the line has earned and what runs in the family.
        const legacy::LegacyState state = game.legacyState();
        std::string traits;
        for (BloodlineTrait t : legacy::bloodlineFor(state)) traits += std::string(traits.empty() ? "" : ", ") + bloodlineName(t);
        std::string echoes;
        const auto e = legacy::skillEchoes(state);
        for (std::size_t i = 0; i < e.size(); ++i)
        {
            if (e[i] > 0) echoes += std::string(echoes.empty() ? "" : ", ") + skillrules::name(static_cast<Skill>(i)) + " " + std::to_string(e[i]);
        }
        std::cout << "[harness] tick " << mTick << ": bloodline [" << traits << "], echoes [" << echoes << "]\n";
        return true;
    }
    if (op == "goto" && a.size() > 1 && (lowered(a[1]) == "cemetery" || lowered(a[1]) == "stone"))
    {
        const LayoutData::CemeterySite& site = game.mMapGenerator.layoutData().cemetery;
        if (!site.placed)
        {
            fail(command, "no cemetery in this town");
            return true;
        }
        if (lowered(a[1]) == "cemetery")
        {
            // Just inside the gap in the fence.
            teleportTo(site.opening + glm::ivec2(0, -1), "cemetery");
            return true;
        }
        // goto stone [NAME]: in front of a stone of the line.
        for (const GraveRecord& g : game.mLegacyGraves)
        {
            if (!g.buried || (a.size() > 2 && g.name != a[2])) continue;
            const glm::ivec2 tile = game.graveSlotTile(g);
            teleportTo(tile + glm::ivec2(0, 1), g.name + "'s stone");
            return true;
        }
        fail(command, "no stone");
        return true;
    }
    if (op == "bury")
    {
        // bury: empty the nearest dead survivor's pockets and start burying
        // them (30 s: waits 31). bury now: done at once. bury kill: the
        // nearest living survivor dies first.
        const glm::vec2 here = game.playerWorldCenter();
        if (a.size() > 1 && lowered(a[1]) == "kill")
        {
            Entity victim = kInvalidEntity;
            float nearest = 1e9f;
            game.mWorld.forEach<SurvivorAI, Transform>(
                [&](Entity e, SurvivorAI& ai, Transform& t)
                {
                    if (!ai.alive) return;
                    const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here);
                    if (d < nearest)
                    {
                        nearest = d;
                        victim = e;
                    }
                });
            if (victim == kInvalidEntity)
            {
                fail(command, "no survivor to die");
                return true;
            }
            game.onSurvivorDeath(victim);
        }
        Entity body = kInvalidEntity;
        float best = 1e9f;
        game.mWorld.forEach<SurvivorAI, Transform>(
            [&](Entity e, SurvivorAI& ai, Transform& t)
            {
                if (ai.alive || ai.buried) return;
                const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here);
                if (d < best)
                {
                    best = d;
                    body = e;
                }
            });
        if (body == kInvalidEntity)
        {
            fail(command, "no body to bury");
            return true;
        }
        if (game.mWorld.hasComponent<Inventory>(body))
        {
            for (InventorySlot& slot : game.mWorld.getComponent<Inventory>(body).slots) slot = InventorySlot{};
        }
        if (!game.beginBurial(body))
        {
            fail(command, "could not start the burial");
            return true;
        }
        if (a.size() > 1 && lowered(a.back()) == "now") game.finishLegacyAction();
        return true;
    }
    if (op == "aging")
    {
        // aging: the crates, the boards and the districts as they stand.
        game.mWorld.forEach<Structure, Transform>(
            [&](Entity, Structure& s, Transform& t)
            {
                if (s.type != StructureType::StorageCrate) return;
                const float ratio = s.maxHp > 0.0f ? s.hp / s.maxHp : 0.0f;
                std::cout << "[harness] crate at " << static_cast<int>(t.x / 32.0f) << "," << static_cast<int>(t.y / 32.0f) << ": "
                          << stageName(aging::stageOf(ratio)) << " (" << ratio << ")\n";
            });
        for (const BoardedDoorRecord& d : game.boardedDoorRecords())
        {
            std::cout << "[harness] boards on the door at " << d.x << "," << d.y << ": " << d.boardHp << '\n';
        }
        const DistrictMemoryRecord& memory = game.mSaveManager.districtMemory();
        for (const DistrictInfection& d : game.mDistrictInfectionSystem.districts())
        {
            float created = -1.0f;
            for (const auto& [id, v] : memory.created)
            {
                if (id == d.districtId) created = v;
            }
            std::cout << "[harness] district " << d.districtId << ": infection " << d.infection << " (at creation " << created << ")\n";
        }
        std::cout << "[harness] bones " << game.mSaveManager.aging().bones.size() << '\n';
        return true;
    }
    if (op == "expect" && a.size() > 1)
    {
        const std::string want = lowered(a[1]);
        if (want == "journal")
        {
            // expect journal [last] MIN [MAX]: entries in this life's journal (or the last life's).
            const bool last = a.size() > 2 && lowered(a[2]) == "last";
            const std::size_t at = last ? 3 : 2;
            if (a.size() <= at)
            {
                fail(command, "expect journal [last] MIN [MAX]");
                return true;
            }
            if (last && history.empty())
            {
                fail(command, "no earlier life");
                return true;
            }
            const int have = static_cast<int>(last ? history.back().journal.size() : game.mJournal.size());
            const int lo = std::stoi(a[at]);
            const int hi = a.size() > at + 1 ? std::stoi(a[at + 1]) : 1000;
            if (have < lo || have > hi) fail(command, "expected " + a[at] + ".." + std::to_string(hi) + " entries, there are " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": journal " << have << " entries\n";
            return true;
        }
        if (want == "crate")
        {
            // expect crate intact|damaged|breaking|gone: the crate nearest the player.
            if (a.size() < 3) return fail(command, "expect crate STAGE"), true;
            const glm::vec2 here = game.playerWorldCenter();
            float best = 1e9f;
            std::string have = "gone";
            game.mWorld.forEach<Structure, Transform>(
                [&](Entity, Structure& s, Transform& t)
                {
                    if (s.type != StructureType::StorageCrate || s.hp <= 0.0f) return;
                    const float d = glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here);
                    if (d >= best) return;
                    best = d;
                    have = stageName(aging::stageOf(s.maxHp > 0.0f ? s.hp / s.maxHp : 0.0f));
                });
            if (have != lowered(a[2])) fail(command, "expected crate " + a[2] + ", it is " + have);
            else std::cout << "[harness] tick " << mTick << ": crate " << have << '\n';
            return true;
        }
        if (want == "boardhp")
        {
            // expect boardhp LO HI: the boards on the boarded door nearest the player.
            if (a.size() < 4) return fail(command, "expect boardhp LO HI"), true;
            const glm::vec2 here = game.playerWorldCenter();
            float best = 1e9f;
            float have = 0.0f;
            for (const BoardedDoorRecord& d : game.boardedDoorRecords())
            {
                const float dist = glm::length(glm::vec2((static_cast<float>(d.x) + 0.5f) * 32.0f, (static_cast<float>(d.y) + 0.5f) * 32.0f) - here);
                if (dist < best)
                {
                    best = dist;
                    have = d.boardHp;
                }
            }
            if (have < std::stof(a[2]) || have > std::stof(a[3])) fail(command, "expected boards " + a[2] + ".." + a[3] + ", they are " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": boards " << have << '\n';
            return true;
        }
        if (want == "infection")
        {
            // expect infection above creation: some district is worse than when the world was made.
            const DistrictMemoryRecord& memory = game.mSaveManager.districtMemory();
            float most = -1000.0f;
            int where = 0;
            for (const DistrictInfection& d : game.mDistrictInfectionSystem.districts())
            {
                for (const auto& [id, v] : memory.created)
                {
                    if (id == d.districtId && d.infection - v > most)
                    {
                        most = d.infection - v;
                        where = id;
                    }
                }
            }
            if (most <= 0.0f) fail(command, "no district is worse than at creation (best " + std::to_string(most) + ")");
            else std::cout << "[harness] tick " << mTick << ": district " << where << " is " << most << " worse than at creation\n";
            return true;
        }
        if (want == "echo")
        {
            // expect echo SKILL N: what runs in the family this life.
            if (a.size() < 4) return fail(command, "expect echo SKILL N"), true;
            const auto skill = skillrules::parse(a[2]);
            const int have = skill ? game.mSkillEchoes[static_cast<std::size_t>(*skill)] : -1;
            if (have != std::stoi(a[3])) fail(command, "expected echo " + a[2] + " " + a[3] + ", it is " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": echo " << a[2] << " " << have << '\n';
            return true;
        }
        if (want == "bloodline")
        {
            // expect bloodline NAME|none: the player has it (or none at all).
            if (a.size() < 3) return fail(command, "expect bloodline NAME|none"), true;
            const Traits* traits = game.playerTraits();
            const std::string names = traits != nullptr ? lowered(traits->bloodlineNames()) : std::string();
            const bool ok = lowered(a[2]) == "none" ? names.empty() || names == "none" : names.find(lowered(a[2])) != std::string::npos;
            if (!ok) fail(command, "expected bloodline " + a[2] + ", it is '" + names + "'");
            else std::cout << "[harness] tick " << mTick << ": bloodline '" << names << "'\n";
            return true;
        }
        if (want == "stone")
        {
            // expect stone NAME|any: their marker stands in the cemetery.
            if (a.size() < 3) return fail(command, "expect stone NAME"), true;
            const bool any = lowered(a[2]) == "any";
            const auto it = std::find_if(game.mLegacyGraves.begin(), game.mLegacyGraves.end(),
                                         [&](const GraveRecord& g) { return (any || g.name == a[2]) && g.buried; });
            if (it == game.mLegacyGraves.end()) fail(command, "no stone for " + a[2]);
            else std::cout << "[harness] tick " << mTick << ": " << a[2] << "'s stone, order " << it->burialOrder << '\n';
            return true;
        }
        if (want == "graves")
        {
            // expect graves N: graves in this life's town.
            if (a.size() < 3) return fail(command, "expect graves N"), true;
            const int have = static_cast<int>(game.mLegacyGraves.size());
            if (have != std::stoi(a[2])) fail(command, "expected " + a[2] + " graves, there are " + std::to_string(have));
            else std::cout << "[harness] tick " << mTick << ": " << have << " graves\n";
            return true;
        }
        if (want == "remembered")
        {
            // expect remembered nothing|something: what the save keeps of the town.
            if (a.size() < 3) return fail(command, "expect remembered nothing|something"), true;
            const SaveManager& s = game.mSaveManager;
            const std::size_t kept = s.graves().size() + s.retirements().size() + s.structures().size() + s.boardedDoors().size() +
                                     s.districtMemory().kills.size() + s.aging().bones.size() + (s.baseRecord().building > 0 ? 1 : 0);
            const bool nothing = kept == 0;
            if (nothing != (lowered(a[2]) == "nothing")) fail(command, "expected " + a[2] + " remembered, " + std::to_string(kept) + " records");
            else std::cout << "[harness] tick " << mTick << ": " << kept << " records remembered\n";
            return true;
        }
        if (want == "reader")
        {
            // expect reader open|closed: a journal being read.
            if (a.size() < 3) return fail(command, "expect reader open|closed"), true;
            const bool open = game.mReader.open && game.mShowNotebook;
            if (open != (lowered(a[2]) == "open")) fail(command, std::string("expected the reader ") + a[2]);
            else std::cout << "[harness] tick " << mTick << ": reader " << (open ? "open" : "closed") << '\n';
            return true;
        }
        if (want == "marker")
        {
            // expect marker N: markers carried.
            if (a.size() < 3) return fail(command, "expect marker N"), true;
            int have = 0;
            if (game.mPlayerEntity != kInvalidEntity && game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
            {
                have = InventorySystem::countItem(game.mWorld.getComponent<Inventory>(game.mPlayerEntity), game.mItems.marker);
            }
            if (have != std::stoi(a[2])) fail(command, "expected " + a[2] + " markers, " + std::to_string(have) + " carried");
            else std::cout << "[harness] tick " << mTick << ": " << have << " markers\n";
            return true;
        }
    }
    return false;
}
