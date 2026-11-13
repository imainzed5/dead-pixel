#include "world/FurnitureUse.h"

#include <algorithm>
#include <array>

namespace furnitureuse
{
namespace
{
using K = FurnitureKind;
using W = Weight;

// Indexed by FurnitureKind.
constexpr std::array<UseInfo, static_cast<std::size_t>(K::Count)> kUse = {{
    {W::Heavy, 2, 2, false, 250.0f}, // Bed
    {W::Heavy, 1, 2, false, 300.0f}, // Couch
    {W::Heavy, 2, 0, false, 350.0f}, // Table
    {W::Heavy, 3, 0, false, 350.0f}, // Bookshelf
    {W::Heavy, 3, 0, false, 350.0f}, // Wardrobe
    {W::Heavy, 2, 0, false, 350.0f}, // Dresser
    {W::Fixed, 0, 0, true, 0.0f},    // Fridge
    {W::Fixed, 0, 0, true, 0.0f},    // Stove
    {W::Fixed, 0, 0, false, 0.0f},   // Counter
    {W::Fixed, 0, 0, false, 0.0f},   // Sink
    {W::Fixed, 0, 0, false, 0.0f},   // Toilet
    {W::Light, 1, 0, false, 150.0f}, // TvStand
    {W::Light, 0, 0, false, 150.0f}, // Plant
    {W::Heavy, 2, 0, false, 350.0f}, // StoreShelf
    {W::Fixed, 0, 0, false, 0.0f},   // Checkout
    {W::Light, 1, 0, false, 150.0f}, // Crates
    {W::Fixed, 0, 0, true, 0.0f},    // MetalRack
    {W::Fixed, 0, 0, false, 0.0f},   // Workbench
    {W::Fixed, 0, 0, false, 0.0f},   // Barrels
}};
} // namespace

const UseInfo& useInfo(FurnitureKind kind)
{
    const auto i = static_cast<std::size_t>(kind);
    return kUse[std::min(i, kUse.size() - 1)];
}

float breakSeconds(FurnitureKind kind, Tool tool, bool hasCrowbar)
{
    const UseInfo& use = useInfo(kind);
    if (use.weight == W::Fixed)
    {
        return use.scrapWithCrowbar && hasCrowbar ? kScrapSeconds : -1.0f;
    }
    const bool light = use.weight == W::Light;
    switch (tool)
    {
    case Tool::Hands: return light ? 8.0f : 14.0f;
    case Tool::Hammer: return light ? 5.0f : 9.0f;
    case Tool::AxeOrCrowbar: return light ? 3.0f : 5.0f;
    }
    return -1.0f;
}

Yield breakYield(FurnitureKind kind, bool hasSaw)
{
    const UseInfo& use = useInfo(kind);
    Yield yield;
    if (use.weight == W::Fixed)
    {
        yield.scrap = use.scrapWithCrowbar ? 1 : 0;
        return yield;
    }
    yield.planks = use.planks + (hasSaw && use.planks >= 2 ? 1 : 0);
    yield.rags = use.rags;
    return yield;
}

bool canPushInto(const TileMap& map, const glm::ivec2& tile, const std::vector<glm::ivec2>& occupied)
{
    if (!map.inBounds(tile.x, tile.y) || map.isSolid(tile.x, tile.y)) return false;
    if (map.doorState(tile.x, tile.y) != DoorState::None || map.hasWindow(tile.x, tile.y)) return false;
    return std::find(occupied.begin(), occupied.end(), tile) == occupied.end();
}

glm::ivec2 pushTarget(const TileMap& map, const glm::ivec2& tile, const glm::ivec2& dir,
                      const std::vector<glm::ivec2>& occupied)
{
    const glm::ivec2 next = tile + dir;
    if (canPushInto(map, next, occupied)) return next;
    const DoorState door = map.doorState(next.x, next.y);
    const bool through = (door == DoorState::Open || door == DoorState::Broken) &&
                         std::find(occupied.begin(), occupied.end(), next) == occupied.end();
    if (through && canPushInto(map, next + dir, occupied)) return next + dir;
    return glm::ivec2(-1);
}

glm::ivec2 blockedDoor(const TileMap& map, const glm::ivec2& tile)
{
    const int building = map.getBuildingId(tile.x, tile.y);
    if (building <= 0) return glm::ivec2(-1);
    for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
    {
        const glm::ivec2 door = tile + side;
        if (map.doorState(door.x, door.y) != DoorState::None && map.getBuildingId(door.x, door.y) == building)
        {
            return door;
        }
    }
    return glm::ivec2(-1);
}

void apply(std::vector<FurniturePiece>& pieces, const Memory& memory)
{
    std::erase_if(pieces, [&](const FurniturePiece& piece)
    {
        return std::find(memory.removed.begin(), memory.removed.end(), piece.origin) != memory.removed.end();
    });
    for (const Move& move : memory.moves)
    {
        const auto piece = std::find_if(pieces.begin(), pieces.end(),
                                        [&](const FurniturePiece& p) { return p.origin == move.from; });
        if (piece != pieces.end()) piece->tile = move.to;
    }
    // A move onto a tile something else ends up on (a stale record) is undone.
    for (FurniturePiece& piece : pieces)
    {
        if (piece.tile == piece.origin) continue;
        const bool taken = std::any_of(pieces.begin(), pieces.end(),
                                       [&](const FurniturePiece& p) { return &p != &piece && p.tile == piece.tile; });
        if (taken) piece.tile = piece.origin;
    }
}

Memory memoryOf(const std::vector<FurniturePiece>& pieces, const std::vector<glm::ivec2>& removed)
{
    Memory memory;
    memory.removed = removed;
    for (const FurniturePiece& piece : pieces)
    {
        if (piece.tile != piece.origin) memory.moves.push_back(Move{piece.origin, piece.tile});
    }
    return memory;
}
} // namespace furnitureuse
