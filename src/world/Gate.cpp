#include "world/Gate.h"

#include <algorithm>
#include <cmath>

#include "map/TileIds.h"
#include "world/BuildingKinds.h"

namespace
{
int baseGid(int gid)
{
    return gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
}

// Local coordinates at one side of the map: u inward from the boundary
// (0 is the wall), v along it.
struct Frame
{
    int side = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] glm::ivec2 world(int u, int v) const
    {
        switch (static_cast<gate::Side>(side))
        {
        case gate::Side::East: return {width - 1 - u, v};
        case gate::Side::West: return {u, v};
        case gate::Side::South: return {v, height - 1 - u};
        case gate::Side::North: return {v, u};
        }
        return {u, v};
    }
    [[nodiscard]] int along() const { return side <= 1 ? height : width; }
};

std::uint32_t hashMix(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

glm::vec2 centreOf(const LayoutData::BuildingRect& b)
{
    return glm::vec2(static_cast<float>(b.x) + static_cast<float>(b.w) * 0.5f,
                     static_cast<float>(b.y) + static_cast<float>(b.h) * 0.5f);
}

glm::vec2 centreOf(const LayoutData::DistrictDef& d)
{
    return glm::vec2(static_cast<float>(d.minX + d.maxX) * 0.5f + 0.5f, static_cast<float>(d.minY + d.maxY) * 0.5f + 0.5f);
}

const char* deedWords(const std::string& deed, int count)
{
    if (deed == "fuse") return "fitted the fuse";
    if (deed == "can") return count == 1 ? "poured a can" : nullptr;
    if (deed == "repair") return count == 1 ? "worked on the generator" : nullptr;
    if (deed == "freed") return "freed the generator";
    if (deed == "sergeant") return "killed the Sergeant";
    if (deed == "started") return "started the motor";
    if (deed == "held") return "held the gate";
    if (deed == "left") return "got out";
    return nullptr;
}
} // namespace

