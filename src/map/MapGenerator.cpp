#include "map/MapGenerator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

LayoutData MapGenerator::generateLayout(std::uint32_t layoutSeed, int width, int height)
{
    mWidth = width;
    mHeight = height;
    mRng.seed(layoutSeed);

    const std::size_t total = static_cast<std::size_t>(mWidth * mHeight);
    mGround.resize(total);
    mCollision.resize(total);
    mSurface.resize(total);
    mDistrict.assign(total, 0);
    mBuildings.clear();
    mDoorTiles.clear();

    fillBase();
    borderWalls();
    carveRoads();
    placeBuildings();
    addInteriorWalls();
    scatterDecor();
    assignDistricts();

    // Determine canonical player start (center building)
    const int centerX = mWidth / 2;
    const int centerY = mHeight / 2;
    glm::ivec2 playerStart{centerX, centerY};

    // Find building nearest to center for player start
    float bestDist = 1e9f;
    for (const auto& b : mBuildings)
    {
        const float bx = static_cast<float>(b.x + b.w / 2);
        const float by = static_cast<float>(b.y + b.h / 2);
        const float dx = bx - static_cast<float>(centerX);
        const float dy = by - static_cast<float>(centerY);
        const float dist = dx * dx + dy * dy;
        if (dist < bestDist)
        {
            bestDist = dist;
            // Pick an interior floor tile in this building
            for (int ty = b.y + 1; ty < b.y + b.h - 1; ++ty)
            {
                for (int tx = b.x + 1; tx < b.x + b.w - 1; ++tx)
                {
                    if (isFloorTile(tx, ty) && !isDoorTile(tx, ty))
                    {
                        playerStart = glm::ivec2(tx, ty);
                        goto foundStart;
                    }
                }
            }
            foundStart:;
        }
    }

    packLayout();
    mLayout.playerStart = playerStart;
    return mLayout;
}

void MapGenerator::loadLayout(const LayoutData& layout)
{
    mLayout = layout;
    unpackLayout();
}

void MapGenerator::applyToTileMap(TileMap& outMap) const
{
    outMap.resize(mLayout.width, mLayout.height, 32, 32);

    TilesetInfo tileset{};
    tileset.firstGid = 1;
    tileset.tileCount = 16;
    tileset.columns = 4;
    tileset.tileWidth = 64;
    tileset.tileHeight = 32;
    tileset.imageWidth = 256;
    tileset.imageHeight = 128;
    tileset.imagePath = "../sprites/placeholder.png";
    outMap.setTileset(tileset);

    outMap.setGroundLayer(mLayout.ground);
    outMap.setCollisionLayer(mLayout.collision);
    outMap.setSurfaceLayer(mLayout.surface);
    outMap.setDistrictLayer(mLayout.district);
}

void MapGenerator::generateSpawns(SpawnData& outSpawns, std::uint32_t runSeed)
{
    mRng.seed(runSeed);
    selectSpawns(outSpawns);
    outSpawns.playerStart = mLayout.playerStart;
}

void MapGenerator::packLayout()
{
    mLayout.width = mWidth;
    mLayout.height = mHeight;
    mLayout.ground = mGround;
    mLayout.collision = mCollision;
    mLayout.surface = mSurface;
    mLayout.district = mDistrict;

    mLayout.buildings.clear();
    mLayout.buildings.reserve(mBuildings.size());
    for (const auto& b : mBuildings)
    {
        mLayout.buildings.push_back({b.x, b.y, b.w, b.h});
    }

    mLayout.doors.clear();
    mLayout.doors.reserve(mDoorTiles.size());
    for (const auto& d : mDoorTiles)
    {
        mLayout.doors.push_back({d.x, d.y});
    }
}

void MapGenerator::unpackLayout()
{
    mWidth = mLayout.width;
    mHeight = mLayout.height;
    mGround = mLayout.ground;
    mCollision = mLayout.collision;
    mSurface = mLayout.surface;
    mDistrict = mLayout.district;

    mBuildings.clear();
    mBuildings.reserve(mLayout.buildings.size());
    for (const auto& b : mLayout.buildings)
    {
        mBuildings.push_back({b.x, b.y, b.w, b.h});
    }

    mDoorTiles.clear();
    mDoorTiles.reserve(mLayout.doors.size());
    for (const auto& d : mLayout.doors)
    {
        mDoorTiles.emplace_back(d.x, d.y);
    }
}

