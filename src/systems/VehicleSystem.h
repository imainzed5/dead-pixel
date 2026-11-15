#pragma once

#include <array>
#include <functional>
#include <vector>

#include <glm/vec2.hpp>

#include "components/Vehicle.h"
#include "world/VehicleRules.h"

// Moves a vehicle (overhaul phase 11): the arcade drive from vehiclerules,
// then its body (three circles along its length) against the town's tiles.
// A wall stops it and reports the knock; thin things (a hydrant, a bin, a
// fence) give way above 150 px/s and are reported to be removed. Only a
// vehicle with someone in it or still rolling is ever stepped: a parked one
// costs nothing. The game does the rest (GameVehicles.cpp).
class VehicleSystem
{
public:
    enum class TileKind
    {
        Open,
        Wall, // walls, doorways, furniture, trees, lamp posts
        Thin  // run down at speed
    };

    struct Env
    {
        std::function<TileKind(int, int)> tile;
        float tileSize = 32.0f;
    };

    struct Hit
    {
        enum class Type
        {
            Wall,
            Thin
        } type = Type::Wall;
        glm::ivec2 tile{0};
        float impact = 0.0f; // speed into it, px/s
    };

    // One step. `centre` is the body's middle in world px; it and the
    // vehicle's heading and speed move. Hits are appended.
    static void step(VehicleKind kind, glm::vec2& centre, float& heading, float& speed,
                     const vehiclerules::DriveInput& input, const Env& env, float dt, std::vector<Hit>& hits);

    // Does the body at `centre` overlap anything solid (Thin counts as solid)?
    [[nodiscard]] static bool blocked(VehicleKind kind, const glm::vec2& centre, float heading, const Env& env);

    // A body of `radius` at `point` pushed out of the vehicle: the offset to
    // add to it, or zero when it is clear.
    [[nodiscard]] static glm::vec2 pushOut(VehicleKind kind, const glm::vec2& centre, float heading,
                                           const glm::vec2& point, float radius);
    // Distance from `point` to the vehicle's skin (negative inside).
    [[nodiscard]] static float distanceToBody(VehicleKind kind, const glm::vec2& centre, float heading,
                                              const glm::vec2& point);
    // The nearest point on the body's centre line to `point`.
    [[nodiscard]] static glm::vec2 nearestOnAxis(VehicleKind kind, const glm::vec2& centre, float heading,
                                                 const glm::vec2& point);
};
