#include "world/Windows.h"

#include <algorithm>
#include <cmath>

#include "map/TileIds.h"

namespace
{
std::uint32_t hashTile(int x, int y)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093u ^ static_cast<std::uint32_t>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}
} // namespace

namespace windows
{
std::vector<Spot> place(const TileMap& map, const LayoutData& layout)
{
    const int width = map.width();
    const int height = map.height();
    const auto inBounds = [&](int x, int y) { return x >= 0 && y >= 0 && x < width && y < height; };
    const auto index = [&](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x); };

    // 0 none, 1 wall, 2 doorway: as StructureView sees them.
    std::vector<std::uint8_t> kind(static_cast<std::size_t>(std::max(0, width * height)), 0);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (map.isSolid(x, y) && TileGid::isWallGid(map.groundGid(x, y))) kind[index(x, y)] = 1;
        }
    }
    for (const auto& door : layout.doors)
    {
        if (inBounds(door.x, door.y) && kind[index(door.x, door.y)] == 0) kind[index(door.x, door.y)] = 2;
    }
    const auto wallLike = [&](int x, int y) { return inBounds(x, y) && kind[index(x, y)] != 0; };
    const auto isDoorway = [&](int x, int y) { return inBounds(x, y) && kind[index(x, y)] == 2; };

    std::vector<Spot> spots;
    const auto buildingCount = static_cast<int>(layout.buildings.size());
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (kind[index(x, y)] != 1) continue;
            const int building = map.getBuildingId(x, y);
            if (building <= 0 || building > buildingCount) continue;
            if (archetypeForWidth(layout.buildings[static_cast<std::size_t>(building - 1)].w) ==
                BuildingArchetype::Industrial)
            {
                continue; // sheds have no windows
            }
            const bool alongX = wallLike(x - 1, y) || wallLike(x + 1, y);
            const bool alongY = wallLike(x, y - 1) || wallLike(x, y + 1);
            if (alongX == alongY) continue; // a corner, or a lone post
            bool perimeter = false;
            for (int dy = -1; dy <= 1 && !perimeter; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (!inBounds(x + dx, y + dy) || map.getBuildingId(x + dx, y + dy) != building)
                    {
                        perimeter = true;
                        break;
                    }
                }
            }
            if (!perimeter) continue;
            const int along = alongX ? x : y;
            const bool nearDoor = alongX ? (isDoorway(x - 1, y) || isDoorway(x + 1, y))
                                         : (isDoorway(x, y - 1) || isDoorway(x, y + 1));
            const bool flanked = alongX ? (wallLike(x - 1, y) && wallLike(x + 1, y))
                                        : (wallLike(x, y - 1) && wallLike(x, y + 1));
            if (!flanked || nearDoor || along % 3 != 1 || (hashTile(x, y) % 4) == 0) continue;

            // The floor on one side and the street on the other.
            const glm::ivec2 a = alongX ? glm::ivec2(x, y - 1) : glm::ivec2(x - 1, y);
            const glm::ivec2 b = alongX ? glm::ivec2(x, y + 1) : glm::ivec2(x + 1, y);
            const auto floorOf = [&](const glm::ivec2& t)
            {
                return inBounds(t.x, t.y) && kind[index(t.x, t.y)] == 0 && map.getBuildingId(t.x, t.y) == building;
            };
            const auto streetOf = [&](const glm::ivec2& t)
            {
                return inBounds(t.x, t.y) && kind[index(t.x, t.y)] == 0 && map.getBuildingId(t.x, t.y) != building;
            };
            Spot spot;
            spot.tile = glm::ivec2(x, y);
            spot.building = building;
            if (floorOf(a) && streetOf(b))
            {
                spot.inside = a;
                spot.outside = b;
            }
            else if (floorOf(b) && streetOf(a))
            {
                spot.inside = b;
                spot.outside = a;
            }
            else
            {
                continue; // nothing to see through
            }
            spots.push_back(spot);
        }
    }
    return spots;
}
} // namespace windows

int Windows::boardsFor(float boardHp)
{
    if (boardHp <= 0.0f) return 0;
    return std::clamp(static_cast<int>(std::ceil(boardHp / kBoardHp - 0.001f)), 1, kMaxBoards);
}

void Windows::reset(TileMap& map, const std::vector<windows::Spot>& spots)
{
    for (const Window& window : mWindows)
    {
        map.setWindowState(window.spot.tile.x, window.spot.tile.y, WindowState::None);
        map.setWindowBoards(window.spot.tile.x, window.spot.tile.y, 0);
        map.setWindowCovered(window.spot.tile.x, window.spot.tile.y, false);
    }
    mWindows.clear();
    mWindows.reserve(spots.size());
    for (const windows::Spot& spot : spots)
    {
        Window window;
        window.spot = spot;
        mWindows.push_back(window);
        map.setWindowState(spot.tile.x, spot.tile.y, WindowState::Intact);
        map.setWindowBoards(spot.tile.x, spot.tile.y, 0);
        map.setWindowCovered(spot.tile.x, spot.tile.y, false);
    }
}