void MapGenerator::assignDistricts()
{
    const std::size_t total = static_cast<std::size_t>(mWidth * mHeight);
    mDistrict.assign(total, 0); // 0 = Wilderness

    mLayout.districts.clear();

    // Each building gets its own district (ID starting from 1)
    for (int bi = 0; bi < static_cast<int>(mBuildings.size()); ++bi)
    {
        const int districtId = bi + 1;
        const auto& b = mBuildings[static_cast<std::size_t>(bi)];

        // Determine type based on building width
        int districtType;
        if (b.w <= 9)
        {
            districtType = static_cast<int>(DistrictType::Residential);
        }
        else if (b.w <= 11)
        {
            districtType = static_cast<int>(DistrictType::Commercial);
        }
        else
        {
            districtType = static_cast<int>(DistrictType::Industrial);
        }

        // Check if majority of interior is carpet → Residential override
        int carpetCount = 0;
        int floorCount = 0;
        for (int ty = b.y + 1; ty < b.y + b.h - 1; ++ty)
        {
            for (int tx = b.x + 1; tx < b.x + b.w - 1; ++tx)
            {
                if (!inBounds(tx, ty)) continue;
                const int idx = index(tx, ty);
                if (mCollision[idx] == 0)
                {
                    ++floorCount;
                    if (mSurface[idx] == kSurfCarpet)
                    {
                        ++carpetCount;
                    }
                }
            }
        }
        if (floorCount > 0 && carpetCount * 2 > floorCount)
        {
            districtType = static_cast<int>(DistrictType::Residential);
        }

        // Assign district ID to all building tiles (walls + interior)
        for (int ty = b.y; ty < b.y + b.h; ++ty)
        {
            for (int tx = b.x; tx < b.x + b.w; ++tx)
            {
                if (inBounds(tx, ty))
                {
                    mDistrict[static_cast<std::size_t>(index(tx, ty))] = districtId;
                }
            }
        }

        LayoutData::DistrictDef def;
        def.id = districtId;
        def.type = districtType;
        def.minX = b.x;
        def.minY = b.y;
        def.maxX = b.x + b.w - 1;
        def.maxY = b.y + b.h - 1;
        def.buildingIndices.push_back(bi);
        mLayout.districts.push_back(def);
    }

    // Compute adjacency: two districts are adjacent if bounding boxes are within 5 tiles
    constexpr int kAdjacencyThreshold = 5;
    for (std::size_t i = 0; i < mLayout.districts.size(); ++i)
    {
        auto& a = mLayout.districts[i];
        for (std::size_t j = i + 1; j < mLayout.districts.size(); ++j)
        {
            auto& b = mLayout.districts[j];

            const int hGap = std::max(0, std::max(a.minX - b.maxX, b.minX - a.maxX));
            const int vGap = std::max(0, std::max(a.minY - b.maxY, b.minY - a.maxY));

            if (hGap <= kAdjacencyThreshold && vGap <= kAdjacencyThreshold)
            {
                a.adjacentDistricts.push_back(b.id);
                b.adjacentDistricts.push_back(a.id);
            }
        }
    }
}

void MapGenerator::fillBase()
{
    for (int y = 0; y < mHeight; ++y)
    {
        for (int x = 0; x < mWidth; ++x)
        {
            const int idx = index(x, y);
            mGround[idx] = kGidGrass;
            mCollision[idx] = 0;
            mSurface[idx] = kSurfGrass;
        }
    }
}

void MapGenerator::borderWalls()
{
    for (int x = 0; x < mWidth; ++x)
    {
        mGround[index(x, 0)] = kGidWall;
        mCollision[index(x, 0)] = 1;
        mSurface[index(x, 0)] = kSurfDefault;

        mGround[index(x, mHeight - 1)] = kGidWall;
        mCollision[index(x, mHeight - 1)] = 1;
        mSurface[index(x, mHeight - 1)] = kSurfDefault;
    }
    for (int y = 0; y < mHeight; ++y)
    {
        mGround[index(0, y)] = kGidWall;
        mCollision[index(0, y)] = 1;
        mSurface[index(0, y)] = kSurfDefault;

        mGround[index(mWidth - 1, y)] = kGidWall;
        mCollision[index(mWidth - 1, y)] = 1;
        mSurface[index(mWidth - 1, y)] = kSurfDefault;
    }
}

void MapGenerator::carveRoads()
{
    // Horizontal roads: evenly spaced with jitter
    std::uniform_int_distribution<int> hCountDist(3, 4);
    const int hCount = hCountDist(mRng);
    const int hSpacing = (mHeight - 4) / (hCount + 1);
    std::uniform_int_distribution<int> jitterDist(-2, 2);

    for (int i = 1; i <= hCount; ++i)
    {
        int baseY = 2 + hSpacing * i + jitterDist(mRng);
        baseY = std::clamp(baseY, 2, mHeight - 4);
        for (int x = 1; x < mWidth - 1; ++x)
        {
            for (int dy = 0; dy < 2; ++dy)
            {
                const int y = baseY + dy;
                if (y >= 1 && y < mHeight - 1)
                {
                    const int idx = index(x, y);
                    mGround[idx] = kGidGravel;
                    mCollision[idx] = 0;
                    mSurface[idx] = kSurfGravel;
                }
            }
        }
    }

    // Vertical roads: evenly spaced with jitter
    std::uniform_int_distribution<int> vCountDist(3, 4);
    const int vCount = vCountDist(mRng);
    const int vSpacing = (mWidth - 4) / (vCount + 1);

    for (int i = 1; i <= vCount; ++i)
    {
        int baseX = 2 + vSpacing * i + jitterDist(mRng);
        baseX = std::clamp(baseX, 2, mWidth - 4);
        for (int y = 1; y < mHeight - 1; ++y)
        {
            for (int dx = 0; dx < 2; ++dx)
            {
                const int x = baseX + dx;
                if (x >= 1 && x < mWidth - 1)
                {
                    const int idx = index(x, y);
                    mGround[idx] = kGidGravel;
                    mCollision[idx] = 0;
                    mSurface[idx] = kSurfGravel;
                }
            }
        }
    }
}

