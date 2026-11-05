#include "rendering/StructureView.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "map/TileIds.h"
#include "map/TileMap.h"
#include "rendering/Camera.h"
#include "rendering/SpriteSheet.h"

namespace
{
constexpr float kTile = 32.0f;
constexpr float kIsoTileW = 64.0f;
constexpr float kIsoTileH = 32.0f;

const StructureTile kEmptyTile{};

StructureMaterial materialFor(BuildingArchetype archetype)
{
    switch (archetype)
    {
    case BuildingArchetype::Residential:
        return StructureMaterial::Plaster;
    case BuildingArchetype::Commercial:
        return StructureMaterial::Brick;
    case BuildingArchetype::Industrial:
        return StructureMaterial::Metal;
    }
    return StructureMaterial::Plaster;
}

std::size_t mat(StructureMaterial material)
{
    return static_cast<std::size_t>(material);
}

std::uint32_t hashTile(int x, int y)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093u ^ static_cast<std::uint32_t>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}
} // namespace

StructureSprites StructureSprites::fromSheet(const SpriteSheet& sheet)
{
    StructureSprites s;
    static constexpr const char* kMaterials[3] = {"plaster", "brick", "metal"};
    static constexpr const char* kRoofs[3] = {"residential", "commercial", "industrial"};
    for (std::size_t i = 0; i < 3; ++i)
    {
        const std::string base = std::string("wall_") + kMaterials[i];
        s.wall[i] = sheet.uvRect(base);
        s.wallLow[i] = sheet.uvRect(base + "_low");
        s.windowLeft[i] = sheet.uvRect(base + "_window_l");
        s.windowRight[i] = sheet.uvRect(base + "_window_r");
        s.doorLeft[i] = sheet.uvRect(base + "_door_l");
        s.doorRight[i] = sheet.uvRect(base + "_door_r");
        s.doorClosedLeft[i] = sheet.uvRect(base + "_doorclosed_l");
        s.doorClosedRight[i] = sheet.uvRect(base + "_doorclosed_r");
        s.doorBrokenLeft[i] = sheet.uvRect(base + "_doorbroken_l");
        s.doorBrokenRight[i] = sheet.uvRect(base + "_doorbroken_r");
        s.roof[i] = sheet.uvRect(std::string("roof_") + kRoofs[i]);
        s.boardsLeft[i] = sheet.uvRect("door_boards" + std::to_string(i + 1) + "_l");
        s.boardsRight[i] = sheet.uvRect("door_boards" + std::to_string(i + 1) + "_r");
    }
    s.boardsLowLeft = sheet.uvRect("door_boardslow_l");
    s.boardsLowRight = sheet.uvRect("door_boardslow_r");
    return s;
}

void StructureView::build(const TileMap& map, const LayoutData& layout)
{
    mWidth = map.width();
    mHeight = map.height();
    mTiles.assign(static_cast<std::size_t>(std::max(0, mWidth * mHeight)), StructureTile{});

    const std::size_t buildingCount = layout.buildings.size();
    mBuildingMaterial.assign(buildingCount + 1, StructureMaterial::Brick);
    mRoofAlpha.assign(buildingCount + 1, 1.0f);
    for (std::size_t i = 0; i < buildingCount; ++i)
    {
        mBuildingMaterial[i + 1] = materialFor(archetypeForWidth(layout.buildings[i].w));
    }

    // Kinds and building ids.
    for (int y = 0; y < mHeight; ++y)
    {
        for (int x = 0; x < mWidth; ++x)
        {
            StructureTile& t = mTiles[index(x, y)];
            t.building = map.getBuildingId(x, y);
            if (map.isSolid(x, y) && TileGid::isWallGid(map.groundGid(x, y)))
            {
                t.kind = StructureKind::Wall;
            }
            const int b = (t.building > 0 && static_cast<std::size_t>(t.building) <= buildingCount) ? t.building : 0;
            t.material = mBuildingMaterial[static_cast<std::size_t>(b)];
        }
    }
    for (const auto& door : layout.doors)
    {
        if (inBounds(door.x, door.y) && !map.isSolid(door.x, door.y))
        {
            mTiles[index(door.x, door.y)].kind = StructureKind::Doorway;
        }
    }

    auto wallLike = [&](int x, int y)
    {
        return inBounds(x, y) && mTiles[index(x, y)].kind != StructureKind::None;
    };
    auto isDoorway = [&](int x, int y)
    {
        return inBounds(x, y) && mTiles[index(x, y)].kind == StructureKind::Doorway;
    };

    // Runs, perimeter and windows.
    for (int y = 0; y < mHeight; ++y)
    {
        for (int x = 0; x < mWidth; ++x)
        {
            StructureTile& t = mTiles[index(x, y)];
            if (t.kind == StructureKind::None)
            {
                continue;
            }

            const bool alongX = wallLike(x - 1, y) || wallLike(x + 1, y);
            const bool alongY = wallLike(x, y - 1) || wallLike(x, y + 1);
            if (alongX && !alongY)
            {
                t.run = StructureRun::AlongX;
            }
            else if (alongY && !alongX)
            {
                t.run = StructureRun::AlongY;
            }
            else
            {
                t.run = t.kind == StructureKind::Doorway ? StructureRun::AlongX : StructureRun::Corner;
            }

            t.perimeter = t.building == 0;
            for (int dy = -1; dy <= 1 && !t.perimeter; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (!inBounds(nx, ny) || mTiles[index(nx, ny)].building != t.building)
                    {
                        t.perimeter = true;
                        break;
                    }
                }
            }

            if (t.kind == StructureKind::Wall && t.perimeter && t.building > 0 &&
                t.material != StructureMaterial::Metal && t.run != StructureRun::Corner)
            {
                const int along = t.run == StructureRun::AlongX ? x : y;
                const bool nearDoor = t.run == StructureRun::AlongX
                    ? (isDoorway(x - 1, y) || isDoorway(x + 1, y))
                    : (isDoorway(x, y - 1) || isDoorway(x, y + 1));
                const bool flanked = t.run == StructureRun::AlongX
                    ? (wallLike(x - 1, y) && wallLike(x + 1, y))
                    : (wallLike(x, y - 1) && wallLike(x, y + 1));
                t.window = flanked && !nearDoor && along % 3 == 1 && (hashTile(x, y) % 4) != 0;
            }
        }
    }
}

