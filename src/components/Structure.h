#pragma once

#include <glm/vec2.hpp>

// Things the player builds or sets down (phase 2). The numbers are saved in
// the structures file: 0 and 1 were the old barricade and noise trap (dropped
// on load), 2 to 5 keep their meaning (the old sleeping area is a bedroll,
// the old supply cache a storage crate).
enum class StructureType
{
    Campfire = 2,
    Bedroll = 3,
    StorageCrate = 4,
    RainCollector = 5,
    Workbench = 6,
    CanAlarm = 7,
    SpikeBoard = 8,
    Light = 9 // a candle or a can lantern
};

enum class StructureCondition
{
    Intact,
    Damaged,
    Breaking
};

struct Structure
{
    StructureType type = StructureType::Campfire;
    StructureCondition condition = StructureCondition::Intact;
    float hp = 100.0f;
    float maxHp = 100.0f;
    float decayRatePerHour = 0.5f;
    float noiseTimer = 0.0f;    // campfire: seconds to the next crackle
    float water = 0.0f;         // rain collector: bottles' worth held
    float burnHours = 0.0f;     // campfire, light: game hours of burning left
    bool lit = false;           // campfire, light
    bool lantern = false;       // light: a can lantern (not seen through windows)
    int uses = 0;               // spike board: the dead it can still catch
    glm::ivec2 target{-1};      // can alarm: the door or window tile it hangs on
};

// Health, wear and a sprite for each kind.
struct StructureInfo
{
    const char* name = "";
    const char* sprite = "";
    float hp = 100.0f;
    float decayPerHour = 0.0f;
    bool solid = false;   // stands on its tile like furniture
    bool wooden = false;  // wears twice as fast in rain outdoors
};

[[nodiscard]] inline const StructureInfo& structureInfo(StructureType type)
{
    static const StructureInfo kCampfire{"Campfire", "prop_campfire", 60.0f, 0.5f, false, true};
    static const StructureInfo kBedroll{"Bedroll", "prop_bedroll", 80.0f, 0.2f, false, false};
    static const StructureInfo kCrate{"Storage Crate", "prop_crate", 150.0f, 0.2f, true, true};
    static const StructureInfo kCollector{"Rain Collector", "prop_collector", 100.0f, 0.3f, true, false};
    static const StructureInfo kWorkbench{"Workbench", "prop_workbench", 150.0f, 0.2f, true, true};
    static const StructureInfo kAlarm{"Can Alarm", "prop_canalarm", 20.0f, 0.0f, false, false};
    static const StructureInfo kSpikes{"Spike Board", "prop_spikeboard", 30.0f, 0.0f, false, true};
    static const StructureInfo kLight{"Candle", "prop_candle", 10.0f, 0.0f, false, false};
    switch (type)
    {
    case StructureType::Campfire: return kCampfire;
    case StructureType::Bedroll: return kBedroll;
    case StructureType::StorageCrate: return kCrate;
    case StructureType::RainCollector: return kCollector;
    case StructureType::Workbench: return kWorkbench;
    case StructureType::CanAlarm: return kAlarm;
    case StructureType::SpikeBoard: return kSpikes;
    case StructureType::Light: return kLight;
    }
    return kCampfire;
}
