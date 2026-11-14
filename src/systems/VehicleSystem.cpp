#include "systems/VehicleSystem.h"

#include <algorithm>
#include <cmath>

namespace
{
float lengthOf(const glm::vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

// The overlap of a circle with one solid tile: the push out of it, or zero.
glm::vec2 circleTile(const glm::vec2& c, float r, int tx, int ty, float tile)
{
    const float x0 = static_cast<float>(tx) * tile;
    const float y0 = static_cast<float>(ty) * tile;
    const glm::vec2 nearest(std::clamp(c.x, x0, x0 + tile), std::clamp(c.y, y0, y0 + tile));
    const glm::vec2 d = c - nearest;
    const float dist = lengthOf(d);
    if (dist >= r) return glm::vec2(0.0f);
    if (dist > 0.001f) return d / dist * (r - dist);
    // The centre is inside the tile: out by the nearest edge.
    const float left = c.x - x0;
    const float right = x0 + tile - c.x;
    const float up = c.y - y0;
    const float down = y0 + tile - c.y;
    const float m = std::min(std::min(left, right), std::min(up, down));
    if (m == left) return glm::vec2(-(left + r), 0.0f);
    if (m == right) return glm::vec2(right + r, 0.0f);
    if (m == up) return glm::vec2(0.0f, -(up + r));
    return glm::vec2(0.0f, down + r);
}

template <typename Fn>
void eachTileUnder(const glm::vec2& c, float r, float tile, Fn&& fn)
{
    const int x0 = static_cast<int>(std::floor((c.x - r) / tile));
    const int x1 = static_cast<int>(std::floor((c.x + r) / tile));
    const int y0 = static_cast<int>(std::floor((c.y - r) / tile));
    const int y1 = static_cast<int>(std::floor((c.y + r) / tile));
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) fn(x, y);
}
} // namespace

bool VehicleSystem::blocked(VehicleKind kind, const glm::vec2& centre, float heading, const Env& env)
{
    const float r = vehiclerules::info(kind).radius;
    for (const glm::vec2& c : vehiclerules::circles(kind, centre, heading))
    {
        bool hit = false;
        eachTileUnder(c, r, env.tileSize,
                      [&](int x, int y)
                      {
                          if (hit || env.tile(x, y) == TileKind::Open) return;
                          const glm::vec2 push = circleTile(c, r, x, y, env.tileSize);
                          if (push.x != 0.0f || push.y != 0.0f) hit = true;
                      });
        if (hit) return true;
    }
    return false;
}