void MapGenerator::placeBuildings()
{
    // Scan for rectangular regions of grass (city blocks between roads)
    // Then try to place buildings inside each block
    std::uniform_int_distribution<int> buildingsPerBlock(1, 3);
    std::uniform_int_distribution<int> widthDist(7, 14);
    std::uniform_int_distribution<int> heightDist(6, 10);
    std::uniform_int_distribution<int> surfRoll(0, 99);
    std::uniform_int_distribution<int> notchChance(0, 99);
    std::uniform_int_distribution<int> notchSizeDist(2, 3);

    // Find city blocks by flood-filling grass areas
    std::vector<bool> visited(static_cast<std::size_t>(mWidth * mHeight), false);
    std::vector<Rect> blocks;

    for (int y = 2; y < mHeight - 2; ++y)
    {
        for (int x = 2; x < mWidth - 2; ++x)
        {
            if (visited[static_cast<std::size_t>(index(x, y))]) continue;
            if (mGround[index(x, y)] != kGidGrass) { visited[static_cast<std::size_t>(index(x, y))] = true; continue; }

            // Find bounding rect of contiguous grass
            int minX = x, maxX = x, minY = y, maxY = y;
            // Expand right and down from this point while still grass
            while (maxX + 1 < mWidth - 1 && mGround[index(maxX + 1, y)] == kGidGrass && !visited[static_cast<std::size_t>(index(maxX + 1, y))])
                ++maxX;
            // Expand down checking full row
            for (int ty = y; ty < mHeight - 1; ++ty)
            {
                bool fullRow = true;
                for (int tx = minX; tx <= maxX; ++tx)
                {
                    if (mGround[index(tx, ty)] != kGidGrass || visited[static_cast<std::size_t>(index(tx, ty))])
                    {
                        fullRow = false;
                        break;
                    }
                }
                if (fullRow) maxY = ty;
                else break;
            }

            // Mark visited
            for (int ty = minY; ty <= maxY; ++ty)
                for (int tx = minX; tx <= maxX; ++tx)
                    visited[static_cast<std::size_t>(index(tx, ty))] = true;

            const int bw = maxX - minX + 1;
            const int bh = maxY - minY + 1;
            if (bw >= 10 && bh >= 8)
            {
                blocks.push_back({minX, minY, bw, bh});
            }
        }
    }

    // Place buildings in each block
    for (const auto& block : blocks)
    {
        const int count = buildingsPerBlock(mRng);
        int usedX = block.x + 1; // offset from block edge

        for (int b = 0; b < count; ++b)
        {
            int bw = widthDist(mRng);
            int bh = heightDist(mRng);
            bw = std::min(bw, block.w - (usedX - block.x) - 1);
            bh = std::min(bh, block.h - 2);
            if (bw < 6 || bh < 5) break;

            const int bx = usedX;
            const int by = block.y + 1;

            enum class BuildingArchetype
            {
                Residential,
                Commercial,
                Industrial
            };

            BuildingArchetype archetype = BuildingArchetype::Residential;
            if (bw <= 9)
            {
                archetype = BuildingArchetype::Residential;
            }
            else if (bw <= 11)
            {
                archetype = BuildingArchetype::Commercial;
            }
            else
            {
                archetype = BuildingArchetype::Industrial;
            }

            const int wallTile = (archetype == BuildingArchetype::Industrial) ? kGidMetal : kGidWall;

            // Walls
            for (int tx = bx; tx < bx + bw; ++tx)
            {
                for (int ty = by; ty < by + bh; ++ty)
                {
                    if (!inBounds(tx, ty)) continue;
                    const int idx = index(tx, ty);
                    if (tx == bx || tx == bx + bw - 1 || ty == by || ty == by + bh - 1)
                    {
                        mGround[idx] = wallTile;
                        mCollision[idx] = 1;
                        mSurface[idx] = (archetype == BuildingArchetype::Industrial) ? kSurfMetal : kSurfDefault;
                    }
                    else
                    {
                        // Interior floor by building archetype for clearer district identity.
                        const int roll = surfRoll(mRng);
                        if (archetype == BuildingArchetype::Residential)
                        {
                            if (roll < 68)
                            {
                                mGround[idx] = kGidCarpet;
                                mSurface[idx] = kSurfCarpet;
                            }
                            else if (roll < 92)
                            {
                                mGround[idx] = kGidFloor;
                                mSurface[idx] = kSurfDefault;
                            }
                            else
                            {
                                mGround[idx] = kGidDirt;
                                mSurface[idx] = kSurfDefault;
                            }
                        }
                        else if (archetype == BuildingArchetype::Commercial)
                        {
                            if (roll < 14)
                            {
                                mGround[idx] = kGidCarpet;
                                mSurface[idx] = kSurfCarpet;
                            }
                            else if (roll < 82)
                            {
                                mGround[idx] = kGidFloor;
                                mSurface[idx] = kSurfDefault;
                            }
                            else
                            {
                                mGround[idx] = kGidGravel;
                                mSurface[idx] = kSurfGravel;
                            }
                        }
                        else
                        {
                            if (roll < 38)
                            {
                                mGround[idx] = kGidFloor;
                                mSurface[idx] = kSurfDefault;
                            }
                            else if (roll < 78)
                            {
                                mGround[idx] = kGidGravel;
                                mSurface[idx] = kSurfGravel;
                            }
                            else
                            {
                                mGround[idx] = kGidMetal;
                                mSurface[idx] = kSurfMetal;
                            }
                        }
                    }
                }
            }

            // Add occasional corner notch to break box-like footprints and improve silhouette readability.
            if (bw >= 10 && bh >= 8 && notchChance(mRng) < 45)
            {
                const int notchW = notchSizeDist(mRng);
                const int notchH = notchSizeDist(mRng);
                const int corner = std::uniform_int_distribution<int>(0, 3)(mRng);

                int startX = bx;
                int startY = by;
                if (corner == 1 || corner == 3)
                {
                    startX = bx + bw - notchW;
                }
                if (corner == 2 || corner == 3)
                {
                    startY = by + bh - notchH;
                }

                for (int ty = startY; ty < startY + notchH; ++ty)
                {
                    for (int tx = startX; tx < startX + notchW; ++tx)
                    {
                        if (!inBounds(tx, ty)) continue;
                        const int idx = index(tx, ty);
                        mGround[idx] = kGidGrass2;
                        mCollision[idx] = 0;
                        mSurface[idx] = kSurfGrass;
                    }
                }
            }

            // Doorways: one on bottom wall, one on side wall if building is large
            int primaryDoorX = bx + bw / 2;
            int primaryDoorY = by + bh - 1;
            {
                std::uniform_int_distribution<int> doorXDist(bx + 2, bx + bw - 3);
                primaryDoorX = doorXDist(mRng);
                if (inBounds(primaryDoorX, primaryDoorY))
                {
                    mGround[index(primaryDoorX, primaryDoorY)] = kGidFloor;
                    mCollision[index(primaryDoorX, primaryDoorY)] = 0;
                    mSurface[index(primaryDoorX, primaryDoorY)] = kSurfDefault;
                    mDoorTiles.emplace_back(primaryDoorX, primaryDoorY);
                }
                // Clear the tile just outside the door so it's reachable
                if (inBounds(primaryDoorX, primaryDoorY + 1) && mGround[index(primaryDoorX, primaryDoorY + 1)] == kGidGrass)
                {
                    mCollision[index(primaryDoorX, primaryDoorY + 1)] = 0;
                }
            }

            // Second door on right wall for larger buildings
            if (bw >= 10)
            {
                std::uniform_int_distribution<int> doorYDist(by + 2, by + bh - 3);
                const int doorY2 = doorYDist(mRng);
                const int doorX2 = bx + bw - 1;
                if (inBounds(doorX2, doorY2))
                {
                    mGround[index(doorX2, doorY2)] = kGidFloor;
                    mCollision[index(doorX2, doorY2)] = 0;
                    mSurface[index(doorX2, doorY2)] = kSurfDefault;
                    mDoorTiles.emplace_back(doorX2, doorY2);
                }
            }

            // Exterior entrance treatment by archetype to improve district readability.
            auto paintExteriorTile = [&](int x, int y, int gid, int surface)
            {
                if (!inBounds(x, y)) return;
                const int idx = index(x, y);
                if (mCollision[idx] != 0) return;
                mGround[idx] = gid;
                mSurface[idx] = surface;
            };

            if (archetype == BuildingArchetype::Residential)
            {
                paintExteriorTile(primaryDoorX, primaryDoorY + 1, kGidDirt, kSurfDefault);
                paintExteriorTile(primaryDoorX - 1, primaryDoorY + 1, kGidGrass2, kSurfGrass);
                paintExteriorTile(primaryDoorX + 1, primaryDoorY + 1, kGidGrass2, kSurfGrass);
            }
            else if (archetype == BuildingArchetype::Commercial)
            {
                paintExteriorTile(primaryDoorX, primaryDoorY + 1, kGidGravel, kSurfGravel);
                paintExteriorTile(primaryDoorX - 1, primaryDoorY + 1, kGidGravel, kSurfGravel);
                paintExteriorTile(primaryDoorX + 1, primaryDoorY + 1, kGidGravel, kSurfGravel);
            }
            else
            {
                paintExteriorTile(primaryDoorX, primaryDoorY + 1, kGidMetal, kSurfMetal);
                paintExteriorTile(primaryDoorX - 1, primaryDoorY + 1, kGidGravel, kSurfGravel);
                paintExteriorTile(primaryDoorX + 1, primaryDoorY + 1, kGidGravel, kSurfGravel);
            }

            mBuildings.push_back({bx, by, bw, bh});
            usedX = bx + bw + 2; // gap between buildings
        }
    }
}

