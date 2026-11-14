#include "rendering/SceneryView.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

#include <glm/geometric.hpp>

#include "map/TileMap.h"
#include "rendering/Camera.h"
#include "rendering/SpriteBatch.h"
#include "rendering/SpriteSheet.h"

namespace
{
constexpr float kTile = 32.0f;

bool startsWith(const char* text, std::string_view prefix)
{
    return std::string_view(text).substr(0, prefix.size()) == prefix;
}

float phaseOf(const glm::vec2& position)
{
    return std::fmod(position.x * 0.071f + position.y * 0.113f, 6.2831853f);
}
} // namespace

void SceneryView::build(const std::vector<SceneryPiece>& pieces)
{
    mStanding.clear();
    mFlat.clear();
    mLoose.clear();
    mShaking.clear();
    mShrubsByTile.clear();
    mLamps.clear();
    mTrees.clear();
    mTreeByTile.clear();
    for (const SceneryPiece& piece : pieces)
    {
        Entry entry;
        entry.piece = piece;
        entry.tile = scenery::tileOf(piece);
        entry.phase = phaseOf(piece.position);
        if (piece.flat)
        {
            if (std::strcmp(piece.sprite, "scn_can") == 0) entry.motion = Motion::Can;
            if (std::strcmp(piece.sprite, "scn_paper") == 0) entry.motion = Motion::Paper;
            if (entry.motion != Motion::Still)
            {
                mLoose.push_back(static_cast<std::uint32_t>(mFlat.size()));
            }
            mFlat.push_back(entry);
            continue;
        }
        const auto id = static_cast<std::uint32_t>(mStanding.size());
        if (piece.tall)
        {
            entry.motion = Motion::Tree;
            mTreeByTile[bucketKey(entry.tile.x, entry.tile.y)] = id;
            entry.autumn = std::strcmp(piece.sprite, "scn_tree_autumn") == 0;
            if (startsWith(piece.sprite, "scn_tree_oak") || entry.autumn)
            {
                mTrees.push_back(id);
            }
        }
        else if (startsWith(piece.sprite, "scn_bush") || startsWith(piece.sprite, "scn_tuft") ||
                 startsWith(piece.sprite, "scn_flowers"))
        {
            entry.motion = Motion::Shrub;
            mShrubsByTile[bucketKey(entry.tile.x, entry.tile.y)].push_back(id);
        }
        else if (std::strcmp(piece.sprite, "scn_lamppost") == 0)
        {
            entry.lamp = true;
            mLamps.push_back(id);
        }
        mStanding.push_back(entry);
    }
    mResolved = false;
}

void SceneryView::resolve(const SpriteSheet& sheet)
{
    if (mResolved)
    {
        return;
    }
    const auto fill = [&](Entry& entry)
    {
        entry.uv = sheet.uvRect(entry.piece.sprite);
        entry.size = sheet.pixelSize(entry.piece.sprite);
        // The lamp post stands at the left of its sprite, its arm reaching right.
        entry.anchorX = entry.lamp ? 14.0f : std::floor(entry.size.x * 0.5f);
    };
    for (Entry& entry : mStanding) fill(entry);
    for (Entry& entry : mFlat) fill(entry);
    mResolved = true;
}

bool SceneryView::hidesPlayer(const glm::ivec2& piece, const glm::ivec2& player)
{
    // Up to two tile diagonals in front of the player and overlapping them on
    // screen.
    const glm::ivec2 d = piece - player;
    return d.x >= 0 && d.y >= 0 && d.x <= 2 && d.y <= 2 && (d.x + d.y) > 0 && std::abs(d.x - d.y) <= 1;
}

float SceneryView::lean(float clock, float phase, float gust, float fraction, float stiff)
{
    const float free = std::clamp((fraction - stiff) / std::max(0.001f, 1.0f - stiff), 0.0f, 1.0f);
    // A slow sway that grows with the wind, a steady lean downwind, and a
    // quick shiver in the gusts.
    const float sway = std::sin(clock * 1.3f + phase) * (0.7f + 2.6f * gust) + gust * 2.2f +
                       std::sin(clock * 4.1f + phase * 2.0f) * gust * 0.8f;
    return sway * free * free;
}

