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
};
