// Sound rules (phase 12), all pure: how far a sound carries, walls,
// voices and priority, crossfades, the ambience a place and an hour make,
// and the music's policy by tone.
#include "TestFramework.h"

#include <vector>

#include "audio/Ambience.h"
#include "audio/AudioRules.h"

using namespace audiorules;

namespace
{
float layer(const ambience::Mix& m, LayerId id)
{
    return m[static_cast<std::size_t>(id)];
}

ambience::Facts dayFacts()
{
    ambience::Facts f;
    f.day = true;
    f.condition = WeatherCondition::Clear;
    f.windStrength = 0.3f;
    f.infection = 20.0f;
    f.temperature = 15.0f;
    return f;
}
} // namespace

DP_TEST(volume_falls_from_two_tiles_to_the_range)
{
    DP_CHECK_NEAR(distanceGain(0.0f, 18.0f), 1.0f, 1e-5f);
    DP_CHECK_NEAR(distanceGain(2.0f, 18.0f), 1.0f, 1e-5f);
    DP_CHECK_NEAR(distanceGain(10.0f, 18.0f), 0.5f, 1e-5f);
    DP_CHECK_NEAR(distanceGain(18.0f, 18.0f), 0.0f, 1e-5f);
    DP_CHECK_NEAR(distanceGain(40.0f, 18.0f), 0.0f, 1e-5f);
    // Louder tiers carry further.
    DP_CHECK(distanceGain(20.0f, tierRangeTiles(5)) > distanceGain(20.0f, tierRangeTiles(4)));
}

DP_TEST(a_shut_shell_cuts_and_listening_lifts_it)
{
    const Shell street{0, false};
    const Shell shutHouse{3, true};
    const Shell openHouse{3, false};
    const Shell otherHouse{7, true};
    // The same building, or both outdoors: no cut.
    DP_CHECK(!occluded(shutHouse, shutHouse, false));
    DP_CHECK(!occluded(street, street, false));
    // From the street into a shut house, or out of it, or between two: cut.
    DP_CHECK(occluded(street, shutHouse, false));
    DP_CHECK(occluded(shutHouse, street, false));
    DP_CHECK(occluded(otherHouse, shutHouse, false));
    // A house with a door open lets the street in.
    DP_CHECK(!occluded(street, openHouse, false));
    // An ear to the door: nothing is cut.
    DP_CHECK(!occluded(otherHouse, shutHouse, true));
    DP_CHECK_NEAR(kOcclusionGain, 0.45f, 1e-6f);
}

DP_TEST(pan_follows_the_screen)
{
    DP_CHECK_NEAR(panFromScreen(640.0f, 640.0f, 640.0f), 0.0f, 1e-6f);
    DP_CHECK_NEAR(panFromScreen(1280.0f, 640.0f, 640.0f), 1.0f, 1e-6f);
    DP_CHECK_NEAR(panFromScreen(-500.0f, 640.0f, 640.0f), -1.0f, 1e-6f);
    DP_CHECK_EQ(panAngle(0.0f), 0);
    DP_CHECK_EQ(panAngle(1.0f), 90);
    DP_CHECK_EQ(panAngle(-1.0f), 270);
}

DP_TEST(voices_cap_the_same_sound_and_drop_the_least)
{
    std::vector<Voice> voices(4);
    const double now = 10.0;
    // Three groans already: a fourth replaces the quietest only if louder.
    for (int i = 0; i < 3; ++i) voices[static_cast<std::size_t>(i)] = Voice{7, 5, 0.2f + 0.1f * static_cast<float>(i), 20.0};
    DP_CHECK_EQ(pickVoice(voices, 7, 5, 0.1f, now), -1);
    DP_CHECK_EQ(pickVoice(voices, 7, 5, 0.5f, now), 0);
    // Another sound takes the free voice.
    DP_CHECK_EQ(pickVoice(voices, 8, 5, 0.1f, now), 3);
    // All busy: the quietest of the lowest priority gives way to a higher one.
    voices[3] = Voice{9, 2, 0.9f, 20.0};
    DP_CHECK_EQ(pickVoice(voices, 8, 9, 0.5f, now), 3);
    // ...but not to something of lower priority.
    DP_CHECK_EQ(pickVoice(voices, 8, 1, 0.5f, now), -1);
    // A voice whose sound has ended is free again.
    voices[1].endsAt = 5.0;
    DP_CHECK_EQ(pickVoice(voices, 8, 1, 0.5f, now), 1);
}

