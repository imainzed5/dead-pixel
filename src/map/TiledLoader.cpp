#include "map/TiledLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
std::string toLowerCopy(std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
    return text;
}

std::vector<int> readLayerData(const nlohmann::json& layer)
{
    std::vector<int> data;
    if (!layer.contains("data"))
    {
        return data;
    }

    const nlohmann::json* dataJson = nullptr;
    if (layer["data"].is_array())
    {
        dataJson = &layer["data"];
    }
    else if (layer["data"].is_object() && layer["data"].contains("value") && layer["data"]["value"].is_array())
    {
        // PowerShell ConvertTo-Json can wrap arrays as { value: [...], Count: N }.
        dataJson = &layer["data"]["value"];
    }

    if (dataJson == nullptr)
    {
        return data;
    }

    data.reserve(dataJson->size());
    for (const auto& value : *dataJson)
    {
        data.push_back(value.get<int>());
    }

    return data;
}
}

bool TiledLoader::loadFromFile(const std::string& filePath, TileMap& outMap) const
{
    std::ifstream input(filePath);
    if (!input)
    {
        std::cerr << "Could not open Tiled map file: " << filePath << '\n';
        return false;
    }

    nlohmann::json json;
    input >> json;

    const int mapWidth = json.value("width", 0);
    const int mapHeight = json.value("height", 0);
    const int tileWidth = json.value("tilewidth", 32);
    const int tileHeight = json.value("tileheight", 32);

    if (mapWidth <= 0 || mapHeight <= 0)
    {
        std::cerr << "Invalid map dimensions in Tiled map: " << filePath << '\n';
        return false;
    }

    outMap.resize(mapWidth, mapHeight, tileWidth, tileHeight);

    if (!json.contains("tilesets") || !json["tilesets"].is_array() || json["tilesets"].empty())
    {
        std::cerr << "Map has no tilesets: " << filePath << '\n';
        return false;
    }

    const auto& tilesetJson = json["tilesets"][0];
    TilesetInfo tileset;
    tileset.firstGid = tilesetJson.value("firstgid", 1);
    tileset.tileCount = tilesetJson.value("tilecount", 0);
    tileset.columns = tilesetJson.value("columns", 1);
    tileset.tileWidth = tilesetJson.value("tilewidth", tileWidth);
    tileset.tileHeight = tilesetJson.value("tileheight", tileHeight);
    tileset.imageWidth = tilesetJson.value("imagewidth", tileWidth);
    tileset.imageHeight = tilesetJson.value("imageheight", tileHeight);
    tileset.imagePath = tilesetJson.value("image", std::string{});
    outMap.setTileset(tileset);

    const std::size_t expectedLayerSize = static_cast<std::size_t>(mapWidth * mapHeight);
    std::vector<int> groundLayer(expectedLayerSize, 0);
    std::vector<int> collisionLayer(expectedLayerSize, 0);
    std::vector<int> surfaceLayer(expectedLayerSize, 0);

    if (json.contains("layers") && json["layers"].is_array())
    {
        for (const auto& layer : json["layers"])
        {
            if (layer.value("type", std::string{}) != "tilelayer")
            {
                continue;
            }

            const std::string layerName = toLowerCopy(layer.value("name", std::string{}));
            std::vector<int> data = readLayerData(layer);
            if (data.size() != expectedLayerSize)
            {
                continue;
            }

            if (layerName == "ground")
            {
                groundLayer = std::move(data);
            }
            else if (layerName == "collision")
            {
                collisionLayer = std::move(data);
            }
            else if (layerName == "surface")
            {
                surfaceLayer = std::move(data);
            }
        }
    }

    outMap.setGroundLayer(std::move(groundLayer));
    outMap.setCollisionLayer(std::move(collisionLayer));
    outMap.setSurfaceLayer(std::move(surfaceLayer));

    return true;
}
