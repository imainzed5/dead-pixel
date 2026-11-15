#pragma once

// The cemetery plot (phase 10): one per world, a fenced patch of grass near
// the middle of town, stamped once over the layout like the gate (phase 9).
// Stones stand in rows inside it (legacy::cemeterySlot). Pure.

#include <string>
#include <vector>

#include <glm/vec2.hpp>

#include "map/LayoutData.h"

namespace cemetery
{

constexpr int kWidth = 9;  // the fence, west to east
constexpr int kHeight = 8; // the fence, north to south

// Stamps the plot on open ground nearest the middle of the map: grass inside
// the fence, a one-tile gap in its south side. Deterministic from the layout.
// False (and `why`) when there is no room. True at once if already placed.
bool stamp(LayoutData& layout, std::string* why = nullptr);

// On the fence line (not the gap).
[[nodiscard]] bool isFence(const LayoutData::CemeterySite& site, const glm::ivec2& tile);
// Within the fence's rectangle, fence included.
[[nodiscard]] bool inside(const LayoutData::CemeterySite& site, const glm::ivec2& tile);
// Every fence tile, in order round the plot.
[[nodiscard]] std::vector<glm::ivec2> fenceTiles(const LayoutData::CemeterySite& site);

} // namespace cemetery