int StructureView::openBuilding(const glm::ivec2& playerTile) const
{
    if (!inBounds(playerTile.x, playerTile.y))
    {
        return 0;
    }
    const int here = mTiles[index(playerTile.x, playerTile.y)].building;
    if (here > 0)
    {
        return here;
    }
    // Standing right outside a doorway lets you look in.
    constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& d : kDirs)
    {
        const int nx = playerTile.x + d[0];
        const int ny = playerTile.y + d[1];
        if (inBounds(nx, ny))
        {
            const StructureTile& n = mTiles[index(nx, ny)];
            if (n.kind == StructureKind::Doorway && n.building > 0)
            {
                return n.building;
            }
        }
    }
    return 0;
}

void StructureView::update(float dtSeconds, const glm::ivec2& playerTile)
{
    const int open = openBuilding(playerTile);
    const float step = kRoofFadePerSecond * std::max(0.0f, dtSeconds);
    for (std::size_t b = 1; b < mRoofAlpha.size(); ++b)
    {
        const float target = static_cast<int>(b) == open ? 0.0f : 1.0f;
        float& alpha = mRoofAlpha[b];
        alpha = alpha < target ? std::min(target, alpha + step) : std::max(target, alpha - step);
    }
}

void StructureView::snapRoofs(const glm::ivec2& playerTile)
{
    const int open = openBuilding(playerTile);
    for (std::size_t b = 1; b < mRoofAlpha.size(); ++b)
    {
        mRoofAlpha[b] = static_cast<int>(b) == open ? 0.0f : 1.0f;
    }
}

const StructureTile& StructureView::tile(int tileX, int tileY) const
{
    return inBounds(tileX, tileY) ? mTiles[index(tileX, tileY)] : kEmptyTile;
}

float StructureView::roofAlpha(int building) const
{
    if (building <= 0 || static_cast<std::size_t>(building) >= mRoofAlpha.size())
    {
        return 0.0f;
    }
    return mRoofAlpha[static_cast<std::size_t>(building)];
}

bool StructureView::isHiddenAt(int tileX, int tileY) const
{
    const StructureTile& t = tile(tileX, tileY);
    if (t.building <= 0)
    {
        return false;
    }
    // Doorways and walls on the outside edge are seen from the street.
    if (t.kind != StructureKind::None && t.perimeter)
    {
        return false;
    }
    return roofAlpha(t.building) >= 0.999f;
}

bool StructureView::coversGround(int tileX, int tileY) const
{
    return tile(tileX, tileY).kind == StructureKind::Wall;
}

bool StructureView::isCutAway(const glm::ivec2& wall, const glm::ivec2& player) const
{
    const int depthDelta = (wall.x + wall.y) - (player.x + player.y);
    if (depthDelta <= 0)
    {
        return false; // behind the player or level with them
    }
    const int wallBuilding = tile(wall.x, wall.y).building;
    const int playerBuilding = tile(player.x, player.y).building;
    if (wallBuilding > 0 && wallBuilding == playerBuilding)
    {
        return true; // open up the front of the building you are in
    }
    const int lateral = (wall.x - wall.y) - (player.x - player.y);
    return depthDelta <= kCutDepth && std::abs(lateral) <= kCutWidth;
}

