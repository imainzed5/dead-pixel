#include "world/VehicleRules.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>

#include "map/TileIds.h"
#include "world/BuildingKinds.h"
#include "world/SkillRules.h"

namespace vehiclerules
{
namespace
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTile = 32.0f;

KindInfo makeSedan()
{
    KindInfo k;
    k.name = "sedan";
    k.topSpeed = 250.0f;
    k.reverseSpeed = 80.0f;
    k.accel = 120.0f;
    k.brake = 320.0f;
    k.drag = 70.0f;
    k.turnRate = 2.4f;
    k.length = 72.0f;
    k.width = 30.0f;
    k.radius = 15.0f;
    k.circleOffset = 22.0f;
    k.trunkSlots = 12;
    k.trunkKg = 60.0f;
    k.maxFuel = 30.0f;
    k.fuelPerGameHour = 6.0f;
    k.seats = 2;
    k.hearing = 1.0f;
    k.engine = true;
    return k;
}

KindInfo makePickup()
{
    KindInfo k;
    k.name = "pickup";
    k.topSpeed = 210.0f;
    k.reverseSpeed = 70.0f;
    k.accel = 95.0f;
    k.brake = 280.0f;
    k.drag = 75.0f;
    k.turnRate = 2.0f;
    k.length = 80.0f;
    k.width = 32.0f;
    k.radius = 16.0f;
    k.circleOffset = 25.0f;
    k.trunkSlots = 20;
    k.trunkKg = 150.0f;
    k.maxFuel = 40.0f;
    k.fuelPerGameHour = 8.0f;
    k.seats = 2;
    k.hearing = 1.2f;
    k.engine = true;
    return k;
}

KindInfo makeBicycle()
{
    KindInfo k;
    k.name = "bicycle";
    k.topSpeed = 170.0f;
    k.reverseSpeed = 30.0f;
    k.accel = 110.0f;
    k.brake = 240.0f;
    k.drag = 40.0f;
    k.turnRate = 3.2f;
    k.length = 30.0f;
    k.width = 10.0f;
    k.radius = 7.0f;
    k.circleOffset = 9.0f;
    k.trunkSlots = 0;
    k.trunkKg = 0.0f;
    k.maxFuel = 0.0f;
    k.fuelPerGameHour = 0.0f;
    k.seats = 1;
    k.hearing = 0.0f;
    k.engine = false;
    return k;
}

const KindInfo kKinds[] = {makeSedan(), makePickup(), makeBicycle()};

float wrapAngle(float a)
{
    while (a > kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

float signedAngle(const glm::vec2& from, const glm::vec2& to)
{
    const float cross = from.x * to.y - from.y * to.x;
    const float dot = from.x * to.x + from.y * to.y;
    return std::atan2(cross, dot);
}

float approach(float value, float target, float step)
{
    if (value < target) return std::min(target, value + step);
    return std::max(target, value - step);
}

int baseGid(int gid)
{
    return gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
}
} // namespace

const KindInfo& info(VehicleKind kind)
{
    const int i = std::clamp(static_cast<int>(kind), 0, static_cast<int>(VehicleKind::Count) - 1);
    return kKinds[i];
}

const char* kindName(VehicleKind kind) { return info(kind).name; }

bool parseKind(const char* text, VehicleKind& out)
{
    if (text == nullptr) return false;
    std::string lowered;
    for (const char* c = text; *c != '\0'; ++c) lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*c))));
    for (int i = 0; i < static_cast<int>(VehicleKind::Count); ++i)
    {
        if (lowered == kKinds[i].name)
        {
            out = static_cast<VehicleKind>(i);
            return true;
        }
    }
    if (lowered == "car")
    {
        out = VehicleKind::Sedan;
        return true;
    }
    if (lowered == "bike")
    {
        out = VehicleKind::Bicycle;
        return true;
    }
    return false;
}

int colorCount(VehicleKind kind)
{
    switch (kind)
    {
    case VehicleKind::Sedan: return kSedanColors;
    case VehicleKind::Pickup: return kPickupColors;
    default: return 1;
    }
}

