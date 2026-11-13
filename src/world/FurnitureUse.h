#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "map/TileMap.h"
#include "world/Furniture.h"

// Furniture as material: what can be pushed, what it breaks into, and how
// long it holds a doorway against the dead. Plain rules, apart from the game.
namespace furnitureuse
{
enum class Weight
{
    Fixed, // stays where it stands
    Light,
    Heavy
};

struct UseInfo
{
    Weight weight = Weight::Fixed;
    int planks = 0;          // when broken up
    int rags = 0;
    bool scrapWithCrowbar = false; // fixed metal pieces: a crowbar takes 1 Scrap Metal, the piece is gone
    float barrierHp = 0.0f;  // health while it blocks a doorway
};

[[nodiscard]] const UseInfo& useInfo(FurnitureKind kind);
[[nodiscard]] inline bool movable(FurnitureKind kind) { return useInfo(kind).weight != Weight::Fixed; }

// What is in the hand (or the pack) for breaking things up. The best one
// carried counts.
enum class Tool
{
    Hands,
    Hammer,
    AxeOrCrowbar
};

// Seconds to break a piece up with `tool`; negative when it cannot be.
// Scrapping a fixed metal piece takes kScrapSeconds with a crowbar.
constexpr float kScrapSeconds = 20.0f;
[[nodiscard]] float breakSeconds(FurnitureKind kind, Tool tool, bool hasCrowbar);

struct Yield
{
    int planks = 0;
    int rags = 0;
    int scrap = 0;
};
// What breaking a piece gives. A saw adds a plank to pieces that give two or more.
[[nodiscard]] Yield breakYield(FurnitureKind kind, bool hasSaw);

// Pushing: walk speed while pushing, stamina and noise per tile moved.
constexpr float kHoldToGrabSeconds = 0.4f;
constexpr float kLightPushSpeed = 0.6f;  // of walk speed
constexpr float kHeavyPushSpeed = 0.35f;
constexpr float kLightPushStamina = 6.0f; // per tile
constexpr float kHeavyPushStamina = 12.0f;
constexpr float kClimbOverSeconds = 1.2f; // over a light piece in a doorway

// A piece can slide onto `tile`: free floor (not wall, door frame, other
// furniture, standing scenery) inside or outside, and nobody stands there
// (`occupied` lists the tiles bodies and people stand on).
[[nodiscard]] bool canPushInto(const TileMap& map, const glm::ivec2& tile, const std::vector<glm::ivec2>& occupied);

// Where a push from `tile` one step along `dir` leaves the piece: the next
// tile if it can slide there, or through an open (or broken) doorway to the
// floor beyond it; a piece never comes to rest in a door frame. (-1, -1)
// when it cannot move.
[[nodiscard]] glm::ivec2 pushTarget(const TileMap& map, const glm::ivec2& tile, const glm::ivec2& dir,
                                    const std::vector<glm::ivec2>& occupied);

// The door a piece standing on `tile` blocks: the doorway beside it whose
// floor this tile is (a door opens onto it). (-1, -1) when it blocks none.
[[nodiscard]] glm::ivec2 blockedDoor(const TileMap& map, const glm::ivec2& tile);

// World memory: how the player changed the furnishing. Pieces are named by
// the tile the town put them on (their origin).
struct Move
{
    glm::ivec2 from{0}; // origin
    glm::ivec2 to{0};
};
struct Memory
{
    std::vector<Move> moves;
    std::vector<glm::ivec2> removed; // origins of pieces broken up
};

// Applies remembered changes on top of a fresh furnishing (which must still
// carry its origins). Moves onto a tile already taken are skipped.
void apply(std::vector<FurniturePiece>& pieces, const Memory& memory);
// The changes the current pieces show, given the origins already removed.
[[nodiscard]] Memory memoryOf(const std::vector<FurniturePiece>& pieces, const std::vector<glm::ivec2>& removed);
} // namespace furnitureuse
