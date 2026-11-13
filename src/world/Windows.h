#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"
#include "map/TileMap.h"

// Windows in the outside walls of houses and shops. The wall tile stays
// solid; glass keeps the dead out until it is broken, sight passes through
// glass (not through boards or a drawn curtain), and an empty frame can be
// climbed. Like Doors: the state lives on the tile map, this keeps each
// window's glass and board health.
namespace windows
{
// A wall tile that holds a window, with the floor inside it and the ground
// outside.
struct Spot
{
    glm::ivec2 tile{0};
    glm::ivec2 inside{0};
    glm::ivec2 outside{0};
    int building = 0; // building id (index + 1)
};

// The windows of a town: outside walls of houses and shops (not industrial
// sheds), flanked by wall, not beside a doorway, every third tile along the
// wall and three in four of those by a hash of the tile. Drawing and the
// rules both read this, so a seed keeps its windows. Call on a tile map
// with walls applied and doors not yet closed.
[[nodiscard]] std::vector<Spot> place(const TileMap& map, const LayoutData& layout);
} // namespace windows

class Windows
{
public:
    static constexpr float kGlassHp = 60.0f;
    static constexpr int kMaxBoards = 2;
    static constexpr float kBoardHp = 300.0f;
    // Seconds to climb through an empty frame.
    static constexpr float kPlayerClimbSeconds = 1.5f;
    static constexpr float kZombieClimbSeconds = 4.0f;
    // Seconds between audible bangs on one window.
    static constexpr float kBangInterval = 0.9f;

    struct Window
    {
        windows::Spot spot;
        float glassHp = kGlassHp;
        float boardHp = 0.0f; // all its boards together
        float sinceBang = kBangInterval;
    };

    enum class Blow
    {
        None,      // no window, or an open frame (climbed, not battered)
        Thud,
        Bang,
        Splinters, // a board gave way
        Shatters   // the glass broke
    };

    [[nodiscard]] static int boardsFor(float boardHp);

    // Every window whole, no boards, no curtains.
    void reset(TileMap& map, const std::vector<windows::Spot>& spots);
    Blow batter(TileMap& map, const glm::ivec2& tile, float damage);
    // Breaks the glass. False if there is none (no window, already broken).
    bool breakGlass(TileMap& map, const glm::ivec2& tile);
    bool addBoard(TileMap& map, const glm::ivec2& tile);
    bool removeBoard(TileMap& map, const glm::ivec2& tile);
    // Restores a saved window: glass broken or not, what is left of its boards.
    void restore(TileMap& map, const glm::ivec2& tile, bool broken, float boardHp, bool covered);
    [[nodiscard]] int boards(const glm::ivec2& tile) const;
    void tick(float dtSeconds);

    [[nodiscard]] const Window* find(const glm::ivec2& tile) const;
    // Nearest window whose tile centre is within `range` world pixels.
    [[nodiscard]] const Window* nearest(const TileMap& map, const glm::vec2& world, float range) const;
    [[nodiscard]] const std::vector<Window>& all() const { return mWindows; }

private:
    [[nodiscard]] Window* findMutable(const glm::ivec2& tile);

    std::vector<Window> mWindows;
};
