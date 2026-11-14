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

} // namespace

StructureSprites StructureSprites::fromSheet(const SpriteSheet& sheet)
{
    StructureSprites s;
    static constexpr const char* kMaterials[3] = {"plaster", "brick", "metal"};
    static constexpr const char* kRoofs[3] = {"residential", "commercial", "industrial"};
    static constexpr const char* kFinishes[4] = {"plaster", "brick", "metal", "interior"};
    static constexpr const char* kAxes[2] = {"_x", "_y"};
    for (std::size_t f = 0; f < 4; ++f)
    {
        WallPieces& p = s.pieces[f];
        const std::string finish = std::string("wallp_") + kFinishes[f];
        for (std::size_t a = 0; a < 2; ++a)
        {
            const std::string base = finish + kAxes[a];
            p.full[a] = sheet.uvRect(base);
            p.window[a] = sheet.uvRect(base + "_window");
            p.windowBroken[a] = sheet.uvRect(base + "_window_broken");
            p.windowCurtain[a] = sheet.uvRect(base + "_window_curtain");
            p.windowBoard1[a] = sheet.uvRect(base + "_window_board1");
            p.windowBoard2[a] = sheet.uvRect(base + "_window_board2");
            p.windowLowBroken[a] = sheet.uvRect(base + "_window_low_broken");
            p.windowLowBoard[a] = sheet.uvRect(base + "_window_low_board");
            p.door[a] = sheet.uvRect(base + "_door");
            p.doorClosed[a] = sheet.uvRect(base + "_doorclosed");
            p.doorBroken[a] = sheet.uvRect(base + "_doorbroken");
            p.half[a] = sheet.uvRect(base + "_half");
            p.low[a] = sheet.uvRect(base + "_low");
            p.halfLow[a] = sheet.uvRect(base + "_half_low");
            p.doorLow[a] = sheet.uvRect(base + "_door_low");
        }
        p.post = sheet.uvRect(finish + "_post");
        p.postLow = sheet.uvRect(finish + "_post_low");
    }
    for (std::size_t i = 0; i < 3; ++i)
    {
        const std::string base = std::string("wall_") + kMaterials[i];
        s.wall[i] = sheet.uvRect(base);
        s.wallLow[i] = sheet.uvRect(base + "_low");
        for (std::size_t mask = 0; mask < 16; ++mask)
        {
            s.roof[i][mask] = sheet.uvRect(std::string("roofp_") + kRoofs[i] + "_" + std::to_string(mask));
        }
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

            // Where windows stand is a rule of its own (windows::place), so
            // drawing and the rules agree.
            t.window = t.kind == StructureKind::Wall && map.hasWindow(x, y);
        }
    }

    // Where each building wall stands on its tile, and what lies under it.
    const auto baseGid = [&](int x, int y)
    {
        const int gid = map.groundGid(x, y);
        return gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
    };
    for (int y = 0; y < mHeight; ++y)
    {
        for (int x = 0; x < mWidth; ++x)
        {
            StructureTile& t = mTiles[index(x, y)];
            if (t.kind == StructureKind::None)
            {
                continue;
            }
            if (t.building <= 0)
            {
                // A wall that belongs to no building (the wall round the
                // town): thin too, down the middle of its tiles.
                if (t.kind == StructureKind::Wall)
                {
                    t.place = WallPlace::Centre;
                    t.arms = static_cast<std::uint8_t>((wallLike(x, y - 1) ? 1 : 0) | (wallLike(x, y + 1) ? 2 : 0) |
                                                       (wallLike(x - 1, y) ? 4 : 0) | (wallLike(x + 1, y) ? 8 : 0));
                    t.underGid = TileGid::kGrass;
                    constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& d : kDirs)
                    {
                        const int nx = x + d[0];
                        const int ny = y + d[1];
                        if (inBounds(nx, ny) && mTiles[index(nx, ny)].kind == StructureKind::None &&
                            mTiles[index(nx, ny)].building == 0 && baseGid(nx, ny) > 0 &&
                            baseGid(nx, ny) != TileGid::kWater)
                        {
                            t.underGid = baseGid(nx, ny);
                            break;
                        }
                    }
                }
                continue;
            }
            const auto outside = [&](int nx, int ny)
            {
                return !inBounds(nx, ny) || mTiles[index(nx, ny)].building != t.building;
            };
            const bool outN = outside(x, y - 1);
            const bool outS = outside(x, y + 1);
            const bool outW = outside(x - 1, y);
            const bool outE = outside(x + 1, y);
            const int count = (outN ? 1 : 0) + (outS ? 1 : 0) + (outW ? 1 : 0) + (outE ? 1 : 0);
            int ox = 0;
            int oy = 0; // the way out, for the ground under the wall
            if (count == 0)
            {
                t.place = WallPlace::Centre;
                t.arms = static_cast<std::uint8_t>((wallLike(x, y - 1) ? 1 : 0) | (wallLike(x, y + 1) ? 2 : 0) |
                                                   (wallLike(x - 1, y) ? 4 : 0) | (wallLike(x + 1, y) ? 8 : 0));
            }
            else if (count == 1)
            {
                t.place = outN ? WallPlace::EdgeS : outS ? WallPlace::EdgeN : outW ? WallPlace::EdgeE : WallPlace::EdgeW;
                ox = outW ? -1 : (outE ? 1 : 0);
                oy = outN ? -1 : (outS ? 1 : 0);
            }
            else if (count == 2 && outN != outS && outW != outE)
            {
                t.place = outN ? (outW ? WallPlace::PostSE : WallPlace::PostSW)
                               : (outW ? WallPlace::PostNE : WallPlace::PostNW);
                ox = outW ? -1 : 1;
                oy = outN ? -1 : 1;
            }
            else
            {
                continue; // an odd shape: leave it a block
            }

            if (t.kind != StructureKind::Wall)
            {
                continue;
            }
            if (t.place == WallPlace::Centre)
            {
                t.underGid = TileGid::kFloor;
                constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& d : kDirs)
                {
                    const int nx = x + d[0];
                    const int ny = y + d[1];
                    if (inBounds(nx, ny) && mTiles[index(nx, ny)].kind == StructureKind::None &&
                        mTiles[index(nx, ny)].building == t.building && baseGid(nx, ny) > 0)
                    {
                        t.underGid = baseGid(nx, ny);
                        break;
                    }
                }
            }
            else
            {
                const int nx = x + ox;
                const int ny = y + oy;
                const bool open = inBounds(nx, ny) && mTiles[index(nx, ny)].kind == StructureKind::None &&
                                  mTiles[index(nx, ny)].building == 0 && baseGid(nx, ny) > 0 &&
                                  baseGid(nx, ny) != TileGid::kWater;
                t.underGid = open ? baseGid(nx, ny) : TileGid::kGrass;
            }
        }
    }

    // Roofs: a hip over each building's whole footprint, walls included.
    for (std::size_t b = 0; b < buildingCount; ++b)
    {
        const auto& rect = layout.buildings[b];
        for (int j = 0; j < rect.h; ++j)
        {
            for (int i = 0; i < rect.w; ++i)
            {
                const int x = rect.x + i;
                const int y = rect.y + j;
                if (!inBounds(x, y) || mTiles[index(x, y)].building != static_cast<int>(b) + 1)
                {
                    continue;
                }
                StructureTile& t = mTiles[index(x, y)];
                const int dW = i;
                const int dE = rect.w - 1 - i;
                const int dN = j;
                const int dS = rect.h - 1 - j;
                const int level = std::min(std::min(dW, dE), std::min(dN, dS));
                t.roofed = true;
                if (level >= kRoofMaxLevel)
                {
                    t.roofLevel = static_cast<std::uint8_t>(kRoofMaxLevel);
                    t.roofMask = 0;
                }
                else
                {
                    t.roofLevel = static_cast<std::uint8_t>(level);
                    t.roofMask = static_cast<std::uint8_t>((dW == level ? 1 : 0) | (dE == level ? 2 : 0) |
                                                           (dN == level ? 4 : 0) | (dS == level ? 8 : 0));
                }
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
    const StructureTile& t = tile(tileX, tileY);
    return t.kind == StructureKind::Wall && t.place == WallPlace::Block;
}

int StructureView::groundUnder(int tileX, int tileY) const
{
    return tile(tileX, tileY).underGid;
}

bool StructureView::wallAtEdge(int tileX, int tileY, int edge) const
{
    const StructureTile& t = tile(tileX, tileY);
    if (t.kind == StructureKind::None)
    {
        return false;
    }
    switch (t.place)
    {
    case WallPlace::Block:
        return t.kind == StructureKind::Wall;
    case WallPlace::EdgeW:
        return edge == 0;
    case WallPlace::EdgeN:
        return edge == 1;
    case WallPlace::EdgeE:
        return edge == 2;
    case WallPlace::EdgeS:
        return edge == 3;
    default:
        return false;
    }
}

bool StructureView::roofHidesPlayer(const glm::ivec2& roofTile, const glm::ivec2& player)
{
    const int depthDelta = (roofTile.x + roofTile.y) - (player.x + player.y);
    const int lateral = (roofTile.x - roofTile.y) - (player.x - player.y);
    return depthDelta > 0 && depthDelta <= kCutDepth + 4 && std::abs(lateral) <= kCutWidth + 1;
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
    const float roofReach = wallH + (static_cast<float>(kRoofMaxLevel + 1) * kRoofRise[0]) * zoom;
    const int playerBuilding = tile(playerTile.x, playerTile.y).building;

    // House paint and wallpaper: a tint per building.
    static constexpr glm::vec4 kPaint[6] = {
        {1.0f, 1.0f, 1.0f, 1.0f},    {0.84f, 0.92f, 1.0f, 1.0f},  {0.88f, 0.98f, 0.86f, 1.0f},
        {1.0f, 0.93f, 0.78f, 1.0f},  {0.90f, 0.88f, 0.88f, 1.0f}, {1.0f, 0.86f, 0.82f, 1.0f}};
    static constexpr glm::vec4 kPaper[4] = {
        {1.0f, 1.0f, 1.0f, 1.0f}, {0.88f, 0.95f, 1.0f, 1.0f}, {0.92f, 1.0f, 0.90f, 1.0f}, {1.0f, 0.94f, 0.86f, 1.0f}};
    const glm::vec4 kDoorWood(0.80f, 0.58f, 0.38f, 1.0f);

    // A wall piece with a window in it, as the window stands now.
    const auto fullPiece = [&](const WallPieces& p, std::size_t axis, int x, int y, bool window) -> const glm::vec4&
    {
        if (!window) return p.full[axis];
        if (doors == nullptr) return p.window[axis];
        const int boards = doors->windowBoards(x, y);
        if (boards >= 2) return p.windowBoard2[axis];
        if (boards == 1) return p.windowBoard1[axis];
        if (doors->windowState(x, y) == WindowState::Broken) return p.windowBroken[axis];
        return doors->windowCovered(x, y) ? p.windowCurtain[axis] : p.window[axis];
    };
    const auto lowPiece = [&](const WallPieces& p, std::size_t axis, int x, int y, bool window) -> const glm::vec4&
    {
        if (!window || doors == nullptr) return p.low[axis];
        if (doors->windowBoards(x, y) > 0) return p.windowLowBoard[axis];
        if (doors->windowState(x, y) == WindowState::Broken) return p.windowLowBroken[axis];
        return p.low[axis];
    };

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
            if (left + tileW < 0.0f || left > viewport.x || top - roofReach > viewport.y || top + tileH < 0.0f)
            {
                continue;
            }

            const float depth = static_cast<float>(x + y) * kTile;
            const float roof = t.building > 0 ? roofAlpha(t.building) : 0.0f;
            const bool interiorVisible = roof < 0.999f;
            const bool drawStructure = t.kind != StructureKind::None && (t.perimeter || interiorVisible);
            const std::size_t m = mat(t.material);

            // A piece `height` tall, slid (dx, dy) world pixels from the
            // middle of its tile.
            const auto piece = [&](const glm::vec4& uv, float height, float dx, float dy, const glm::vec4& color,
                                   float bias = 0.0f)
            {
                out.push_back(StaticSprite{
                    glm::vec2(left + std::round((dx - dy) * zoom), top - height + std::round((dx + dy) * 0.5f * zoom)),
                    glm::vec2(tileW, tileH + height), uv, color, depth + dx + dy + bias});
            };

            if (drawStructure && t.place == WallPlace::Block)
            {
                if (t.kind == StructureKind::Wall)
                {
                    const bool cut = isCutAway(glm::ivec2(x, y), playerTile);
                    piece(cut ? sprites.wallLow[m] : sprites.wall[m], cut ? lowH : wallH, 0.0f, 0.0f, glm::vec4(1.0f));
                }
            }
            else if (drawStructure)
            {
                const bool cut = isCutAway(glm::ivec2(x, y), playerTile);
                const float edge = kTile * 0.5f - kWallThickness * 0.5f;
                float dx = 0.0f;
                float dy = 0.0f;
                bool post = false;
                std::size_t axis = t.run == StructureRun::AlongY ? 1 : 0;
                bool outsideFace = false; // the face the camera sees is the outside of the building
                switch (t.place)
                {
                case WallPlace::EdgeN: dy = -edge; axis = 0; outsideFace = true; break;
                case WallPlace::EdgeS: dy = edge; axis = 0; break;
                case WallPlace::EdgeW: dx = -edge; axis = 1; outsideFace = true; break;
                case WallPlace::EdgeE: dx = edge; axis = 1; break;
                case WallPlace::PostNW: dx = -edge; dy = -edge; post = true; outsideFace = true; break;
                case WallPlace::PostNE: dx = edge; dy = -edge; post = true; outsideFace = true; break;
                case WallPlace::PostSW: dx = -edge; dy = edge; post = true; outsideFace = true; break;
                case WallPlace::PostSE: dx = edge; dy = edge; post = true; break;
                default: outsideFace = t.building <= 0; break;
                }
                const std::size_t finish = outsideFace ? m : StructureSprites::kInterior;
                const WallPieces& p = sprites.pieces[finish];
                const auto b = static_cast<std::size_t>(t.building);
                const glm::vec4 tint = !outsideFace ? kPaper[b % 4]
                                       : (t.material == StructureMaterial::Plaster && t.building > 0 ? kPaint[(b * 7) % 6]
                                                                                                    : glm::vec4(1.0f));
                const float height = cut ? lowH : wallH;

                if (t.kind == StructureKind::Wall)
                {
                    if (post)
                    {
                        piece(cut ? p.postLow : p.post, height, dx, dy, tint);
                    }
                    else if (t.place != WallPlace::Centre)
                    {
                        piece(cut ? lowPiece(p, axis, x, y, t.window) : fullPiece(p, axis, x, y, t.window), height, dx, dy,
                              tint);
                    }
                    else if (t.arms == 0)
                    {
                        piece(cut ? p.postLow : p.post, height, 0.0f, 0.0f, tint);
                    }
                    else
                    {
                        // Half pieces reaching to whichever neighbours are walls.
                        const glm::vec4& along = cut ? p.halfLow[0] : p.half[0];
                        const glm::vec4& across = cut ? p.halfLow[1] : p.half[1];
                        if (t.arms & 4) piece(along, height, -8.0f, 0.0f, tint);
                        if (t.arms & 1) piece(across, height, 0.0f, -8.0f, tint);
                        if (t.arms & 8) piece(along, height, 8.0f, 0.0f, tint);
                        if (t.arms & 2) piece(across, height, 0.0f, 8.0f, tint);
                    }
                }
                else if (!post)
                {
                    const DoorState door = doors != nullptr ? doors->doorState(x, y) : DoorState::Open;
                    const int boards = door == DoorState::Closed && doors != nullptr ? std::min(doors->doorBoards(x, y), 3) : 0;
                    // The board overlays were drawn for a door in the near face
                    // of a whole block; the door now hangs 14 px short of the
                    // middle of its wall.
                    const float bx = axis == 1 ? dx - 14.0f : dx;
                    const float by = axis == 0 ? dy - 14.0f : dy;
                    if (!cut)
                    {
                        const glm::vec4& uv = door == DoorState::Closed ? p.doorClosed[axis]
                                              : (door == DoorState::Broken ? p.doorBroken[axis] : p.door[axis]);
                        piece(uv, wallH, dx, dy, tint);
                        if (boards > 0)
                        {
                            const auto i = static_cast<std::size_t>(boards - 1);
                            piece(axis == 1 ? sprites.boardsRight[i] : sprites.boardsLeft[i], wallH, bx, by,
                                  glm::vec4(1.0f), 14.5f);
                        }
                    }
                    else if (door == DoorState::Closed)
                    {
                        // Cut away, a closed door still shows: a wooden stub in the gap.
                        piece(p.low[axis], lowH, dx, dy, kDoorWood);
                        if (boards > 0)
                        {
                            piece(axis == 1 ? sprites.boardsLowRight : sprites.boardsLowLeft, lowH, bx, by,
                                  glm::vec4(1.0f), 14.5f);
                        }
                    }
                    else
                    {
                        piece(p.doorLow[axis], lowH, dx, dy, tint);
                    }
                }
            }

            // The roof, over walls and rooms alike.
            if (t.roofed && roof > 0.001f)
            {
                const std::size_t kind = mat(mBuildingMaterial[static_cast<std::size_t>(t.building)]);
                const float rise = kRoofRise[kind] * zoom;
                const float base = wallH + static_cast<float>(t.roofLevel) * rise;
                const bool thin = playerBuilding != t.building && roofHidesPlayer(glm::ivec2(x, y), playerTile);
                out.push_back(StaticSprite{
                    glm::vec2(left, top - base - rise), glm::vec2(tileW, tileH + rise + kRoofFascia * zoom),
                    sprites.roof[kind][t.roofMask], glm::vec4(1.0f, 1.0f, 1.0f, thin ? roof * kRoofThinAlpha : roof),
                    depth + kTile * 0.5f + 0.5f});
            }
        }
    }

    // Shop signs on the street face over the door, sheared along the wall.
    const float signSize = kSignSize * zoom;
    for (const ShopSign& sign : mSigns)
    {
        const int x = sign.door.x;
        const int y = sign.door.y;
        if (!inBounds(x, y) || isCutAway(sign.door, playerTile)) continue;
        // The outside face of a bottom wall stands at the tile's -y edge
        // (StructureView puts that wall there), a slab's thickness in.
        const float faceY = static_cast<float>(y) * kTile + kWallThickness;
        const glm::vec2 foot = camera.worldToScreen(glm::vec2(static_cast<float>(x) * kTile, faceY));
        const glm::vec2 at(std::round(foot.x), std::round(foot.y - kSignLift * zoom));
        if (at.x + signSize < 0.0f || at.x > viewport.x || at.y + signSize < 0.0f || at.y > viewport.y) continue;
        // In front of the roofs over this tile and the next one along.
        out.push_back(StaticSprite{at, glm::vec2(signSize), sign.uv, glm::vec4(1.0f),
                                   static_cast<float>(x + y + 1) * kTile + 17.0f});
    }
}
