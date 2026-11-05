#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"
#include "map/TileIds.h"
#include "map/TileMap.h"

struct SpawnData
{
    glm::ivec2 playerStart{0, 0};
    std::vector<glm::ivec2> zombieSpawns;
    std::vector<glm::ivec2> foodSpawns;
    std::vector<glm::ivec2> waterSpawns;
    std::vector<glm::ivec2> weaponSpawns;
    std::vector<glm::ivec2> bandageSpawns;
    std::vector<glm::ivec2> medicalCacheSpawns;
    std::vector<glm::ivec2> supplyCacheSpawns;
    std::vector<glm::ivec2> sleepSpawns;
    std::vector<glm::ivec2> homeFurnitureSpawns; // visual furniture pass for lived-in interiors
    std::vector<glm::ivec2> containerSpawns;
    std::vector<glm::ivec2> decorIndoorSpawns;   // shelves, beds, blood inside buildings
    std::vector<glm::ivec2> decorOutdoorSpawns;  // graves, debris outside
    std::vector<glm::ivec2> bloodSpawns;         // blood splatters (outdoor)
    std::vector<glm::ivec2> vehicleSpawns;       // narrow M8 sedan slice anchors
};

class MapGenerator
{
public:
    /// Generate permanent world layout from a seed. Stores result internally.
    LayoutData generateLayout(std::uint32_t layoutSeed, int width = 150, int height = 100);

    /// Restore internal state from a previously saved layout.
    void loadLayout(const LayoutData& layout);

    /// Apply the current layout to a TileMap (sets tile layers + tileset).
    void applyToTileMap(TileMap& outMap) const;

    /// Generate run-variant spawn points against the current layout.
    void generateSpawns(SpawnData& outSpawns, std::uint32_t runSeed);

    /// Access the current layout data (for saving).
    [[nodiscard]] const LayoutData& layoutData() const { return mLayout; }

private:
    struct Rect
    {
        int x, y, w, h;
    };

    void fillBase();
    void borderWalls();
    void carveRoads();
    void placeBuildings();
    void addInteriorWalls();
    void assignRoomFloors();
    void paveSidewalks();
    void scatterDecor();
    void assignDistricts();
    void selectSpawns(SpawnData& outSpawns);

    void packLayout();
    void unpackLayout();

    int index(int x, int y) const { return y * mWidth + x; }
    bool inBounds(int x, int y) const { return x >= 0 && x < mWidth && y >= 0 && y < mHeight; }
    bool isFloorTile(int x, int y) const;
    bool isDoorTile(int x, int y) const;
    bool isNextToDoor(int x, int y) const;
    int buildingIndexAt(int x, int y) const;

    int mWidth = 0;
    int mHeight = 0;
    std::mt19937 mRng;

    std::vector<int> mGround;
    std::vector<int> mCollision;
    std::vector<int> mSurface;
    std::vector<int> mDistrict;

    std::vector<Rect> mBuildings;
    std::vector<glm::ivec2> mDoorTiles;

    LayoutData mLayout;

    static constexpr int kGidGrass = TileGid::kGrass;
    static constexpr int kGidGrass2 = TileGid::kGrass2;
    static constexpr int kGidDirt = TileGid::kDirt;
    static constexpr int kGidFloor = TileGid::kFloor;
    static constexpr int kGidCarpet = TileGid::kCarpet;
    static constexpr int kGidGravel = TileGid::kGravel;
    static constexpr int kGidWater = TileGid::kWater;
    static constexpr int kGidWall = TileGid::kWall;
    static constexpr int kGidMetal = TileGid::kMetal;
    static constexpr int kGidWood = TileGid::kWood;
    static constexpr int kGidTileFloor = TileGid::kTileFloor;
    static constexpr int kGidAsphalt = TileGid::kAsphalt;
    static constexpr int kGidSidewalk = TileGid::kSidewalk;

    static constexpr int kSurfDefault = 0;
    static constexpr int kSurfCarpet = 1;
    static constexpr int kSurfGravel = 2;
    static constexpr int kSurfGrass = 3;
    static constexpr int kSurfMetal = 4;
};
