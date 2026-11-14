#pragma once

#include <utility>
#include <vector>

#include "items/ItemDatabase.h"

// The items code names by hand, resolved from their keys in items.json once
// the database is loaded. Code never writes an item id as a number: it asks
// this struct, so the data file stays the one place ids are given out.
struct ItemIds
{
    // Every field, with its key. Keep the list and the fields in step.
#define DEAD_PIXEL_ITEM_IDS(X)              \
    X(cannedFood, "canned_food")            \
    X(bread, "bread")                       \
    X(waterBottle, "water_bottle")          \
    X(bandage, "bandage")                   \
    X(knife, "knife")                       \
    X(baseballBat, "baseball_bat")          \
    X(pipe, "pipe")                         \
    X(woodPlanks, "wood_planks")            \
    X(rope, "rope")                         \
    X(fuelCan, "fuel_can")                  \
    X(antibiotics, "antibiotics")           \
    X(painkillers, "painkillers")           \
    X(hammer, "hammer")                     \
    X(flashlight, "flashlight")             \
    X(firstAidKit, "first_aid_kit")         \
    X(notebook, "notebook")                 \
    X(molotov, "molotov")                   \
    X(emptyBottle, "empty_bottle")          \
    X(axe, "axe")                           \
    X(scrapMetal, "scrap_metal")            \
    X(glassShards, "glass_shards")          \
    X(matches, "matches")                   \
    X(jacket, "jacket")                     \
    X(nails, "nails")                       \
    X(crowbar, "crowbar")                   \
    X(saw, "saw")                           \
    X(tinCan, "tin_can")                    \
    X(wire, "wire")                         \
    X(rags, "rags")                         \
    X(candle, "candle")                     \
    X(canAlarm, "can_alarm")                \
    X(spikeBoard, "spike_board")            \
    X(canLantern, "can_lantern")            \
    X(rock, "rock")                         \
    X(alarmClock, "alarm_clock")            \
    X(flare, "flare")                       \
    X(pistol, "pistol")                     \
    X(pistolAmmo, "pistol_ammo")            \
    X(shotgun, "shotgun")                   \
    X(shotgunShells, "shotgun_shells")      \
    X(machete, "machete")                   \
    X(sledgehammer, "sledgehammer")         \
    X(chairLeg, "chair_leg")                \
    X(fryingPan, "frying_pan")              \
    X(glassShiv, "glass_shiv")              \
    X(splint, "splint")                     \
    X(sutureKit, "suture_kit")              \
    X(needleAndThread, "needle_and_thread") \
    X(spirits, "spirits")                   \
    X(coffee, "coffee")                     \
    X(book, "book")                         \
    X(keepsake, "keepsake")                 \
    X(chocolate, "chocolate")               \
    X(rawMeat, "rat_meat")                  \
    X(herbalTea, "herbal_tea")              \
    X(cookedMeat, "cooked_meat")            \
    X(berries, "berries")                   \
    X(mushrooms, "mushrooms")               \
    X(herbs, "herbs")                       \
    X(dirtyWater, "dirty_water")            \
    X(cookingPot, "cooking_pot")            \
    X(rice, "rice")                         \
    X(cookedRice, "cooked_rice")            \
    X(battery, "battery")                   \
    X(radio, "radio")                       \
    X(backpack, "backpack")                 \
    X(ductTape, "duct_tape")                \
    X(snare, "snare")                       \
    X(seedPacket, "seed_packet")            \
    X(vegetables, "vegetables")             \
    X(toolbox, "toolbox")                   \
    X(purificationTablets, "purification_tablets") \
    X(disinfectedBandage, "disinfected_bandage") \
    X(winterCoat, "winter_coat")            \
    X(raincoat, "raincoat")

#define DEAD_PIXEL_ITEM_FIELD(field, key) int field = -1;
    DEAD_PIXEL_ITEM_IDS(DEAD_PIXEL_ITEM_FIELD)
#undef DEAD_PIXEL_ITEM_FIELD

    // (key, field) for every entry, for resolving and for tests.
    [[nodiscard]] std::vector<std::pair<const char*, int*>> entries()
    {
        return {
#define DEAD_PIXEL_ITEM_ENTRY(field, key) {key, &field},
            DEAD_PIXEL_ITEM_IDS(DEAD_PIXEL_ITEM_ENTRY)
#undef DEAD_PIXEL_ITEM_ENTRY
        };
    }

    // Fills every field from the database. False when any key is missing
    // (each missing key is logged by the database).
    bool resolve(const ItemDatabase& db)
    {
        bool all = true;
        for (auto& [key, field] : entries())
        {
            *field = db.idOf(key);
            all = all && *field >= 0;
        }
        return all;
    }
};