const char* colorName(VehicleKind kind, int color)
{
    static const char* const kSedan[kSedanColors] = {"red", "blue", "grey", "white"};
    static const char* const kPickup[kPickupColors] = {"green", "tan"};
    switch (kind)
    {
    case VehicleKind::Sedan: return kSedan[std::clamp(color, 0, kSedanColors - 1)];
    case VehicleKind::Pickup: return kPickup[std::clamp(color, 0, kPickupColors - 1)];
    default: return "black";
    }
}

const char* describe(VehicleKind kind, int color, char* buffer, std::size_t size)
{
    if (buffer == nullptr || size == 0) return "";
    if (kind == VehicleKind::Bicycle)
    {
        std::snprintf(buffer, size, "the bicycle");
    }
    else
    {
        std::snprintf(buffer, size, "the %s %s", colorName(kind, color), kindName(kind));
    }
    return buffer;
}

// --- Starting it ------------------------------------------------------------------

float hotwireSeconds(int mechanics) { return kHotwireSeconds * skillrules::hotwireTimeScale(mechanics); }

float hotwireFailChance(int mechanics)
{
    return std::max(0.0f, kHotwireFailChance - kHotwireFailPerLevel * static_cast<float>(std::max(0, mechanics)));
}

bool hotwireFails(float roll, int mechanics) { return roll < hotwireFailChance(mechanics); }

float startFailChance(float condition)
{
    if (condition >= 50.0f) return 0.0f;
    return std::clamp((50.0f - condition) * 0.02f, 0.0f, 1.0f);
}

bool startFails(float roll, float condition) { return roll < startFailChance(condition); }

// --- Condition ---------------------------------------------------------------------

Band band(float condition)
{
    if (condition >= 70.0f) return Band::Good;
    if (condition >= 40.0f) return Band::Worn;
    if (condition >= 10.0f) return Band::Damaged;
    return Band::Broken;
}

const char* bandName(Band b)
{
    switch (b)
    {
    case Band::Good: return "Good";
    case Band::Worn: return "Worn";
    case Band::Damaged: return "Damaged";
    case Band::Broken: return "Broken";
    }
    return "Good";
}

float bandSpeedScale(Band b)
{
    switch (b)
    {
    case Band::Good: return 1.0f;
    case Band::Worn: return 0.86f;
    case Band::Damaged: return 0.72f;
    case Band::Broken: return 0.0f;
    }
    return 1.0f;
}

bool runs(float condition) { return band(condition) != Band::Broken; }

float wearPerGameHour(bool wetOrSnow, bool offRoad)
{
    float wear = kWearPerGameHour;
    if (wetOrSnow) wear *= 1.5f;
    if (offRoad) wear *= 2.0f;
    return wear;
}

float topSpeed(VehicleKind kind, const SpeedInput& in)
{
    const KindInfo& k = info(kind);
    float top = k.topSpeed;
    // A bicycle has no engine to wear: its condition only stops it when broken.
    if (kind != VehicleKind::Bicycle) top *= bandSpeedScale(band(in.condition));
    else if (!runs(in.condition)) top = 0.0f;
    if (in.offRoad) top *= kOffRoadSpeedScale;
    const int flats = std::clamp(4 - in.tyres, 0, 4);
    for (int i = 0; i < flats; ++i) top *= kFlatTyreSpeedScale;
    return top;
}

float turnRate(VehicleKind kind, float speed, bool snow)
{
    const KindInfo& k = info(kind);
    const float s = std::fabs(speed);
    const float rolling = kind == VehicleKind::Bicycle ? std::min(1.0f, 0.5f + s / 40.0f) : std::min(1.0f, s / 40.0f);
    const float fast = 1.0f - 0.5f * std::clamp(s / std::max(1.0f, k.topSpeed), 0.0f, 1.0f);
    float rate = k.turnRate * rolling * fast;
    if (snow) rate *= 0.5f;
    return rate;
}

const char* speedWord(float speed)
{
    const float s = std::fabs(speed);
    if (s < 8.0f) return "Idle";
    if (s < 130.0f) return "Slow";
    return "Fast";
}

