#include "TestFramework.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include <nlohmann/json.hpp>

#include "map/MapGenerator.h"
#include "save/SaveManager.h"

namespace
{
// SaveManager writes to "save/" relative to the working directory, so each test
// runs inside its own fresh temporary directory.
class ScopedTempCwd
{
public:
    ScopedTempCwd()
    {
        static std::atomic<int> counter{0};
        mOriginal = std::filesystem::current_path();
        mPath = std::filesystem::temp_directory_path() /
                ("dead_pixel_test_" + std::to_string(++counter) + "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::remove_all(mPath);
        std::filesystem::create_directories(mPath);
        std::filesystem::current_path(mPath);
    }

    ~ScopedTempCwd()
    {
        std::error_code ec;
        std::filesystem::current_path(mOriginal, ec);
        std::filesystem::remove_all(mPath, ec);
    }

    ScopedTempCwd(const ScopedTempCwd&) = delete;
    ScopedTempCwd& operator=(const ScopedTempCwd&) = delete;

private:
    std::filesystem::path mOriginal;
    std::filesystem::path mPath;
};
} // namespace

DP_TEST(save_new_slot_uses_seed_override_and_persists_meta)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(424242u);

    SaveManager first;
    DP_CHECK(first.loadOrCreateSlot("test_slot"));
    DP_CHECK_EQ(first.worldSeed(), 424242u);
    DP_CHECK_EQ(first.runNumber(), 1);

    SaveManager second;
    DP_CHECK(second.loadOrCreateSlot("test_slot"));
    DP_CHECK_EQ(second.worldSeed(), 424242u);
}

DP_TEST(save_world_layout_round_trips)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(99u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("layout_slot"));
    DP_CHECK(!save.hasWorldLayout());

    MapGenerator gen;
    const LayoutData original = gen.generateLayout(save.worldSeed());
    DP_CHECK(save.saveWorldLayout(original));
    DP_CHECK(save.hasWorldLayout());

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("layout_slot"));
    const LayoutData loaded = reloaded.loadWorldLayout();
    DP_CHECK_EQ(loaded.width, original.width);
    DP_CHECK_EQ(loaded.height, original.height);
    DP_CHECK(loaded.ground == original.ground);
    DP_CHECK(loaded.collision == original.collision);
    DP_CHECK(loaded.surface == original.surface);
    DP_CHECK(loaded.district == original.district);
    DP_CHECK_EQ(loaded.buildings.size(), original.buildings.size());
    DP_CHECK_EQ(loaded.doors.size(), original.doors.size());
}

DP_TEST(save_death_records_grave_and_advances_run)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(7u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("grave_slot"));
    const std::uint32_t firstRunSeed = save.currentRunSeed();

    GraveRecord grave;
    grave.name = "Ada Vance";
    grave.x = 320.0f;
    grave.y = 480.0f;
    grave.day = 4;
    grave.runIndex = 1;
    grave.cause = "Bitten";
    grave.items = {{0, 2}, {5, 1}};
    grave.risen = true;
    DP_CHECK(save.recordDeath(grave));
    DP_CHECK_EQ(save.runNumber(), 2);
    DP_CHECK(save.currentRunSeed() != firstRunSeed);

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("grave_slot"));
    DP_CHECK_EQ(reloaded.runNumber(), 2);
    DP_CHECK_EQ(reloaded.graves().size(), std::size_t{1});
    DP_CHECK_EQ(reloaded.graves()[0].name, std::string("Ada Vance"));
    DP_CHECK_EQ(reloaded.graves()[0].day, 4);
    DP_CHECK_EQ(reloaded.graves()[0].cause, std::string("Bitten"));
    DP_CHECK_NEAR(reloaded.graves()[0].x, 320.0f, 0.001);
    DP_CHECK(reloaded.graves()[0].risen);
    DP_CHECK(!reloaded.graves()[0].laidToRest);
    DP_CHECK_EQ(reloaded.graves()[0].items.size(), std::size_t{2});
    DP_CHECK_EQ(reloaded.graves()[0].items[1].first, 5);

    // A later run puts them down and takes the pack.
    GraveRecord rested = reloaded.graves()[0];
    rested.risen = false;
    rested.laidToRest = true;
    rested.items.clear();
    reloaded.updateGrave(0, rested);
    DP_CHECK(reloaded.flushLegacy());

    SaveManager third;
    DP_CHECK(third.loadOrCreateSlot("grave_slot"));
    DP_CHECK(!third.graves()[0].risen);
    DP_CHECK(third.graves()[0].laidToRest);
    DP_CHECK(third.graves()[0].items.empty());
}

