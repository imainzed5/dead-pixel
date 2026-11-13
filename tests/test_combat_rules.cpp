#include "TestFramework.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "components/Combat.h"
#include "core/GameShared.h"
#include "items/ItemDatabase.h"
#include "items/ItemIds.h"
#include "systems/InventorySystem.h"
#include "world/CombatRules.h"
#include "world/Storage.h"

using namespace combatrules;

namespace
{
ItemDatabase items()
{
    ItemDatabase db;
    DP_CHECK(db.loadFromFile(std::string(DEAD_PIXEL_ASSETS_DIR) + "/data/items.json"));
    return db;
}

WeaponStats weaponFor(const ItemDatabase& db, const char* key)
{
    const ItemDef* def = db.find(db.idOf(key));
    DP_CHECK(def != nullptr);
    return def != nullptr ? gameshared::weaponStatsForItem(*def, &db) : WeaponStats{};
}
} // namespace

// --- Swings ---------------------------------------------------------------

DP_TEST(combat_rules_exhausted_at_a_fifth)
{
    DP_CHECK(exhausted(20.0f, 100.0f));
    DP_CHECK(!exhausted(21.0f, 100.0f));
    DP_CHECK(!exhausted(0.0f, 0.0f)); // no stamina pool: never out of breath
}

DP_TEST(combat_rules_windup_reach_and_breath)
{
    WeaponStats w{};
    w.windupSeconds = 0.30f;
    DP_CHECK_NEAR(windupSeconds(w, false), 0.30f, 1e-5f);
    DP_CHECK_NEAR(windupSeconds(w, true), 0.30f * kExhaustedWindupScale, 1e-5f);
    w.reach = true; // the spear lands first
    DP_CHECK_NEAR(windupSeconds(w, false), 0.30f - kReachHeadStartSeconds, 1e-5f);
}

DP_TEST(combat_rules_wear_by_contact)
{
    DP_CHECK_EQ(wearForSwing(SwingContact::Miss), 0);
    DP_CHECK_EQ(wearForSwing(SwingContact::Body), 1);
    DP_CHECK_EQ(wearForSwing(SwingContact::Wall), 2);
}

DP_TEST(combat_rules_weapons_differ_by_item)
{
    const ItemDatabase db = items();
    const WeaponStats knife = weaponFor(db, "knife");
    const WeaponStats sledge = weaponFor(db, "sledgehammer");
    const WeaponStats spear = weaponFor(db, "spear");
    const WeaponStats pan = weaponFor(db, "frying_pan");
    const WeaponStats machete = weaponFor(db, "machete");
    DP_CHECK(sledge.knocksDown);
    DP_CHECK(sledge.windupSeconds > knife.windupSeconds * 2.0f);
    DP_CHECK(spear.reach);
    DP_CHECK(machete.canStick);
    DP_CHECK(!knife.canStick);
    DP_CHECK_EQ(pan.noiseTier, 4);
    DP_CHECK(speedRating(knife) > speedRating(sledge));
    DP_CHECK(specialLine(sledge).find("Knocks down.") != std::string::npos);
    DP_CHECK(specialLine(machete).find("Can stick.") != std::string::npos);
}

// --- The shove ------------------------------------------------------------

DP_TEST(combat_rules_shove_outcomes)
{
    DP_CHECK_NEAR(shoveStamina(true), kShoveStamina, 1e-5f);
    DP_CHECK_NEAR(shoveStamina(false), kShoveStaminaUnarmed, 1e-5f);

    const ShoveOutcome plain = shove(Body::Ordinary, 1.0f, false, false);
    DP_CHECK_NEAR(plain.distancePixels, kShoveDistancePixels, 1e-5f);
    DP_CHECK_NEAR(plain.staggerSeconds, kShoveStaggerSeconds, 1e-5f);
    DP_CHECK(!plain.falls);

    DP_CHECK(shove(Body::Ordinary, 0.2f, false, false).falls); // hurt: it goes over
    DP_CHECK(shove(Body::Ordinary, 1.0f, true, false).falls);  // from behind: over too
    DP_CHECK_NEAR(shove(Body::Ordinary, 1.0f, false, true).distancePixels, kShoveDistancePixels * 0.5f, 1e-5f);

    const ShoveOutcome brute = shove(Body::Brute, 0.1f, true, false);
    DP_CHECK_NEAR(brute.distancePixels, kBruteShoveDistancePixels, 1e-5f);
    DP_CHECK(!brute.falls);
    DP_CHECK_NEAR(shove(Body::Crawler, 1.0f, false, false).distancePixels, 0.0f, 1e-5f);
}

// --- Held -----------------------------------------------------------------