Windows::Blow Windows::batter(TileMap& map, const glm::ivec2& tile, float damage)
{
    Window* window = findMutable(tile);
    if (window == nullptr) return Blow::None;
    if (window->boardHp > 0.0f)
    {
        const int before = boardsFor(window->boardHp);
        window->boardHp = std::max(0.0f, window->boardHp - damage / std::max(0.1f, window->boardToughness));
        const int after = boardsFor(window->boardHp);
        map.setWindowBoards(tile.x, tile.y, after);
        if (after < before)
        {
            window->sinceBang = 0.0f;
            return Blow::Splinters;
        }
    }
    else if (map.windowState(tile.x, tile.y) == WindowState::Intact)
    {
        window->glassHp -= damage;
        if (window->glassHp <= 0.0f)
        {
            breakGlass(map, tile);
            window->sinceBang = 0.0f;
            return Blow::Shatters;
        }
    }
    else
    {
        return Blow::None; // an open frame: climbed, not battered
    }
    if (window->sinceBang >= kBangInterval)
    {
        window->sinceBang = 0.0f;
        return Blow::Bang;
    }
    return Blow::Thud;
}

bool Windows::breakGlass(TileMap& map, const glm::ivec2& tile)
{
    Window* window = findMutable(tile);
    if (window == nullptr || map.windowState(tile.x, tile.y) != WindowState::Intact) return false;
    window->glassHp = 0.0f;
    map.setWindowState(tile.x, tile.y, WindowState::Broken);
    return true;
}

bool Windows::addBoard(TileMap& map, const glm::ivec2& tile, float toughness)
{
    Window* window = findMutable(tile);
    if (window == nullptr || boardsFor(window->boardHp) >= kMaxBoards) return false;
    const float before = static_cast<float>(boardsFor(window->boardHp));
    window->boardToughness = (window->boardToughness * before + toughness) / (before + 1.0f);
    window->boardHp = std::min(window->boardHp + kBoardHp, kBoardHp * static_cast<float>(kMaxBoards));
    map.setWindowBoards(tile.x, tile.y, boardsFor(window->boardHp));
    return true;
}

bool Windows::removeBoard(TileMap& map, const glm::ivec2& tile)
{
    Window* window = findMutable(tile);
    if (window == nullptr || window->boardHp <= 0.0f) return false;
    window->boardHp = kBoardHp * static_cast<float>(boardsFor(window->boardHp) - 1);
    if (window->boardHp <= 0.0f) window->boardToughness = 1.0f;
    map.setWindowBoards(tile.x, tile.y, boardsFor(window->boardHp));
    return true;
}

void Windows::restore(TileMap& map, const glm::ivec2& tile, bool broken, float boardHp, bool covered, float toughness)
{
    Window* window = findMutable(tile);
    if (window == nullptr) return;
    window->glassHp = broken ? 0.0f : kGlassHp;
    window->boardHp = std::clamp(boardHp, 0.0f, kBoardHp * static_cast<float>(kMaxBoards));
    window->boardToughness = window->boardHp > 0.0f ? std::max(0.1f, toughness) : 1.0f;
    map.setWindowState(tile.x, tile.y, broken ? WindowState::Broken : WindowState::Intact);
    map.setWindowBoards(tile.x, tile.y, boardsFor(window->boardHp));
    map.setWindowCovered(tile.x, tile.y, covered);
}

int Windows::boards(const glm::ivec2& tile) const
{
    const Window* window = find(tile);
    return window != nullptr ? boardsFor(window->boardHp) : 0;
}

void Windows::tick(float dtSeconds)
{
    for (Window& window : mWindows) window.sinceBang += dtSeconds;
}

const Windows::Window* Windows::find(const glm::ivec2& tile) const
{
    for (const Window& window : mWindows)
    {
        if (window.spot.tile == tile) return &window;
    }
    return nullptr;
}

Windows::Window* Windows::findMutable(const glm::ivec2& tile)
{
    for (Window& window : mWindows)
    {
        if (window.spot.tile == tile) return &window;
    }
    return nullptr;
}

const Windows::Window* Windows::nearest(const TileMap& map, const glm::vec2& world, float range) const
{
    const float tileW = static_cast<float>(map.tileWidth());
    const float tileH = static_cast<float>(map.tileHeight());
    const Window* best = nullptr;
    float bestDist = range * range;
    for (const Window& window : mWindows)
    {
        const float dx = (static_cast<float>(window.spot.tile.x) + 0.5f) * tileW - world.x;
        const float dy = (static_cast<float>(window.spot.tile.y) + 0.5f) * tileH - world.y;
        const float dist = dx * dx + dy * dy;
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = &window;
        }
    }
    return best;
}