void StructureView::collect(const Camera& camera, const glm::ivec2& playerTile, const StructureSprites& sprites,
                            std::vector<StaticSprite>& out, const TileMap* doors) const
{
    const float zoom = camera.zoom();
    const glm::vec2 viewport = camera.viewportSize();
    const float tileW = kIsoTileW * zoom;
    const float tileH = kIsoTileH * zoom;
    const float wallH = kWallHeight * zoom;
    const float lowH = kWallLowHeight * zoom;

    for (int y = 0; y < mHeight; ++y)
    {
        for (int x = 0; x < mWidth; ++x)
        {
            const StructureTile& t = mTiles[index(x, y)];
            if (t.kind == StructureKind::None && t.building == 0)
            {
                continue;
            }

            const glm::vec2 ground = camera.worldToScreen(glm::vec2(
                static_cast<float>(x) * kTile + kTile * 0.5f,
                static_cast<float>(y) * kTile + kTile * 0.5f));
            const float left = std::round(ground.x - tileW * 0.5f);
            const float top = std::round(ground.y - tileH * 0.5f);
            if (left + tileW < 0.0f || left > viewport.x || top - wallH > viewport.y || top + tileH < 0.0f)
            {
                continue;
            }

            const float depth = static_cast<float>(x + y) * kTile;
            const float roof = t.building > 0 ? roofAlpha(t.building) : 0.0f;
            const bool interiorVisible = roof < 0.999f;
            const bool drawStructure = t.kind != StructureKind::None && (t.perimeter || interiorVisible);
            const std::size_t m = mat(t.material);

            if (drawStructure)
            {
                const bool cut = isCutAway(glm::ivec2(x, y), playerTile);
                if (t.kind == StructureKind::Wall)
                {
                    glm::vec4 uv = sprites.wall[m];
                    float height = wallH;
                    if (cut)
                    {
                        uv = sprites.wallLow[m];
                        height = lowH;
                    }
                    else if (t.window)
                    {
                        uv = t.run == StructureRun::AlongY ? sprites.windowRight[m] : sprites.windowLeft[m];
                    }
                    out.push_back(StaticSprite{
                        glm::vec2(left, top - height), glm::vec2(tileW, tileH + height), uv, glm::vec4(1.0f), depth});
                }
                else
                {
                    const DoorState door = doors != nullptr ? doors->doorState(x, y) : DoorState::Open;
                    const bool alongY = t.run == StructureRun::AlongY;
                    if (!cut)
                    {
                        glm::vec4 uv = alongY ? sprites.doorRight[m] : sprites.doorLeft[m];
                        if (door == DoorState::Closed) uv = alongY ? sprites.doorClosedRight[m] : sprites.doorClosedLeft[m];
                        if (door == DoorState::Broken) uv = alongY ? sprites.doorBrokenRight[m] : sprites.doorBrokenLeft[m];
                        // Slightly behind anyone standing in the doorway.
                        out.push_back(StaticSprite{
                            glm::vec2(left, top - wallH), glm::vec2(tileW, tileH + wallH), uv, glm::vec4(1.0f), depth - 8.0f});
                        const int boards = door == DoorState::Closed ? std::min(doors->doorBoards(x, y), 3) : 0;
                        if (boards > 0)
                        {
                            const auto i = static_cast<std::size_t>(boards - 1);
                            out.push_back(StaticSprite{glm::vec2(left, top - wallH), glm::vec2(tileW, tileH + wallH),
                                                       alongY ? sprites.boardsRight[i] : sprites.boardsLeft[i],
                                                       glm::vec4(1.0f), depth - 7.5f});
                        }
                    }
                    else if (door == DoorState::Closed)
                    {
                        // Cut away, a closed door still shows: a wooden stub in the gap.
                        out.push_back(StaticSprite{
                            glm::vec2(left, top - lowH), glm::vec2(tileW, tileH + lowH), sprites.wallLow[m],
                            glm::vec4(0.80f, 0.58f, 0.38f, 1.0f), depth});
                        if (doors != nullptr && doors->doorBoards(x, y) > 0)
                        {
                            out.push_back(StaticSprite{glm::vec2(left, top - lowH), glm::vec2(tileW, tileH + lowH),
                                                       alongY ? sprites.boardsLowRight : sprites.boardsLowLeft,
                                                       glm::vec4(1.0f), depth + 0.25f});
                        }
                    }
                }
            }

            // Roof over the interior (everything that is not the outer wall).
            if (t.building > 0 && !(t.kind != StructureKind::None && t.perimeter) && roof > 0.001f)
            {
                out.push_back(StaticSprite{
                    glm::vec2(left, top - wallH), glm::vec2(tileW, tileH),
                    sprites.roof[mat(mBuildingMaterial[static_cast<std::size_t>(t.building)])],
                    glm::vec4(1.0f, 1.0f, 1.0f, roof), depth + kTile * 0.5f + 0.5f});
            }
        }
    }
}
