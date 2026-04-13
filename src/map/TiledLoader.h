#pragma once

#include <string>

#include "map/TileMap.h"

class TiledLoader
{
public:
    bool loadFromFile(const std::string& filePath, TileMap& outMap) const;
};
