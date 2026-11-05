#include "world/Furniture.h"

#include <algorithm>
#include <array>
#include <random>

#include "map/TileIds.h"

namespace
{
using K = FurnitureKind;
using L = FurnitureLoot;

// Indexed by FurnitureKind. "tall" pieces rise more than 30 px.
constexpr std::array<FurnitureInfo, static_cast<std::size_t>(K::Count)> kInfo = {{
    {"furn_bed", true, false, L::None, "Sleep in bed"},
    {"furn_couch", true, false, L::None, nullptr},
    {"furn_table", false, false, L::None, nullptr},
    {"furn_bookshelf", true, true, L::Living, "Search bookshelf"},
    {"furn_wardrobe", true, true, L::Bedroom, "Search wardrobe"},
    {"furn_dresser", true, false, L::Bedroom, "Search dresser"},
    {"furn_fridge", true, true, L::Fridge, "Search fridge"},
    {"furn_stove", true, false, L::Kitchen, "Search oven"},
    {"furn_counter", true, false, L::Kitchen, "Search cupboard"},
    {"furn_sink", true, true, L::Bathroom, "Search cabinet"},
    {"furn_toilet", true, false, L::None, nullptr},
    {"furn_tvstand", true, false, L::Living, "Search TV stand"},
    {"furn_plant", false, false, L::None, nullptr},
    {"furn_storeshelf", true, true, L::Store, "Search shelf"},
    {"furn_checkout", true, false, L::Checkout, "Search register"},
    {"furn_crates", false, false, L::Storage, "Search crates"},
    {"furn_metalrack", true, true, L::Storage, "Search rack"},
    {"furn_workbench", true, true, L::Workshop, "Search workbench"},
    {"furn_barrels", false, false, L::Storage, "Search barrels"},
}};

enum class Role
{
    Studio,
    Kitchen,
    Bathroom,
    Bedroom,
    Living,
    KitchenLiving,
    Shop,
    Backroom,
    Workshop
};

struct Order
{
    K kind;
    int run = 1;          // pieces in a row along the same wall
    float chance = 1.0f;
};

std::vector<Order> recipeFor(Role role)
{
    switch (role)
    {
    case Role::Studio:
        return {{K::Bed}, {K::Fridge}, {K::Counter}, {K::Stove, 1, 0.6f}, {K::Couch, 1, 0.7f},
                {K::Table, 1, 0.6f}, {K::Bookshelf, 1, 0.4f}, {K::Plant, 1, 0.4f}};
    case Role::Kitchen:
        return {{K::Fridge}, {K::Stove}, {K::Counter, 2}, {K::Table, 1, 0.7f}, {K::Plant, 1, 0.3f}};
    case Role::Bathroom:
        return {{K::Toilet}, {K::Sink}};
    case Role::Bedroom:
        return {{K::Bed}, {K::Wardrobe}, {K::Dresser, 1, 0.8f}, {K::Bookshelf, 1, 0.3f}, {K::Plant, 1, 0.4f}};
    case Role::Living:
        return {{K::Couch}, {K::TvStand}, {K::Bookshelf, 1, 0.7f}, {K::Table, 1, 0.6f}, {K::Plant, 1, 0.6f}};
    case Role::KitchenLiving:
        return {{K::Fridge}, {K::Stove}, {K::Counter}, {K::Couch}, {K::TvStand, 1, 0.7f},
                {K::Table, 1, 0.6f}, {K::Plant, 1, 0.4f}};
    case Role::Shop:
        return {{K::Checkout}, {K::StoreShelf, 3}, {K::StoreShelf, 3}, {K::StoreShelf, 2},
                {K::StoreShelf, 2, 0.7f}, {K::Plant, 1, 0.5f}};
    case Role::Backroom:
        return {{K::MetalRack, 2}, {K::Crates}, {K::Barrels, 1, 0.6f}, {K::Crates, 1, 0.6f}, {K::Table, 1, 0.3f}};
    case Role::Workshop:
        return {{K::Workbench}, {K::MetalRack, 2}, {K::MetalRack, 2, 0.7f}, {K::Crates}, {K::Barrels},
                {K::Crates, 1, 0.7f}, {K::Barrels, 1, 0.5f}};
    }
    return {};
}

constexpr std::uint8_t kOccupied = 1; // furniture stands here
constexpr std::uint8_t kKeepClear = 2; // in front of a piece
constexpr std::uint8_t kReserved = 4; // by a doorway or the player start

struct Room
{
    std::vector<glm::ivec2> tiles;
    int floor = 0; // base ground GID
    Role role = Role::Living;
};

class Furnisher
{
public:
    Furnisher(const LayoutData& layout, std::uint32_t seed)
        : mLayout(layout),
          mW(layout.width),
          mH(layout.height),
          mRoomOf(static_cast<std::size_t>(std::max(0, layout.width * layout.height)), -1),
          mFlags(static_cast<std::size_t>(std::max(0, layout.width * layout.height)), 0),
          mDoor(static_cast<std::size_t>(std::max(0, layout.width * layout.height)), 0),
          mSeed(seed)
    {
    }