float fuelUsed(VehicleKind kind, float gameHours)
{
    return info(kind).fuelPerGameHour * std::max(0.0f, gameHours);
}

float repairAmount(int mechanics, bool lift)
{
    const float amount = kRepairAmount * skillrules::repairScale(mechanics);
    return lift ? amount * 2.0f : amount;
}

// --- Hits ---------------------------------------------------------------------------

WallHit wallHit(float speed)
{
    WallHit hit;
    const float s = std::fabs(speed);
    if (s <= kWallDamageSpeed) return hit;
    hit.hurts = true;
    hit.vehicleDamage = s / 20.0f;
    hit.driverDamage = s / 40.0f;
    return hit;
}

BodyHit bodyHit(float speed, bool brute)
{
    BodyHit hit;
    const float s = std::fabs(speed);
    if (s < kRunDownBodyAbove)
    {
        hit.blocked = true;
        hit.speedKeep = 0.0f;
        return hit;
    }
    hit.damage = kBodyDamage;
    if (brute)
    {
        // It takes the blow standing, and the car stops dead against it.
        hit.thrown = false;
        hit.conditionLoss = kBruteConditionLoss;
        hit.speedKeep = 0.0f;
        return hit;
    }
    hit.thrown = true;
    hit.conditionLoss = kBodyConditionLoss;
    hit.speedKeep = 1.0f - kBodySpeedLoss;
    return hit;
}

// --- The drive ----------------------------------------------------------------------

glm::vec2 forward(float heading) { return glm::vec2(std::cos(heading), std::sin(heading)); }

void drive(VehicleKind kind, DriveState& state, const DriveInput& in, float dt)
{
    const KindInfo& k = info(kind);
    const float brake = k.brake * ((kind == VehicleKind::Bicycle && in.rain) ? kRainBrakeScale : 1.0f);
    const float wantLength = std::sqrt(in.want.x * in.want.x + in.want.y * in.want.y);

    if (wantLength < 0.01f)
    {
        // Coasting: it rolls on and slows.
        state.speed = approach(state.speed, 0.0f, k.drag * dt);
    }
    else
    {
        const glm::vec2 want = in.want / wantLength;
        const glm::vec2 fwd = forward(state.heading);
        const float angle = signedAngle(fwd, want);
        const float rate = turnRate(kind, state.speed, in.snow);
        if (kind == VehicleKind::Bicycle && std::fabs(angle) > kReverseAngle)
        {
            // A bicycle is not backed up: slowed, it is wheeled round.
            state.speed = approach(state.speed, 0.0f, brake * dt);
            if (std::fabs(state.speed) < 20.0f) state.heading += std::clamp(angle, -rate * dt, rate * dt);
        }
        else if (std::fabs(angle) <= kReverseAngle)
        {
            if (state.speed < -1.0f)
            {
                state.speed = approach(state.speed, 0.0f, brake * dt);
            }
            else if (in.canThrottle)
            {
                state.speed = std::min(in.top, state.speed + k.accel * dt);
            }
            else
            {
                state.speed = approach(state.speed, 0.0f, k.drag * dt);
            }
            state.heading += std::clamp(angle, -rate * dt, rate * dt);
        }
        else
        {
            if (state.speed > 1.0f)
            {
                state.speed = approach(state.speed, 0.0f, brake * dt);
            }
            else if (in.canThrottle)
            {
                state.speed = std::max(-std::min(k.reverseSpeed, in.top), state.speed - k.accel * 0.6f * dt);
                // Backing up: the rear swings toward where the keys point.
                const float rearAngle = signedAngle(-fwd, want);
                state.heading += std::clamp(rearAngle, -rate * dt, rate * dt);
            }
            else
            {
                state.speed = approach(state.speed, 0.0f, k.drag * dt);
            }
        }
    }

    // Over the cap (off the road, a flat): it slows to it.
    if (state.speed > in.top) state.speed = std::max(in.top, state.speed - brake * dt);
    const float reverseCap = std::min(k.reverseSpeed, in.top);
    if (state.speed < -reverseCap) state.speed = std::min(-reverseCap, state.speed + brake * dt);
    state.heading = wrapAngle(state.heading);
}

