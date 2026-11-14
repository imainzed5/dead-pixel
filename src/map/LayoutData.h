#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

enum class DistrictType
{
    Wilderness = 0,
    Residential = 1,
    Commercial = 2,
    Industrial = 3
};

enum class BuildingArchetype
{
    Residential,
    Commercial,
    Industrial
};

// Building archetype is derived from footprint width (not saved).
[[nodiscard]] inline BuildingArchetype archetypeForWidth(int width)
{
    if (width <= 9)
    {
        return BuildingArchetype::Residential;
    }
    if (width <= 11)
    {
        return BuildingArchetype::Commercial;
    }
    return BuildingArchetype::Industrial;
}

struct LayoutData
{
    int width = 0;
    int height = 0;

    std::vector<int> ground;
    std::vector<int> collision;
    std::vector<int> surface;
    std::vector<int> district; // per-tile district ID (0 = wilderness)

    struct BuildingRect
    {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
        // BuildingKind as int (world/BuildingKinds.h); -1 until assigned.
        // Old layouts have none and get it computed on load.
        int kind = -1;
    };
    std::vector<BuildingRect> buildings;

    struct DoorPos
    {
        int x = 0;
        int y = 0;
    };
    std::vector<DoorPos> doors;

    struct DistrictDef
    {
        int id = 0;
        int type = 0; // DistrictType as int
        int minX = 0;
        int minY = 0;
        int maxX = 0;
        int maxY = 0;
        std::vector<int> buildingIndices;
        std::vector<int> adjacentDistricts;
    };
    std::vector<DistrictDef> districts;

    glm::ivec2 playerStart{0, 0};

    // The service gate (phase 9): stamped once over the town, for new worlds
    // when they are made and for older ones at first load (world/Gate.h).
    struct GateSite
    {
        bool placed = false;
        int side = 0;                    // gate::Side: 0 east, 1 west, 2 south, 3 north
        std::vector<glm::ivec2> gate;    // the 3 boundary tiles, from the far road edge to the hut side
        glm::ivec2 box{-1, -1};          // the control box (on a post by the hut)
        glm::ivec2 generator{-1, -1};    // under the lean-to between the hut and the wall
        std::vector<glm::ivec2> windows; // the hut's two windows
        int hut = 0;                     // building index + 1
        int district = 0;                // the district it was placed for
        glm::ivec2 yardMin{0, 0};        // the yard round the hut: no scenery grows there
        glm::ivec2 yardMax{-1, -1};
    };
    GateSite gate;
};
