// Furniture: placed against walls, never in doorways, and never boxing in a
// room or another piece; blocks movement but not sight.
#include "TestFramework.h"

#include <algorithm>
#include <cstdlib>

#include "map/TileIds.h"
#include "map/TileMap.h"
#include "world/Furniture.h"

namespace
{
// A layout of grass with buildings stamped onto it.
struct Town
{
    LayoutData layout;

    Town(int w, int h)
    {
        layout.width = w;
        layout.height = h;
        layout.ground.assign(static_cast<std::size_t>(w * h), TileGid::kGrass);
        layout.collision.assign(static_cast<std::size_t>(w * h), 0);
        layout.surface.assign(static_cast<std::size_t>(w * h), 0);
        layout.district.assign(static_cast<std::size_t>(w * h), 0);
        layout.playerStart = {0, 0};
    }

    std::size_t at(int x, int y) const { return static_cast<std::size_t>(y * layout.width + x); }
    void wall(int x, int y) { layout.collision[at(x, y)] = 1; layout.ground[at(x, y)] = TileGid::kWall; }
    void floor(int x, int y, int gid) { layout.collision[at(x, y)] = 0; layout.ground[at(x, y)] = gid; }
    void door(int x, int y)
    {
        floor(x, y, TileGid::kFloor);
        layout.doors.push_back(LayoutData::DoorPos{x, y});
    }
    // Outer walls round a rectangle, floor inside.
    void building(int bx, int by, int bw, int bh, int gid)
    {
        layout.buildings.push_back(LayoutData::BuildingRect{bx, by, bw, bh});
        for (int y = by; y < by + bh; ++y)
        {
            for (int x = bx; x < bx + bw; ++x)
            {
                const bool edge = x == bx || y == by || x == bx + bw - 1 || y == by + bh - 1;
                if (edge) wall(x, y);
                else floor(x, y, gid);
            }
        }
    }
    [[nodiscard]] bool isWall(int x, int y) const
    {
        return x < 0 || y < 0 || x >= layout.width || y >= layout.height || layout.collision[at(x, y)] != 0;
    }
};

// A house 9 wide: a tiled kitchen on the left, a carpeted bedroom on the
// right, an inner wall between them with a doorway, a front door on the west.
Town house()
{
    Town town(12, 10);
    town.building(1, 1, 9, 8, TileGid::kTileFloor);
    for (int y = 2; y <= 7; ++y)
    {
        town.wall(5, y);
        for (int x = 6; x <= 8; ++x) town.floor(x, y, TileGid::kCarpet);
    }
    town.door(5, 4);
    town.door(1, 4);
    return town;
}

// One big workshop floor (14 wide: industrial), door on the north wall.
Town workshop()
{
    Town town(16, 11);
    town.building(1, 1, 14, 9, TileGid::kFloor);
    town.door(7, 1);
    return town;
}

bool hasPiece(const std::vector<FurniturePiece>& pieces, FurnitureKind kind)
{
    return std::any_of(pieces.begin(), pieces.end(), [&](const FurniturePiece& p) { return p.kind == kind; });
}

bool occupied(const std::vector<FurniturePiece>& pieces, int x, int y)
{
    return std::any_of(pieces.begin(), pieces.end(), [&](const FurniturePiece& p) { return p.tile.x == x && p.tile.y == y; });
}

// Every open floor tile in the building is reachable from its first door.
bool allReachable(const Town& town, const std::vector<FurniturePiece>& pieces, const LayoutData::BuildingRect& b)
{
    const auto open = [&](int x, int y)
    {
        return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h && !town.isWall(x, y) && !occupied(pieces, x, y);
    };
    std::vector<int> seen(static_cast<std::size_t>(town.layout.width * town.layout.height), 0);
    const auto& start = town.layout.doors.back();
    std::vector<glm::ivec2> stack{{start.x, start.y}};
    seen[town.at(start.x, start.y)] = 1;
    while (!stack.empty())
    {
        const glm::ivec2 t = stack.back();
        stack.pop_back();
        constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& d : kDirs)
        {
            const int nx = t.x + d[0];
            const int ny = t.y + d[1];
            if (!open(nx, ny) || seen[town.at(nx, ny)] != 0) continue;
            seen[town.at(nx, ny)] = 1;
            stack.push_back({nx, ny});
        }
    }
    for (int y = b.y; y < b.y + b.h; ++y)
    {
        for (int x = b.x; x < b.x + b.w; ++x)
        {
            if (open(x, y) && seen[town.at(x, y)] == 0) return false;
        }
    }
    // And every piece can be used from a reachable tile.
    for (const FurniturePiece& p : pieces)
    {
        const bool usable = (open(p.tile.x + 1, p.tile.y) && seen[town.at(p.tile.x + 1, p.tile.y)] != 0) ||
                            (open(p.tile.x - 1, p.tile.y) && seen[town.at(p.tile.x - 1, p.tile.y)] != 0) ||
                            (open(p.tile.x, p.tile.y + 1) && seen[town.at(p.tile.x, p.tile.y + 1)] != 0) ||
                            (open(p.tile.x, p.tile.y - 1) && seen[town.at(p.tile.x, p.tile.y - 1)] != 0);
        if (!usable) return false;
    }
    return true;
}
} // namespace