std::array<glm::vec2, 3> circles(VehicleKind kind, const glm::vec2& centre, float heading)
{
    const KindInfo& k = info(kind);
    const glm::vec2 f = forward(heading);
    return {centre - f * k.circleOffset, centre, centre + f * k.circleOffset};
}

glm::vec2 rear(VehicleKind kind, const glm::vec2& centre, float heading)
{
    const KindInfo& k = info(kind);
    return centre - forward(heading) * (k.length * 0.5f + 10.0f);
}

glm::vec2 side(VehicleKind kind, const glm::vec2& centre, float heading, bool driver)
{
    const KindInfo& k = info(kind);
    const glm::vec2 f = forward(heading);
    // Left of the heading in world space (y down): rotate by -90 degrees.
    const glm::vec2 left(f.y, -f.x);
    const float out = k.width * 0.5f + 18.0f;
    return centre + (driver ? left : -left) * out;
}

Facing8 facing8(float heading)
{
    Facing8 f;
    const float step = kPi * 0.25f;
    int d = static_cast<int>(std::lround(heading / step)) % 8;
    if (d < 0) d += 8;
    f.dir = d;
    switch (d)
    {
    case 2: f.drawn = 0; f.mirrored = true; break;
    case 3: f.drawn = 7; f.mirrored = true; break;
    case 4: f.drawn = 6; f.mirrored = true; break;
    default: f.drawn = d; f.mirrored = false; break;
    }
    return f;
}

// --- Where the town parks them ---------------------------------------------------

namespace
{
struct Town
{
    const LayoutData& layout;
    std::vector<std::uint8_t> blocked; // 1: no vehicle on this tile

    explicit Town(const LayoutData& l) : layout(l)
    {
        const int w = l.width;
        const int h = l.height;
        blocked.assign(static_cast<std::size_t>(std::max(0, w * h)), 0);
        auto mark = [&](int x, int y)
        {
            if (x >= 0 && y >= 0 && x < w && y < h) blocked[static_cast<std::size_t>(y * w + x)] = 1;
        };
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const std::size_t i = static_cast<std::size_t>(y * w + x);
                const bool solid = i < l.collision.size() && l.collision[i] != 0;
                const int ground = i < l.ground.size() ? baseGid(l.ground[i]) : 0;
                if (solid || ground == TileGid::kWater || ground == 0) blocked[i] = 1;
                // The map's edge.
                if (x == 0 || y == 0 || x == w - 1 || y == h - 1) blocked[i] = 1;
            }
        }
        for (const LayoutData::BuildingRect& b : l.buildings)
        {
            for (int y = b.y - 1; y <= b.y + b.h; ++y)
                for (int x = b.x - 1; x <= b.x + b.w; ++x) mark(x, y);
        }
        for (const LayoutData::DoorPos& d : l.doors)
        {
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) mark(d.x + dx, d.y + dy);
        }
        if (l.gate.placed)
        {
            for (int y = l.gate.yardMin.y - 1; y <= l.gate.yardMax.y + 1; ++y)
                for (int x = l.gate.yardMin.x - 1; x <= l.gate.yardMax.x + 1; ++x) mark(x, y);
            for (const glm::ivec2& t : l.gate.gate)
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx) mark(t.x + dx, t.y + dy);
        }
        if (l.cemetery.placed)
        {
            for (int y = l.cemetery.fenceMin.y - 1; y <= l.cemetery.fenceMax.y + 1; ++y)
                for (int x = l.cemetery.fenceMin.x - 1; x <= l.cemetery.fenceMax.x + 1; ++x) mark(x, y);
        }
        // Not on the spot a life starts on.
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx) mark(l.playerStart.x + dx, l.playerStart.y + dy);
    }

    [[nodiscard]] bool open(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= layout.width || y >= layout.height) return false;
        return blocked[static_cast<std::size_t>(y * layout.width + x)] == 0;
    }
};