DP_TEST(save_world_memory_round_trips)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(11u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("memory_slot"));
    ContainerRecord cupboard;
    cupboard.tileX = 10;
    cupboard.tileY = 12;
    cupboard.runIndex = 1;
    cupboard.items = {{0, 2, 0.5f}};
    save.setContainerRecord(cupboard);
    DP_CHECK(save.addDiscoveredRecipe(3, 5));

    VehicleRecord sedan;
    sedan.x = 100.0f;
    sedan.y = 200.0f;
    sedan.condition = 55.0f;
    sedan.fuel = 9.5f;
    sedan.items = {{4, 2}};
    save.setVehicleRecords({sedan});
    save.setBurnedTiles({{1, 2}, {3, 4}, {1, 2}});
    save.setBrokenDoors({{7, 8}});
    DP_CHECK(save.saveWorldMemory());

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("memory_slot"));
    DP_CHECK(reloaded.loadWorldMemory());
    DP_CHECK_EQ(reloaded.containerRecords().size(), std::size_t{1});
    DP_CHECK(reloaded.containerRecord(10, 12) != nullptr);
    DP_CHECK_EQ(reloaded.containerRecord(10, 12)->items.size(), std::size_t{1});
    DP_CHECK(reloaded.isRecipeDiscovered(3, 5));
    DP_CHECK_EQ(reloaded.vehicles().size(), std::size_t{1});
    DP_CHECK_NEAR(reloaded.vehicles()[0].condition, 55.0f, 0.001);
    DP_CHECK_NEAR(reloaded.vehicles()[0].fuel, 9.5f, 0.001);
    DP_CHECK_EQ(reloaded.vehicles()[0].items.size(), std::size_t{1});
    DP_CHECK_EQ(reloaded.burnedTiles().size(), std::size_t{2}); // duplicates collapse
    DP_CHECK_EQ(reloaded.brokenDoors().size(), std::size_t{1});
    DP_CHECK_EQ(reloaded.brokenDoors()[0].first, 7);
}