    std::vector<FurniturePiece> run()
    {
        if (mW <= 0 || mH <= 0 || mLayout.collision.size() < static_cast<std::size_t>(mW * mH))
        {
            return {};
        }
        for (const auto& door : mLayout.doors)
        {
            if (!inBounds(door.x, door.y)) continue;
            mDoor[index(door.x, door.y)] = 1;
            reserveAround(door.x, door.y);
        }
        reserveAround(mLayout.playerStart.x, mLayout.playerStart.y);

        for (std::size_t b = 0; b < mLayout.buildings.size(); ++b)
        {
            furnishBuilding(static_cast<int>(b));
        }
        return std::move(mPieces);
    }

private:
    [[nodiscard]] bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < mW && y < mH; }
    [[nodiscard]] std::size_t index(int x, int y) const
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(mW) + static_cast<std::size_t>(x);
    }
    [[nodiscard]] bool isWall(int x, int y) const
    {
        return !inBounds(x, y) || mLayout.collision[index(x, y)] != 0;
    }
    [[nodiscard]] int floorAt(int x, int y) const
    {
        if (!inBounds(x, y) || mLayout.ground.size() <= index(x, y)) return 0;
        const int gid = mLayout.ground[index(x, y)];
        return gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
    }
    [[nodiscard]] int roomOf(int x, int y) const { return inBounds(x, y) ? mRoomOf[index(x, y)] : -1; }

    void reserveAround(int cx, int cy)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (inBounds(cx + dx, cy + dy)) mFlags[index(cx + dx, cy + dy)] |= kReserved;
            }
        }
    }

    void furnishBuilding(int b)
    {
        const LayoutData::BuildingRect& rect = mLayout.buildings[static_cast<std::size_t>(b)];
        std::mt19937 rng(mSeed ^ (0x9E3779B9u * static_cast<std::uint32_t>(b + 1)));

        // Rooms: floor regions between walls and doorways.
        std::vector<Room> rooms;
        for (int y = rect.y + 1; y < rect.y + rect.h - 1; ++y)
        {
            for (int x = rect.x + 1; x < rect.x + rect.w - 1; ++x)
            {
                if (!isRoomFloor(x, y) || mRoomOf[index(x, y)] >= 0) continue;
                const int id = static_cast<int>(mRooms.size());
                Room room;
                room.floor = floorAt(x, y);
                std::vector<glm::ivec2> stack{{x, y}};
                mRoomOf[index(x, y)] = id;
                while (!stack.empty())
                {
                    const glm::ivec2 t = stack.back();
                    stack.pop_back();
                    room.tiles.push_back(t);
                    constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& d : kDirs)
                    {
                        const int nx = t.x + d[0];
                        const int ny = t.y + d[1];
                        if (nx <= rect.x || nx >= rect.x + rect.w - 1 || ny <= rect.y || ny >= rect.y + rect.h - 1) continue;
                        if (!isRoomFloor(nx, ny) || mRoomOf[index(nx, ny)] >= 0) continue;
                        mRoomOf[index(nx, ny)] = id;
                        stack.push_back({nx, ny});
                    }
                }
                // Scan order, so placement does not depend on the fill order.
                std::sort(room.tiles.begin(), room.tiles.end(), [](const glm::ivec2& a, const glm::ivec2& c)
                          { return a.y != c.y ? a.y < c.y : a.x < c.x; });
                mRooms.push_back(std::move(room));
                rooms.push_back(mRooms.back());
            }
        }
        if (rooms.empty()) return;

        const int firstRoom = static_cast<int>(mRooms.size() - rooms.size());
        assignRoles(rooms, archetypeForWidth(rect.w));
        for (std::size_t r = 0; r < rooms.size(); ++r)
        {
            furnishRoom(firstRoom + static_cast<int>(r), rooms[r].role, b + 1, rng);
        }
    }

    [[nodiscard]] bool isRoomFloor(int x, int y) const
    {
        if (!inBounds(x, y) || isWall(x, y) || mDoor[index(x, y)] != 0) return false;
        return !TileGid::isGrass(floorAt(x, y));
    }

    static void assignRoles(std::vector<Room>& rooms, BuildingArchetype archetype)
    {
        const auto largest = [&](auto pred) -> int
        {
            int best = -1;
            for (std::size_t i = 0; i < rooms.size(); ++i)
            {
                if (!pred(rooms[i])) continue;
                if (best < 0 || rooms[i].tiles.size() > rooms[static_cast<std::size_t>(best)].tiles.size())
                {
                    best = static_cast<int>(i);
                }
            }
            return best;
        };

        if (archetype != BuildingArchetype::Residential)
        {
            for (Room& room : rooms) room.role = Role::Backroom;
            const int main = largest([](const Room&) { return true; });
            rooms[static_cast<std::size_t>(main)].role =
                archetype == BuildingArchetype::Commercial ? Role::Shop : Role::Workshop;
            return;
        }

        if (rooms.size() == 1)
        {
            rooms[0].role = Role::Studio;
            return;
        }

        // Homes: the floor says what a room was.
        bool bathroom = false;
        bool kitchen = false;
        bool bedroom = false;
        std::vector<std::size_t> order(rooms.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t c)
                         { return rooms[a].tiles.size() < rooms[c].tiles.size(); });
        for (const std::size_t i : order)
        {
            Room& room = rooms[i];
            const int area = static_cast<int>(room.tiles.size());
            if (room.floor == TileGid::kTileFloor && !bathroom && area <= 8)
            {
                room.role = Role::Bathroom;
                bathroom = true;
            }
            else if (room.floor == TileGid::kTileFloor && !kitchen)
            {
                room.role = Role::Kitchen;
                kitchen = true;
            }
            else if (room.floor == TileGid::kCarpet)
            {
                room.role = Role::Bedroom;
                bedroom = true;
            }
            else
            {
                room.role = Role::Living;
            }
        }
        if (!kitchen)
        {
            int pick = largest([](const Room& r) { return r.role == Role::Living; });
            if (pick < 0) pick = largest([](const Room& r) { return r.role != Role::Bathroom; });
            if (pick >= 0) rooms[static_cast<std::size_t>(pick)].role = Role::KitchenLiving;
        }
        if (!bedroom)
        {
            const int pick = largest([](const Room& r) { return r.role == Role::Living; });
            if (pick >= 0) rooms[static_cast<std::size_t>(pick)].role = Role::Bedroom;
        }
    }

    void furnishRoom(int roomId, Role role, int building, std::mt19937& rng)
    {
        const Room& room = mRooms[static_cast<std::size_t>(roomId)];
        const int area = static_cast<int>(room.tiles.size());
        if (area < 4) return;
        const int capacity = std::max(1, area / 3);
        int placed = 0;

        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        for (const Order& order : recipeFor(role))
        {
            if (placed >= capacity) break;
            if (chance(rng) > order.chance) continue;

            std::vector<glm::ivec2> candidates = room.tiles;
            std::shuffle(candidates.begin(), candidates.end(), rng);
            for (const glm::ivec2& tile : candidates)
            {
                FurniturePiece piece;
                piece.kind = order.kind;
                piece.building = building;
                piece.tile = tile;
                if (!tryPlace(roomId, piece, rng)) continue;
                ++placed;

                // Runs continue along the same wall, either way.
                const glm::ivec2 along = piece.backNorth ? glm::ivec2(1, 0) : glm::ivec2(0, 1);
                int left = order.run - 1;
                for (const int sign : {1, -1})
                {
                    for (int k = 1; left > 0 && placed < capacity; ++k)
                    {
                        FurniturePiece next = piece;
                        next.tile = piece.tile + along * (sign * k);
                        if (!tryPlaceOriented(roomId, next, piece.backNorth)) break;
                        ++placed;
                        --left;
                    }
                }
                break;
            }
        }
    }

    // Places a piece at piece.tile if the room allows it; picks its facing.
    bool tryPlace(int roomId, FurniturePiece& piece, std::mt19937& rng)
    {
        const FurnitureInfo& def = kInfo[static_cast<std::size_t>(piece.kind)];
        if (def.oriented)
        {
            const bool northFirst = (rng() & 1u) != 0;
            return tryPlaceOriented(roomId, piece, northFirst) || tryPlaceOriented(roomId, piece, !northFirst);
        }
        const glm::ivec2 t = piece.tile;
        if (!isFree(roomId, t)) return false;
        const bool byWall = isWall(t.x - 1, t.y) || isWall(t.x + 1, t.y) || isWall(t.x, t.y - 1) || isWall(t.x, t.y + 1);
        if (!byWall && piece.kind != K::Table) return false;
        return commit(roomId, piece, nullptr);
    }

    bool tryPlaceOriented(int roomId, FurniturePiece& piece, bool backNorth)
    {
        const glm::ivec2 t = piece.tile;
        if (!isFree(roomId, t)) return false;
        const glm::ivec2 back = backNorth ? glm::ivec2(t.x, t.y - 1) : glm::ivec2(t.x - 1, t.y);
        if (!isWall(back.x, back.y)) return false;
        const glm::ivec2 front = backNorth ? glm::ivec2(t.x, t.y + 1) : glm::ivec2(t.x + 1, t.y);
        if (roomOf(front.x, front.y) != roomId || (mFlags[index(front.x, front.y)] & kOccupied) != 0) return false;
        piece.backNorth = backNorth;
        return commit(roomId, piece, &front);
    }

    [[nodiscard]] bool isFree(int roomId, const glm::ivec2& t) const
    {
        if (roomOf(t.x, t.y) != roomId) return false;
        return (mFlags[index(t.x, t.y)] & (kOccupied | kKeepClear | kReserved)) == 0;
    }

    // Occupies the tile if the room stays connected and every piece in it can
    // still be reached; otherwise leaves everything as it was.
    bool commit(int roomId, const FurniturePiece& piece, const glm::ivec2* front)
    {
        const std::size_t at = index(piece.tile.x, piece.tile.y);
        const std::uint8_t savedFront = front != nullptr ? mFlags[index(front->x, front->y)] : 0;
        mFlags[at] |= kOccupied;
        if (front != nullptr) mFlags[index(front->x, front->y)] |= kKeepClear;

        if (roomStaysOpen(roomId, piece.tile))
        {
            mPieces.push_back(piece);
            return true;
        }
        mFlags[at] &= static_cast<std::uint8_t>(~kOccupied);
        if (front != nullptr) mFlags[index(front->x, front->y)] = savedFront;
        return false;
    }

    [[nodiscard]] bool roomStaysOpen(int roomId, const glm::ivec2& placed) const
    {
        const Room& room = mRooms[static_cast<std::size_t>(roomId)];
        const auto open = [&](int x, int y)
        { return roomOf(x, y) == roomId && (mFlags[index(x, y)] & kOccupied) == 0; };

        // Every free tile reachable from every other.
        int freeCount = 0;
        glm::ivec2 start{-1, -1};
        for (const glm::ivec2& t : room.tiles)
        {
            if (!open(t.x, t.y)) continue;
            ++freeCount;
            if (start.x < 0) start = t;
        }
        if (freeCount == 0) return false;

        std::vector<std::uint8_t> seen(room.tiles.size(), 0);
        const auto slot = [&](const glm::ivec2& t) -> std::size_t
        {
            const auto it = std::lower_bound(room.tiles.begin(), room.tiles.end(), t, [](const glm::ivec2& a, const glm::ivec2& c)
                                             { return a.y != c.y ? a.y < c.y : a.x < c.x; });
            return static_cast<std::size_t>(it - room.tiles.begin());
        };
        std::vector<glm::ivec2> stack{start};
        seen[slot(start)] = 1;
        int reached = 0;
        constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        while (!stack.empty())
        {
            const glm::ivec2 t = stack.back();
            stack.pop_back();
            ++reached;
            for (const auto& d : kDirs)
            {
                const glm::ivec2 n(t.x + d[0], t.y + d[1]);
                if (!open(n.x, n.y)) continue;
                const std::size_t s = slot(n);
                if (seen[s] != 0) continue;
                seen[s] = 1;
                stack.push_back(n);
            }
        }
        if (reached != freeCount) return false;

        // Every piece in the room has an open side to be used from.
        for (const FurniturePiece& p : mPieces)
        {
            if (roomOf(p.tile.x, p.tile.y) != roomId) continue;
            if (!hasOpenSide(p.tile, open)) return false;
        }
        return hasOpenSide(placed, open);
    }

    template <typename Open>
    static bool hasOpenSide(const glm::ivec2& t, const Open& open)
    {
        return open(t.x + 1, t.y) || open(t.x - 1, t.y) || open(t.x, t.y + 1) || open(t.x, t.y - 1);
    }

    const LayoutData& mLayout;
    int mW = 0;
    int mH = 0;
    std::vector<int> mRoomOf;
    std::vector<std::uint8_t> mFlags;
    std::vector<std::uint8_t> mDoor;
    std::vector<Room> mRooms;
    std::vector<FurniturePiece> mPieces;
    std::uint32_t mSeed = 0;
};
} // namespace

namespace furniture
{
const FurnitureInfo& info(FurnitureKind kind)
{
    const auto i = std::min(static_cast<std::size_t>(kind), kInfo.size() - 1);
    return kInfo[i];
}

std::string spriteName(const FurniturePiece& piece)
{
    const FurnitureInfo& def = info(piece.kind);
    std::string name = def.sprite;
    if (def.oriented)
    {
        name += piece.backNorth ? "_l" : "_r";
    }
    return name;
}

glm::ivec2 frontTile(const FurniturePiece& piece)
{
    return piece.backNorth ? piece.tile + glm::ivec2(0, 1) : piece.tile + glm::ivec2(1, 0);
}

std::vector<FurniturePiece> furnish(const LayoutData& layout, std::uint32_t seed)
{
    return Furnisher(layout, seed).run();
}
} // namespace furniture