bool bodyFits(const Town& town, VehicleKind kind, const glm::vec2& centre, float heading)
{
    const KindInfo& k = info(kind);
    for (const glm::vec2& c : circles(kind, centre, heading))
    {
        const int x0 = static_cast<int>(std::floor((c.x - k.radius) / kTile));
        const int x1 = static_cast<int>(std::floor((c.x + k.radius) / kTile));
        const int y0 = static_cast<int>(std::floor((c.y - k.radius) / kTile));
        const int y1 = static_cast<int>(std::floor((c.y + k.radius) / kTile));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                if (!town.open(x, y)) return false;
    }
    return true;
}

bool clearOf(const std::vector<Spot>& spots, VehicleKind kind, const glm::vec2& centre, float heading)
{
    const KindInfo& k = info(kind);
    const auto mine = circles(kind, centre, heading);
    for (const Spot& other : spots)
    {
        const KindInfo& o = info(other.kind);
        const auto theirs = circles(other.kind, other.centre, other.heading);
        for (const glm::vec2& a : mine)
        {
            for (const glm::vec2& b : theirs)
            {
                const glm::vec2 d = a - b;
                const float gap = k.radius + o.radius + 10.0f;
                if (d.x * d.x + d.y * d.y < gap * gap) return false;
            }
        }
    }
    return true;
}

// The tile just outside each of a building's doors, or its middle when it has none.
std::vector<glm::ivec2> frontsOf(const LayoutData& layout, const LayoutData::BuildingRect& b)
{
    std::vector<glm::ivec2> fronts;
    for (const LayoutData::DoorPos& d : layout.doors)
    {
        if (d.x < b.x || d.y < b.y || d.x >= b.x + b.w || d.y >= b.y + b.h) continue;
        glm::ivec2 out(d.x, d.y);
        if (d.x == b.x) out.x -= 2;
        else if (d.x == b.x + b.w - 1) out.x += 2;
        else if (d.y == b.y) out.y -= 2;
        else if (d.y == b.y + b.h - 1) out.y += 2;
        else continue; // an inner door
        fronts.push_back(out);
    }
    if (fronts.empty()) fronts.emplace_back(b.x + b.w / 2, b.y + b.h + 1);
    return fronts;
}

bool parkNear(const Town& town, std::vector<Spot>& spots, Spot spot, const std::vector<glm::ivec2>& fronts, int reach,
              std::mt19937& rng)
{
    const float axes[4] = {0.0f, kPi * 0.5f, kPi, -kPi * 0.5f};
    const int firstAxis = static_cast<int>(rng() % 4u);
    for (int r = 0; r <= reach; ++r)
    {
        for (const glm::ivec2& front : fronts)
        {
            std::vector<glm::ivec2> ring;
            for (int dy = -r; dy <= r; ++dy)
            {
                for (int dx = -r; dx <= r; ++dx)
                {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    ring.emplace_back(front.x + dx, front.y + dy);
                }
            }
            std::shuffle(ring.begin(), ring.end(), rng);
            for (const glm::ivec2& tile : ring)
            {
                const glm::vec2 centre((static_cast<float>(tile.x) + 0.5f) * kTile, (static_cast<float>(tile.y) + 0.5f) * kTile);
                for (int a = 0; a < 4; ++a)
                {
                    const float heading = axes[(firstAxis + a) % 4];
                    if (!bodyFits(town, spot.kind, centre, heading)) continue;
                    if (!clearOf(spots, spot.kind, centre, heading)) continue;
                    spot.centre = centre;
                    spot.heading = heading;
                    spots.push_back(spot);
                    return true;
                }
            }
        }
    }
    return false;
}
} // namespace

bool fits(const LayoutData& layout, VehicleKind kind, const glm::vec2& centre, float heading)
{
    const Town town(layout);
    return bodyFits(town, kind, centre, heading);
}