DP_TEST(save_checkpoint_round_trips_and_is_run_scoped)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(21u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("cp_slot"));

    RunCheckpoint cp;
    cp.valid = true;
    cp.runSeed = save.currentRunSeed();
    cp.characterName = "Mara Quill";
    cp.currentDay = 3;
    cp.gameHours = 14.5;
    cp.playerX = 640.0f;
    cp.playerY = 320.0f;
    cp.healthCurrent = 62.0f;
    cp.needs.hunger = 40.0f;
    cp.mental.depression = 33.0f;
    cp.inventory.slots[0].itemId = 2;
    cp.inventory.slots[0].count = 3;
    cp.kills = 5;
    cp.openDoors = {{4, 5}};
    cp.brokenDoors = {{6, 7}, {8, 9}};
    cp.burnedTiles = {{1, 1}};
    FireTileRecord fire;
    fire.tileX = 2;
    fire.tileY = 3;
    fire.intensity = 1.2f;
    fire.lifeRemaining = 20.0f;
    cp.fires = {fire};
    cp.equippedSlot = 0;
    ZombieRecord zombie;
    zombie.x = 320.0f;
    zombie.y = 96.0f;
    zombie.health = 18.0f;
    zombie.maxHealth = 55.0f;
    zombie.baseMoveSpeed = 120.0f;
    zombie.spawnDay = 2;
    ZombieRecord dead = zombie;
    dead.health = 0.0f;
    cp.zombies = {zombie, dead};
    CorpseRecord body;
    body.x = 200.0f;
    body.y = 64.0f;
    body.flipX = true;
    cp.corpses = {body};
    cp.boardedDoors = {BoardedDoorRecord{7, 9, 150.0f}};
    DP_CHECK(save.saveCheckpoint(cp));

    const RunCheckpoint loaded = save.loadCheckpoint();
    DP_CHECK(loaded.valid);
    DP_CHECK_EQ(loaded.characterName, std::string("Mara Quill"));
    DP_CHECK_EQ(loaded.currentDay, 3);
    DP_CHECK_NEAR(loaded.gameHours, 14.5, 0.001);
    DP_CHECK_NEAR(loaded.healthCurrent, 62.0f, 0.001);
    DP_CHECK_NEAR(loaded.needs.hunger, 40.0f, 0.001);
    DP_CHECK_NEAR(loaded.mental.depression, 33.0f, 0.001);
    DP_CHECK_EQ(loaded.inventory.slots[0].itemId, 2);
    DP_CHECK_EQ(loaded.inventory.slots[0].count, 3);
    DP_CHECK_EQ(loaded.kills, 5);
    // The town as the run left it.
    DP_CHECK_EQ(loaded.openDoors.size(), std::size_t{1});
    DP_CHECK_EQ(loaded.openDoors[0].second, 5);
    DP_CHECK_EQ(loaded.brokenDoors.size(), std::size_t{2});
    DP_CHECK_EQ(loaded.burnedTiles.size(), std::size_t{1});
    DP_CHECK_EQ(loaded.fires.size(), std::size_t{1});
    DP_CHECK_EQ(loaded.fires[0].tileY, 3);
    DP_CHECK_NEAR(loaded.fires[0].lifeRemaining, 20.0f, 0.001);
    DP_CHECK_EQ(loaded.equippedSlot, 0);
    // The dead still walking, wounds and all; the slain do not come back.
    DP_CHECK(loaded.zombiesSaved);
    DP_CHECK_EQ(loaded.zombies.size(), std::size_t{1});
    DP_CHECK_NEAR(loaded.zombies[0].x, 320.0f, 0.001);
    DP_CHECK_NEAR(loaded.zombies[0].health, 18.0f, 0.001);
    DP_CHECK_NEAR(loaded.zombies[0].maxHealth, 55.0f, 0.001);
    DP_CHECK_EQ(loaded.zombies[0].spawnDay, 2);
    DP_CHECK_EQ(loaded.corpses.size(), std::size_t{1});
    DP_CHECK_NEAR(loaded.corpses[0].x, 200.0f, 0.001);
    DP_CHECK(loaded.corpses[0].flipX);
    DP_CHECK(loaded.boardsSaved);
    DP_CHECK_EQ(loaded.boardedDoors.size(), std::size_t{1});
    DP_CHECK_EQ(loaded.boardedDoors[0].y, 9);
    DP_CHECK_NEAR(loaded.boardedDoors[0].boardHp, 150.0f, 0.001);

    // A death ends the run: the old checkpoint must not restore into the next run.
    GraveRecord grave;
    grave.name = "Mara Quill";
    DP_CHECK(save.recordDeath(grave));
    DP_CHECK(!save.loadCheckpoint().valid);
}