DP_TEST(combat_rules_from_behind)
{
    // Facing east, struck by something coming from the west: from behind.
    DP_CHECK(fromBehind(glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 0.0f)));
    DP_CHECK(!fromBehind(glm::vec2(1.0f, 0.0f), glm::vec2(-1.0f, 0.0f)));
    DP_CHECK(!fromBehind(glm::vec2(0.0f), glm::vec2(1.0f, 0.0f)));
}

DP_TEST(combat_rules_lunge_claw_or_grab)
{
    DP_CHECK(lungeResult(0.10f, false, false, false) == LungeResult::Claw);
    DP_CHECK(lungeResult(0.90f, false, false, false) == LungeResult::Grab);
    // From behind, out of breath, or already held: always a grab.
    DP_CHECK(lungeResult(0.0f, true, false, false) == LungeResult::Grab);
    DP_CHECK(lungeResult(0.0f, false, true, false) == LungeResult::Grab);
    DP_CHECK(lungeResult(0.0f, false, false, true) == LungeResult::Grab);
    DP_CHECK_NEAR(clawDamage(7.0f, true), 7.0f * kBackClawScale, 1e-5f);
    DP_CHECK_NEAR(clawDamage(7.0f, false), 7.0f, 1e-5f);
}

DP_TEST(combat_rules_struggle_meter)
{
    Struggle s;
    float stamina = 100.0f;
    for (int i = 0; i < 3; ++i) struggleTap(s, stamina);
    DP_CHECK(!struggleFree(s, 1));
    struggleTap(s, stamina);
    DP_CHECK(struggleFree(s, 1)); // four taps for one
    DP_CHECK(!struggleFree(s, 2)); // eight for two
    DP_CHECK_NEAR(stamina, 100.0f - 4.0f * kStruggleTapStamina, 1e-4f);

    // With no breath a tap counts half and costs what is left.
    Struggle weak;
    float none = 3.0f;
    DP_CHECK_NEAR(struggleTap(weak, none), 3.0f, 1e-5f);
    DP_CHECK_NEAR(weak.meter, 0.5f, 1e-5f);

    // Quick taps lose nothing: the meter holds for a moment after each.
    Struggle quick;
    for (int i = 0; i < 4; ++i)
    {
        struggleDrain(quick, 0.2f);
        struggleTap(quick, stamina);
    }
    DP_CHECK(struggleFree(quick, 1));

    // It drains once you stop tapping, after the pause.
    struggleDrain(s, kStruggleDrainDelaySeconds);
    DP_CHECK_NEAR(s.meter, 4.0f, 1e-4f);
    struggleDrain(s, 1.0f);
    DP_CHECK_NEAR(s.meter, 4.0f - kStruggleDrainPerSecond, 1e-4f);
    struggleDrain(s, 100.0f);
    DP_CHECK_NEAR(s.meter, 0.0f, 1e-5f);
}

// --- Knockdown, stomp, stick, reach -----------------------------------------

DP_TEST(combat_rules_knockdown_and_stomp)
{
    DP_CHECK_NEAR(downSeconds(Body::Ordinary), kDownSeconds, 1e-5f);
    DP_CHECK_NEAR(downSeconds(Body::Brute), kBruteDownSeconds, 1e-5f);

    WeaponStats blunt = makeWeaponStats(WeaponCategory::Blunt);
    DP_CHECK(!blowKnocksDown(blunt, 30.0f));
    DP_CHECK(blowKnocksDown(blunt, 45.0f));
    WeaponStats blade = makeWeaponStats(WeaponCategory::Bladed);
    DP_CHECK(!blowKnocksDown(blade, 80.0f));
    blade.knocksDown = true;
    DP_CHECK(blowKnocksDown(blade, 1.0f));

    WeaponStats hands = makeWeaponStats(WeaponCategory::Unarmed);
    DP_CHECK_NEAR(stompDamage(hands, 8.0f), kUnarmedStompDamage, 1e-5f);
    DP_CHECK_NEAR(stompStamina(hands, 8.0f), kUnarmedStompStamina, 1e-5f);
    blunt.itemId = 1;
    DP_CHECK_NEAR(stompDamage(blunt, 30.0f), 60.0f, 1e-5f);
}

