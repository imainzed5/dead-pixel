// Outdoor scenery: it only grows outdoors, never closes a path, is the same
// every time for the same town, and every sprite it names is in the atlas.
#include "TestFramework.h"

#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "map/MapGenerator.h"
#include "map/TileIds.h"
#include "map/TileMap.h"
#include "rendering/SceneryView.h"
#include "world/Scenery.h"

namespace
{
TileMap townMap(unsigned int seed)
{
    MapGenerator generator;
    generator.generateLayout(seed);
    TileMap map;
    generator.applyToTileMap(map);
    return map;
}
} // namespace

DP_TEST(scenery_grows_outdoors_and_blockers_stand_alone)
{
    const TileMap map = townMap(1234);
    const std::vector<SceneryPiece> pieces = scenery::plant(map);
    DP_CHECK(pieces.size() > 200);

    std::set<std::pair<int, int>> blockers;
    int trees = 0;
    for (const SceneryPiece& piece : pieces)
    {
        const glm::ivec2 tile = scenery::tileOf(piece);
        DP_CHECK(map.inBounds(tile.x, tile.y));
        DP_CHECK(!map.isIndoors(tile.x, tile.y));
        DP_CHECK(!map.isSolid(tile.x, tile.y));
        DP_CHECK(!(piece.flat && piece.blocker));
        if (piece.blocker)
        {
            DP_CHECK(blockers.insert({tile.x, tile.y}).second);
        }
        if (piece.tall)
        {
            ++trees;
        }
    }
    DP_CHECK(trees > 40);

    // Every tile round a blocker is open and holds no other blocker, so one
    // can always be walked round.
    for (const auto& [x, y] : blockers)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx == 0 && dy == 0) continue;
                DP_CHECK(!map.isSolid(x + dx, y + dy));
                DP_CHECK(!map.isIndoors(x + dx, y + dy));
                DP_CHECK(blockers.count({x + dx, y + dy}) == 0);
            }
        }
    }
}

DP_TEST(scenery_is_the_same_every_time_and_blocks_its_tile)
{
    TileMap map = townMap(77);
    const std::vector<SceneryPiece> first = scenery::plant(map);
    const std::vector<SceneryPiece> second = scenery::plant(map);
    DP_CHECK_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size() && i < second.size(); ++i)
    {
        DP_CHECK(first[i].position == second[i].position);
        DP_CHECK(std::string(first[i].sprite) == second[i].sprite);
    }

    for (const SceneryPiece& piece : first)
    {
        if (!piece.blocker) continue;
        const glm::ivec2 tile = scenery::tileOf(piece);
        DP_CHECK(!map.isSolid(tile.x, tile.y));
        map.setScenery(tile.x, tile.y, true);
        DP_CHECK(map.isSolid(tile.x, tile.y));
        DP_CHECK(!map.blocksSight(tile.x, tile.y));
        break;
    }
    map.clearScenery();
    DP_CHECK(scenery::plant(map).size() == first.size());
}

DP_TEST(the_atlas_has_every_scenery_sprite_and_the_tileset_its_overlays)
{
    std::ifstream file(std::string(DEAD_PIXEL_ASSETS_DIR) + "/sprites/spritesheet.json");
    DP_CHECK(file.good());
    const nlohmann::json atlas = nlohmann::json::parse(file);
    const auto& sprites = atlas["sprites"];
    for (unsigned int seed : {1234u, 77u, 9u})
    {
        const TileMap map = townMap(seed);
        for (const SceneryPiece& piece : scenery::plant(map))
        {
            DP_CHECK(sprites.contains(piece.sprite));
        }
    }

    // Overlay tiles sit in the rows after the ground variants.
    const TileMap map = townMap(1234);
    glm::vec4 uv;
    DP_CHECK(map.gidToUvRect(TileGid::fringeGid(3, 3), uv));
    DP_CHECK(map.gidToUvRect(TileGid::shoreGid(0, 0), uv));
    DP_CHECK(map.gidToUvRect(TileGid::shadowGid(3, true), uv));
    DP_CHECK_EQ(TileGid::shadowGid(3, true), 1 + 6 * 16 + 7);
}

DP_TEST(a_tree_fades_only_when_it_stands_in_front_of_the_player)
{
    const glm::ivec2 player(10, 10);
    DP_CHECK(SceneryView::hidesPlayer(glm::ivec2(11, 11), player));
    DP_CHECK(SceneryView::hidesPlayer(glm::ivec2(12, 12), player));
    DP_CHECK(SceneryView::hidesPlayer(glm::ivec2(11, 10), player));
    DP_CHECK(!SceneryView::hidesPlayer(player, player));
    DP_CHECK(!SceneryView::hidesPlayer(glm::ivec2(9, 9), player));
    DP_CHECK(!SceneryView::hidesPlayer(glm::ivec2(12, 10), player));
}
