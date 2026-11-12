#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "rendering/StaticSprite.h"
#include "world/Scenery.h"

class Camera;
class SpriteBatch;
class SpriteSheet;
class TileMap;

// Someone walking about: scenery rustles and litter is kicked where they go.
struct SceneryMover
{
    glm::vec2 position{0.0f}; // feet, world pixels
    glm::vec2 velocity{0.0f}; // world pixels per second
};

// A tree that sheds leaves.
struct SceneryCanopy
{
    glm::vec2 position{0.0f};
    bool autumn = false;
};

// Draws outdoor scenery and moves the parts of it that move. Standing pieces
// sort with walls and characters; flat ones are laid straight on the ground
// before anything else is drawn.
//
//  - Trees lean with the wind and bushes stir in it.
//  - Bushes, tufts and flowers shake when someone pushes through them.
//  - Cans and paper are kicked along by passing feet, roll to a stop and
//    bounce off walls; paper is also carried by a strong wind.
//
// None of the movement is saved: a new run finds the town as it was planted.
class SceneryView
{
public:
    // Must match FOOT in tools/scenery.py: a standing sprite touches the
    // ground this many pixels above its bottom edge.
    static constexpr float kFoot = 6.0f;
    static constexpr float kFadedAlpha = 0.4f;
    static constexpr float kRustleSeconds = 0.4f;
    static constexpr float kKickRadius = 8.0f;
    static constexpr float kRustleRadius = 10.0f;

    void build(const std::vector<SceneryPiece>& pieces);
    // `rustled` (optional) receives where something was newly disturbed.
    void update(float dtSeconds, const glm::vec2& wind, float gust, const std::vector<SceneryMover>& movers,
                const TileMap& map, std::vector<glm::vec2>* rustled = nullptr);
    void drawFlat(SpriteBatch& batch, const Camera& camera, const SpriteSheet& sheet);
    void collect(const Camera& camera, const glm::ivec2& playerTile, const SpriteSheet& sheet,
                 std::vector<StaticSprite>& out);

    // Lamp posts and leafy trees within `radius` world pixels of `centre`.
    void lampsNear(const glm::vec2& centre, float radius, std::vector<glm::vec2>& out) const;
    void canopiesNear(const glm::vec2& centre, float radius, std::vector<SceneryCanopy>& out) const;

    // Whether the street lamp standing at `position` still works: 0 dead,
    // 1 burning, 2 failing (it stutters and drops out).
    [[nodiscard]] static int lampState(const glm::vec2& position)
    {
        const auto h = static_cast<std::uint32_t>(position.x) * 73856093u ^ static_cast<std::uint32_t>(position.y) * 19349663u;
        if ((h >> 4) % 3u == 0u)
        {
            return 0;
        }
        return (h >> 9) % 4u == 0u ? 2 : 1;
    }

    // A tall piece on `piece` would hide the player standing on `player`.
    [[nodiscard]] static bool hidesPlayer(const glm::ivec2& piece, const glm::ivec2& player);
    // How far, in sprite pixels, a point `fraction` of the way up a piece
    // leans at `clock` seconds. `stiff` is the part of the height that does
    // not move at all (a trunk).
    [[nodiscard]] static float lean(float clock, float phase, float gust, float fraction, float stiff);

    // For tests: where a piece lying on the ground is now.
    [[nodiscard]] std::size_t looseCount() const { return mLoose.size(); }
    [[nodiscard]] glm::vec2 loosePosition(std::size_t i) const { return mFlat[mLoose[i]].piece.position; }

private:
    enum class Motion : std::uint8_t
    {
        Still,
        Tree,   // sways from the trunk up
        Shrub,  // stirs, and shakes when pushed through
        Can,    // kicked: rolls a long way
        Paper   // kicked or blown: stops soon
    };
    struct Entry
    {
        SceneryPiece piece;
        glm::ivec2 tile{0};
        glm::vec4 uv{0.0f};
        glm::vec2 size{0.0f};  // sprite pixels
        float anchorX = 0.0f;  // sprite pixels from its left edge to the ground point
        Motion motion = Motion::Still;
        float phase = 0.0f;
        float rustle = 0.0f;   // seconds of shaking left
        glm::vec2 velocity{0.0f};
        bool lamp = false;
        bool autumn = false;
    };
    void resolve(const SpriteSheet& sheet);
    [[nodiscard]] static int bucketKey(int tileX, int tileY) { return tileY * 4096 + tileX; }

    std::vector<Entry> mStanding;
    std::vector<Entry> mFlat;
    std::vector<std::uint32_t> mLoose;   // indices into mFlat: cans and paper
    std::vector<std::uint32_t> mShaking; // indices into mStanding with rustle left
    std::unordered_map<int, std::vector<std::uint32_t>> mShrubsByTile;
    std::vector<std::uint32_t> mLamps;
    std::vector<std::uint32_t> mTrees;
    float mClock = 0.0f;
    float mGust = 0.0f;
    bool mResolved = false; // the sprite sheet loads after the world
};
