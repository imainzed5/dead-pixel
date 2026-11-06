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
};

namespace furniture
{
[[nodiscard]] const FurnitureInfo& info(FurnitureKind kind);
// Atlas sprite for a placed piece.
[[nodiscard]] std::string spriteName(const FurniturePiece& piece);
// The tile in front of an oriented piece (where you stand to use it).
[[nodiscard]] glm::ivec2 frontTile(const FurniturePiece& piece);

// Furnishes every building in the layout. Doorways, the tiles around them
// and the player start stay clear, every room stays walkable end to end and
// every piece can be reached. Deterministic for a given layout and seed.
[[nodiscard]] std::vector<FurniturePiece> furnish(const LayoutData& layout, std::uint32_t seed);
} // namespace furniture