DP_TEST(layers_crossfade_over_a_second_and_a_half)
{
    float v = 0.0f;
    for (int i = 0; i < 45; ++i) v = fadeToward(v, 1.0f, 1.0f / 60.0f);
    DP_CHECK_NEAR(v, 0.5f, 0.01f);
    for (int i = 0; i < 60; ++i) v = fadeToward(v, 1.0f, 1.0f / 60.0f);
    DP_CHECK_NEAR(v, 1.0f, 1e-6f);
    for (int i = 0; i < 200; ++i) v = fadeToward(v, 0.0f, 1.0f / 60.0f);
    DP_CHECK_NEAR(v, 0.0f, 1e-6f);
}

DP_TEST(birds_sing_until_the_dead_come_near)
{
    ambience::Facts f = dayFacts();
    DP_CHECK(layer(ambience::mix(f), LayerId::Birds) > 0.5f);
    f.deadNear = 1; // one within 12 tiles: the tell
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::Birds), 0.0f, 1e-6f);
    f.deadNear = 0;
    f.infection = 60.0f; // a district too far gone
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::Birds), 0.0f, 1e-6f);
    f.infection = 20.0f;
    f.condition = WeatherCondition::Rain;
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::Birds), 0.0f, 1e-6f);
}

DP_TEST(insects_at_warm_nights_and_crows_over_ruin)
{
    ambience::Facts f = dayFacts();
    f.day = false;
    f.temperature = 14.0f;
    DP_CHECK(layer(ambience::mix(f), LayerId::Insects) > 0.5f);
    f.temperature = 5.0f;
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::Insects), 0.0f, 1e-6f);
    f.temperature = 14.0f;
    f.deadNear = 2;
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::Insects), 0.0f, 1e-6f);

    ambience::Facts g = dayFacts();
    DP_CHECK_NEAR(layer(ambience::mix(g), LayerId::Crows), 0.0f, 1e-6f);
    g.ruin = 2;
    DP_CHECK(layer(ambience::mix(g), LayerId::Crows) > 0.0f);
}

DP_TEST(rain_is_on_the_roof_indoors)
{
    ambience::Facts f = dayFacts();
    f.condition = WeatherCondition::Rain;
    ambience::Mix out = ambience::mix(f);
    DP_CHECK(layer(out, LayerId::RainLight) > 0.5f);
    DP_CHECK_NEAR(layer(out, LayerId::RainRoof), 0.0f, 1e-6f);
    f.indoors = true;
    ambience::Mix in = ambience::mix(f);
    DP_CHECK(layer(in, LayerId::RainRoof) > 0.5f);
    DP_CHECK_NEAR(layer(in, LayerId::RainLight), 0.0f, 1e-6f);
    // A door to outside open: some of the street's rain comes in.
    f.openToOutside = true;
    DP_CHECK(layer(ambience::mix(f), LayerId::RainLight) > 0.0f);
    // A shut room with no windows: no weather at all.
    f.openToOutside = false;
    f.closedIn = true;
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::RainRoof), 0.0f, 1e-6f);
}

DP_TEST(wind_is_quieter_indoors_and_gusts_when_strong)
{
    ambience::Facts f = dayFacts();
    f.windStrength = 0.8f;
    const float outside = layer(ambience::mix(f), LayerId::Wind);
    DP_CHECK(layer(ambience::mix(f), LayerId::WindGust) > 0.0f);
    f.indoors = true;
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::Wind), outside * 0.4f, 1e-5f);
}

DP_TEST(the_crowd_follows_the_number_of_the_dead)
{
    ambience::Facts f = dayFacts();
    f.deadAround = 2;
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::Crowd), 0.0f, 1e-6f);
    f.deadAround = 11;
    const float some = layer(ambience::mix(f), LayerId::Crowd);
    f.deadAround = 20;
    const float many = layer(ambience::mix(f), LayerId::Crowd);
    DP_CHECK(some > 0.0f && many > some);
    DP_CHECK_NEAR(many, 1.0f, 1e-6f);
}