void SceneryView::update(float dtSeconds, const glm::vec2& wind, float gust, const std::vector<SceneryMover>& movers,
                         const TileMap& map, std::vector<glm::vec2>* rustled)
{
    const float dt = std::max(0.0f, dtSeconds);
    mClock += dt;
    mGust = gust;

    // Shaking dies away.
    for (std::size_t i = 0; i < mShaking.size();)
    {
        Entry& entry = mStanding[mShaking[i]];
        entry.rustle = std::max(0.0f, entry.rustle - dt);
        if (entry.rustle <= 0.0f)
        {
            mShaking[i] = mShaking.back();
            mShaking.pop_back();
        }
        else
        {
            ++i;
        }
    }

    for (const SceneryMover& mover : movers)
    {
        const float speed = glm::length(mover.velocity);
        if (speed < 12.0f)
        {
            continue;
        }

        // Pushing through undergrowth.
        const int tx = static_cast<int>(std::floor(mover.position.x / kTile));
        const int ty = static_cast<int>(std::floor(mover.position.y / kTile));
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const auto bucket = mShrubsByTile.find(bucketKey(tx + dx, ty + dy));
                if (bucket == mShrubsByTile.end())
                {
                    continue;
                }
                for (const std::uint32_t id : bucket->second)
                {
                    Entry& entry = mStanding[id];
                    const glm::vec2 d = entry.piece.position - mover.position;
                    if (glm::dot(d, d) > kRustleRadius * kRustleRadius)
                    {
                        continue;
                    }
                    if (entry.rustle <= 0.0f)
                    {
                        mShaking.push_back(id);
                        if (rustled != nullptr)
                        {
                            rustled->push_back(entry.piece.position);
                        }
                    }
                    entry.rustle = kRustleSeconds;
                }
            }
        }

        // Kicking what lies in the way.
        for (const std::uint32_t id : mLoose)
        {
            Entry& entry = mFlat[id];
            const glm::vec2 d = entry.piece.position - mover.position;
            const float dist2 = glm::dot(d, d);
            if (dist2 > kKickRadius * kKickRadius)
            {
                continue;
            }
            const glm::vec2 away = dist2 > 0.01f ? d / std::sqrt(dist2) : mover.velocity / speed;
            const float carry = entry.motion == Motion::Can ? 1.25f : 0.8f;
            entry.velocity = mover.velocity * carry + away * 45.0f;
        }
    }

    // Loose things slide to a stop, and bounce off whatever is solid.
    for (const std::uint32_t id : mLoose)
    {
        Entry& entry = mFlat[id];
        if (entry.motion == Motion::Paper && gust > 0.5f)
        {
            // Lifted and tumbled along in fits and starts.
            const float lift = std::max(0.0f, std::sin(mClock * 1.7f + entry.phase * 3.0f));
            entry.velocity += wind * (lift * (gust - 0.5f) * 3.0f * dt);
        }
        const float speed2 = glm::dot(entry.velocity, entry.velocity);
        if (speed2 < 1.0f)
        {
            entry.velocity = glm::vec2(0.0f);
            continue;
        }
        const float friction = entry.motion == Motion::Can ? 1.6f : 3.4f;
        entry.velocity *= std::max(0.0f, 1.0f - friction * dt);

        const auto blocked = [&](const glm::vec2& at)
        {
            return map.isSolid(static_cast<int>(std::floor(at.x / kTile)), static_cast<int>(std::floor(at.y / kTile)));
        };
        glm::vec2 next = entry.piece.position + entry.velocity * dt;
        if (blocked(next))
        {
            const glm::vec2 alongX(next.x, entry.piece.position.y);
            const glm::vec2 alongY(entry.piece.position.x, next.y);
            if (!blocked(alongX))
            {
                entry.velocity.y *= -0.45f;
                next = alongX;
            }
            else if (!blocked(alongY))
            {
                entry.velocity.x *= -0.45f;
                next = alongY;
            }
            else
            {
                entry.velocity *= -0.45f;
                next = entry.piece.position;
            }
        }
        entry.piece.position = next;
        entry.tile = scenery::tileOf(entry.piece);
    }
}

void SceneryView::lampsNear(const glm::vec2& centre, float radius, std::vector<glm::vec2>& out) const
{
    for (const std::uint32_t id : mLamps)
    {
        const glm::vec2 d = mStanding[id].piece.position - centre;
        if (glm::dot(d, d) <= radius * radius)
        {
            out.push_back(mStanding[id].piece.position);
        }
    }
}

void SceneryView::canopiesNear(const glm::vec2& centre, float radius, std::vector<SceneryCanopy>& out) const
{
    for (const std::uint32_t id : mTrees)
    {
        const glm::vec2 d = mStanding[id].piece.position - centre;
        if (glm::dot(d, d) <= radius * radius)
        {
            out.push_back(SceneryCanopy{mStanding[id].piece.position, mStanding[id].autumn});
        }
    }
}

bool SceneryView::treeAt(const glm::ivec2& tile) const
{
    const auto it = mTreeByTile.find(bucketKey(tile.x, tile.y));
    return it != mTreeByTile.end() && !mStanding[it->second].burnt;
}

void SceneryView::burnTree(const glm::ivec2& tile)
{
    const auto it = mTreeByTile.find(bucketKey(tile.x, tile.y));
    if (it == mTreeByTile.end()) return;
    Entry& entry = mStanding[it->second];
    if (entry.burnt) return;
    entry.burnt = true;
    entry.autumn = false;
    entry.piece.sprite = "scn_tree_dead";
    std::erase(mTrees, it->second); // no leaves, no shade
    mResolved = false;
}

