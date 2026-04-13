#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"
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
    std::vector<glm::ivec2> containerSpawns;
    std::vector<glm::ivec2> decorIndoorSpawns;   // shelves, beds, blood inside buildings
    std::vector<glm::ivec2> decorOutdoorSpawns;  // graves, debris outside
    std::vector<glm::ivec2> bloodSpawns;         // blood splatters (outdoor)
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
    void scatterDecor();
    void assignDistricts();
    void selectSpawns(SpawnData& outSpawns);

    void packLayout();
    void unpackLayout();

    int index(int x, int y) const { return y * mWidth + x; }
    bool inBounds(int x, int y) const { return x >= 0 && x < mWidth && y >= 0 && y < mHeight; }
    bool isFloorTile(int x, int y) const;
    bool isDoorTile(int x, int y) const;

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

    static constexpr int kGidGrass = 9;
    static constexpr int kGidGrass2 = 10;
    static constexpr int kGidDirt = 4;
    static constexpr int kGidFloor = 2;
    static constexpr int kGidCarpet = 6;
    static constexpr int kGidGravel = 3;
    static constexpr int kGidWater = 5;
    static constexpr int kGidWall = 7;
    static constexpr int kGidMetal = 8;

    static constexpr int kSurfDefault = 0;
    static constexpr int kSurfCarpet = 1;
    static constexpr int kSurfGravel = 2;
    static constexpr int kSurfGrass = 3;
    static constexpr int kSurfMetal = 4;
};
