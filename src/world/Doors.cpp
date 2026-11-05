#include "world/Doors.h"

#include <algorithm>
#include <cmath>

void Doors::reset(TileMap& map, const LayoutData& layout)
{
    for (const Door& door : mDoors)
    {
        map.setDoorState(door.tile.x, door.tile.y, DoorState::None);
        map.setDoorBoards(door.tile.x, door.tile.y, 0);
    }
    mDoors.clear();
    mDoors.reserve(layout.doors.size());
    for (const LayoutData::DoorPos& pos : layout.doors)
    {
        if (!map.inBounds(pos.x, pos.y))
        {
            continue;
        }
        mDoors.push_back(Door{glm::ivec2(pos.x, pos.y)});
        map.setDoorState(pos.x, pos.y, DoorState::Closed);
    }
}

int Doors::boardsFor(float boardHp)
{
    if (boardHp <= 0.0f)
    {
        return 0;
    }
    return std::clamp(static_cast<int>(std::ceil(boardHp / kBoardHp - 0.001f)), 1, kMaxBoards);
}

DoorState Doors::toggle(TileMap& map, const glm::ivec2& tile)
{
    const DoorState state = map.doorState(tile.x, tile.y);
    if (boards(tile) > 0)
    {
        return state; // nailed shut
    }
    if (state == DoorState::Open)
    {
        map.setDoorState(tile.x, tile.y, DoorState::Closed);
        return DoorState::Closed;
    }
    if (state == DoorState::Closed)
    {
        map.setDoorState(tile.x, tile.y, DoorState::Open);
        return DoorState::Open;
    }
    return state;
}

void Doors::open(TileMap& map, const glm::ivec2& tile)
{
    if (map.doorState(tile.x, tile.y) == DoorState::Closed && boards(tile) == 0)
    {
        map.setDoorState(tile.x, tile.y, DoorState::Open);
    }
}

void Doors::setState(TileMap& map, const glm::ivec2& tile, DoorState state)
{
    Door* door = findMutable(tile);
    if (door == nullptr || state == DoorState::None)
    {
        return;
    }
    door->hp = state == DoorState::Broken ? 0.0f : kMaxHp;
    if (state != DoorState::Closed)
    {
        door->boardHp = 0.0f;
        map.setDoorBoards(tile.x, tile.y, 0);
    }
    map.setDoorState(tile.x, tile.y, state);
}

Doors::Blow Doors::batter(TileMap& map, const glm::ivec2& tile, float damage)
{
    Door* door = findMutable(tile);
    if (door == nullptr || map.doorState(tile.x, tile.y) != DoorState::Closed)
    {
        return Blow::None;
    }
    if (door->boardHp > 0.0f)
    {
        const int before = boardsFor(door->boardHp);
        door->boardHp = std::max(0.0f, door->boardHp - damage);
        const int after = boardsFor(door->boardHp);
        map.setDoorBoards(tile.x, tile.y, after);
        if (after < before)
        {
            door->sinceBang = 0.0f;
            return Blow::Splinters;
        }
        if (door->sinceBang >= kBangInterval)
        {
            door->sinceBang = 0.0f;
            return Blow::Bang;
        }
        return Blow::Thud;
    }
    door->hp -= damage;
    if (door->hp <= 0.0f)
    {
        door->hp = 0.0f;
        map.setDoorState(tile.x, tile.y, DoorState::Broken);
        return Blow::Breaks;
    }
    if (door->sinceBang >= kBangInterval)
    {
        door->sinceBang = 0.0f;
        return Blow::Bang;
    }
    return Blow::Thud;
}

bool Doors::addBoard(TileMap& map, const glm::ivec2& tile)
{
    Door* door = findMutable(tile);
    if (door == nullptr || map.doorState(tile.x, tile.y) != DoorState::Closed || boardsFor(door->boardHp) >= kMaxBoards)
    {
        return false;
    }
    // A fresh board on top; a split one underneath still counts for what is left of it.
    door->boardHp = std::min(door->boardHp + kBoardHp, kBoardHp * static_cast<float>(kMaxBoards));
    map.setDoorBoards(tile.x, tile.y, boardsFor(door->boardHp));
    return true;
}

bool Doors::removeBoard(TileMap& map, const glm::ivec2& tile)
{
    Door* door = findMutable(tile);
    if (door == nullptr || door->boardHp <= 0.0f)
    {
        return false;
    }
    door->boardHp = kBoardHp * static_cast<float>(boardsFor(door->boardHp) - 1);
    map.setDoorBoards(tile.x, tile.y, boardsFor(door->boardHp));
    return true;
}

void Doors::setBoards(TileMap& map, const glm::ivec2& tile, float boardHp)
{
    Door* door = findMutable(tile);
    if (door == nullptr || map.doorState(tile.x, tile.y) != DoorState::Closed)
    {
        return;
    }
    door->boardHp = std::clamp(boardHp, 0.0f, kBoardHp * static_cast<float>(kMaxBoards));
    map.setDoorBoards(tile.x, tile.y, boardsFor(door->boardHp));
}

int Doors::boards(const glm::ivec2& tile) const
{
    const Door* door = find(tile);
    return door != nullptr ? boardsFor(door->boardHp) : 0;
}

void Doors::tick(float dtSeconds)
{
    for (Door& door : mDoors)
    {
        door.sinceBang += dtSeconds;
    }
}

const Doors::Door* Doors::find(const glm::ivec2& tile) const
{
    for (const Door& door : mDoors)
    {
        if (door.tile == tile) return &door;
    }
    return nullptr;
}

Doors::Door* Doors::findMutable(const glm::ivec2& tile)
{
    for (Door& door : mDoors)
    {
        if (door.tile == tile) return &door;
    }
    return nullptr;
}

const Doors::Door* Doors::nearest(const TileMap& map, const glm::vec2& world, float range) const
{
    const float tileW = static_cast<float>(map.tileWidth());
    const float tileH = static_cast<float>(map.tileHeight());
    const Door* best = nullptr;
    float bestDist = range * range;
    for (const Door& door : mDoors)
    {
        const float dx = (static_cast<float>(door.tile.x) + 0.5f) * tileW - world.x;
        const float dy = (static_cast<float>(door.tile.y) + 0.5f) * tileH - world.y;
        const float dist = dx * dx + dy * dy;
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = &door;
        }
    }
    return best;
}

int Doors::count(const TileMap& map, DoorState state) const
{
    int n = 0;
    for (const Door& door : mDoors)
    {
        n += map.doorState(door.tile.x, door.tile.y) == state ? 1 : 0;
    }
    return n;
}

std::vector<glm::ivec2> Doors::tilesIn(const TileMap& map, DoorState state) const
{
    std::vector<glm::ivec2> tiles;
    for (const Door& door : mDoors)
    {
        if (map.doorState(door.tile.x, door.tile.y) == state)
        {
            tiles.push_back(door.tile);
        }
    }
    return tiles;
}
