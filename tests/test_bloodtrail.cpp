// Blood trails (overhaul phase 4): drops as you walk, fading by place.
#include "TestFramework.h"

#include "world/BloodTrail.h"

using namespace bloodtrail;

DP_TEST(trail_markers_last_by_place_and_rain)
{
    DP_CHECK_NEAR(lifeSeconds(Place::Indoor, false), 20.0f, 1e-6f);
    DP_CHECK_NEAR(lifeSeconds(Place::Outdoor, false), 10.0f, 1e-6f);
    DP_CHECK_NEAR(lifeSeconds(Place::Outdoor, true), 3.0f, 1e-6f);
    DP_CHECK_NEAR(lifeSeconds(Place::Water, false), 0.0f, 1e-6f);
    DP_CHECK_NEAR(lifeSeconds(Place::Outdoor, false, 3.0f), 30.0f, 1e-6f); // the Stalker
}

DP_TEST(walking_bleeding_drops_a_marker_every_tile_and_a_half)
{
    Trail trail;
    const float tile = 32.0f;
    glm::vec2 at(0.0f);
    for (int i = 0; i < 15; ++i)
    {
        const glm::vec2 next = at + glm::vec2(tile, 0.0f);
        trail.walk(at, next, kBleedingStrength, Place::Outdoor, tile);
        at = next;
    }
    DP_CHECK_EQ(static_cast<int>(trail.markers().size()), 10);
}

DP_TEST(trail_fades_outdoors_in_ten_seconds_and_at_once_on_water)
{
    Trail trail;
    trail.addMarker(glm::vec2(0.0f), 1.0f, Place::Outdoor);
    trail.addMarker(glm::vec2(64.0f, 0.0f), 1.0f, Place::Indoor);
    trail.addMarker(glm::vec2(128.0f, 0.0f), 1.0f, Place::Water);
    trail.update(0.1f, false);
    DP_CHECK_EQ(static_cast<int>(trail.markers().size()), 2);
    trail.update(10.0f, false);
    DP_CHECK_EQ(static_cast<int>(trail.markers().size()), 1); // the indoor one
    trail.update(10.0f, false);
    DP_CHECK(trail.markers().empty());
}

DP_TEST(rain_washes_a_trail_in_three_seconds)
{
    Trail trail;
    trail.addMarker(glm::vec2(0.0f), 1.0f, Place::Outdoor);
    trail.update(3.5f, true);
    DP_CHECK(trail.markers().empty());
}

DP_TEST(the_freshest_marker_near_leads_on_along_the_trail)
{
    Trail trail;
    trail.addMarker(glm::vec2(0.0f), 1.0f, Place::Indoor);
    trail.addMarker(glm::vec2(48.0f, 0.0f), 1.0f, Place::Indoor);
    trail.addMarker(glm::vec2(96.0f, 0.0f), 1.0f, Place::Indoor);
    const Marker* first = trail.freshestNear(glm::vec2(0.0f), 5.0f * 32.0f, 0);
    DP_CHECK(first != nullptr);
    if (first == nullptr) return;
    DP_CHECK_NEAR(first->world.x, 96.0f, 1e-4f);
    DP_CHECK(trail.freshestNear(glm::vec2(0.0f), 5.0f * 32.0f, first->seq) == nullptr);
    DP_CHECK(Trail::freshness(*first) > 0.0f);
    trail.addMarker(glm::vec2(500.0f, 0.0f), kPoolStrength, Place::Outdoor, true);
    const Marker* pool = trail.freshestNear(glm::vec2(500.0f, 0.0f), 32.0f, 0);
    DP_CHECK(pool != nullptr && pool->pool);
}