DP_TEST(combat_rules_stick_chance)
{
    WeaponStats axe = makeWeaponStats(WeaponCategory::Bladed);
    DP_CHECK_NEAR(stickChance(axe, 80.0f, true), 0.0f, 1e-5f); // cannot stick
    axe.canStick = true;
    DP_CHECK_NEAR(stickChance(axe, 30.0f, false), 0.0f, 1e-5f); // too light a blow
    DP_CHECK_NEAR(stickChance(axe, 40.0f, false), 0.12f, 1e-5f);
    DP_CHECK_NEAR(stickChance(axe, 10.0f, true), 0.12f, 1e-5f); // a kill always may
    axe.condition = WeaponCondition::Worn;
    DP_CHECK_NEAR(stickChance(axe, 50.0f, false), 0.20f, 1e-5f);
    axe.condition = WeaponCondition::Damaged;
    DP_CHECK_NEAR(stickChance(axe, 50.0f, false), 0.30f, 1e-5f);
    DP_CHECK_NEAR(stickChance(axe, 50.0f, false, 3), 0.15f, 1e-5f);
}

DP_TEST(combat_rules_reach_keeps_one_off)
{
    WeaponStats spear = makeWeaponStats(WeaponCategory::Bladed);
    spear.reach = true;
    spear.rangePixels = 72.0f;
    DP_CHECK_NEAR(reachKnockback(spear, 30.0f, 12.0f, true), 42.0f, 1e-5f);
    DP_CHECK_NEAR(reachKnockback(spear, 30.0f, 12.0f, false), 12.0f, 1e-5f);
    spear.reach = false;
    DP_CHECK_NEAR(reachKnockback(spear, 30.0f, 12.0f, true), 12.0f, 1e-5f);
}

// --- Guns -------------------------------------------------------------------

DP_TEST(combat_rules_spread)
{
    SpreadInputs still;
    SpreadInputs running;
    running.running = true;
    SpreadInputs crouched;
    crouched.crouchedStill = true;
    SpreadInputs panicked;
    panicked.panic = 75.0f;
    DP_CHECK(spreadDegrees(running) > spreadDegrees(still));
    DP_CHECK(spreadDegrees(crouched) < spreadDegrees(still));
    DP_CHECK(spreadDegrees(panicked) > spreadDegrees(still));
}

DP_TEST(combat_rules_bullets_and_pellets)
{
    DP_CHECK_NEAR(bulletDamage(55.0f, 20.0f, 32.0f), 55.0f * kPointBlankScale, 1e-4f);
    DP_CHECK_NEAR(bulletDamage(55.0f, 100.0f, 32.0f), 55.0f, 1e-4f);
    DP_CHECK(pelletsFell(3, 60.0f, 32.0f));
    DP_CHECK(!pelletsFell(2, 30.0f, 32.0f));
    DP_CHECK(!pelletsFell(5, 80.0f, 32.0f));
    DP_CHECK_NEAR(jamChance(0.5f), 0.0f, 1e-5f);
    DP_CHECK_NEAR(jamChance(0.2f), kJamChance, 1e-5f);
    DP_CHECK_NEAR(reloadSeconds(1, 8), kPistolReloadSeconds, 1e-5f);
    DP_CHECK_NEAR(reloadSeconds(5, 2), kShellReloadSeconds * 2.0f, 1e-5f);
    DP_CHECK_NEAR(reloadSeconds(5, 0), 0.0f, 1e-5f);
    DP_CHECK_NEAR(shotPanic(10.0f), kShotPanic, 1e-5f);
    DP_CHECK_NEAR(shotPanic(40.0f), 0.0f, 1e-5f);
}

DP_TEST(combat_rules_guns_from_data)
{
    const ItemDatabase db = items();
    const WeaponStats pistol = weaponFor(db, "pistol");
    const WeaponStats shotgun = weaponFor(db, "shotgun");
    DP_CHECK_EQ(pistol.magazine, 8);
    DP_CHECK_EQ(pistol.ammoItemId, db.idOf("pistol_ammo"));
    DP_CHECK_EQ(shotgun.pellets, 5);
    DP_CHECK_EQ(shotgun.magazine, 2);
    DP_CHECK_EQ(shotgun.ammoItemId, db.idOf("shotgun_shells"));
    DP_CHECK_EQ(shotgun.noiseTier, 5);
}

// --- Throwing ---------------------------------------------------------------

DP_TEST(combat_rules_throw_landing_and_arc)
{
    const glm::vec2 from(0.0f);
    DP_CHECK_NEAR(glm::length(throwLanding(from, glm::vec2(100.0f, 0.0f), 288.0f) - glm::vec2(100.0f, 0.0f)), 0.0f, 1e-4f);
    DP_CHECK_NEAR(glm::length(throwLanding(from, glm::vec2(1000.0f, 0.0f), 288.0f) - glm::vec2(288.0f, 0.0f)), 0.0f, 1e-3f);
    DP_CHECK_NEAR(throwArcHeight(0.0f, 200.0f), 0.0f, 1e-5f);
    DP_CHECK_NEAR(throwArcHeight(1.0f, 200.0f), 0.0f, 1e-5f);
    DP_CHECK(throwArcHeight(0.5f, 200.0f) > throwArcHeight(0.25f, 200.0f));
    DP_CHECK(alarmSurvives(0.1f));
    DP_CHECK(!alarmSurvives(0.5f));
    DP_CHECK_NEAR(flareSeconds(true), kFlareSeconds * 0.5f, 1e-5f);
}

