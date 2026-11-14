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

// After the variants come rows of edge overlays, drawn over a ground tile
// along one edge (see tools/generate_sprites.py). Edge 0 is shared with the
// tile at (x-1, y), 1 with (x, y-1), 2 with (x+1, y), 3 with (x, y+1).
inline constexpr int kOverlayRows = 3;
inline constexpr int kEdgeVariants = 4;

// Grass growing in over a neighbouring tile's edge.
[[nodiscard]] inline constexpr int fringeGid(int edge, int variant)
{
    return 1 + kVariants * kPerRow + edge * kEdgeVariants + variant;
}
// The bank where water meets land, drawn on the water tile.
[[nodiscard]] inline constexpr int shoreGid(int edge, int variant)
{
    return 1 + (kVariants + 1) * kPerRow + edge * kEdgeVariants + variant;
}
// The shadow at the foot of a thin wall standing along that edge of the tile
// itself (edges 0 and 1 only: shadows fall toward the camera).
[[nodiscard]] inline constexpr int shadowInsetGid(int edge)
{
    return 1 + (kVariants + 2) * kPerRow + 8 + edge;
}
// The shadow at the foot of a wall.
[[nodiscard]] inline constexpr int shadowGid(int edge, bool faint)
{
    return 1 + (kVariants + 2) * kPerRow + edge + (faint ? 4 : 0);
}

// Winter (phase 7), in the free end of the shadow row: snow lying on open
// ground (four variants, drawn with alpha by how much has fallen) and a
// frozen pond.
inline constexpr int kSnowVariants = 4;
[[nodiscard]] inline constexpr int snowGid(int variant)
{
    return 1 + (kVariants + 2) * kPerRow + 10 + variant;
}
[[nodiscard]] inline constexpr int iceGid()
{
    return 1 + (kVariants + 2) * kPerRow + 14;
}

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
