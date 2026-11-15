#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include <glm/vec2.hpp>

// Fire on a grid of tiles (overhaul phase 7). Each burning tile keeps its
// fuel and heat in a sparse map; nothing scans entities. Game asks the grid
// by tile and applies what it reports (GameFire.cpp).
namespace firerules
{
// What lies on a tile, for its fuel.
enum class Ground : std::uint8_t
{
    Bare,      // brick, tile, concrete, asphalt, metal, dirt, water
    Grass,
    WoodFloor,
    Carpet,
    WoodWall,  // a wall of a wooden house: walls burn now
    Tree
};
struct TileFacts
{
    Ground ground = Ground::Bare;
    bool woodFurniture = false;
    bool metalFurniture = false;
    bool boards = false;   // boards on a door or window
    bool heatwave = false; // dry grass
    bool wet = false;      // rain lately: wet grass
    bool snow = false;     // grass under snow does not burn
};
// Fuel 0..1: grass 0.5 (0.8 in a heatwave, 0.2 wet, 0 under snow), wood
// floor 0.6, carpet 0.7, a wooden wall 0.4, a tree 0.9; wooden furniture
// +0.3, metal +0.05, boards +0.2; the rest 0.
[[nodiscard]] float fuelOf(const TileFacts& facts);

inline constexpr float kSpreadEverySeconds = 2.0f;
inline constexpr float kFuelPerSecond = 0.02f;
inline constexpr float kHeatRiseSeconds = 20.0f; // heat rises to 1 over this
inline constexpr float kSpreadStartHeat = 0.2f;  // a tile caught from its neighbour
inline constexpr float kMolotovHeat = 0.8f;
inline constexpr float kDouseHeat = 0.9f;
inline constexpr float kScorchedFuel = 0.1f;     // a fire put out after using this much leaves a scar
// Wind over this blows fire along: x1.6 downwind, x0.6 upwind; over 0.5 it
// also carries to the two downwind diagonals.
inline constexpr float kWindCarries = 0.25f;
inline constexpr float kWindDiagonals = 0.5f;
inline constexpr float kDownwind = 1.6f;
inline constexpr float kUpwind = 0.6f;
// Damage a second at heat 1.
inline constexpr float kPlayerDamagePerSecond = 14.0f;
inline constexpr float kOtherDamagePerSecond = 18.0f;
inline constexpr float kFirewiseScale = 0.6f;
// The dead set alight: one in five keeps walking, burning, for 6 s and
// lights what it passes.
inline constexpr float kWalkingTorchChance = 0.2f;
inline constexpr float kWalkingTorchSeconds = 6.0f;
// A fire this many tiles big is Loud every 2 s.
inline constexpr int kLoudTiles = 6;
inline constexpr float kNoiseEverySeconds = 2.0f;
// Campfires catch what they stand on: grass in a heatwave 10 % an hour,
// wood or carpet 5 % an hour at any time.
inline constexpr float kCampfireGrassPerHour = 0.10f;
inline constexpr float kCampfireFloorPerHour = 0.05f;
// A building of wood is gutted when this share of its floor has burned.
inline constexpr float kGuttedShare = 0.4f;

// How the wind takes fire from a tile toward its neighbour at `dir`.
[[nodiscard]] float windFactor(const glm::ivec2& dir, const glm::vec2& windTo, float windStrength);
// The chance a neighbour with `fuel` catches in one try.
[[nodiscard]] float spreadChance(float fuel, float heat, float weatherScale, float windFactor);
// The neighbours a burning tile tries: four, and the two downwind diagonals
// in a strong wind.
[[nodiscard]] std::vector<glm::ivec2> spreadDirections(const glm::vec2& windTo, float windStrength);
} // namespace firerules

class FireSystem
{
public:
    struct Tile
    {
        float fuel = 0.0f;
        float startFuel = 0.0f;
        float heat = 0.0f;
        float spreadTimer = 0.0f;
        bool byPlayer = false; // kills it makes are the player's
    };
    struct Burning
    {
        glm::ivec2 tile{0};
        float heat = 0.0f;
        float fuel = 0.0f;
        bool byPlayer = false;
    };
    // The world this tick, as the fire needs it.
    struct Env
    {
        std::function<float(int, int)> fuelAt;   // fuel of a tile not yet burning (0: cannot burn)
        std::function<bool(int, int)> outdoors;  // rain reaches it
        std::function<bool(int, int)> grass;     // grass burns quicker in a heatwave
        float spreadScale = 1.0f;     // weatherrules::fireSpreadScale
        float grassScale = 1.0f;      // the same for grass
        float outdoorDieSeconds = 0.0f; // rain: seconds to put out a fire in the open (0: never)
        glm::vec2 windTo{1.0f, 0.0f};
        float windStrength = 0.0f;
        std::uint32_t seed = 0;
    };
    struct Events
    {
        std::vector<glm::ivec2> ignited;
        std::vector<glm::ivec2> burnedOut;   // used all its fuel: scorched
        std::vector<glm::ivec2> wentOut;     // put out by rain or water
        std::vector<glm::ivec2> scorched;    // of those put out, the ones that left a scar
    };

    void clear() { mTiles.clear(); }
    // Set a tile alight at `heat` (or more of one already burning). False
    // where there is nothing to burn.
    bool ignite(const glm::ivec2& tile, float heat, bool byPlayer, const Env& env);
    // A tile from a save: its fuel and heat as they were.
    void restore(const glm::ivec2& tile, float fuel, float heat, bool byPlayer);
    // Water on a tile: heat down by `amount`; true if it was burning.
    bool douse(const glm::ivec2& tile, float amount, Events* events = nullptr);
    void update(float dtSeconds, const Env& env, Events& events);

    [[nodiscard]] const Tile* at(const glm::ivec2& tile) const;
    [[nodiscard]] bool burning(const glm::ivec2& tile) const { return at(tile) != nullptr; }
    [[nodiscard]] std::size_t count() const { return mTiles.size(); }
    [[nodiscard]] std::vector<Burning> all() const;
    [[nodiscard]] std::vector<Burning> burningNear(const glm::ivec2& centre, int radius) const;

    [[nodiscard]] static std::int64_t key(const glm::ivec2& tile)
    {
        return (static_cast<std::int64_t>(tile.y) << 32) | static_cast<std::uint32_t>(tile.x);
    }
    [[nodiscard]] static glm::ivec2 tileOf(std::int64_t key)
    {
        return glm::ivec2(static_cast<std::int32_t>(static_cast<std::uint32_t>(key & 0xFFFFFFFF)),
                          static_cast<std::int32_t>(key >> 32));
    }

private:
    // Ordered, so every run spreads in the same order.
    std::map<std::int64_t, Tile> mTiles;
    std::uint32_t mSteps = 0;
};
