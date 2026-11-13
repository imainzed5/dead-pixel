#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

// Blood the living leave behind, which the dead follow by smell (overhaul
// phase 4). Plain rules and state, no ECS. Times are real seconds (one is a
// game minute). Checkpoints keep the markers; world memory does not.
namespace bloodtrail
{
// A marker is dropped every this many tiles walked while bleeding.
inline constexpr float kDropEveryTiles = 1.5f;
inline constexpr float kBleedingStrength = 1.0f;
inline constexpr float kWoundStrength = 0.4f;
// A bladed kill leaves a pool.
inline constexpr float kPoolStrength = 0.3f;
inline constexpr float kPoolSeconds = 10.0f; // 10 game minutes
// How long a marker lasts, by where it lies (game minutes = real seconds).
inline constexpr float kIndoorSeconds = 20.0f;
inline constexpr float kOutdoorSeconds = 10.0f;
inline constexpr float kRainSeconds = 3.0f;
// The dead smell markers within this of themselves, twice a second.
inline constexpr float kSmellTiles = 5.0f;
inline constexpr float kSmellEverySeconds = 0.5f;

enum class Place : std::uint8_t
{
    Outdoor,
    Indoor,
    Water
};

struct Marker
{
    glm::vec2 world{0.0f};
    float strength = 1.0f;
    float age = 0.0f;
    float life = kOutdoorSeconds; // a pool's fixed life; markers recompute it
    Place place = Place::Outdoor;
    bool pool = false;
    std::uint32_t seq = 0;        // later markers are fresher
};

// How long a marker in `place` lasts under rain or not, scaled (the Stalker
// keeps trails three times as long).
[[nodiscard]] float lifeSeconds(Place place, bool raining, float scale = 1.0f);

class Trail
{
public:
    void clear();
    // The player moved from `from` to `to`; while they bleed (strength > 0)
    // a marker drops every kDropEveryTiles. `place` is where `to` lies.
    void walk(const glm::vec2& from, const glm::vec2& to, float strength, Place place, float tileSize);
    void addMarker(const glm::vec2& world, float strength, Place place, bool pool = false);
    // Age every marker and drop the faded ones.
    void update(float dtSeconds, bool raining, float scale = 1.0f);

    // How fresh a marker is: its strength, fading to nothing over its life.
    [[nodiscard]] static float freshness(const Marker& m);
    // The freshest marker within `radius` of `at` that is later than
    // `afterSeq`, or nullptr.
    [[nodiscard]] const Marker* freshestNear(const glm::vec2& at, float radius, std::uint32_t afterSeq) const;

    [[nodiscard]] const std::vector<Marker>& markers() const { return mMarkers; }
    void restore(const std::vector<Marker>& markers);

private:
    std::vector<Marker> mMarkers;
    float mWalked = 0.0f;
    std::uint32_t mNextSeq = 1;
};
} // namespace bloodtrail
