#pragma once

#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"
#include "map/TileMap.h"

// Every doorway in town holds a door. Doors start each run closed; the player
// opens and shuts them, survivors open them as they pass, and the dead batter
// them until they break. The door's state lives on the tile map (a closed
// door is solid); this keeps each door's damage.
class Doors
{
public:
    static constexpr float kMaxHp = 100.0f;
    // One zombie takes about 14 s to break a door; a crowd far less.
    static constexpr float kZombieDamagePerSecond = 7.0f;
    // Seconds between audible bangs on one door, however many are at it.
    static constexpr float kBangInterval = 0.9f;

    struct Door
    {
        glm::ivec2 tile{0};
        float hp = kMaxHp;
        float sinceBang = kBangInterval;
    };

    enum class Blow
    {
        None,   // no door there, or not closed
        Thud,   // damage taken, quietly
        Bang,   // damage taken, and it is time for an audible bang
        Breaks  // this blow broke it
    };

    // All doors closed and whole.
    void reset(TileMap& map, const LayoutData& layout);
    // Open <-> closed; broken doors stay broken. Returns the new state.
    DoorState toggle(TileMap& map, const glm::ivec2& tile);
    void open(TileMap& map, const glm::ivec2& tile);
    Blow batter(TileMap& map, const glm::ivec2& tile, float damage);
    void tick(float dtSeconds);

    [[nodiscard]] const Door* find(const glm::ivec2& tile) const;
    // Nearest door whose tile centre is within `range` world pixels.
    [[nodiscard]] const Door* nearest(const TileMap& map, const glm::vec2& world, float range) const;
    [[nodiscard]] const std::vector<Door>& all() const { return mDoors; }
    [[nodiscard]] int count(const TileMap& map, DoorState state) const;

private:
    [[nodiscard]] Door* findMutable(const glm::ivec2& tile);

    std::vector<Door> mDoors;
};
