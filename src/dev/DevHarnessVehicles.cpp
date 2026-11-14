// Harness commands for vehicles (overhaul phase 11): goto vehicle, vehicles,
// set fuel|condition|tyres|key|alarm, drive, vehicle mark|park|fetch, and
// the expect vehicle lines. See DevHarness.h for the list.
#include "dev/DevHarness.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>

#include "core/Game.h"
#include "core/GameShared.h"

namespace
{
constexpr double kTicks = 60.0;

std::string lowered(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + 16.0f, t.y + 16.0f);
}

// The keys whose walk direction (InputSystem: W up the screen) is nearest a
// world direction: one key for a diagonal, two for a tile axis.
void pressToward(Input& input, const glm::vec2& want)
{
    struct Combo
    {
        glm::vec2 dir;
        bool w, a, s, d;
    };
    static const Combo kCombos[] = {
        {{-1.0f, -1.0f}, true, false, false, false}, {{1.0f, 1.0f}, false, false, true, false},
        {{-1.0f, 1.0f}, false, true, false, false},  {{1.0f, -1.0f}, false, false, false, true},
        {{-1.0f, 0.0f}, true, true, false, false},   {{0.0f, -1.0f}, true, false, false, true},
        {{0.0f, 1.0f}, false, true, true, false},    {{1.0f, 0.0f}, false, false, true, true},
    };
    bool w = false, a = false, s = false, d = false;
    if (glm::length(want) > 0.01f)
    {
        const glm::vec2 n = glm::normalize(want);
        float best = -2.0f;
        for (const Combo& c : kCombos)
        {
            const float dot = glm::dot(glm::normalize(c.dir), n);
            if (dot > best)
            {
                best = dot;
                w = c.w;
                a = c.a;
                s = c.s;
                d = c.d;
            }
        }
    }
    input.setKeyState(SDL_SCANCODE_W, w);
    input.setKeyState(SDL_SCANCODE_A, a);
    input.setKeyState(SDL_SCANCODE_S, s);
    input.setKeyState(SDL_SCANCODE_D, d);
}

bool parseWorldDir(const std::string& word, glm::ivec2& dir)
{
    if (word == "n") dir = {0, -1};
    else if (word == "s") dir = {0, 1};
    else if (word == "e") dir = {1, 0};
    else if (word == "w") dir = {-1, 0};
    else if (word == "ne") dir = {1, -1};
    else if (word == "nw") dir = {-1, -1};
    else if (word == "se") dir = {1, 1};
    else if (word == "sw") dir = {-1, 1};
    else return false;
    return true;
}

} // namespace