namespace gate
{
const char* sideName(int side)
{
    switch (static_cast<Side>(side))
    {
    case Side::East: return "east";
    case Side::West: return "west";
    case Side::South: return "south";
    case Side::North: return "north";
    }
    return "east";
}

bool ready(const State& state, int fuelNeeded)
{
    return state.fuseFitted && state.freed && state.fuel >= std::max(1, fuelNeeded);
}

std::string missingText(const State& state, bool keyInHand, int fuelNeeded)
{
    std::vector<std::string> parts;
    if (!state.fuseFitted) parts.emplace_back("fuse");
    if (state.fuel < std::max(1, fuelNeeded)) parts.push_back("fuel " + std::to_string(state.fuel) + " of " + std::to_string(kFuelNeeded));
    if (!state.freed) parts.emplace_back("generator");
    if (!keyInHand) parts.emplace_back("key");
    if (parts.empty()) return "";
    std::string text = "Needs: ";
    for (std::size_t i = 0; i < parts.size(); ++i) text += (i > 0 ? ", " : "") + parts[i];
    return text + ".";
}

void credit(State& state, const std::string& name, const std::string& deed, int run)
{
    if (name.empty()) return;
    // One of each deed per person, except the cans and the repair sessions.
    if (deed != "can" && deed != "repair")
    {
        for (const Contributor& c : state.contributors)
        {
            if (c.name == name && c.deed == deed) return;
        }
    }
    state.contributors.push_back(Contributor{name, deed, run});
}

std::vector<std::string> contributorNames(const State& state)
{
    std::vector<std::string> names;
    for (const Contributor& c : state.contributors)
    {
        if (std::find(names.begin(), names.end(), c.name) == names.end()) names.push_back(c.name);
    }
    return names;
}

std::vector<std::string> contributorLines(const State& state)
{
    std::vector<std::string> lines;
    for (const std::string& name : contributorNames(state))
    {
        // Deeds in the order first done, repeats counted.
        std::vector<std::pair<std::string, int>> deeds;
        bool freed = false;
        for (const Contributor& c : state.contributors)
        {
            if (c.name != name) continue;
            if (c.deed == "freed") freed = true;
            auto it = std::find_if(deeds.begin(), deeds.end(), [&](const auto& d) { return d.first == c.deed; });
            if (it == deeds.end()) deeds.emplace_back(c.deed, 1);
            else ++it->second;
        }
        std::string line = name + ":";
        bool first = true;
        for (const auto& [deed, count] : deeds)
        {
            if (deed == "repair" && freed) continue; // freeing it says it
            std::string words;
            if (const char* w = deedWords(deed, count)) words = w;
            else if (deed == "can") words = "poured " + std::to_string(count) + " cans";
            else if (deed == "repair") words = "worked on the generator " + std::to_string(count) + " times";
            else words = deed;
            line += (first ? " " : ", ") + words;
            first = false;
        }
        lines.push_back(line + ".");
    }
    return lines;
}

int baseCrowd(int infection)
{
    const int clamped = std::clamp(infection, 0, 100);
    const int crowd = 4 + clamped / 12;
    return clamped >= kOverwhelmedInfection ? crowd * 2 : crowd;
}

int waveSize(int wave)
{
    return 3 + std::max(0, wave);
}

int stage(float opened)
{
    if (opened >= kOpenSeconds) return 3;
    return std::clamp(static_cast<int>(std::floor(opened / (kOpenSeconds / 3.0f))), 0, 2);
}

void start(Stand& stand)
{
    stand.grind = stand.started ? kGrindSeconds : 0.0f;
    stand.started = true;
    stand.running = true;
    stand.noiseTimer = 0.0f;
}

int run(Stand& stand, State& state, float dtSeconds)
{
    if (!stand.running || dtSeconds <= 0.0f) return 0;
    stand.elapsed += dtSeconds;
    if (stand.grind > 0.0f)
    {
        stand.grind = std::max(0.0f, stand.grind - dtSeconds);
    }
    else
    {
        stand.opened = std::min(kOpenSeconds, stand.opened + dtSeconds);
    }
    int cans = 0;
    state.burned += dtSeconds;
    while (state.burned >= kSecondsPerCan && state.fuel > 0)
    {
        state.burned -= kSecondsPerCan;
        --state.fuel;
        ++cans;
    }
    if (stand.opened >= kOpenSeconds)
    {
        state.open = true;
        stand.running = false;
    }
    else if (state.fuel <= 0)
    {
        state.burned = 0.0f;
        stand.running = false;
    }
    return cans;
}

int wavesDue(Stand& stand)
{
    const int due = static_cast<int>(std::floor(stand.elapsed / kWaveSeconds));
    const int fresh = std::max(0, due - stand.waves);
    stand.waves = std::max(stand.waves, due);
    return fresh;
}

int farthestDistrict(const LayoutData& layout)
{
    const glm::vec2 start(static_cast<float>(layout.playerStart.x), static_cast<float>(layout.playerStart.y));
    int best = 0;
    float bestDistance = -1.0f;
    for (const LayoutData::DistrictDef& d : layout.districts)
    {
        if (d.id == 0) continue;
        // Never the guard hut's own district.
        bool hut = false;
        for (const int bi : d.buildingIndices)
        {
            if (bi >= 0 && bi < static_cast<int>(layout.buildings.size()) &&
                layout.buildings[static_cast<std::size_t>(bi)].kind == static_cast<int>(BuildingKind::GuardHut))
            {
                hut = true;
            }
        }
        if (hut) continue;
        const glm::vec2 c(static_cast<float>((d.minX + d.maxX) / 2), static_cast<float>((d.minY + d.maxY) / 2));
        const glm::vec2 delta = c - start;
        const float distance = delta.x * delta.x + delta.y * delta.y;
        if (distance > bestDistance)
        {
            bestDistance = distance;
            best = d.id;
        }
    }
    return best;
}

bool stamp(LayoutData& layout, int district, std::string* why)
{
    const int w = layout.width;
    const int h = layout.height;
    const auto size = static_cast<std::size_t>(std::max(0, w * h));
    if (w < 24 || h < 24 || layout.ground.size() < size || layout.collision.size() < size)
    {
        if (why != nullptr) *why = "no map";
        return false;
    }
    if (layout.gate.placed) return true;
    if (layout.surface.size() < size) layout.surface.resize(size, 0);
    if (layout.district.size() < size) layout.district.resize(size, 0);

    if (district <= 0) district = farthestDistrict(layout);
    glm::vec2 target(static_cast<float>(w) * 0.5f, static_cast<float>(h) * 0.5f);
    for (const LayoutData::DistrictDef& d : layout.districts)
    {
        if (d.id == district) target = centreOf(d);
    }

    const auto index = [&](const glm::ivec2& t) { return static_cast<std::size_t>(t.y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(t.x); };
    const auto inMap = [&](const glm::ivec2& t) { return t.x >= 0 && t.y >= 0 && t.x < w && t.y < h; };
    const auto asphalt = [&](const glm::ivec2& t) { return inMap(t) && baseGid(layout.ground[index(t)]) == TileGid::kAsphalt; };

    // Road ends: two rows of asphalt meeting the boundary wall.
    struct Candidate
    {
        int side;
        int road; // v of the first road row
        float distance;
    };
    std::vector<Candidate> candidates;
    for (int side = 0; side < 4; ++side)
    {
        const Frame f{side, w, h};
        for (int v = 2; v < f.along() - 3; ++v)
        {
            if (!asphalt(f.world(1, v)) || !asphalt(f.world(1, v + 1)) || asphalt(f.world(1, v - 1))) continue;
            const glm::ivec2 end = f.world(0, v);
            const glm::vec2 d = glm::vec2(end) - target;
            candidates.push_back(Candidate{side, v, std::sqrt(d.x * d.x + d.y * d.y)});
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });

    constexpr int kHutDepth = 6;  // u 2..7
    constexpr int kHutLength = 5; // along the road
    for (const Candidate& c : candidates)
    {
        const Frame f{c.side, w, h};
        for (const int s : {1, -1})
        {
            const int walk = s > 0 ? c.road + 2 : c.road - 1;       // the pavement row
            const int near = s > 0 ? c.road + 3 : c.road - 2;       // the hut wall facing the road
            const int far = near + (kHutLength - 1) * s;           // its far wall
            const int yardEnd = far + s;
            const int vLo = std::min(walk, yardEnd);
            const int vHi = std::max(walk, yardEnd);
            if (vLo < 1 || vHi > f.along() - 2) continue;

            // Room: no road and no building in the yard or one tile round it.
            bool room = true;
            for (int u = 1; u <= kHutDepth + 3 && room; ++u)
            {
                for (int v = vLo - 1; v <= vHi + 1 && room; ++v)
                {
                    const glm::ivec2 t = f.world(u, v);
                    if (!inMap(t)) continue;
                    if (buildingkinds::buildingAt(layout, t) != 0) room = false;
                    const bool yard = u <= kHutDepth + 2 && v >= vLo && v <= vHi;
                    if (yard && asphalt(t)) room = false;
                }
            }
            if (!room) continue;

            // The yard: the pavement row, gravel round the hut.
            for (int u = 1; u <= kHutDepth + 2; ++u)
            {
                for (int v = vLo; v <= vHi; ++v)
                {
                    const std::size_t i = index(f.world(u, v));
                    layout.ground[i] = v == walk ? TileGid::kSidewalk : TileGid::kGravel;
                    layout.surface[i] = v == walk ? 0 : 2;
                    layout.collision[i] = 0;
                    layout.district[i] = 0;
                }
            }
            // The hut: brick walls, a concrete floor, one door to the road.
            const int hutIndex = static_cast<int>(layout.buildings.size());
            const int hutId = hutIndex + 1;
            for (int u = 2; u <= kHutDepth + 1; ++u)
            {
                for (int v = std::min(near, far); v <= std::max(near, far); ++v)
                {
                    const std::size_t i = index(f.world(u, v));
                    const bool wall = u == 2 || u == kHutDepth + 1 || v == near || v == far;
                    layout.ground[i] = wall ? TileGid::kWall : TileGid::kFloor;
                    layout.collision[i] = wall ? 1 : 0;
                    layout.surface[i] = 0;
                    layout.district[i] = hutId;
                }
            }
            const glm::ivec2 door = f.world(kHutDepth, near);
            layout.ground[index(door)] = TileGid::kFloor;
            layout.collision[index(door)] = 0;
            layout.doors.push_back(LayoutData::DoorPos{door.x, door.y});

            // The gate: the two road rows and the pavement, in the wall.
            LayoutData::GateSite site;
            site.placed = true;
            site.side = c.side;
            const int roadFar = s > 0 ? c.road : c.road + 1;
            const int roadNear = s > 0 ? c.road + 1 : c.road;
            for (const int v : {roadFar, roadNear, walk})
            {
                const glm::ivec2 t = f.world(0, v);
                const std::size_t i = index(t);
                layout.ground[i] = v == walk ? TileGid::kSidewalk : TileGid::kAsphalt;
                layout.surface[i] = v == walk ? 0 : 2;
                layout.collision[i] = 0;
                site.gate.push_back(t);
            }
            site.box = f.world(3, walk);
            site.generator = f.world(1, near + 2 * s);
            site.windows = {f.world(4, near), f.world(kHutDepth + 1, near + 2 * s)};
            site.hut = hutId;
            site.district = district;
            const glm::ivec2 a = f.world(0, vLo);
            const glm::ivec2 b = f.world(kHutDepth + 2, vHi);
            site.yardMin = glm::ivec2(std::min(a.x, b.x), std::min(a.y, b.y));
            site.yardMax = glm::ivec2(std::max(a.x, b.x), std::max(a.y, b.y));

            // The building and its district.
            const glm::ivec2 h0 = f.world(2, near);
            const glm::ivec2 h1 = f.world(kHutDepth + 1, far);
            LayoutData::BuildingRect rect;
            rect.x = std::min(h0.x, h1.x);
            rect.y = std::min(h0.y, h1.y);
            rect.w = std::abs(h1.x - h0.x) + 1;
            rect.h = std::abs(h1.y - h0.y) + 1;
            rect.kind = static_cast<int>(BuildingKind::GuardHut);
            layout.buildings.push_back(rect);

            LayoutData::DistrictDef def;
            def.id = hutId;
            def.type = static_cast<int>(DistrictType::Commercial);
            def.minX = rect.x;
            def.minY = rect.y;
            def.maxX = rect.x + rect.w - 1;
            def.maxY = rect.y + rect.h - 1;
            def.buildingIndices.push_back(hutIndex);
            constexpr int kAdjacency = 5;
            for (LayoutData::DistrictDef& other : layout.districts)
            {
                const int hGap = std::max(0, std::max(def.minX - other.maxX, other.minX - def.maxX));
                const int vGap = std::max(0, std::max(def.minY - other.maxY, other.minY - def.maxY));
                if (hGap <= kAdjacency && vGap <= kAdjacency)
                {
                    def.adjacentDistricts.push_back(other.id);
                    other.adjacentDistricts.push_back(def.id);
                }
            }
            layout.districts.push_back(def);
            layout.gate = site;
            return true;
        }
    }
    if (why != nullptr) *why = "no road end with room for the hut";
    return false;
}

bool inYard(const LayoutData::GateSite& site, const glm::ivec2& tile)
{
    return site.placed && tile.x >= site.yardMin.x && tile.x <= site.yardMax.x && tile.y >= site.yardMin.y &&
           tile.y <= site.yardMax.y;
}

glm::ivec2 insideGate(const LayoutData& layout)
{
    const LayoutData::GateSite& site = layout.gate;
    if (!site.placed || site.gate.size() < 2) return glm::ivec2(-1);
    const Frame f{site.side, layout.width, layout.height};
    // One tile in from the road tile nearest the hut.
    const glm::ivec2 road = site.gate[1];
    const glm::ivec2 edge = f.world(0, 0);
    const glm::ivec2 in = f.world(1, 0);
    return road + (in - edge);
}

int sergeantBuilding(const LayoutData& layout, std::uint32_t worldSeed)
{
    const glm::vec2 start(static_cast<float>(layout.playerStart.x), static_cast<float>(layout.playerStart.y));
    const glm::vec2 gateAt = layout.gate.placed && !layout.gate.gate.empty() ? glm::vec2(layout.gate.gate[1]) : start;
    const int startBuilding = buildingkinds::buildingAt(layout, layout.playerStart);
    for (const float keepAway : {30.0f, 18.0f, 0.0f})
    {
        std::vector<int> candidates;
        for (std::size_t i = 0; i < layout.buildings.size(); ++i)
        {
            const LayoutData::BuildingRect& b = layout.buildings[i];
            const int id = static_cast<int>(i) + 1;
            if (b.kind == static_cast<int>(BuildingKind::GuardHut) || id == startBuilding || id == layout.gate.district) continue;
            const glm::vec2 c = centreOf(b);
            const glm::vec2 s = c - start;
            if (s.x * s.x + s.y * s.y < keepAway * keepAway) continue;
            const glm::vec2 g = c - gateAt;
            if (g.x * g.x + g.y * g.y < keepAway * keepAway) continue;
            candidates.push_back(id);
        }
        if (!candidates.empty())
        {
            return candidates[hashMix(worldSeed, 0x5E26u) % candidates.size()];
        }
    }
    return 0;
}

std::vector<glm::ivec2> sergeantLoop(const LayoutData& layout, int building)
{
    std::vector<glm::ivec2> points;
    if (building <= 0 || building > static_cast<int>(layout.buildings.size())) return points;
    const glm::vec2 c = centreOf(layout.buildings[static_cast<std::size_t>(building - 1)]);
    const int w = layout.width;
    const int h = layout.height;
    for (const float radius : {14.0f, 22.0f, 40.0f})
    {
        std::vector<glm::ivec2> roads;
        for (int y = 1; y < h - 1; ++y)
        {
            for (int x = 1; x < w - 1; ++x)
            {
                const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x);
                if (baseGid(layout.ground[i]) != TileGid::kAsphalt || layout.collision[i] != 0) continue;
                const glm::vec2 d = glm::vec2(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f) - c;
                if (d.x * d.x + d.y * d.y <= radius * radius) roads.emplace_back(x, y);
            }
        }
        if (roads.size() < static_cast<std::size_t>(kLoopPoints)) continue;
        const auto angleOf = [&](const glm::ivec2& t)
        {
            const glm::vec2 d = glm::vec2(static_cast<float>(t.x) + 0.5f, static_cast<float>(t.y) + 0.5f) - c;
            float a = std::atan2(d.y, d.x);
            if (a < 0.0f) a += 6.2831853f;
            return a;
        };
        const auto distanceOf = [&](const glm::ivec2& t)
        {
            const glm::vec2 d = glm::vec2(static_cast<float>(t.x) + 0.5f, static_cast<float>(t.y) + 0.5f) - c;
            return std::sqrt(d.x * d.x + d.y * d.y);
        };
        // One point in each fifth of the circle, as near 9 tiles out as the roads allow.
        points.clear();
        for (int k = 0; k < kLoopPoints; ++k)
        {
            const float lo = 6.2831853f * static_cast<float>(k) / static_cast<float>(kLoopPoints);
            const float hi = 6.2831853f * static_cast<float>(k + 1) / static_cast<float>(kLoopPoints);
            const glm::ivec2* best = nullptr;
            float bestScore = 0.0f;
            for (const glm::ivec2& t : roads)
            {
                const float a = angleOf(t);
                if (a < lo || a >= hi) continue;
                const float score = std::abs(distanceOf(t) - 9.0f);
                if (best == nullptr || score < bestScore)
                {
                    best = &t;
                    bestScore = score;
                }
            }
            if (best != nullptr) points.push_back(*best);
        }
        // Too few sectors with a road: the nearest road tiles, spread apart.
        std::stable_sort(roads.begin(), roads.end(),
                         [&](const glm::ivec2& a, const glm::ivec2& b) { return distanceOf(a) < distanceOf(b); });
        for (const glm::ivec2& t : roads)
        {
            if (static_cast<int>(points.size()) >= kLoopPoints) break;
            const bool spaced = std::all_of(points.begin(), points.end(), [&](const glm::ivec2& p)
                                            { return std::abs(p.x - t.x) + std::abs(p.y - t.y) >= 4; });
            if (spaced) points.push_back(t);
        }
        if (static_cast<int>(points.size()) < kLoopPoints) continue;
        std::stable_sort(points.begin(), points.end(),
                         [&](const glm::ivec2& a, const glm::ivec2& b) { return angleOf(a) < angleOf(b); });
        return points;
    }
    return points;
}

int openGateExtra(int deadNear)
{
    return (std::max(0, deadNear) + 1) / 2;
}
} // namespace gate
