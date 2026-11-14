// Mood on the screen (phase 12): the character's state to the full-screen
// pass's parameters, the idles, blinks and the low sun. All pure.
#include "TestFramework.h"

#include <string>

#include "world/MoodRules.h"

using namespace moodrules;

DP_TEST(a_calm_body_draws_no_pass)
{
    const Facts calm;
    DP_CHECK(params(calm, 1.0f).neutral());
    // Effects off: nothing, whatever the state.
    Facts awful;
    awful.panic = 100.0f;
    awful.depression = 100.0f;
    awful.held = true;
    DP_CHECK(params(awful, 0.0f).neutral());
    DP_CHECK(!params(awful, 1.0f).neutral());
}

DP_TEST(panic_closes_the_view)
{
    DP_CHECK_NEAR(vignetteFor(49.0f), 0.0f, 1e-6f);
    DP_CHECK_NEAR(vignetteFor(50.0f), 0.15f, 1e-5f);
    DP_CHECK_NEAR(vignetteFor(100.0f), 0.35f, 1e-5f);
    Facts f;
    f.panic = 80.0f;
    f.heartbeat = 1.0f;
    const Params p = params(f, 1.0f);
    DP_CHECK(p.vignette > 0.15f && p.vignette < 0.35f);
    DP_CHECK_NEAR(p.shakePx, 1.0f, 1e-6f); // a pixel on the beat at 75+
    f.heartbeat = 0.2f;
    DP_CHECK_NEAR(params(f, 1.0f).shakePx, 0.0f, 1e-6f);
    // Reduced effects: half.
    f.heartbeat = 1.0f;
    DP_CHECK_NEAR(params(f, 0.5f).vignette, p.vignette * 0.5f, 1e-5f);
}

DP_TEST(depression_drains_the_colour_in_steps)
{
    DP_CHECK_NEAR(saturationFor(40.0f), 1.0f, 1e-6f);
    DP_CHECK_NEAR(saturationFor(50.0f), 0.8f, 1e-6f);
    DP_CHECK_NEAR(saturationFor(70.0f), 0.6f, 1e-6f);
    DP_CHECK_NEAR(saturationFor(90.0f), 0.45f, 1e-6f);
    Facts f;
    f.depression = 90.0f;
    DP_CHECK_NEAR(params(f, 1.0f).saturation, 0.45f, 1e-6f);
}

DP_TEST(tiredness_blurs_and_blinks)
{
    Facts f;
    f.sleepDebt = 75.0f;
    Params p = params(f, 1.0f);
    DP_CHECK(p.blur > 0.0f);
    DP_CHECK_NEAR(p.blink, 0.0f, 1e-6f);
    f.sleepDebt = 90.0f;
    f.blink = 1.0f;
    p = params(f, 1.0f);
    DP_CHECK_NEAR(p.blink, kBlinkDepth, 1e-6f); // to 60 % black
    DP_CHECK_NEAR(nextBlinkSeconds(0.0f), 20.0f, 1e-6f);
    DP_CHECK_NEAR(nextBlinkSeconds(1.0f), 40.0f, 1e-6f);
    DP_CHECK_NEAR(blinkShut(0.0f), 0.0f, 1e-6f);
    DP_CHECK_NEAR(blinkShut(kBlinkSeconds * 0.5f), 1.0f, 1e-5f);
    DP_CHECK_NEAR(blinkShut(kBlinkSeconds), 0.0f, 1e-6f);
}

DP_TEST(cold_heat_fever_smoke_held_listening_and_dying)
{
    Facts f;
    f.bodyTemperature = 34.5f;
    DP_CHECK(params(f, 1.0f).coldEdge > 0.0f);
    f.bodyTemperature = 38.8f;
    Params p = params(f, 1.0f);
    DP_CHECK(p.warmCast > 0.0f && p.shimmer > 0.0f);
    f.illness = 70.0f; // a fever, not the heat
    p = params(f, 1.0f);
    DP_CHECK(p.doubleImage > 0.0f);
    DP_CHECK_NEAR(p.shimmer, 0.0f, 1e-6f);
    Facts g;
    g.smoke = true;
    DP_CHECK(params(g, 1.0f).haze > 0.0f);
    g = Facts{};
    g.held = true;
    DP_CHECK_NEAR(params(g, 1.0f).zoom, 0.06f, 1e-6f);
    g = Facts{};
    g.listening = true;
    DP_CHECK_NEAR(params(g, 1.0f).dimEdges, 0.10f, 1e-6f);
    g = Facts{};
    g.health = 0.15f;
    g.heartbeat = 1.0f;
    const float onBeat = params(g, 1.0f).redPulse;
    g.heartbeat = 0.0f;
    DP_CHECK(onBeat > params(g, 1.0f).redPulse);
    g.health = 0.5f;
    DP_CHECK_NEAR(params(g, 1.0f).redPulse, 0.0f, 1e-6f);
}

DP_TEST(the_body_falls_into_an_idle_by_its_state)
{
    DP_CHECK(idleFor(10.0f, 37.0f, 10.0f, 1.0f) == Idle::None);
    DP_CHECK(idleFor(60.0f, 37.0f, 10.0f, 1.0f) == Idle::Low);
    DP_CHECK(idleFor(60.0f, 34.0f, 10.0f, 1.0f) == Idle::Cold);
    DP_CHECK(idleFor(60.0f, 34.0f, 90.0f, 1.0f) == Idle::Knee);
    DP_CHECK(idleFor(0.0f, 37.0f, 0.0f, 0.1f) == Idle::Knee);
    DP_CHECK(std::string(idleName(Idle::Cold)) == "cold");
    DP_CHECK(std::string(idleName(Idle::None)).empty());
}

DP_TEST(shadows_are_long_at_the_ends_of_the_day_and_none_at_night)
{
    const double dawn = 6.0;
    const double dusk = 20.0;
    DP_CHECK_NEAR(sunAt(2.0, dawn, dusk).length, 0.0f, 1e-6f);
    DP_CHECK_NEAR(sunAt(22.0, dawn, dusk).length, 0.0f, 1e-6f);
    const Sun morning = sunAt(7.0, dawn, dusk);
    const Sun noon = sunAt(13.0, dawn, dusk);
    const Sun evening = sunAt(19.0, dawn, dusk);
    DP_CHECK(morning.length > noon.length * 2.0f);
    DP_CHECK(evening.length > noon.length * 2.0f);
    // They swing: west in the morning, east in the evening.
    DP_CHECK(morning.dirX < 0.0f);
    DP_CHECK(evening.dirX > 0.0f);
}
