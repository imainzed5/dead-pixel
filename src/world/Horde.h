#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <glm/vec2.hpp>

namespace zombiekinds
{
struct ModeRules;
}

// Hordes: a crowd that crosses the town at night (overhaul phase 4). When,
// how many and which way, as plain rules; Game gathers and walks them.
namespace horde
{
// The horde sets out at this hour of a horde night.
inline constexpr double kSetOutHour = 21.0;
// Walking pace, a share of each one's speed.
inline constexpr float kPaceScale = 0.6f;
// It passes this close to the player's home (tiles).
inline constexpr float kPassMinTiles = 6.0f;
inline constexpr float kPassMaxTiles = 10.0f;
// Where it ends, the district's infection rises by this.
inline constexpr float kInfectionRise = 5.0f;

// Whether night N (dusk of day N) brings a horde: night 7, then every 4 to 6
// nights by seed; in Swarm and Hybrid every 3 to 4 from night 5.
[[nodiscard]] bool isHordeNight(int night, std::uint32_t runSeed, const zombiekinds::ModeRules& mode);
// The horde nights up to and including `lastNight`.
[[nodiscard]] std::vector<int> hordeNights(int lastNight, std::uint32_t runSeed, const zombiekinds::ModeRules& mode);
// 14, and 2 more per day past day 7, up to 40.
[[nodiscard]] int hordeSize(int day);

// Its way across town: from one edge of the map, past the home at 6 to 10
// tiles, to the far edge. Waypoints are snapped to road tiles where there
// are any near (`isRoad`), else to walkable ones (`walkable`).
struct RouteInput
{
    int width = 0;  // tiles
    int height = 0;
    glm::ivec2 home{0};
    std::uint32_t seed = 0;
    std::function<bool(int, int)> walkable;
    std::function<bool(int, int)> isRoad;
};
[[nodiscard]] std::vector<glm::ivec2> route(const RouteInput& in);
} // namespace horde