void MapGenerator::addInteriorWalls()
{
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> jitterDist(-1, 1);

    auto addInteriorDoorTile = [&](int x, int y)
    {
        if (!inBounds(x, y)) return;
        const int idx = index(x, y);
        mGround[idx] = kGidFloor;
        mCollision[idx] = 0;
        mSurface[idx] = kSurfDefault;
        mDoorTiles.emplace_back(x, y);
    };

    for (const auto& bldg : mBuildings)
    {
        if (bldg.w < 9 && bldg.h < 7) continue;

        const bool primaryVertical = bldg.w >= bldg.h;
        if (primaryVertical)
        {
            const int wallX = bldg.x + bldg.w / 2;
            if (wallX <= bldg.x + 1 || wallX >= bldg.x + bldg.w - 2) continue;

            std::uniform_int_distribution<int> gapDist(bldg.y + 2, bldg.y + bldg.h - 3);
            const int gapY = gapDist(mRng);

            for (int y = bldg.y + 1; y < bldg.y + bldg.h - 1; ++y)
            {
                if (y == gapY) continue;
                if (!inBounds(wallX, y)) continue;
                const int idx = index(wallX, y);
                mGround[idx] = kGidWall;
                mCollision[idx] = 1;
                mSurface[idx] = kSurfDefault;
            }
            addInteriorDoorTile(wallX, gapY);

            if (bldg.w >= 12 && bldg.h >= 8)
            {
                int wallY = bldg.y + bldg.h / 2 + jitterDist(mRng);
                wallY = std::clamp(wallY, bldg.y + 2, bldg.y + bldg.h - 3);

                const bool leftHalf = sideDist(mRng) == 0;
                const int startX = leftHalf ? bldg.x + 1 : wallX + 1;
                const int endX = leftHalf ? wallX - 1 : bldg.x + bldg.w - 2;
                if (endX - startX >= 2)
                {
                    std::uniform_int_distribution<int> gapXDist(startX + 1, endX - 1);
                    const int gapX = gapXDist(mRng);

                    for (int x = startX; x <= endX; ++x)
                    {
                        if (x == gapX) continue;
                        if (!inBounds(x, wallY)) continue;
                        const int idx = index(x, wallY);
                        mGround[idx] = kGidWall;
                        mCollision[idx] = 1;
                        mSurface[idx] = kSurfDefault;
                    }
                    addInteriorDoorTile(gapX, wallY);
                }
            }
        }
        else
        {
            const int wallY = bldg.y + bldg.h / 2;
            if (wallY <= bldg.y + 1 || wallY >= bldg.y + bldg.h - 2) continue;

            std::uniform_int_distribution<int> gapDist(bldg.x + 2, bldg.x + bldg.w - 3);
            const int gapX = gapDist(mRng);

            for (int x = bldg.x + 1; x < bldg.x + bldg.w - 1; ++x)
            {
                if (x == gapX) continue;
                if (!inBounds(x, wallY)) continue;
                const int idx = index(x, wallY);
                mGround[idx] = kGidWall;
                mCollision[idx] = 1;
                mSurface[idx] = kSurfDefault;
            }
            addInteriorDoorTile(gapX, wallY);

            if (bldg.h >= 10 && bldg.w >= 8)
            {
                int wallX = bldg.x + bldg.w / 2 + jitterDist(mRng);
                wallX = std::clamp(wallX, bldg.x + 2, bldg.x + bldg.w - 3);

                const bool topHalf = sideDist(mRng) == 0;
                const int startY = topHalf ? bldg.y + 1 : wallY + 1;
                const int endY = topHalf ? wallY - 1 : bldg.y + bldg.h - 2;
                if (endY - startY >= 2)
                {
                    std::uniform_int_distribution<int> gapYDist(startY + 1, endY - 1);
                    const int gapY = gapYDist(mRng);

                    for (int y = startY; y <= endY; ++y)
                    {
                        if (y == gapY) continue;
                        if (!inBounds(wallX, y)) continue;
                        const int idx = index(wallX, y);
                        mGround[idx] = kGidWall;
                        mCollision[idx] = 1;
                        mSurface[idx] = kSurfDefault;
                    }
                    addInteriorDoorTile(wallX, gapY);
                }
            }
        }
    }
}