DP_TEST(save_checkpoint_keeps_traits_skills_and_wounds)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(31u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("body_slot"));
    RunCheckpoint cp;
    cp.valid = true;
    cp.runSeed = save.currentRunSeed();
    cp.characterName = "Ines Hale";
    cp.traitsSaved = true;
    cp.positiveTraits = {"strong_back", "handy"};
    cp.negativeTraits = {"frail"};
    cp.skillsSaved = true;
    cp.skills.xp[static_cast<std::size_t>(Skill::Carpentry)] = 260.0f;
    cp.skills.level[static_cast<std::size_t>(Skill::Carpentry)] = 2;
    cp.skills.lastUsedHour[static_cast<std::size_t>(Skill::Carpentry)] = 40.5;
    Wound leg;
    leg.type = WoundType::Fracture;
    leg.part = BodyPart::LeftLeg;
    leg.splinted = true;
    Wound cut;
    cut.type = WoundType::Cut;
    cut.part = BodyPart::RightArm;
    cut.depth = WoundDepth::Deep;
    cut.dressed = true;
    cut.dressingHours = 5.0f;
    cp.wounds = {leg, cut};
    cp.boardedDoors = {BoardedDoorRecord{2, 3, 90.0f, 1.5f}};
    DP_CHECK(save.saveCheckpoint(cp));

    const RunCheckpoint loaded = save.loadCheckpoint();
    DP_CHECK(loaded.traitsSaved);
    DP_CHECK(loaded.positiveTraits == cp.positiveTraits);
    DP_CHECK(loaded.negativeTraits == cp.negativeTraits);
    DP_CHECK(loaded.skillsSaved);
    DP_CHECK_EQ(loaded.skills.of(Skill::Carpentry), 2);
    DP_CHECK_NEAR(loaded.skills.xp[static_cast<std::size_t>(Skill::Carpentry)], 260.0f, 0.01);
    DP_CHECK_NEAR(loaded.skills.lastUsedHour[static_cast<std::size_t>(Skill::Carpentry)], 40.5, 0.01);
    DP_CHECK_EQ(loaded.wounds.size(), std::size_t{2});
    DP_CHECK(loaded.wounds[0].type == WoundType::Fracture);
    DP_CHECK(loaded.wounds[0].part == BodyPart::LeftLeg);
    DP_CHECK(loaded.wounds[0].splinted);
    DP_CHECK(loaded.wounds[1].depth == WoundDepth::Deep);
    DP_CHECK(loaded.wounds[1].dressed);
    DP_CHECK_NEAR(loaded.wounds[1].dressingHours, 5.0f, 0.01);
    DP_CHECK_EQ(loaded.boardedDoors.size(), std::size_t{1});
    DP_CHECK_NEAR(loaded.boardedDoors[0].toughness, 1.5f, 0.001);
}

DP_TEST(save_old_checkpoint_without_traits_or_parts_still_loads)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(41u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("old_slot"));
    RunCheckpoint cp;
    cp.valid = true;
    cp.runSeed = save.currentRunSeed();
    cp.characterName = "Old Timer";
    cp.boardedDoors = {BoardedDoorRecord{2, 3, 90.0f}};
    DP_CHECK(save.saveCheckpoint(cp));

    // Rewrite it as a phase 4 checkpoint: no traits, no skills, wounds with a
    // severity and no part or depth, boards with no toughness.
    const std::string path = "save/old_slot_checkpoint.json";
    nlohmann::json json;
    {
        std::ifstream in(path);
        DP_CHECK(in.good());
        in >> json;
    }
    json.erase("positiveTraits");
    json.erase("negativeTraits");
    json.erase("skills");
    json["wounds"] = nlohmann::json::array({
        {{"type", 0}, {"severity", 0.9f}, {"infected", false}, {"infectionTimer", 0.0f}, {"contaminated", false}, {"healTimer", 30.0f}},
        {{"type", 2}, {"severity", 0.5f}, {"infected", true}, {"infectionTimer", 2.0f}, {"contaminated", true}, {"healTimer", 0.0f}},
    });
    for (auto& [key, value] : json.items())
    {
        if (!value.is_array()) continue;
        for (auto& entry : value)
        {
            if (entry.is_object() && entry.contains("toughness")) entry.erase("toughness");
        }
    }
    {
        std::ofstream out(path, std::ios::trunc);
        out << json.dump();
    }

    const RunCheckpoint loaded = save.loadCheckpoint();
    DP_CHECK(loaded.valid);
    DP_CHECK(!loaded.traitsSaved); // traits come from the seed's old pick
    DP_CHECK(!loaded.skillsSaved);
    DP_CHECK_EQ(loaded.wounds.size(), std::size_t{2});
    DP_CHECK(loaded.wounds[0].type == WoundType::Cut);
    DP_CHECK(loaded.wounds[0].part == BodyPart::Torso);
    DP_CHECK(loaded.wounds[0].depth == WoundDepth::Deep); // severity above 0.75
    DP_CHECK(loaded.wounds[1].type == WoundType::Bite);
    DP_CHECK(loaded.wounds[1].depth == WoundDepth::Shallow);
    DP_CHECK(loaded.wounds[1].infected);
    DP_CHECK_EQ(loaded.boardedDoors.size(), std::size_t{1});
    DP_CHECK_NEAR(loaded.boardedDoors[0].toughness, 1.0f, 0.001);
}

