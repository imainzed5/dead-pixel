#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"

// Furniture that fills building interiors. Every piece stands on one tile and
// blocks movement through it (not sight or shots); most can be searched.
enum class FurnitureKind : std::uint8_t
{
    Bed,
    Couch,
    Table,
    Bookshelf,
    Wardrobe,
    Dresser,
    Fridge,
    Stove,
    Counter,
    Sink,
    Toilet,
    TvStand,
    Plant,
    StoreShelf,
    Checkout,
    Crates,
    MetalRack,
    Workbench,
    Barrels,
    // Phase 6: what shops, offices and the police station hold.
    Cot,
    Desk,
    FilingCabinet,
    Locker,
    PalletStack,
    PharmacyShelf,
    ToolRack,
    DinerTable,
    FloorLamp, // lights a house room while the town has power
    Count
};

// What a container piece holds. The item tables live in the game layer.
enum class FurnitureLoot : std::uint8_t
{
    None,
    Fridge,
    Kitchen,
    Bathroom,
    Bedroom,
    Living,
    Store,
    Checkout,
    Storage,
    Workshop
};

struct FurnitureInfo
{
    const char* sprite = "";      // atlas name; oriented pieces add "_r" or "_l"
    bool oriented = false;        // has a back that stands against a wall
    bool tall = false;            // fades while it stands in front of the player
    FurnitureLoot loot = FurnitureLoot::None;
    const char* prompt = nullptr; // interaction prompt; nullptr = nothing to do
};

struct FurniturePiece
{
    glm::ivec2 tile{0};
    FurnitureKind kind = FurnitureKind::Table;
    // Oriented pieces: back against the north wall, front facing south
    // (sprite "_l"); otherwise back against the west wall, front facing east
    // (sprite "_r").
    bool backNorth = false;
    int building = 0; // building index + 1
    // The tile the town put it on: its name in world memory, however far it
    // has been pushed since.
    glm::ivec2 origin{0};
    // Blows taken while it blocks a doorway (not saved).
    float damage = 0.0f;
    // Phase 6: what the building was (BuildingKind as int), whether the piece
    // stands in a back room (stock), and the police station's one armoury
    // locker. All follow from the layout; none is saved.
    int buildingKind = 0;
    bool stock = false;
    bool armoury = false;
};

namespace furniture
{
[[nodiscard]] const FurnitureInfo& info(FurnitureKind kind);
// Atlas sprite for a placed piece. `alt`: a searched container standing ajar,
// or a floor lamp that is lit (phase 6).
[[nodiscard]] std::string spriteName(const FurniturePiece& piece, bool alt = false);
// The piece has a second look (ajar, or lit).
[[nodiscard]] bool hasAltSprite(FurnitureKind kind);
// The loot class (assets/data/loot.json) a container piece rolls from, by
// the building it stands in; "" when it holds nothing.
[[nodiscard]] std::string lootClass(const FurniturePiece& piece);
// Seconds to search it (phase 6), before the Scavenging skill: 1.5 for a
// cupboard, 3 for a wardrobe, crate or shelf, 5 for a locker or files.
[[nodiscard]] float searchSeconds(FurnitureKind kind);
// The armoury locker needs a crowbar and this long, loudly.
constexpr float kArmouryForceSeconds = 30.0f;
// The tile in front of an oriented piece (where you stand to use it).
[[nodiscard]] glm::ivec2 frontTile(const FurniturePiece& piece);

// True when a piece of `kind` stands within `radiusTiles` of `tile`
// (centre to centre).
[[nodiscard]] bool kindWithin(const std::vector<FurniturePiece>& pieces, FurnitureKind kind, const glm::ivec2& tile,
                              float radiusTiles);

// Furnishes every building in the layout. Doorways, the tiles around them
// and the player start stay clear, every room stays walkable end to end and
// every piece can be reached. Deterministic for a given layout and seed.
[[nodiscard]] std::vector<FurniturePiece> furnish(const LayoutData& layout, std::uint32_t seed);
} // namespace furniture