std::vector<Spot> place(const LayoutData& layout, std::uint32_t worldSeed)
{
    std::vector<Spot> spots;
    if (layout.width <= 0 || layout.height <= 0) return spots;
    const Town town(layout);
    std::mt19937 rng(worldSeed ^ 0xCA125EEDu);
    const auto unit = [&]() { return static_cast<float>(rng() % 100000u) / 100000.0f; };
    const auto between = [&](float lo, float hi) { return lo + (hi - lo) * unit(); };
    const auto count = [&](int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)); };

    std::vector<int> houses;
    std::vector<int> police;
    std::vector<int> garages;
    std::vector<int> yards; // warehouse, workshop, hardware
    int biggest = -1;
    int biggestArea = 0;
    for (int i = 0; i < static_cast<int>(layout.buildings.size()); ++i)
    {
        const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(i)];
        const BuildingKind kind = buildingkinds::kindOf(b);
        switch (kind)
        {
        case BuildingKind::House: houses.push_back(i); break;
        case BuildingKind::Police: police.push_back(i); break;
        case BuildingKind::Garage: garages.push_back(i); break;
        case BuildingKind::Warehouse:
        case BuildingKind::Workshop:
        case BuildingKind::Hardware: yards.push_back(i); break;
        default: break;
        }
        if (kind != BuildingKind::GuardHut && b.w * b.h > biggestArea)
        {
            biggestArea = b.w * b.h;
            biggest = i;
        }
    }
    std::shuffle(houses.begin(), houses.end(), rng);
    std::shuffle(yards.begin(), yards.end(), rng);

    const auto parkFor = [&](VehicleKind kind, int building, int reach) -> bool
    {
        Spot spot;
        spot.kind = kind;
        spot.building = building + 1;
        spot.color = static_cast<int>(rng() % static_cast<unsigned>(colorCount(kind)));
        switch (kind)
        {
        case VehicleKind::Sedan:
            spot.condition = between(35.0f, 90.0f);
            spot.fuel = between(4.0f, 18.0f);
            break;
        case VehicleKind::Pickup:
            spot.condition = between(30.0f, 85.0f);
            spot.fuel = between(6.0f, 24.0f);
            break;
        default:
            spot.condition = between(55.0f, 95.0f);
            spot.fuel = 0.0f;
            break;
        }
        const bool engine = info(kind).engine;
        spot.keyInside = engine && unit() < kKeyInsideChance;
        spot.alarm = engine && unit() < kAlarmChance;
        const LayoutData::BuildingRect& b = layout.buildings[static_cast<std::size_t>(building)];
        return parkNear(town, spots, spot, frontsOf(layout, b), reach, rng);
    };

    // Sedans: the police lot, then driveways.
    {
        const int want = count(kSedansMin, kSedansMax);
        int placed = 0;
        if (!police.empty() && parkFor(VehicleKind::Sedan, police.front(), 6)) ++placed;
        for (int house : houses)
        {
            if (placed >= want) break;
            if (parkFor(VehicleKind::Sedan, house, 4)) ++placed;
        }
    }
    // Pickups: the garage, then a yard.
    {
        const int want = count(kPickupsMin, kPickupsMax);
        int placed = 0;
        std::vector<int> order = garages;
        order.insert(order.end(), yards.begin(), yards.end());
        for (int building : order)
        {
            if (placed >= want) break;
            if (parkFor(VehicleKind::Pickup, building, 6)) ++placed;
        }
        // A town with no yards: a house keeps one.
        for (auto it = houses.rbegin(); it != houses.rend() && placed < want; ++it)
        {
            if (parkFor(VehicleKind::Pickup, *it, 4)) ++placed;
        }
    }
    // Bicycles: two by the biggest building in town, the rest by houses.
    {
        const int want = count(kBicyclesMin, kBicyclesMax);
        int placed = 0;
        if (biggest >= 0)
        {
            for (int i = 0; i < 2 && placed < want; ++i)
            {
                if (parkFor(VehicleKind::Bicycle, biggest, 6)) ++placed;
            }
        }
        for (auto it = houses.rbegin(); it != houses.rend() && placed < want; ++it)
        {
            if (parkFor(VehicleKind::Bicycle, *it, 3)) ++placed;
        }
    }
    return spots;
}
} // namespace vehiclerules
