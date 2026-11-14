#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"

// What each building in town was (phase 6, 03_content_tables.md section 4).
// A building's kind follows from its archetype and a hash of where it stands,
// with one of each kind the town must have given to the commercial (or
// industrial) building nearest the centre. Pure: the same layout always gives
// the same kinds. Saved in the layout as an int (keep the numbers).
enum class BuildingKind : std::uint8_t
{
    House = 0,
    Grocery = 1,
    Pharmacy = 2,
    Hardware = 3,
    Clinic = 4,
    Police = 5,
    Diner = 6,
    Office = 7,
    Garage = 8,
    Warehouse = 9,
    Workshop = 10,
    Count
};

// The small board over a street door: a pictogram, no text.
enum class BuildingSign : std::uint8_t
{
    None,
    Cross,  // pharmacy, clinic
    Wrench, // hardware, garage
    Cart,   // grocery
    Badge,  // police
    Cup,    // diner
    Crate   // warehouse
};

namespace buildingkinds
{
[[nodiscard]] const char* name(BuildingKind kind);      // "Pharmacy"
[[nodiscard]] const char* key(BuildingKind kind);       // "pharmacy"
// By key or name, any case. False when unknown.
[[nodiscard]] bool fromName(const std::string& text, BuildingKind& out);
[[nodiscard]] BuildingArchetype archetypeOf(BuildingKind kind);
[[nodiscard]] BuildingSign signOf(BuildingKind kind);
// Atlas sprite of a sign's pictogram ("sign_cross"), or "" for none.
[[nodiscard]] const char* signSprite(BuildingSign sign);

// Chance its doors are locked when the town is made, and that breaking in
// sets off an alarm while the power is on.
[[nodiscard]] float lockedChance(BuildingKind kind);
[[nodiscard]] float alarmChance(BuildingKind kind);

// A value in [0, 1) from a building's footprint and a salt: the building's
// own dice, the same in every life.
[[nodiscard]] float roll(const LayoutData::BuildingRect& rect, std::uint32_t salt);

// Kinds for every building, in layout order. `log` (if given) gets a line
// for each guarantee the town is too small to keep.
[[nodiscard]] std::vector<BuildingKind> assign(const LayoutData& layout, std::vector<std::string>* log = nullptr);
// Fills each building's `kind` where it is not set (old layouts).
// Returns true when anything changed.
bool assignMissing(LayoutData& layout, std::vector<std::string>* log = nullptr);
[[nodiscard]] BuildingKind kindOf(const LayoutData::BuildingRect& rect);

// Locks, chosen when the town is made. The primary (street) door of a locked
// building is locked; half of locked buildings leave their back door open.
// Inside, only a pharmacy's stock room door is locked.
struct DoorLock
{
    glm::ivec2 tile{0};
    int building = 0;      // index + 1
    bool alarm = false;    // breaking in rings it while the power is on
};
[[nodiscard]] bool buildingLocked(const LayoutData::BuildingRect& rect);
[[nodiscard]] bool buildingHasAlarm(const LayoutData::BuildingRect& rect);
// Every locked door in town. `home` (index + 1, or 0) is never locked.
[[nodiscard]] std::vector<DoorLock> lockedDoors(const LayoutData& layout, int home);
// The building whose rectangle holds `tile` (index + 1), or 0.
[[nodiscard]] int buildingAt(const LayoutData& layout, const glm::ivec2& tile);
// The street door of a building: its door on the bottom wall, or (-1, -1).
[[nodiscard]] glm::ivec2 streetDoor(const LayoutData& layout, int building);
} // namespace buildingkinds