void VehicleSystem::step(VehicleKind kind, glm::vec2& centre, float& heading, float& speed,
                         const vehiclerules::DriveInput& input, const Env& env, float dt, std::vector<Hit>& hits)
{
    const vehiclerules::KindInfo& k = vehiclerules::info(kind);
    vehiclerules::DriveState state{heading, speed};
    vehiclerules::drive(kind, state, input, dt);

    const glm::vec2 fwd = vehiclerules::forward(state.heading);
    glm::vec2 next = centre + fwd * state.speed * dt;

    // Against the town: walls push the body out (and take the speed going
    // into them); thin things give way to a fast one.
    glm::vec2 pushTotal(0.0f);
    float worstInto = 0.0f;
    glm::ivec2 worstTile{0};
    bool wall = false;
    for (int pass = 0; pass < 2; ++pass)
    {
        glm::vec2 push(0.0f);
        for (const glm::vec2& c : vehiclerules::circles(kind, next, state.heading))
        {
            eachTileUnder(c, k.radius, env.tileSize,
                          [&](int x, int y)
                          {
                              const TileKind tk = env.tile(x, y);
                              if (tk == TileKind::Open) return;
                              const glm::vec2 out = circleTile(c, k.radius, x, y, env.tileSize);
                              if (out.x == 0.0f && out.y == 0.0f) return;
                              if (tk == TileKind::Thin && std::fabs(state.speed) > vehiclerules::kRunDownSceneryAbove)
                              {
                                  // Down it goes (the game removes it).
                                  for (const Hit& h : hits)
                                      if (h.type == Hit::Type::Thin && h.tile == glm::ivec2(x, y)) return;
                                  hits.push_back(Hit{Hit::Type::Thin, glm::ivec2(x, y), std::fabs(state.speed)});
                                  return;
                              }
                              const float l = lengthOf(out);
                              const glm::vec2 n = out / l;
                              const float into = -(fwd.x * n.x + fwd.y * n.y) * state.speed;
                              if (into > worstInto)
                              {
                                  worstInto = into;
                                  worstTile = glm::ivec2(x, y);
                              }
                              wall = true;
                              // Take the biggest push along each axis.
                              if (std::fabs(out.x) > std::fabs(push.x)) push.x = out.x;
                              if (std::fabs(out.y) > std::fabs(push.y)) push.y = out.y;
                          });
        }
        if (push.x == 0.0f && push.y == 0.0f) break;
        next += push;
        pushTotal += push;
    }

    if (wall)
    {
        const float pushed = lengthOf(pushTotal);
        if (pushed > 0.001f)
        {
            // Head on, it stops; a glancing knock keeps some way on.
            const glm::vec2 n = pushTotal / pushed;
            const float headOn = std::fabs(fwd.x * n.x + fwd.y * n.y);
            state.speed *= std::max(0.0f, 1.0f - headOn * 1.2f);
        }
        if (worstInto > 1.0f) hits.push_back(Hit{Hit::Type::Wall, worstTile, worstInto});
    }

    // Still inside something (a corner, a turn into a wall): it does not move.
    if (blocked(kind, next, state.heading, env))
    {
        // Thin things already knocked down this step do not hold it.
        bool onlyThin = true;
        for (const glm::vec2& c : vehiclerules::circles(kind, next, state.heading))
        {
            eachTileUnder(c, k.radius, env.tileSize,
                          [&](int x, int y)
                          {
                              const TileKind tk = env.tile(x, y);
                              if (tk == TileKind::Wall)
                              {
                                  const glm::vec2 out = circleTile(c, k.radius, x, y, env.tileSize);
                                  if (out.x != 0.0f || out.y != 0.0f) onlyThin = false;
                              }
                              else if (tk == TileKind::Thin)
                              {
                                  bool knocked = false;
                                  for (const Hit& h : hits)
                                      if (h.type == Hit::Type::Thin && h.tile == glm::ivec2(x, y)) knocked = true;
                                  const glm::vec2 out = circleTile(c, k.radius, x, y, env.tileSize);
                                  if (!knocked && (out.x != 0.0f || out.y != 0.0f)) onlyThin = false;
                              }
                          });
        }
        if (!onlyThin)
        {
            next = centre;
            if (blocked(kind, centre, state.heading, env)) state.heading = heading; // the turn did it
            state.speed = 0.0f;
        }
    }

    centre = next;
    heading = state.heading;
    speed = state.speed;
}

glm::vec2 VehicleSystem::nearestOnAxis(VehicleKind kind, const glm::vec2& centre, float heading, const glm::vec2& point)
{
    const vehiclerules::KindInfo& k = vehiclerules::info(kind);
    const glm::vec2 f = vehiclerules::forward(heading);
    const glm::vec2 d = point - centre;
    const float t = std::clamp(d.x * f.x + d.y * f.y, -k.circleOffset, k.circleOffset);
    return centre + f * t;
}

float VehicleSystem::distanceToBody(VehicleKind kind, const glm::vec2& centre, float heading, const glm::vec2& point)
{
    const glm::vec2 near = nearestOnAxis(kind, centre, heading, point);
    return lengthOf(point - near) - vehiclerules::info(kind).radius;
}

glm::vec2 VehicleSystem::pushOut(VehicleKind kind, const glm::vec2& centre, float heading, const glm::vec2& point,
                                 float radius)
{
    const glm::vec2 near = nearestOnAxis(kind, centre, heading, point);
    const glm::vec2 d = point - near;
    const float dist = lengthOf(d);
    const float reach = vehiclerules::info(kind).radius + radius;
    if (dist >= reach) return glm::vec2(0.0f);
    if (dist < 0.001f)
    {
        // Dead on the axis: out to the side.
        const glm::vec2 f = vehiclerules::forward(heading);
        return glm::vec2(f.y, -f.x) * reach;
    }
    return d / dist * (reach - dist);
}