void MapGenerator::scatterDecor()
{
    std::uniform_int_distribution<int> chance(0, 99);
    for (int y = 2; y < mHeight - 2; ++y)
    {
        for (int x = 2; x < mWidth - 2; ++x)
        {
            const int idx = index(x, y);
            if (mGround[idx] != kGidGrass) continue;

            const int roll = chance(mRng);
            if (roll < 2)
            {
                // Gravel patch
                mGround[idx] = kGidGravel;
                mSurface[idx] = kSurfGravel;
            }
            else if (roll < 3)
            {
                // Water accent (impassable)
                mGround[idx] = kGidWater;
                mCollision[idx] = 1;
                mSurface[idx] = kSurfDefault;
            }
            else if (roll < 8)
            {
                // Dark grass variant
                mGround[idx] = kGidGrass2;
                // Surface stays grass
            }
            else if (roll < 11)
            {
                // Dirt patch
                mGround[idx] = kGidDirt;
                mSurface[idx] = kSurfDefault;
            }
        }
    }
}

bool MapGenerator::isFloorTile(int x, int y) const
{
    if (!inBounds(x, y)) return false;
    const int gid = mGround[index(x, y)];
    return (gid == kGidFloor || gid == kGidCarpet) && mCollision[index(x, y)] == 0;
}