DP_TEST(save_v3_scavenging_round_trips)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(61u);

    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("scavenge_slot"));
    DP_CHECK(SaveManager::kCurrentSaveVersion >= 3);

    // World memory: doors unlocked, containers searched, kinds seen, a
    // toilet drawn from, a key ring found, a pile indoors.
    ScavengeMemoryRecord memory;
    memory.unlockedDoors = {{4, 5}};
    memory.searched = {{10, 12}, {11, 12}};
    memory.seenBuildings = {3, 7};
    memory.usedWater = {{20, 21}};
    memory.usedKeyRings = {{30, 31}};
    FloorItemRecord pile;
    pile.x = 8;
    pile.y = 9;
    pile.items = {ContainerItemRecord{2, 3, 0.5f, 0}};
    memory.floorItems = {pile};
    save.setScavengeMemory(memory);
    DP_CHECK(save.saveWorldMemory());

    // Structures: a garden bed and a snare keep their state.
    StructureRecord bed;
    bed.type = 10;
    bed.x = 64.0f;
    bed.y = 96.0f;
    bed.growHours = 50.0f;
    bed.sinceWaterHours = 12.0f;
    bed.regrowing = true;
    StructureRecord snare;
    snare.type = 11;
    snare.snareState = 1;
    snare.lastDawn = 4;
    StructureRecord collector;
    collector.type = 5;
    collector.water = 3.0f;
    collector.standingHours = 80.0f;
    DP_CHECK(save.saveStructures({bed, snare, collector}));

    // The checkpoint: power, searched, foraged, piles, lamps, alarms, radio.
    RunCheckpoint cp;
    cp.valid = true;
    cp.runSeed = save.currentRunSeed();
    cp.characterName = "Nell Ashby";
    cp.needs.badStomachHours = 3.5f;
    cp.needs.hotMealHours = 1.0f;
    cp.scavenge.saved = true;
    cp.scavenge.powerForced = 0;
    cp.scavenge.dipAnnounced = true;
    cp.scavenge.outAnnounced = true;
    cp.scavenge.searched = {{1, 2}};
    cp.scavenge.foraged = {{{5, 6, 3}}};
    cp.scavenge.floorItems = {pile};
    cp.scavenge.litLamps = {{7, 7}};
    cp.scavenge.alarmsSpent = {12};
    cp.scavenge.radioOn = true;
    DP_CHECK(save.saveCheckpoint(cp));

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("scavenge_slot"));
    DP_CHECK_EQ(reloaded.migratedFromVersion(), 0);
    const ScavengeMemoryRecord& m = reloaded.scavengeMemory();
    DP_CHECK_EQ(m.unlockedDoors.size(), std::size_t{1});
    DP_CHECK_EQ(m.searched.size(), std::size_t{2});
    DP_CHECK(m.seenBuildings == std::vector<int>({3, 7}));
    DP_CHECK_EQ(m.usedWater.size(), std::size_t{1});
    DP_CHECK_EQ(m.usedKeyRings.size(), std::size_t{1});
    DP_CHECK_EQ(m.floorItems.size(), std::size_t{1});
    DP_CHECK_EQ(m.floorItems[0].items.size(), std::size_t{1});
    DP_CHECK_NEAR(m.floorItems[0].items[0].condition, 0.5f, 0.001);

    const auto& structures = reloaded.structures();
    DP_CHECK_EQ(structures.size(), std::size_t{3});
    DP_CHECK_EQ(structures[0].type, 10);
    DP_CHECK_NEAR(structures[0].growHours, 50.0f, 0.001);
    DP_CHECK_NEAR(structures[0].sinceWaterHours, 12.0f, 0.001);
    DP_CHECK(structures[0].regrowing);
    DP_CHECK_EQ(structures[1].snareState, 1);
    DP_CHECK_EQ(structures[1].lastDawn, 4);
    DP_CHECK_NEAR(structures[2].standingHours, 80.0f, 0.001);

    const RunCheckpoint back = reloaded.loadCheckpoint();
    DP_CHECK(back.valid);
    DP_CHECK(back.scavenge.saved);
    DP_CHECK_EQ(back.scavenge.powerForced, 0);
    DP_CHECK(back.scavenge.dipAnnounced && back.scavenge.outAnnounced);
    DP_CHECK_EQ(back.scavenge.searched.size(), std::size_t{1});
    DP_CHECK_EQ(back.scavenge.foraged.size(), std::size_t{1});
    DP_CHECK_EQ(back.scavenge.foraged[0][2], 3);
    DP_CHECK_EQ(back.scavenge.floorItems.size(), std::size_t{1});
    DP_CHECK_EQ(back.scavenge.litLamps.size(), std::size_t{1});
    DP_CHECK(back.scavenge.alarmsSpent == std::vector<int>({12}));
    DP_CHECK(back.scavenge.radioOn);
    DP_CHECK_NEAR(back.needs.badStomachHours, 3.5f, 0.001);
    DP_CHECK_NEAR(back.needs.hotMealHours, 1.0f, 0.001);

    // Building kinds are kept in the layout.
    MapGenerator generator;
    LayoutData layout = generator.generateLayout(61u);
    DP_CHECK(!layout.buildings.empty());
    DP_CHECK(layout.buildings[0].kind >= 0);
    DP_CHECK(reloaded.saveWorldLayout(layout));
    const LayoutData loadedLayout = reloaded.loadWorldLayout();
    DP_CHECK_EQ(loadedLayout.buildings.size(), layout.buildings.size());
    for (std::size_t i = 0; i < layout.buildings.size(); ++i)
    {
        DP_CHECK_EQ(loadedLayout.buildings[i].kind, layout.buildings[i].kind);
    }
}

