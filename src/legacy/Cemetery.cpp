#include "legacy/Cemetery.h"

#include <algorithm>
#include <cmath>

#include "map/TileIds.h"

namespace cemetery
{
namespace
{
int baseGid(int gid)
{
    return gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
}

bool openGround(int gid)
{
    const int base = baseGid(gid);
    return base == TileGid::kGrass || base == TileGid::kGrass2 || base == TileGid::kDirt;
}
} // namespace

bool inside(const LayoutData::CemeterySite& site, const glm::ivec2& tile)
{
    return site.placed && tile.x >= site.fenceMin.x && tile.x <= site.fenceMax.x && tile.y >= site.fenceMin.y &&
           tile.y <= site.fenceMax.y;
}

bool isFence(const LayoutData::CemeterySite& site, const glm::ivec2& tile)
{
    if (!inside(site, tile) || tile == site.opening) return false;
    return tile.x == site.fenceMin.x || tile.x == site.fenceMax.x || tile.y == site.fenceMin.y || tile.y == site.fenceMax.y;
}

std::vector<glm::ivec2> fenceTiles(const LayoutData::CemeterySite& site)
{
    std::vector<glm::ivec2> out;
    if (!site.placed) return out;
    for (int y = site.fenceMin.y; y <= site.fenceMax.y; ++y)
    {
        for (int x = site.fenceMin.x; x <= site.fenceMax.x; ++x)
        {
            if (isFence(site, glm::ivec2(x, y))) out.emplace_back(x, y);
        }
    }
    return out;
}

bool stamp(LayoutData& layout, std::string* why)
{
    if (layout.cemetery.placed) return true;
    const int w = layout.width;
    const int h = layout.height;
    const auto size = static_cast<std::size_t>(std::max(0, w * h));
    if (w < kWidth + 4 || h < kHeight + 4 || layout.ground.size() < size || layout.collision.size() < size)
    {
        if (why != nullptr) *why = "no map";
        return false;
    }
    const auto index = [&](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x); };
    std::vector<char> built(size, 0);
    for (const LayoutData::BuildingRect& b : layout.buildings)
    {
        for (int y = std::max(0, b.y); y < std::min(h, b.y + b.h); ++y)
        {
            for (int x = std::max(0, b.x); x < std::min(w, b.x + b.w); ++x) built[index(x, y)] = 1;
        }
    }
    const auto inBuilding = [&](int x, int y) { return built[index(x, y)] != 0; };
    const auto inYard = [&](int x, int y)
    {
        const LayoutData::GateSite& g = layout.gate;
        return g.placed && x >= g.yardMin.x - 1 && x <= g.yardMax.x + 1 && y >= g.yardMin.y - 1 && y <= g.yardMax.y + 1;
    };
    // Room: open ground, nothing built and nothing in the way, in the plot and
    // a tile round it.
    const auto fits = [&](int x0, int y0)
    {
        for (int y = y0 - 1; y <= y0 + kHeight; ++y)
        {
            for (int x = x0 - 1; x <= x0 + kWidth; ++x)
            {
                if (x < 1 || y < 1 || x >= w - 1 || y >= h - 1) return false;
                const std::size_t i = index(x, y);
                if (layout.collision[i] != 0 || !openGround(layout.ground[i]) || inBuilding(x, y) || inYard(x, y)) return false;
            }
        }
        return true;
    };

    struct Candidate
    {
        int x;
        int y;
        float distance;
    };
    std::vector<Candidate> candidates;
    const float cx = static_cast<float>(w) * 0.5f;
    const float cy = static_cast<float>(h) * 0.5f;
    for (int y = 1; y + kHeight < h - 1; ++y)
    {
        for (int x = 1; x + kWidth < w - 1; ++x)
        {
            const float dx = static_cast<float>(x) + kWidth * 0.5f - cx;
            const float dy = static_cast<float>(y) + kHeight * 0.5f - cy;
            candidates.push_back(Candidate{x, y, dx * dx + dy * dy});
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
    for (const Candidate& c : candidates)
    {
        if (!fits(c.x, c.y)) continue;
        LayoutData::CemeterySite site;
        site.placed = true;
        site.fenceMin = glm::ivec2(c.x, c.y);
        site.fenceMax = glm::ivec2(c.x + kWidth - 1, c.y + kHeight - 1);
        site.opening = glm::ivec2(c.x + kWidth / 2, c.y + kHeight - 1);
        // Mown grass inside, the fence line on grass too.
        for (int y = site.fenceMin.y; y <= site.fenceMax.y; ++y)
        {
            for (int x = site.fenceMin.x; x <= site.fenceMax.x; ++x)
            {
                layout.ground[index(x, y)] = TileGid::kGrass;
            }
        }
        layout.cemetery = site;
        return true;
    }
    if (why != nullptr) *why = "no open ground for it";
    return false;
}

} // namespace cemetery
