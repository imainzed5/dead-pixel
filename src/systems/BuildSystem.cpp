#include "systems/BuildSystem.h"

#include <algorithm>

#include "map/TileIds.h"

using Where = StationRecipe::Where;

const std::array<StationRecipe, BuildSystem::kRecipeCount> BuildSystem::kRecipes = {{
    {StructureType::Campfire, "Campfire", "wood_planks", 2, nullptr, 0, false, false, 5.0f, false, Where::OutdoorsOrHardFloor},
    {StructureType::Workbench, "Workbench", "wood_planks", 4, "nails", 12, true, true, 20.0f, true, Where::Indoors},
    {StructureType::StorageCrate, "Storage Crate", "wood_planks", 3, "nails", 8, true, true, 12.0f, true, Where::Indoors},
    {StructureType::RainCollector, "Rain Collector", "scrap_metal", 2, "wood_planks", 1, true, false, 12.0f, false, Where::Outdoors},
    {StructureType::Bedroll, "Bedroll", "rags", 2, "rope", 1, false, false, 6.0f, false, Where::Indoors},
    {StructureType::GardenBed, "Garden Bed", "wood_planks", 2, "seed_packet", 1, false, false, 15.0f, false, Where::Soil},
}};

const StationRecipe* BuildSystem::recipeFor(StructureType type)
{
    for (const StationRecipe& recipe : kRecipes)
    {
        if (recipe.type == type) return &recipe;
    }
    return nullptr;
}

bool BuildSystem::placeable(const TileMap& map, const glm::ivec2& tile, StructureType type,
                            const std::vector<glm::ivec2>& occupied)
{
    const StationRecipe* recipe = recipeFor(type);
    if (recipe == nullptr || !map.inBounds(tile.x, tile.y) || map.isSolid(tile.x, tile.y)) return false;
    if (map.doorState(tile.x, tile.y) != DoorState::None || map.hasWindow(tile.x, tile.y)) return false;
    if (std::find(occupied.begin(), occupied.end(), tile) != occupied.end()) return false;
    const bool indoors = map.isIndoors(tile.x, tile.y);
    switch (recipe->where)
    {
    case Where::Indoors:
        return indoors;
    case Where::Outdoors:
        return !indoors;
    case Where::OutdoorsOrHardFloor:
    {
        if (!indoors) return true;
        const int base = ((std::max(1, map.groundGid(tile.x, tile.y)) - 1) % TileGid::kPerRow) + 1;
        return map.getSurfaceType(tile.x, tile.y) != SurfaceType::Carpet && base != TileGid::kWood;
    }
    case Where::Soil:
    {
        if (indoors) return false;
        const int base = ((std::max(1, map.groundGid(tile.x, tile.y)) - 1) % TileGid::kPerRow) + 1;
        return base == TileGid::kGrass || base == TileGid::kGrass2 || base == TileGid::kDirt;
    }
    }
    return false;
}

float BuildSystem::decayPerHour(StructureType type, bool outdoors, bool raining)
{
    const StructureInfo& info = structureInfo(type);
    return info.decayPerHour * (info.wooden && outdoors && raining ? 2.0f : 1.0f);
}

bool BuildSystem::carriesOver(StructureType type, bool sprung)
{
    switch (type)
    {
    case StructureType::Campfire:
    case StructureType::Bedroll:
    case StructureType::Light:
        return false;
    case StructureType::Snare:
        return true; // set again by whoever finds it
    case StructureType::CanAlarm:
        return !sprung;
    default:
        return true;
    }
}

bool BuildSystem::wearsBetweenLives(StructureType type)
{
    // Phase 10: crates and spike boards too (crates go Intact, Damaged,
    // Breaking, gone over three lives).
    return type == StructureType::Workbench || type == StructureType::RainCollector ||
           type == StructureType::StorageCrate || type == StructureType::SpikeBoard;
}