DP_TEST(combat_rules_throwables_in_data)
{
    const ItemDatabase db = items();
    for (const char* key : {"rock", "alarm_clock", "flare", "molotov", "empty_bottle", "tin_can"})
    {
        const ItemDef* def = db.find(db.idOf(key));
        DP_CHECK(def != nullptr && def->throwDef.throwable && def->throwDef.rangeTiles > 0.0f);
    }
    DP_CHECK(db.find(db.idOf("flare"))->throwDef.effect == "flare");
    DP_CHECK(db.find(db.idOf("alarm_clock"))->throwDef.effect == "ring");
    DP_CHECK(db.find(db.idOf("molotov"))->throwDef.effect == "fire");
}

DP_TEST(combat_rules_dir_to_world_is_unit)
{
    for (int d = 0; d < 8; ++d)
    {
        DP_CHECK_NEAR(glm::length(dirToWorld(d)), 1.0f, 1e-4f);
    }
    // Opposite screen directions point opposite ways on the ground.
    DP_CHECK_NEAR(glm::dot(dirToWorld(0), dirToWorld(4)), -1.0f, 1e-4f);
}

// --- Loaded guns in the pack ------------------------------------------------

DP_TEST(combat_rules_unloading_a_gun_gives_loose_rounds)
{
    const ItemDatabase db = items();
    Inventory inv{};
    InventorySystem::addItemWithCondition(inv, db, db.idOf("pistol"), 1, 1.0f, 5);
    InventorySystem::addItem(inv, db, db.idOf("pistol_ammo"), 2);
    int pistols = 0;
    int rounds = 0;
    for (const auto& [item, count] : InventorySystem::itemsUnloaded(inv, db))
    {
        if (item == db.idOf("pistol")) pistols += count;
        if (item == db.idOf("pistol_ammo")) rounds += count;
    }
    DP_CHECK_EQ(pistols, 1);
    DP_CHECK_EQ(rounds, 7);
}

// --- The rare finds ---------------------------------------------------------

DP_TEST(combat_rules_rare_finds_far_and_fixed)
{
    std::vector<FurniturePiece> pieces;
    for (int i = 0; i < 30; ++i)
    {
        FurniturePiece p{};
        p.kind = FurnitureKind::Wardrobe;
        p.tile = glm::ivec2(10 + i * 7, 10 + (i * 13) % 90);
        pieces.push_back(p);
        FurniturePiece table{};
        table.kind = FurnitureKind::Table;
        table.tile = glm::ivec2(200 - i, 5);
        pieces.push_back(table);
    }
    const glm::vec2 centre(110.0f, 55.0f);
    const auto a = storage::rareFinds(pieces, centre, 1234u);
    const auto b = storage::rareFinds(pieces, centre, 1234u);
    DP_CHECK_EQ(a.size(), b.size());
    int pistols = 0;
    int shotguns = 0;
    int shells = 0;
    int machetes = 0;
    int sledges = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        DP_CHECK(a[i].tile == b[i].tile); // the same seed, the same places
        const std::string item = a[i].item;
        pistols += item == "pistol";
        shotguns += item == "shotgun";
        shells += item == "shotgun_shells";
        machetes += item == "machete";
        sledges += item == "sledgehammer";
        if (item == "pistol") DP_CHECK(a[i].loaded > 0);
    }
    DP_CHECK_EQ(pistols, 2);
    DP_CHECK_EQ(shotguns, 1);
    DP_CHECK_EQ(shells, 1);
    DP_CHECK_EQ(machetes, 2);
    DP_CHECK_EQ(sledges, 2);

    // All in the outer third of the town's wardrobes, never a table.
    std::vector<float> distances;
    for (const FurniturePiece& p : pieces)
    {
        if (p.kind == FurnitureKind::Wardrobe) distances.push_back(glm::length(glm::vec2(p.tile) - centre));
    }
    std::sort(distances.begin(), distances.end(), std::greater<float>());
    const float cutoff = distances[9];
    for (const storage::RareFind& find : a)
    {
        DP_CHECK(glm::length(glm::vec2(find.tile) - centre) >= cutoff - 1e-3f);
    }
}