void DevHarness::steerVehicle(Game& game)
{
    // Road: asphalt a vehicle can drive on.
    const auto roadTile = [&game](const Game&, const glm::ivec2& t)
    {
        if (!game.mTileMap.inBounds(t.x, t.y)) return false;
        const int gid = game.mTileMap.groundGid(t.x, t.y);
        const int base = gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
        return base == TileGid::kAsphalt &&
               game.vehicleTileKind(t.x, t.y) == static_cast<int>(VehicleSystem::TileKind::Open);
    };
    const auto roadRun = [&](const Game&, const glm::ivec2& from, const glm::ivec2& axis, int most)
    {
        int run = 0;
        for (int k = 1; k <= most && roadTile(game, from + axis * k); ++k) ++run;
        return run;
    };
    DriveOrder& order = mDrive;
    const Entity vehicle = game.mActiveVehicleEntity;
    const auto stop = [&]()
    {
        order = DriveOrder{};
        pressToward(game.mInput, glm::vec2(0.0f));
    };
    if (vehicle == kInvalidEntity || !game.mWorld.hasComponent<Vehicle>(vehicle))
    {
        std::cout << "[harness] tick " << mTick << ": drive ends: not in a vehicle\n";
        stop();
        mWaitRemaining = 0; // the script goes on at once
        return;
    }
    --order.ticksLeft;
    const Vehicle& v = game.mWorld.getComponent<Vehicle>(vehicle);
    const glm::vec2 centre = game.vehicleCentre(vehicle);
    order.travelled += glm::length(centre - order.last);
    order.last = centre;
    if (order.ticksLeft == 0)
    {
        std::cout << "[harness] tick " << mTick << ": drive done, " << static_cast<int>(order.travelled / 32.0f)
                  << " tiles, speed " << static_cast<int>(v.speed) << '\n';
        stop();
        return;
    }
    glm::vec2 want(0.0f);
    float cap = order.cap;
    if (order.mode == "brake")
    {
        if (std::fabs(v.speed) < 4.0f)
        {
            pressToward(game.mInput, glm::vec2(0.0f));
            return;
        }
        want = -vehiclerules::forward(v.heading) * (v.speed >= 0.0f ? 1.0f : -1.0f);
    }
    else if (order.mode == "dir")
    {
        want = glm::vec2(order.axis);
    }
    else
    {
        // Follow the road: straight on while it runs, the cross road where it
        // ends, keeping to the middle of the asphalt.
        const glm::ivec2 here = gameshared::worldToTile(game.mTileMap, centre);
        if (order.axis == glm::ivec2(0) && !roadTile(game, here))
        {
            // Off the road (a driveway, a yard): the shortest way over open
            // ground to the nearest asphalt, aiming a few tiles along it.
            constexpr int kReach = 30;
            constexpr int kSide = kReach * 2 + 1;
            const auto open = [&](const glm::ivec2& t)
            {
                return game.vehicleTileKind(t.x, t.y) == static_cast<int>(VehicleSystem::TileKind::Open);
            };
            std::vector<int> from(static_cast<std::size_t>(kSide * kSide), -1);
            const auto index = [&](const glm::ivec2& t) { return (t.y - here.y + kReach) * kSide + (t.x - here.x + kReach); };
            std::vector<glm::ivec2> queue{here};
            from[static_cast<std::size_t>(index(here))] = index(here);
            glm::ivec2 nearest(-1);
            for (std::size_t head = 0; head < queue.size() && nearest.x < 0; ++head)
            {
                const glm::ivec2 t = queue[head];
                if (roadTile(game, t))
                {
                    nearest = t;
                    break;
                }
                for (const glm::ivec2 step : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
                {
                    const glm::ivec2 n = t + step;
                    if (std::abs(n.x - here.x) > kReach || std::abs(n.y - here.y) > kReach || !open(n)) continue;
                    int& seen = from[static_cast<std::size_t>(index(n))];
                    if (seen >= 0) continue;
                    seen = index(t);
                    queue.push_back(n);
                }
            }
            if (nearest.x >= 0)
            {
                std::vector<glm::ivec2> path{nearest};
                while (path.back() != here)
                {
                    const int i = from[static_cast<std::size_t>(index(path.back()))];
                    path.emplace_back(here.x - kReach + i % kSide, here.y - kReach + i / kSide);
                }
                const glm::ivec2 aim = path[path.size() > 3 ? path.size() - 4 : 0];
                want = (glm::vec2(aim) + 0.5f) * 32.0f - centre;
            }
            if (std::fabs(v.speed) > 70.0f && order.mode != "brake") want = glm::vec2(0.0f);
            if (order.ticksLeft % 30 == 0)
            {
                std::cout << "[harness] tick " << mTick << ": driving at " << here.x << "," << here.y << " to the road at "
                          << nearest.x << "," << nearest.y << " speed " << static_cast<int>(v.speed) << '\n';
            }
            pressToward(game.mInput, want);
            return;
        }
        if (order.axis == glm::ivec2(0))
        {
            int best = -1;
            const glm::vec2 f = vehiclerules::forward(v.heading);
            for (const glm::ivec2 axis : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
            {
                const int run = roadRun(game, here, axis, 24) * 4 + static_cast<int>(glm::dot(f, glm::vec2(axis)) * 3.0f);
                if (run > best)
                {
                    best = run;
                    order.axis = axis;
                }
            }
        }
        if (order.turnAt.x >= 0)
        {
            const glm::vec2 turnCentre = (glm::vec2(order.turnAt) + 0.5f) * 32.0f;
            const float along = glm::dot(turnCentre - centre, glm::vec2(order.axis));
            if (along < 20.0f)
            {
                order.axis = order.turnTo;
                order.turnAt = glm::ivec2(-1);
            }
            else if (along < 5.0f * 32.0f)
            {
                cap = std::min(cap, 80.0f);
            }
        }
        else if (roadRun(game, here, order.axis, 6) < 6)
        {
            // The road ends ahead: its last tile, and the longest way on from there.
            const int run = roadRun(game, here, order.axis, 6);
            const glm::ivec2 last = here + order.axis * run;
            const glm::ivec2 left(-order.axis.y, order.axis.x);
            const int runLeft = roadRun(game, last, left, 8);
            const int runRight = roadRun(game, last, -left, 8);
            if (std::max(runLeft, runRight) >= 3)
            {
                // Take the far side of a wide road: aim for its middle.
                glm::ivec2 corner = last;
                while (roadTile(game, corner + order.axis)) corner += order.axis;
                const glm::ivec2 mid = last + order.axis * ((glm::dot(glm::vec2(corner - last), glm::vec2(order.axis)) > 0.0f)
                                                                ? static_cast<int>(glm::length(glm::vec2(corner - last)) * 0.5f)
                                                                : 0);
                order.turnAt = mid;
                order.turnTo = runLeft >= runRight ? left : -left;
                cap = std::min(cap, 80.0f);
            }
            else
            {
                // A dead end: slow, then back the way it came.
                cap = std::min(cap, 50.0f);
                if (run <= 2) order.axis = -order.axis;
            }
        }
        want = glm::vec2(order.axis);
        // Keep to the middle of the road across the axis.
        const glm::ivec2 across(-order.axis.y, order.axis.x);
        int lo = 0;
        int hi = 0;
        while (lo < 6 && roadTile(game, here - across * (lo + 1))) ++lo;
        while (hi < 6 && roadTile(game, here + across * (hi + 1))) ++hi;
        const glm::vec2 middle = (glm::vec2(here) + 0.5f + glm::vec2(across) * (static_cast<float>(hi - lo) * 0.5f)) * 32.0f;
        const float off = glm::dot(centre - middle, glm::vec2(across));
        if (std::fabs(off) > 10.0f) want += glm::vec2(across) * (off > 0.0f ? -1.0f : 1.0f);
    }
    // Over the cap: the keys let go (it coasts; turning resumes under it).
    // Well over it: the brake.
    if (std::fabs(v.speed) > cap && order.mode != "brake") want = glm::vec2(0.0f);
    if (std::fabs(v.speed) > cap + 30.0f && order.mode != "brake" && v.speed > 0.0f) want = -vehiclerules::forward(v.heading);
    if (order.ticksLeft % 30 == 0)
    {
        std::cout << "[harness] tick " << mTick << ": driving at " << static_cast<int>(centre.x / 32.0f) << ","
                  << static_cast<int>(centre.y / 32.0f) << " heading " << static_cast<int>(std::lround(v.heading * 57.29578f))
                  << " speed " << static_cast<int>(v.speed) << " axis " << order.axis.x << "," << order.axis.y << " turn at "
                  << order.turnAt.x << "," << order.turnAt.y << '\n';
    }
    pressToward(game.mInput, want);
}

bool DevHarness::vehicleCommand(Game& game, const Command& command)
{
    const auto& a = command.args;
    const std::string op = lowered(a[0]);
    const auto need = [&](std::size_t count)
    {
        if (a.size() < count + 1)
        {
            fail(command, "'" + op + "' needs " + std::to_string(count) + " argument(s)");
            return false;
        }
        return true;
    };
    // The vehicle in hand: the one the player sits in, else the nearest.
    const auto focus = [&]() -> Entity
    {
        if (game.mActiveVehicleEntity != kInvalidEntity) return game.mActiveVehicleEntity;
        Entity best = kInvalidEntity;
        float bestDistance = 1.0e9f;
        const glm::vec2 here = game.playerWorldCenter();
        game.mWorld.forEach<Vehicle, Transform>(
            [&](Entity e, Vehicle&, Transform& t)
            {
                const float d = glm::length(centreOf(t) - here);
                if (d < bestDistance)
                {
                    bestDistance = d;
                    best = e;
                }
            });
        return best;
    };
    const auto describe = [&](Entity e)
    {
        const Vehicle& v = game.mWorld.getComponent<Vehicle>(e);
        const glm::vec2 c = game.vehicleCentre(e);
        int items = 0;
        if (v.trunk != kInvalidEntity && game.mWorld.hasComponent<Inventory>(v.trunk))
        {
            for (const InventorySlot& slot : game.mWorld.getComponent<Inventory>(v.trunk).slots) items += slot.count;
        }
        std::cout << "[harness] tick " << mTick << ": " << game.vehicleName(v) << " at " << static_cast<int>(c.x / 32.0f) << ","
                  << static_cast<int>(c.y / 32.0f) << " heading " << static_cast<int>(std::lround(v.heading * 57.29578f))
                  << " speed " << static_cast<int>(v.speed) << " condition " << static_cast<int>(v.condition) << " ("
                  << vehiclerules::bandName(vehiclerules::band(v.condition)) << ") fuel " << v.fuel << " tyres " << v.tyres
                  << " trunk " << items << (v.keyInside ? " key-in" : v.keyKnown ? " key-known" : " no-key")
                  << (v.started ? " started" : "") << (v.engineOn ? " engine" : "") << (v.alarm ? " alarm" : "")
                  << (v.alarmSeconds > 0.0f ? " ringing" : "") << (v.windowBroken ? " window-out" : "")
                  << (v.hasCargo ? " cargo" : "") << (v.ownerBuilding > 0 ? " owned" : "") << '\n';
    };
    const auto teleport = [&](const glm::vec2& at)
    {
        const glm::ivec2 tile = gameshared::worldToTile(game.mTileMap, at);
        execute(game, Command{command.line, {"teleport", std::to_string(tile.x), std::to_string(tile.y)}});
        Transform& t = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
        t.x = t.prevX = at.x - 16.0f;
        t.y = t.prevY = at.y - 16.0f;
    };

    if (op == "goto" && a.size() > 1 && lowered(a[1]) == "vehicle")
    {
        // goto vehicle [KIND] [tailgate]: beside the nearest one (of KIND), at its driver's door
        // (tailgate: just behind it).
        std::optional<VehicleKind> kind;
        bool tailgate = false;
        for (std::size_t i = 2; i < a.size(); ++i)
        {
            const std::string word = lowered(a[i]);
            VehicleKind parsed = VehicleKind::Sedan;
            if (word == "tailgate") tailgate = true;
            else if (vehiclerules::parseKind(word.c_str(), parsed)) kind = parsed;
            else
            {
                fail(command, "unknown vehicle kind '" + a[i] + "'");
                return true;
            }
        }
        if (game.mActiveVehicleEntity != kInvalidEntity) (void)game.exitVehicle(true);
        Entity best = kInvalidEntity;
        float bestDistance = 1.0e9f;
        const glm::vec2 here = game.playerWorldCenter();
        game.mWorld.forEach<Vehicle, Transform>(
            [&](Entity e, Vehicle& v, Transform& t)
            {
                if (kind && v.kind != *kind) return;
                const float d = glm::length(centreOf(t) - here);
                if (d < bestDistance)
                {
                    bestDistance = d;
                    best = e;
                }
            });
        if (best == kInvalidEntity)
        {
            fail(command, "no vehicle of that kind");
            return true;
        }
        const Vehicle& v = game.mWorld.getComponent<Vehicle>(best);
        const glm::vec2 c = game.vehicleCentre(best);
        glm::vec2 spot = vehiclerules::rear(v.kind, c, v.heading);
        if (!tailgate)
        {
            const std::array<glm::vec2, 3> spots = {vehiclerules::side(v.kind, c, v.heading, true),
                                                    vehiclerules::side(v.kind, c, v.heading, false), spot};
            for (const glm::vec2& s : spots)
            {
                const glm::ivec2 t = gameshared::worldToTile(game.mTileMap, s);
                if (!game.mTileMap.isSolid(t.x, t.y))
                {
                    spot = s;
                    break;
                }
            }
        }
        teleport(spot);
        std::cout << "[harness] tick " << mTick << ": goto vehicle" << (tailgate ? " (tailgate)" : "") << '\n';
        describe(best);
        return true;
    }
    if (op == "vehicles")
    {
        int count = 0;
        game.mWorld.forEach<Vehicle>([&](Entity e, Vehicle&) { describe(e); ++count; });
        std::cout << "[harness] tick " << mTick << ": " << count << " vehicles\n";
        return true;
    }
    if (op == "set" && a.size() > 1)
    {
        const std::string what = lowered(a[1]);
        if (what != "fuel" && what != "condition" && what != "tyres" && what != "key" && what != "alarm" && what != "started") return false;
        if (!need(2)) return true;
        const Entity e = focus();
        if (e == kInvalidEntity)
        {
            fail(command, "no vehicle");
            return true;
        }
        Vehicle& v = game.mWorld.getComponent<Vehicle>(e);
        const std::string value = lowered(a[2]);
        if (what == "fuel") v.fuel = std::clamp(std::stof(value), 0.0f, v.maxFuel);
        else if (what == "condition") v.condition = std::clamp(std::stof(value), 0.0f, 100.0f);
        else if (what == "tyres") v.tyres = std::clamp(std::stoi(value), 0, 4);
        else if (what == "key")
        {
            // set key in|known|none
            v.keyInside = value == "in";
            v.keyKnown = value == "known";
            if (value == "none") v.started = false;
        }
        else if (what == "alarm")
        {
            v.alarm = value == "on";
            v.alarmSpentRun = -1;
            if (!v.alarm) v.alarmSeconds = 0.0f; // off: silent too
        }
        else if (what == "started") v.started = value == "on";
        game.refreshVehicleSprite(e);
        describe(e);
        return true;
    }
    if (op == "drive")
    {
        // drive DIR SECONDS [CAP]: hold the keys toward DIR (n s e w ne nw se sw,
        // the town's axes) for SECONDS; drive road SECONDS [CAP]: follow the road;
        // drive brake: brake to a stop. The script waits while it drives.
        if (!need(1)) return true;
        if (game.mActiveVehicleEntity == kInvalidEntity)
        {
            fail(command, "not in a vehicle");
            return true;
        }
        const std::string mode = lowered(a[1]);
        DriveOrder order;
        order.last = game.vehicleCentre(game.mActiveVehicleEntity);
        double seconds = 0.0;
        if (mode == "brake")
        {
            order.mode = "brake";
            const Vehicle& v = game.mWorld.getComponent<Vehicle>(game.mActiveVehicleEntity);
            seconds = std::fabs(v.speed) / vehiclerules::info(v.kind).brake + 0.5;
        }
        else
        {
            if (!need(2)) return true;
            seconds = std::stod(a[2]);
            if (a.size() > 3) order.cap = std::stof(a[3]);
            if (mode == "road")
            {
                order.mode = "road";
                if (a.size() <= 3) order.cap = 160.0f;
            }
            else if (parseWorldDir(mode, order.axis))
            {
                order.mode = "dir";
            }
            else
            {
                fail(command, "unknown direction '" + a[1] + "' (n s e w ne nw se sw, road, brake)");
                return true;
            }
        }
        order.ticksLeft = static_cast<std::uint64_t>(std::max(1.0, seconds * kTicks));
        mDrive = order;
        mWaitRemaining = order.ticksLeft;
        std::cout << "[harness] tick " << mTick << ": drive " << mode << " for " << seconds << " s\n";
        return true;
    }
    if (op == "vehicle")
    {
        if (!need(1)) return true;
        const std::string what = lowered(a[1]);
        const Entity e = focus();
        if (e == kInvalidEntity)
        {
            fail(command, "no vehicle");
            return true;
        }
        Vehicle& v = game.mWorld.getComponent<Vehicle>(e);
        const glm::vec2 c = game.vehicleCentre(e);
        if (what == "mark")
        {
            // vehicle mark: remember it (expect vehicle same, moved, scrapes).
            mVehicleMark.set = true;
            mVehicleMark.centre = c;
            mVehicleMark.heading = v.heading;
            mVehicleMark.fuel = v.fuel;
            mVehicleMark.condition = v.condition;
            mVehicleMark.scrapes = game.mVehicleScrapes;
            mVehicleMark.trunkItems = 0;
            if (v.trunk != kInvalidEntity && game.mWorld.hasComponent<Inventory>(v.trunk))
            {
                for (const InventorySlot& slot : game.mWorld.getComponent<Inventory>(v.trunk).slots) mVehicleMark.trunkItems += slot.count;
            }
            describe(e);
            return true;
        }
        if (what == "map")
        {
            // vehicle map R: the ground around it as a vehicle sees it
            // (# wall, + run down at speed, = road, . open, V the vehicle).
            const int r = a.size() > 2 ? std::stoi(a[2]) : 12;
            const glm::ivec2 at = gameshared::worldToTile(game.mTileMap, c);
            for (int y = at.y - r; y <= at.y + r; ++y)
            {
                std::string row;
                for (int x = at.x - r; x <= at.x + r; ++x)
                {
                    const int kind = game.vehicleTileKind(x, y);
                    const int gid = game.mTileMap.inBounds(x, y) ? game.mTileMap.groundGid(x, y) : 0;
                    const bool road = gid > 0 && ((gid - 1) % TileGid::kPerRow) + 1 == TileGid::kAsphalt;
                    row += x == at.x && y == at.y ? 'V'
                           : kind == static_cast<int>(VehicleSystem::TileKind::Wall) ? '#'
                           : kind == static_cast<int>(VehicleSystem::TileKind::Thin) ? '+'
                           : road ? '=' : '.';
                }
                std::cout << "[harness] map " << y << ": " << row << '\n';
            }
            return true;
        }
        if (what == "park" || what == "fetch" || what == "turn")
        {
            // vehicle turn DEGREES: face it that way where it stands (screenshots).
            if (what == "turn")
            {
                if (!need(2)) return true;
                const float heading = std::stof(a[2]) / 57.29578f;
                if (VehicleSystem::blocked(v.kind, c, heading, VehicleSystem::Env{[&](int x, int y)
                                                                                 { return static_cast<VehicleSystem::TileKind>(game.vehicleTileKind(x, y)); },
                                                                                 32.0f}))
                {
                    fail(command, "no room to turn it there");
                    return true;
                }
                v.heading = heading;
                game.refreshVehicleSprite(e);
                describe(e);
                return true;
            }
            if (what == "fetch")
            {
                // vehicle fetch KIND: the nearest movable piece of KIND (a sprite name
                // part, e.g. wardrobe) pushed up behind the tailgate.
                if (!need(2)) return true;
                const glm::vec2 back = -vehiclerules::forward(v.heading);
                const glm::vec2 rear = vehiclerules::rear(v.kind, c, v.heading);
                glm::ivec2 target = gameshared::worldToTile(game.mTileMap, rear + back * 28.0f);
                if (target == gameshared::worldToTile(game.mTileMap, rear)) target = gameshared::worldToTile(game.mTileMap, rear + back * 44.0f);
                const FurniturePiece* best = nullptr;
                float bestDistance = 1.0e9f;
                for (const FurniturePiece& piece : game.mFurniture)
                {
                    if (!furnitureuse::movable(piece.kind) || piece.tile.x < 0) continue;
                    if (std::string(furniture::info(piece.kind).sprite).find(lowered(a[2])) == std::string::npos) continue;
                    const float d = glm::length(glm::vec2(piece.tile - target));
                    if (d < bestDistance)
                    {
                        bestDistance = d;
                        best = &piece;
                    }
                }
                if (best == nullptr)
                {
                    fail(command, "no movable " + a[2]);
                    return true;
                }
                if (game.mTileMap.isSolid(target.x, target.y) || game.furnitureAt(target) != nullptr)
                {
                    fail(command, "no room behind the tailgate");
                    return true;
                }
                std::cout << "[harness] tick " << mTick << ": fetched the " << furniture::info(best->kind).sprite << " from "
                          << best->tile.x << "," << best->tile.y << " to " << target.x << "," << target.y << '\n';
                game.moveFurniturePiece(best->tile, target);
                return true;
            }
            // vehicle park door: backed up to the nearest open doorway with a free
            // floor inside, tailgate to the door, engine off.
            const glm::vec2 here = c;
            const Doors::Door* chosen = nullptr;
            glm::ivec2 outward(0);
            float bestDistance = 1.0e9f;
            const vehiclerules::KindInfo& k = vehiclerules::info(v.kind);
            for (const Doors::Door& door : game.mDoors.all())
            {
                const int building = game.mTileMap.getBuildingId(door.tile.x, door.tile.y);
                for (const glm::ivec2 o : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
                {
                    const glm::ivec2 out = door.tile + o;
                    const glm::ivec2 in = door.tile - o;
                    if (game.mTileMap.getBuildingId(out.x, out.y) > 0 || game.mTileMap.isIndoors(out.x, out.y)) continue;
                    if (game.mTileMap.getBuildingId(in.x, in.y) != building || game.mTileMap.isSolid(in.x, in.y) ||
                        game.furnitureAt(in) != nullptr)
                    {
                        continue;
                    }
                    const glm::vec2 doorCentre = (glm::vec2(door.tile) + 0.5f) * 32.0f;
                    const float heading = std::atan2(static_cast<float>(o.y), static_cast<float>(o.x));
                    const glm::vec2 at = doorCentre + glm::vec2(o) * (k.length * 0.5f + 20.0f);
                    const VehicleSystem::Env env{[&](int x, int y)
                                                 { return static_cast<VehicleSystem::TileKind>(game.vehicleTileKind(x, y)); },
                                                 32.0f};
                    if (VehicleSystem::blocked(v.kind, at, heading, env)) continue;
                    const float d = glm::length(at - here);
                    if (d < bestDistance)
                    {
                        bestDistance = d;
                        chosen = &door;
                        outward = o;
                    }
                }
            }
            if (chosen == nullptr)
            {
                fail(command, "no doorway to back up to");
                return true;
            }
            // Its door open, as the player would leave it.
            game.mDoors.setState(game.mTileMap, chosen->tile, DoorState::Open);
            const glm::vec2 doorCentre = (glm::vec2(chosen->tile) + 0.5f) * 32.0f;
            const glm::vec2 at = doorCentre + glm::vec2(outward) * (k.length * 0.5f + 20.0f);
            Transform& t = game.mWorld.getComponent<Transform>(e);
            t.x = t.prevX = at.x - 16.0f;
            t.y = t.prevY = at.y - 16.0f;
            v.heading = std::atan2(static_cast<float>(outward.y), static_cast<float>(outward.x));
            v.speed = 0.0f;
            v.engineOn = false;
            if (v.trunk != kInvalidEntity && game.mWorld.hasComponent<Transform>(v.trunk))
            {
                const glm::vec2 rear = vehiclerules::rear(v.kind, at, v.heading);
                Transform& tt = game.mWorld.getComponent<Transform>(v.trunk);
                tt.x = tt.prevX = rear.x - 16.0f;
                tt.y = tt.prevY = rear.y - 16.0f;
            }
            game.refreshVehicleSprite(e);
            if (game.mActiveVehicleEntity == e)
            {
                Transform& pt = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
                pt.x = pt.prevX = t.x;
                pt.y = pt.prevY = t.y;
                game.mCamera.snapTo(at);
            }
            std::cout << "[harness] tick " << mTick << ": parked with the tailgate to door " << chosen->tile.x << ","
                      << chosen->tile.y << '\n';
            return true;
        }
        fail(command, "unknown vehicle command '" + a[1] + "'");
        return true;
    }
    if (op == "expect" && a.size() > 1 && lowered(a[1]) == "vehicle")
    {
        // expect vehicle in|out|engine|off|started|cargo|nocargo|window|ringing|silent
        // expect vehicle speed|condition|fuel|tyres above|below N
        // expect vehicle trunk N (at least N things in it) | same [worn] | moved N | scrapes N | lost N
        if (!need(2)) return true;
        const std::string what = lowered(a[2]);
        const Entity e = focus();
        if (e == kInvalidEntity)
        {
            fail(command, "no vehicle");
            return true;
        }
        const Vehicle& v = game.mWorld.getComponent<Vehicle>(e);
        const glm::vec2 c = game.vehicleCentre(e);
        int items = 0;
        if (v.trunk != kInvalidEntity && game.mWorld.hasComponent<Inventory>(v.trunk))
        {
            for (const InventorySlot& slot : game.mWorld.getComponent<Inventory>(v.trunk).slots) items += slot.count;
        }
        const auto check = [&](bool ok, const std::string& have)
        {
            if (!ok) fail(command, "expected vehicle " + what + ": " + have);
            else std::cout << "[harness] tick " << mTick << ": vehicle " << what << ": " << have << '\n';
        };
        if (what == "in" || what == "out")
        {
            const bool in = game.mActiveVehicleEntity != kInvalidEntity;
            check(in == (what == "in"), in ? "in one" : "on foot");
            return true;
        }
        if (what == "engine" || what == "off") { check(v.engineOn == (what == "engine"), v.engineOn ? "running" : "off"); return true; }
        if (what == "started") { check(v.started, v.started ? "started" : "never started"); return true; }
        if (what == "cargo" || what == "nocargo") { check(v.hasCargo == (what == "cargo"), v.hasCargo ? "loaded" : "empty bed"); return true; }
        if (what == "window") { check(v.windowBroken, v.windowBroken ? "broken" : "whole"); return true; }
        if (what == "ringing" || what == "silent")
        {
            check((v.alarmSeconds > 0.0f) == (what == "ringing"), v.alarmSeconds > 0.0f ? "ringing" : "silent");
            return true;
        }
        if (what == "speed" || what == "condition" || what == "fuel" || what == "tyres")
        {
            if (!need(4)) return true;
            const float have = what == "speed" ? std::fabs(v.speed) : what == "condition" ? v.condition : what == "fuel" ? v.fuel : static_cast<float>(v.tyres);
            const float limit = std::stof(a[4]);
            const bool above = lowered(a[3]) == "above";
            check(above ? have > limit : have < limit, std::to_string(have));
            return true;
        }
        if (what == "randown" || what == "stopped")
        {
            // expect vehicle randown|stopped N: at least N of the dead thrown by a
            // car, or N times a car stopped dead against one (a Brute), so far.
            if (!need(3)) return true;
            const int have = what == "randown" ? game.mVehicleRunDowns : game.mVehicleStops;
            check(have >= std::stoi(a[3]), std::to_string(have));
            return true;
        }
        if (what == "trunk")
        {
            if (!need(3)) return true;
            check(items >= std::stoi(a[3]), std::to_string(items) + " things");
            return true;
        }
        if (!mVehicleMark.set)
        {
            fail(command, "no vehicle mark");
            return true;
        }
        if (what == "moved")
        {
            if (!need(3)) return true;
            const float tiles = glm::length(c - mVehicleMark.centre) / 32.0f;
            check(tiles >= std::stof(a[3]), std::to_string(tiles) + " tiles");
            return true;
        }
        if (what == "scrapes")
        {
            if (!need(3)) return true;
            const int scrapes = game.mVehicleScrapes - mVehicleMark.scrapes;
            check(scrapes <= std::stoi(a[3]), std::to_string(scrapes) + " wall contacts");
            return true;
        }
        if (what == "lost")
        {
            // expect vehicle lost N: at least N condition lost since the mark.
            if (!need(3)) return true;
            const float lost = mVehicleMark.condition - v.condition;
            check(lost >= std::stof(a[3]), std::to_string(lost) + " condition lost");
            return true;
        }
        if (what == "same")
        {
            // expect vehicle same [worn]: the marked vehicle (nearest the mark) as it was;
            // worn: one stage worse (condition -15), as the next life finds it.
            Entity found = kInvalidEntity;
            float bestDistance = 1.0e9f;
            game.mWorld.forEach<Vehicle, Transform>(
                [&](Entity other, Vehicle&, Transform& t)
                {
                    const float d = glm::length(centreOf(t) - mVehicleMark.centre);
                    if (d < bestDistance)
                    {
                        bestDistance = d;
                        found = other;
                    }
                });
            const Vehicle& m = game.mWorld.getComponent<Vehicle>(found);
            int have = 0;
            if (m.trunk != kInvalidEntity && game.mWorld.hasComponent<Inventory>(m.trunk))
            {
                for (const InventorySlot& slot : game.mWorld.getComponent<Inventory>(m.trunk).slots) have += slot.count;
            }
            const bool worn = a.size() > 3 && lowered(a[3]) == "worn";
            const float wantCondition = std::max(0.0f, mVehicleMark.condition - (worn ? vehiclerules::kBetweenLivesWear : 0.0f));
            const bool ok = bestDistance < 1.0f && std::fabs(m.heading - mVehicleMark.heading) < 0.01f &&
                            std::fabs(m.fuel - mVehicleMark.fuel) < 0.05f && std::fabs(m.condition - wantCondition) < 0.5f &&
                            have == mVehicleMark.trunkItems;
            check(ok, "off by " + std::to_string(bestDistance) + " px, heading " + std::to_string(m.heading) + " (was " +
                          std::to_string(mVehicleMark.heading) + "), fuel " + std::to_string(m.fuel) + " (was " +
                          std::to_string(mVehicleMark.fuel) + "), condition " + std::to_string(m.condition) + " (want " +
                          std::to_string(wantCondition) + "), trunk " + std::to_string(have) + " (was " +
                          std::to_string(mVehicleMark.trunkItems) + ")");
            return true;
        }
        fail(command, "unknown expect vehicle '" + a[2] + "'");
        return true;
    }
    return false;
}