DP_TEST(save_v2_slot_migrates_to_v3)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(71u);
    {
        SaveManager fresh;
        DP_CHECK(fresh.loadOrCreateSlot("v2_slot"));
        ContainerRecord cupboard;
        cupboard.tileX = 3;
        cupboard.tileY = 4;
        cupboard.runIndex = 1;
        cupboard.items = {{0, 1, 1.0f}};
        fresh.setContainerRecord(cupboard);
        DP_CHECK(fresh.saveWorldMemory());
        MapGenerator generator;
        LayoutData layout = generator.generateLayout(71u);
        DP_CHECK(fresh.saveWorldLayout(layout));
    }
    // Make it a version 2 slot: no scavenging in memory, no kinds in the layout.
    const auto rewrite = [](const std::string& path, const std::function<void(nlohmann::json&)>& change)
    {
        nlohmann::json json;
        {
            std::ifstream in(path);
            DP_CHECK(in.good());
            in >> json;
        }
        change(json);
        std::ofstream out(path, std::ios::trunc);
        out << json.dump();
    };
    rewrite("save/v2_slot.json", [](nlohmann::json& j) { j["saveVersion"] = 2; });
    rewrite("save/v2_slot_memory.json", [](nlohmann::json& j) { j.erase("scavenge"); });
    rewrite("save/v2_slot_layout.json", [](nlohmann::json& j) {
        if (!j.contains("buildings")) return;
        for (auto& b : j["buildings"]) b.erase("kind");
    });

    SaveManager migrated;
    DP_CHECK(migrated.loadOrCreateSlot("v2_slot"));
    DP_CHECK_EQ(migrated.migratedFromVersion(), 2);
    DP_CHECK(std::filesystem::exists("save/v2_slot.json.v2.backup"));
    DP_CHECK(migrated.containerRecord(3, 4) != nullptr);
    DP_CHECK(migrated.scavengeMemory().searched.empty());
    // The layout has no kinds; loading it gives them back, the same as new.
    MapGenerator generator;
    generator.loadLayout(migrated.loadWorldLayout());
    MapGenerator fresh;
    const LayoutData expected = fresh.generateLayout(71u);
    DP_CHECK_EQ(generator.layoutData().buildings.size(), expected.buildings.size());
    for (std::size_t i = 0; i < expected.buildings.size(); ++i)
    {
        DP_CHECK_EQ(generator.layoutData().buildings[i].kind, expected.buildings[i].kind);
    }

    SaveManager again;
    DP_CHECK(again.loadOrCreateSlot("v2_slot"));
    DP_CHECK_EQ(again.migratedFromVersion(), 0);
}

