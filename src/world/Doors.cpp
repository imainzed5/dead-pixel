#include "world/Doors.h"

#include <cmath>

void Doors::reset(TileMap& map, const LayoutData& layout)
{
    for (const Door& door : mDoors)
    {
        map.setDoorState(door.tile.x, door.tile.y, DoorState::None);
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

DoorState Doors::toggle(TileMap& map, const glm::ivec2& tile)
{
    const DoorState state = map.doorState(tile.x, tile.y);
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
    if (map.doorState(tile.x, tile.y) == DoorState::Closed)
    {
        map.setDoorState(tile.x, tile.y, DoorState::Open);
    }
}

Doors::Blow Doors::batter(TileMap& map, const glm::ivec2& tile, float damage)
{
    Door* door = findMutable(tile);
    if (door == nullptr || map.doorState(tile.x, tile.y) != DoorState::Closed)
    {
        return Blow::None;
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
