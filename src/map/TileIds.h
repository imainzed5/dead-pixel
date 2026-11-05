#pragma once

// Ground tile ids (GIDs into the ground tileset) shared by the map generator,
// the renderer and the structure view. GIDs are part of saved layouts: never
// renumber, only add.
//
// The tileset holds TileGid::kPerRow GIDs per row and TileGid::kVariants rows;
// variant v of GID g is GID g + v * kPerRow.
namespace TileGid
{
inline constexpr int kFloor = 2;      // concrete floor, doorways
inline constexpr int kGravel = 3;
inline constexpr int kDirt = 4;
inline constexpr int kWater = 5;
inline constexpr int kCarpet = 6;
inline constexpr int kWall = 7;       // plaster or brick wall block
inline constexpr int kMetal = 8;      // metal floor, or metal wall block when solid
inline constexpr int kGrass = 9;
inline constexpr int kGrass2 = 10;
inline constexpr int kWood = 11;      // wooden floorboards
inline constexpr int kTileFloor = 12; // checkered tiles (kitchens, shops)
inline constexpr int kAsphalt = 13;   // roads
inline constexpr int kSidewalk = 14;

inline constexpr int kPerRow = 16;
inline constexpr int kVariants = 4;

[[nodiscard]] inline constexpr bool isGrass(int gid)
{
    return gid == kGrass || gid == kGrass2;
}

// A solid tile with one of these GIDs is drawn as a wall block.
[[nodiscard]] inline constexpr bool isWallGid(int gid)
{
    return gid == kWall || gid == kMetal;
}
} // namespace TileGid