DP_TEST(fire_hum_and_room_tone)
{
    ambience::Facts f = dayFacts();
    f.burningNear = 2;
    DP_CHECK(layer(ambience::mix(f), LayerId::FireSmall) > 0.0f);
    f.burningNear = 6;
    DP_CHECK(layer(ambience::mix(f), LayerId::FireLarge) > 0.0f);
    DP_CHECK_NEAR(layer(ambience::mix(f), LayerId::FireSmall), 0.0f, 1e-6f);
    f.hum = true;
    DP_CHECK(layer(ambience::mix(f), LayerId::Hum) > 0.0f);
    f.indoors = true;
    const float shut = layer(ambience::mix(f), LayerId::Room);
    f.openToOutside = true;
    DP_CHECK(layer(ambience::mix(f), LayerId::Room) < shut);
}

DP_TEST(a_gunshot_rings_and_ducks_the_rest)
{
    DP_CHECK_NEAR(ambience::duck(0.0f), 1.0f, 1e-6f);
    DP_CHECK_NEAR(ambience::duck(ambience::kTinnitusSeconds), 0.4f, 1e-5f); // 60 % down
    DP_CHECK(ambience::duck(0.25f) > 0.4f && ambience::duck(0.25f) < 1.0f);  // easing back
    ambience::Facts f = dayFacts();
    f.tinnitusSeconds = 3.0f;
    DP_CHECK(layer(ambience::mix(f), LayerId::Tinnitus) > 0.9f);
    DP_CHECK(ambience::isAmbience(LayerId::Birds));
    DP_CHECK(!ambience::isAmbience(LayerId::Tinnitus));
    DP_CHECK(!ambience::isAmbience(LayerId::PadA));
}

DP_TEST(getting_out_is_wind_and_nothing_else)
{
    ambience::Facts f = dayFacts();
    f.out = true;
    f.condition = WeatherCondition::Rain;
    const ambience::Mix m = ambience::mix(f);
    for (int i = 0; i < kLayerCount; ++i)
    {
        const LayerId id = static_cast<LayerId>(i);
        if (id == LayerId::Wind) DP_CHECK(layer(m, id) > 0.5f);
        else DP_CHECK_NEAR(layer(m, id), 0.0f, 1e-6f);
    }
}

// Six calm game hours indoors, checked every game minute.
namespace
{
int padsOver(music::Tone tone, double hours, bool indoors = true, bool calm = true)
{
    music::Director director;
    music::Facts f;
    f.tone = tone;
    f.indoors = indoors;
    f.calm = calm;
    for (double h = 0.0; h <= hours; h += 1.0 / 60.0)
    {
        f.hours = h;
        director.update(f);
    }
    return director.starts();
}
} // namespace

DP_TEST(hopeless_has_no_music_and_grim_one_pad_in_six_hours)
{
    DP_CHECK_EQ(padsOver(music::Tone::Hopeless, 6.0), 0);
    DP_CHECK_EQ(padsOver(music::Tone::GrimHuman, 6.0), 1);
    DP_CHECK(padsOver(music::Tone::BrutalArcade, 6.0) >= 2);
    // Never while the dead are near, nor outdoors at midday.
    DP_CHECK_EQ(padsOver(music::Tone::GrimHuman, 6.0, true, false), 0);
    DP_CHECK_EQ(padsOver(music::Tone::GrimHuman, 6.0, false, true), 0);
    DP_CHECK(!music::stingsAllowed(music::Tone::Hopeless));
    DP_CHECK(music::stingsAllowed(music::Tone::GrimHuman));
}

DP_TEST(danger_stops_the_pad_and_brutal_pulses_under_a_fight)
{
    music::Director director;
    music::Facts f;
    f.tone = music::Tone::GrimHuman;
    f.indoors = true;
    for (double h = 0.0; h <= 3.1; h += 1.0 / 60.0)
    {
        f.hours = h;
        director.update(f);
    }
    DP_CHECK(director.padTarget() > 0.0f);
    f.calm = false;
    director.update(f);
    DP_CHECK_NEAR(director.padTarget(), 0.0f, 1e-6f);

    music::Director brutal;
    music::Facts b;
    b.tone = music::Tone::BrutalArcade;
    b.calm = false;
    b.fight = 4;
    brutal.update(b);
    DP_CHECK(brutal.pulseTarget() > 0.0f);
    b.tone = music::Tone::GrimHuman;
    brutal.update(b);
    DP_CHECK_NEAR(brutal.pulseTarget(), 0.0f, 1e-6f);
}