bool MapGenerator::isDoorTile(int x, int y) const
{
    for (const auto& d : mDoorTiles)
    {
        if (d.x == x && d.y == y) return true;
    }
    return false;
}

void MapGenerator::selectSpawns(SpawnData& outSpawns)
{
    outSpawns = SpawnData{};

    // Collect all interior floor tiles per building, tagged with district type
    struct BuildingFloors
    {
        int buildingIdx;
        int districtType; // DistrictType as int (0=Wilderness, 1=Res, 2=Com, 3=Ind)
        std::vector<glm::ivec2> tiles;
        std::vector<glm::ivec2> nearWallTiles;
        std::vector<glm::ivec2> openTiles;
    };
    std::vector<BuildingFloors> buildingFloors;
    buildingFloors.reserve(mBuildings.size());

    for (int bi = 0; bi < static_cast<int>(mBuildings.size()); ++bi)
    {
        const auto& b = mBuildings[static_cast<std::size_t>(bi)];
        BuildingFloors bf;
        bf.buildingIdx = bi;
        bf.districtType = 0;

        // Lookup district type from layout
        const int tileIdx = index(b.x + b.w / 2, b.y + b.h / 2);
        const int distId = mDistrict[static_cast<std::size_t>(tileIdx)];
        for (const auto& def : mLayout.districts)
        {
            if (def.id == distId)
            {
                bf.districtType = def.type;
                break;
            }
        }

        for (int y = b.y + 1; y < b.y + b.h - 1; ++y)
        {
            for (int x = b.x + 1; x < b.x + b.w - 1; ++x)
            {
                if (isFloorTile(x, y) && !isDoorTile(x, y))
                {
                    const glm::ivec2 tile(x, y);
                    bf.tiles.push_back(tile);

                    bool nearWall = false;
                    constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& d : kDirs)
                    {
                        const int nx = x + d[0];
                        const int ny = y + d[1];
                        if (!inBounds(nx, ny)) continue;
                        if (mCollision[index(nx, ny)] != 0)
                        {
                            nearWall = true;
                            break;
                        }
                    }

                    if (nearWall)
                    {
                        bf.nearWallTiles.push_back(tile);
                    }
                    else
                    {
                        bf.openTiles.push_back(tile);
                    }
                }
            }
        }
        if (!bf.tiles.empty())
        {
            std::shuffle(bf.tiles.begin(), bf.tiles.end(), mRng);
            buildingFloors.push_back(std::move(bf));
        }
    }

    std::shuffle(buildingFloors.begin(), buildingFloors.end(), mRng);

    // Player start: building near center
    const int centerX = mWidth / 2;
    const int centerY = mHeight / 2;
    int playerBuildingIdx = -1;

    // Sort buildings by distance to center for player start selection
    std::vector<std::size_t> buildingsByDist(buildingFloors.size());
    std::iota(buildingsByDist.begin(), buildingsByDist.end(), 0);
    std::sort(buildingsByDist.begin(), buildingsByDist.end(),
        [&](std::size_t a, std::size_t b)
        {
            const auto& ba = mBuildings[static_cast<std::size_t>(buildingFloors[a].buildingIdx)];
            const auto& bb = mBuildings[static_cast<std::size_t>(buildingFloors[b].buildingIdx)];
            const float da = static_cast<float>((ba.x + ba.w / 2 - centerX) * (ba.x + ba.w / 2 - centerX) +
                                                 (ba.y + ba.h / 2 - centerY) * (ba.y + ba.h / 2 - centerY));
            const float db = static_cast<float>((bb.x + bb.w / 2 - centerX) * (bb.x + bb.w / 2 - centerX) +
                                                 (bb.y + bb.h / 2 - centerY) * (bb.y + bb.h / 2 - centerY));
            return da < db;
        });

    if (!buildingsByDist.empty())
    {
        const auto& bf = buildingFloors[buildingsByDist[0]];
        outSpawns.playerStart = bf.tiles[0];
        playerBuildingIdx = bf.buildingIdx;
    }
    else
    {
        // Fallback: center of map
        outSpawns.playerStart = glm::ivec2(centerX, centerY);
    }

    const auto playerPos = outSpawns.playerStart;
    auto distFromPlayer = [&](const glm::ivec2& p) -> float
    {
        const float dx = static_cast<float>(p.x - playerPos.x);
        const float dy = static_cast<float>(p.y - playerPos.y);
        return std::sqrt(dx * dx + dy * dy);
    };

    // Keep key spawn categories off the same tile so one interactable doesn't hide another.
    std::vector<glm::ivec2> reservedTiles;
    reservedTiles.reserve(256);

    auto isReservedTile = [&](const glm::ivec2& tile) -> bool
    {
        return std::any_of(
            reservedTiles.begin(),
            reservedTiles.end(),
            [&](const glm::ivec2& existing)
            {
                return existing.x == tile.x && existing.y == tile.y;
            });
    };

    auto reserveTile = [&](const glm::ivec2& tile)
    {
        if (!isReservedTile(tile))
        {
            reservedTiles.push_back(tile);
        }
    };

    reserveTile(outSpawns.playerStart);

    // Collect outdoor passable tiles for zombie outdoor spawns
    std::vector<glm::ivec2> outdoorTiles;
    for (int y = 2; y < mHeight - 2; y += 3) // sample every 3 tiles for speed
    {
        for (int x = 2; x < mWidth - 2; x += 3)
        {
            if (mCollision[index(x, y)] == 0 && mGround[index(x, y)] != kGidFloor)
            {
                outdoorTiles.emplace_back(x, y);
            }
        }
    }
    std::shuffle(outdoorTiles.begin(), outdoorTiles.end(), mRng);

    // Scale spawn counts by map area (baseline 150*100 = 15000)
    const float areaScale = static_cast<float>(mWidth * mHeight) / 15000.0f;

    // Zombie spawns: slightly reduced baseline for early-run survivability
    const int baseZombie = std::uniform_int_distribution<int>(24, 42)(mRng);
    const int zombieCount = static_cast<int>(static_cast<float>(baseZombie) * areaScale);
    const int indoorZombies = zombieCount * 6 / 10;
    const int outdoorZombies = zombieCount - indoorZombies;

    // Indoor zombies (not in player's building)
    {
        int placed = 0;
        for (const auto& bf : buildingFloors)
        {
            if (placed >= indoorZombies) break;
            if (bf.buildingIdx == playerBuildingIdx) continue;
            for (const auto& t : bf.tiles)
            {
                if (placed >= indoorZombies) break;
                if (distFromPlayer(t) < 18.0f) continue;
                outSpawns.zombieSpawns.push_back(t);
                ++placed;
                break; // 1 per building first pass
            }
        }
        // Second pass: fill remaining from larger buildings
        for (const auto& bf : buildingFloors)
        {
            if (placed >= indoorZombies) break;
            if (bf.buildingIdx == playerBuildingIdx) continue;
            for (std::size_t ti = 1; ti < bf.tiles.size() && placed < indoorZombies; ++ti)
            {
                if (distFromPlayer(bf.tiles[ti]) < 18.0f) continue;
                outSpawns.zombieSpawns.push_back(bf.tiles[ti]);
                ++placed;
            }
        }
    }

    // Outdoor zombies
    {
        int placed = 0;
        for (const auto& t : outdoorTiles)
        {
            if (placed >= outdoorZombies) break;
            if (distFromPlayer(t) < 24.0f) continue;
            outSpawns.zombieSpawns.push_back(t);
            ++placed;
        }
    }

    // Helper to pick N tiles from buildings (round-robin, avoiding player building)
    // preferredType: if >= 0, buildings of that district type get 3x weight (visited 3x more often)
    enum class SpawnTileBias
    {
        Any,
        NearWall,
        OpenSpace
    };

    auto pickBuildingTiles = [&](int count, std::vector<glm::ivec2>& out,
                                 bool avoidPlayerBuilding = false,
                                 int preferredType = -1,
                                 bool avoidReserved = true,
                                 SpawnTileBias bias = SpawnTileBias::Any)
    {
        auto sourceTiles = [&](const BuildingFloors& bf) -> const std::vector<glm::ivec2>&
        {
            if (bias == SpawnTileBias::NearWall)
            {
                if (!bf.nearWallTiles.empty()) return bf.nearWallTiles;
                if (!bf.openTiles.empty()) return bf.openTiles;
                return bf.tiles;
            }
            if (bias == SpawnTileBias::OpenSpace)
            {
                if (!bf.openTiles.empty()) return bf.openTiles;
                if (!bf.nearWallTiles.empty()) return bf.nearWallTiles;
                return bf.tiles;
            }
            return bf.tiles;
        };

        // Build a weighted order: preferred-type buildings appear 3x
        std::vector<std::size_t> order;
        order.reserve(buildingFloors.size() * 3);
        for (std::size_t bi = 0; bi < buildingFloors.size(); ++bi)
        {
            if (avoidPlayerBuilding && buildingFloors[bi].buildingIdx == playerBuildingIdx) continue;
            const int repeats = (preferredType >= 0 && buildingFloors[bi].districtType == preferredType) ? 3 : 1;
            for (int r = 0; r < repeats; ++r)
                order.push_back(bi);
        }
        std::shuffle(order.begin(), order.end(), mRng);

        std::vector<std::size_t> cursor(buildingFloors.size(), 0);
        int placed = 0;
        for (int pass = 0; pass < 5 && placed < count; ++pass)
        {
            for (std::size_t oi = 0; oi < order.size() && placed < count; ++oi)
            {
                const std::size_t bi = order[oi];
                auto& bf = buildingFloors[bi];
                const auto& source = sourceTiles(bf);
                if (source.empty())
                {
                    continue;
                }

                while (cursor[bi] < source.size() && avoidReserved && isReservedTile(source[cursor[bi]]))
                {
                    ++cursor[bi];
                }

                if (cursor[bi] >= source.size())
                {
                    continue;
                }

                const glm::ivec2 chosen = source[cursor[bi]];
                out.push_back(chosen);
                if (avoidReserved)
                {
                    reserveTile(chosen);
                }
                ++cursor[bi];
                ++placed;
            }
        }
    };

    // Item spawns inside buildings (scaled by area, district-biased)
    // Residential→food/sleep, Commercial→weapons/medical, Industrial→materials/bandages
    const int foodCount = static_cast<int>(std::uniform_int_distribution<int>(14, 20)(mRng) * areaScale);
    pickBuildingTiles(
        foodCount,
        outSpawns.foodSpawns,
        false,
        static_cast<int>(DistrictType::Residential),
        true,
        SpawnTileBias::OpenSpace);

    const int waterCount = static_cast<int>(std::uniform_int_distribution<int>(10, 16)(mRng) * areaScale);
    pickBuildingTiles(
        waterCount,
        outSpawns.waterSpawns,
        false,
        static_cast<int>(DistrictType::Residential),
        true,
        SpawnTileBias::OpenSpace);

    const int weaponCount = static_cast<int>(std::uniform_int_distribution<int>(6, 10)(mRng) * areaScale);
    pickBuildingTiles(
        weaponCount,
        outSpawns.weaponSpawns,
        false,
        static_cast<int>(DistrictType::Commercial),
        true,
        SpawnTileBias::NearWall);

    const int bandageCount = static_cast<int>(std::uniform_int_distribution<int>(6, 10)(mRng) * areaScale);
    pickBuildingTiles(
        bandageCount,
        outSpawns.bandageSpawns,
        false,
        static_cast<int>(DistrictType::Industrial),
        true,
        SpawnTileBias::NearWall);

    const int medicalCacheCount = std::max(2, static_cast<int>(std::uniform_int_distribution<int>(2, 4)(mRng) * areaScale));
    pickBuildingTiles(
        medicalCacheCount,
        outSpawns.medicalCacheSpawns,
        true,
        static_cast<int>(DistrictType::Commercial),
        true,
        SpawnTileBias::NearWall);

    const int supplyCacheCount = std::max(3, static_cast<int>(std::uniform_int_distribution<int>(3, 5)(mRng) * areaScale));
    pickBuildingTiles(
        supplyCacheCount,
        outSpawns.supplyCacheSpawns,
        true,
        static_cast<int>(DistrictType::Industrial),
        true,
        SpawnTileBias::NearWall);

    // Sleep spots: prefer residential buildings
    const int sleepCount = static_cast<int>(std::uniform_int_distribution<int>(5, 8)(mRng) * areaScale);
    pickBuildingTiles(
        sleepCount,
        outSpawns.sleepSpawns,
        false,
        static_cast<int>(DistrictType::Residential),
        true,
        SpawnTileBias::NearWall);

    // Home furniture dressing pass: mostly residential, with a near-wall bias.
    const int homeFurnitureCount = static_cast<int>(std::uniform_int_distribution<int>(18, 30)(mRng) * areaScale);
    const int homeNearWallCount = (homeFurnitureCount * 7) / 10;
    pickBuildingTiles(
        homeNearWallCount,
        outSpawns.homeFurnitureSpawns,
        false,
        static_cast<int>(DistrictType::Residential),
        true,
        SpawnTileBias::NearWall);
    pickBuildingTiles(
        homeFurnitureCount - homeNearWallCount,
        outSpawns.homeFurnitureSpawns,
        false,
        static_cast<int>(DistrictType::Residential),
        true,
        SpawnTileBias::OpenSpace);

    // Containers
    const int containerCount = static_cast<int>(std::uniform_int_distribution<int>(10, 16)(mRng) * areaScale);
    pickBuildingTiles(containerCount, outSpawns.containerSpawns, true, -1, true, SpawnTileBias::NearWall);

    // Indoor decor: ambient utility and distress dressing inside buildings
    const int indoorDecorCount = static_cast<int>(std::uniform_int_distribution<int>(12, 24)(mRng) * areaScale);
    pickBuildingTiles(indoorDecorCount, outSpawns.decorIndoorSpawns, false, -1, true, SpawnTileBias::NearWall);

    // Outdoor decor: graves, debris scattered across the map
    {
        const int outdoorDecorCount = static_cast<int>(std::uniform_int_distribution<int>(15, 30)(mRng) * areaScale);
        int placed = 0;
        for (const auto& t : outdoorTiles)
        {
            if (placed >= outdoorDecorCount) break;
            if (distFromPlayer(t) < 10.0f) continue;
            outSpawns.decorOutdoorSpawns.push_back(t);
            ++placed;
        }
    }

    // Blood splatters: scattered outdoors
    {
        const int bloodCount = static_cast<int>(std::uniform_int_distribution<int>(10, 25)(mRng) * areaScale);
        int placed = 0;
        for (auto it = outdoorTiles.rbegin(); it != outdoorTiles.rend() && placed < bloodCount; ++it)
        {
            outSpawns.bloodSpawns.push_back(*it);
            ++placed;
        }
    }
}