DP_TEST(furniture_blocks_movement_not_sight)
{
    TileMap map;
    map.resize(4, 4, 32, 32);
    DP_CHECK(!map.isSolid(1, 1));
    map.setFurniture(1, 1, true);
    DP_CHECK(map.hasFurniture(1, 1));
    DP_CHECK(map.isSolid(1, 1));
    DP_CHECK(!map.blocksSight(1, 1));
    map.clearFurniture();
    DP_CHECK(!map.isSolid(1, 1));
}

DP_TEST(furniture_rooms_get_their_pieces)
{
    for (std::uint32_t seed = 1; seed <= 20; ++seed)
    {
        const Town town = house();
        const auto pieces = furniture::furnish(town.layout, seed);
        DP_CHECK(hasPiece(pieces, FurnitureKind::Bed));
        DP_CHECK(hasPiece(pieces, FurnitureKind::Fridge));
        // Kitchen things in the tiled room, the bed on the carpet.
        for (const FurniturePiece& p : pieces)
        {
            if (p.kind == FurnitureKind::Bed) DP_CHECK(p.tile.x >= 6);
            if (p.kind == FurnitureKind::Fridge || p.kind == FurnitureKind::Stove) DP_CHECK(p.tile.x <= 4);
            DP_CHECK(p.building == 1);
        }
    }
    const Town shop = workshop();
    DP_CHECK(hasPiece(furniture::furnish(shop.layout, 7), FurnitureKind::Workbench));
}

DP_TEST(furniture_stays_out_of_doorways)
{
    for (std::uint32_t seed = 1; seed <= 20; ++seed)
    {
        for (const Town& town : {house(), workshop()})
        {
            for (const FurniturePiece& p : furniture::furnish(town.layout, seed))
            {
                DP_CHECK(!town.isWall(p.tile.x, p.tile.y));
                DP_CHECK(town.layout.ground[town.at(p.tile.x, p.tile.y)] != TileGid::kGrass);
                for (const auto& door : town.layout.doors)
                {
                    DP_CHECK(std::max(std::abs(p.tile.x - door.x), std::abs(p.tile.y - door.y)) > 1);
                }
            }
        }
    }
}

DP_TEST(furniture_backs_onto_walls)
{
    for (std::uint32_t seed = 1; seed <= 20; ++seed)
    {
        for (const Town& town : {house(), workshop()})
        {
            const auto pieces = furniture::furnish(town.layout, seed);
            for (const FurniturePiece& p : pieces)
            {
                if (!furniture::info(p.kind).oriented) continue;
                const glm::ivec2 back = p.backNorth ? p.tile + glm::ivec2(0, -1) : p.tile + glm::ivec2(-1, 0);
                DP_CHECK(town.isWall(back.x, back.y));
                const glm::ivec2 front = furniture::frontTile(p);
                DP_CHECK(!town.isWall(front.x, front.y));
                DP_CHECK(!occupied(pieces, front.x, front.y));
            }
        }
    }
}

DP_TEST(furniture_never_boxes_anything_in)
{
    for (std::uint32_t seed = 1; seed <= 40; ++seed)
    {
        for (const Town& town : {house(), workshop()})
        {
            const auto pieces = furniture::furnish(town.layout, seed);
            DP_CHECK(!pieces.empty());
            DP_CHECK(allReachable(town, pieces, town.layout.buildings[0]));
        }
    }
}

DP_TEST(furniture_is_deterministic)
{
    const Town town = house();
    const auto a = furniture::furnish(town.layout, 99);
    const auto b = furniture::furnish(town.layout, 99);
    DP_CHECK(a.size() == b.size());
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i)
    {
        DP_CHECK(a[i].tile == b[i].tile);
        DP_CHECK(a[i].kind == b[i].kind);
        DP_CHECK(a[i].backNorth == b[i].backNorth);
    }
    DP_CHECK(furniture::spriteName(FurniturePiece{{0, 0}, FurnitureKind::Fridge, true, 1}) == "furn_fridge_l");
    DP_CHECK(furniture::spriteName(FurniturePiece{{0, 0}, FurnitureKind::Plant, true, 1}) == "furn_plant");
}