DP_TEST(save_v4_survivors_round_trip)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(81u);
    SaveManager save;
    DP_CHECK(save.loadOrCreateSlot("people_slot"));
    DP_CHECK_EQ(SaveManager::kCurrentSaveVersion, 4);

    SurvivorRecord ana;
    ana.id = 0;
    ana.name = "Ana";
    ana.x = 320.0f;
    ana.y = 448.0f;
    ana.role = 1;
    ana.partner = 1;
    ana.outfit = 2;
    ana.stayer = true;
    ana.homeBuilding = 7;
    ana.homeX = 11;
    ana.homeY = 14;
    ana.trust = 42.5f;
    ana.companion = true;
    ana.helloDay = 3;
    ana.lastSeenDay = 4;
    ana.met = true;
    ana.homeKnown = true;
    ana.told = {"lead_supplies:5"};
    ana.stock = {ContainerItemRecord{0, 3, 1.0f, 0}, ContainerItemRecord{40, 1, 0.8f, 4}};
    SurvivorRecord tomas;
    tomas.id = 1;
    tomas.name = "Tomas";
    tomas.alive = false;
    tomas.deathDay = 6;
    tomas.buried = true;
    DP_CHECK(save.saveSurvivors({ana, tomas}));

    GraveRecord grave;
    grave.name = "Tomas";
    grave.day = 6;
    grave.travelledWith = "Nell Ashby";
    const int run = save.runNumber();
    DP_CHECK(save.addGrave(grave));
    DP_CHECK_EQ(save.runNumber(), run); // not a life of the bloodline

    SaveManager reloaded;
    DP_CHECK(reloaded.loadOrCreateSlot("people_slot"));
    const std::vector<SurvivorRecord> back = reloaded.loadSurvivors();
    DP_CHECK_EQ(back.size(), std::size_t{2});
    if (back.size() == 2)
    {
        DP_CHECK_EQ(back[0].name, std::string("Ana"));
        DP_CHECK_EQ(back[0].role, 1);
        DP_CHECK_EQ(back[0].partner, 1);
        DP_CHECK_EQ(back[0].outfit, 2);
        DP_CHECK(back[0].stayer);
        DP_CHECK_EQ(back[0].homeBuilding, 7);
        DP_CHECK_EQ(back[0].homeX, 11);
        DP_CHECK_NEAR(back[0].trust, 42.5f, 0.001);
        DP_CHECK(back[0].companion && back[0].met && back[0].homeKnown);
        DP_CHECK_EQ(back[0].helloDay, 3);
        DP_CHECK(back[0].told == std::vector<std::string>({"lead_supplies:5"}));
        DP_CHECK_EQ(back[0].stock.size(), std::size_t{2});
        if (back[0].stock.size() == 2)
        {
            DP_CHECK_EQ(back[0].stock[1].itemId, 40);
            DP_CHECK_EQ(back[0].stock[1].loaded, 4);
            DP_CHECK_NEAR(back[0].stock[1].condition, 0.8f, 0.001);
        }
        DP_CHECK(!back[1].alive && back[1].buried);
        DP_CHECK_EQ(back[1].deathDay, 6);
    }
    DP_CHECK(!reloaded.graves().empty());
    DP_CHECK_EQ(reloaded.graves().back().travelledWith, std::string("Nell Ashby"));
}