std::vector<glm::ivec2> SceneryView::treeTiles() const
{
    std::vector<glm::ivec2> out;
    for (const auto& [key, id] : mTreeByTile)
    {
        if (!mStanding[id].burnt) out.push_back(mStanding[id].tile);
    }
    std::sort(out.begin(), out.end(), [](const glm::ivec2& a, const glm::ivec2& b)
              { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    return out;
}

void SceneryView::drawFlat(SpriteBatch& batch, const Camera& camera, const SpriteSheet& sheet)
{
    resolve(sheet);
    const float zoom = camera.zoom();
    const glm::vec2 viewport = camera.viewportSize();
    for (const Entry& entry : mFlat)
    {
        const glm::vec2 ground = camera.worldToScreen(entry.piece.position);
        const glm::vec2 size = entry.size * zoom;
        const glm::vec2 topLeft(std::round(ground.x - size.x * 0.5f), std::round(ground.y - size.y * 0.5f));
        if (topLeft.x + size.x < 0.0f || topLeft.x > viewport.x || topLeft.y + size.y < 0.0f || topLeft.y > viewport.y)
        {
            continue;
        }
        batch.draw(topLeft, size, entry.uv, glm::vec4(1.0f));
    }
    for (RuinMark& mark : mRuin)
    {
        if (mark.size.x <= 0.0f)
        {
            if (sheet.find(mark.sprite) == nullptr) continue;
            mark.uv = sheet.frameUv(mark.sprite, mark.frame);
            mark.size = sheet.pixelSize(mark.sprite);
            if (mark.anchor.x < 0.0f) mark.anchor = mark.size * 0.5f; // centred on its point
        }
        const glm::vec2 ground = camera.worldToScreen(mark.position);
        const glm::vec2 size = mark.size * zoom;
        const glm::vec2 topLeft(std::round(ground.x - mark.anchor.x * zoom), std::round(ground.y - mark.anchor.y * zoom));
        if (topLeft.x + size.x < 0.0f || topLeft.x > viewport.x || topLeft.y + size.y < 0.0f || topLeft.y > viewport.y)
        {
            continue;
        }
        batch.draw(topLeft, size, mark.uv, glm::vec4(1.0f, 1.0f, 1.0f, mark.alpha));
    }
}

void SceneryView::collect(const Camera& camera, const glm::ivec2& playerTile, const SpriteSheet& sheet,
                          std::vector<StaticSprite>& out)
{
    resolve(sheet);
    const float zoom = camera.zoom();
    const glm::vec2 viewport = camera.viewportSize();
    for (const Entry& entry : mStanding)
    {
        const glm::vec2 ground = camera.worldToScreen(entry.piece.position);
        const glm::vec2 size = entry.size * zoom;
        const glm::vec2 topLeft(std::round(ground.x - entry.anchorX * zoom),
                                std::round(ground.y - (entry.size.y - kFoot) * zoom));
        // A little slack either side for whatever leans out of its frame.
        if (topLeft.x + size.x + 12.0f * zoom < 0.0f || topLeft.x - 12.0f * zoom > viewport.x ||
            topLeft.y + size.y < 0.0f || topLeft.y > viewport.y)
        {
            continue;
        }
        const float alpha = entry.piece.tall && hidesPlayer(entry.tile, playerTile) ? kFadedAlpha : 1.0f;
        // A burnt tree is a black trunk.
        const glm::vec4 color = entry.burnt ? glm::vec4(0.22f, 0.19f, 0.18f, alpha) : glm::vec4(1.0f, 1.0f, 1.0f, alpha);
        // Depth in entity units: the top-left corner of a 32 px cell centred
        // on the ground point.
        const float depth = entry.piece.position.x + entry.piece.position.y - 32.0f;

        if (entry.motion == Motion::Still)
        {
            out.push_back(StaticSprite{topLeft, size, entry.uv, color, depth});
            continue;
        }

        // Anything that moves is drawn as bands, each slid sideways a whole
        // sprite pixel or two more than the one below it.
        const bool tree = entry.motion == Motion::Tree;
        const int bands = tree ? 6 : 3;
        const float stiff = tree ? 0.3f : 0.0f;
        const float shake = entry.rustle > 0.0f
            ? std::sin(mClock * 42.0f + entry.phase) * 2.0f * (entry.rustle / kRustleSeconds)
            : 0.0f;
        const float rows = entry.size.y;
        float bandTop = topLeft.y;
        for (int band = 0; band < bands; ++band)
        {
            // Bands run top to bottom; the top one leans most.
            const float row0 = std::floor(rows * static_cast<float>(band) / static_cast<float>(bands));
            const float row1 = std::floor(rows * static_cast<float>(band + 1) / static_cast<float>(bands));
            const float fraction = 1.0f - (row0 + row1) * 0.5f / rows;
            const float gust = tree ? mGust : mGust * 0.45f;
            const float offset = std::round(lean(mClock, entry.phase, gust, fraction, stiff) + shake * fraction);
            const float bandBottom = std::round(topLeft.y + row1 * zoom);
            out.push_back(StaticSprite{
                glm::vec2(topLeft.x + std::round(offset * zoom), bandTop), glm::vec2(size.x, bandBottom - bandTop),
                glm::vec4(entry.uv.x, entry.uv.y + entry.uv.w * row0 / rows, entry.uv.z, entry.uv.w * (row1 - row0) / rows),
                color, depth});
            bandTop = bandBottom;
        }
    }
}