DP_TEST(save_v3_survivors_migrate_to_v4)
{
    ScopedTempCwd cwd;
    SaveManager::setSeedOverride(91u);
    {
        SaveManager fresh;
        DP_CHECK(fresh.loadOrCreateSlot("v3_slot"));
    }
    // A version 3 slot: the meta says 3 and the survivors file has the old fields only.
    {
        nlohmann::json meta;
        {
            std::ifstream in("save/v3_slot.json");
            DP_CHECK(in.good());
            in >> meta;
        }
        meta["saveVersion"] = 3;
        std::ofstream out("save/v3_slot.json", std::ios::trunc);
        out << meta.dump();
    }
    {
        nlohmann::json old;
        old["survivors"] = nlohmann::json::array();
        nlohmann::json alex;
        alex["name"] = "Alex";
        alex["alive"] = true;
        alex["spawnDay"] = 2;
        alex["x"] = 96.0f;
        alex["y"] = 128.0f;
        alex["hunger"] = 70.0f;
        alex["thirst"] = 60.0f;
        alex["aggression"] = 0.3f;
        alex["cooperation"] = 0.7f;
        alex["competence"] = 0.4f;
        alex["homeDistrictId"] = 2;
        old["survivors"].push_back(alex);
        std::ofstream out("save/v3_slot_survivors.json", std::ios::trunc);
        out << old.dump();
    }

    SaveManager migrated;
    DP_CHECK(migrated.loadOrCreateSlot("v3_slot"));
    DP_CHECK_EQ(migrated.migratedFromVersion(), 3);
    DP_CHECK(std::filesystem::exists("save/v3_slot.json.v3.backup"));
    DP_CHECK(std::filesystem::exists("save/v3_slot_survivors.json.v3.backup"));
    const std::vector<SurvivorRecord> people = migrated.loadSurvivors();
    DP_CHECK_EQ(people.size(), std::size_t{1});
    if (!people.empty())
    {
        DP_CHECK_EQ(people[0].name, std::string("Alex"));
        DP_CHECK_NEAR(people[0].cooperation, 0.7f, 0.001);
        DP_CHECK_EQ(people[0].homeBuilding, 0); // found when the town is built
        DP_CHECK(people[0].stock.empty());
        DP_CHECK_NEAR(people[0].trust, 0.0f, 0.001);
    }
    // Written back in the new form.
    {
        std::ifstream in("save/v3_slot_survivors.json");
        nlohmann::json json;
        in >> json;
        DP_CHECK_EQ(json.value("saveVersion", 0), 4);
        DP_CHECK(json["survivors"][0].contains("trust"));
    }
    SaveManager again;
    DP_CHECK(again.loadOrCreateSlot("v3_slot"));
    DP_CHECK_EQ(again.migratedFromVersion(), 0);
}
