// Wounds worth treating (phase 5): where wounds land and what they do,
// healing times, dirty dressings, infection, treatment and the HUD names.
#include "TestFramework.h"

#include "components/Bleeding.h"
#include "components/Health.h"
#include "components/Needs.h"
#include "components/Wounds.h"
#include "ecs/World.h"
#include "systems/WoundSystem.h"
#include "world/WoundRules.h"

namespace
{
struct WoundWorld
{
    World world;
    Entity player = kInvalidEntity;

    WoundWorld()
    {
        world.registerComponent<Health>();
        world.registerComponent<Needs>();
        world.registerComponent<Wounds>();
        world.registerComponent<Bleeding>();
        player = world.createEntity();
        world.addComponent(player, Health{});
        world.addComponent(player, Needs{});
        world.addComponent(player, Wounds{});
    }
    std::vector<Wound>& wounds() { return world.getComponent<Wounds>(player).active; }
    // Game hours, a game minute (a real second) at a time.
    void hours(float h, WoundSystem& system, const WoundContext& context = WoundContext{})
    {
        const int steps = static_cast<int>(h * 60.0f);
        for (int i = 0; i < steps; ++i) system.update(world, player, 1.0f, 12.0, context);
    }
};
} // namespace

DP_TEST(wounds_land_where_the_tables_say)
{
    // Claws: arms 50 %, torso 30 %, head 10 %, legs 10 %.
    DP_CHECK(woundrules::isArm(woundrules::clawPart(0.10f)));
    DP_CHECK(woundrules::clawPart(0.60f) == BodyPart::Torso);
    DP_CHECK(woundrules::clawPart(0.85f) == BodyPart::Head);
    DP_CHECK(woundrules::isLeg(woundrules::clawPart(0.95f)));
    // Crawlers: legs only.
    for (float r = 0.0f; r < 1.0f; r += 0.1f) DP_CHECK(woundrules::isLeg(woundrules::crawlerPart(r)));
}

DP_TEST(wounds_leg_fracture_slows_and_splint_helps)
{
    std::vector<Wound> wounds = {woundrules::make(WoundType::Fracture, BodyPart::LeftLeg)};
    woundrules::BodyEffects body = woundrules::effects(wounds);
    DP_CHECK_NEAR(body.moveScale, 0.55f, 0.001);
    DP_CHECK(!body.canRun);
    DP_CHECK(body.limp);
    woundrules::apply(woundrules::Treatment::Splint, wounds.front());
    body = woundrules::effects(wounds);
    DP_CHECK_NEAR(body.moveScale, 0.80f, 0.001);
    DP_CHECK(body.canRun);
    DP_CHECK(body.runAtWalkSpeed);
}

DP_TEST(wounds_arm_fracture_and_deep_cuts)
{
    std::vector<Wound> wounds = {woundrules::make(WoundType::Fracture, BodyPart::RightArm)};
    DP_CHECK(woundrules::effects(wounds).noTwoHanded);
    DP_CHECK(woundrules::effects(wounds).armHeldIn);
    woundrules::apply(woundrules::Treatment::Splint, wounds.front());
    DP_CHECK(!woundrules::effects(wounds).noTwoHanded);
    DP_CHECK(woundrules::effects(wounds).windupScale > 1.0f);

    // A deep arm cut opens again after a swing until it is stitched.
    std::vector<Wound> cut = {woundrules::make(WoundType::Cut, BodyPart::LeftArm, WoundDepth::Deep)};
    DP_CHECK(woundrules::effects(cut).rebleedOnSwing);
    DP_CHECK(woundrules::effects(cut).swingStaminaScale > 1.0f);
    woundrules::apply(woundrules::Treatment::Dress, cut.front());
    DP_CHECK(woundrules::effects(cut).rebleedOnSwing); // a dressing is not stitches
    DP_CHECK_NEAR(woundrules::effects(cut).swingStaminaScale, 1.0f, 0.001);
    woundrules::apply(woundrules::Treatment::Stitch, cut.front());
    DP_CHECK(!woundrules::effects(cut).rebleedOnSwing);

    // A deep torso wound bleeds slowly until stitched.
    std::vector<Wound> torso = {woundrules::make(WoundType::Cut, BodyPart::Torso, WoundDepth::Deep)};
    DP_CHECK(woundrules::effects(torso).slowBleedPerHour > 0.0f);
}

DP_TEST(wounds_heal_in_the_stated_hours)
{
    const Wound bruise = woundrules::make(WoundType::Bruise, BodyPart::Torso);
    DP_CHECK_NEAR(woundrules::healHours(bruise), 2.0f, 0.01);
    Wound deep = woundrules::make(WoundType::Cut, BodyPart::Torso, WoundDepth::Deep);
    DP_CHECK_NEAR(woundrules::healHours(deep), 30.0f, 0.01);
    deep.stitched = true;
    DP_CHECK_NEAR(woundrules::healHours(deep), 12.0f, 0.01);
    Wound broken = woundrules::make(WoundType::Fracture, BodyPart::LeftLeg);
    DP_CHECK(woundrules::healHours(broken) < 0.0f); // never, unsplinted
    broken.splinted = true;
    DP_CHECK_NEAR(woundrules::healHours(broken), 96.0f, 0.01);
    // Bed rest is quicker, a Slow Healer slower.
    DP_CHECK(woundrules::healRate(true, 0, 1.0f) > woundrules::healRate(false, 0, 1.0f));
    DP_CHECK(woundrules::healRate(false, 0, 1.5f) < woundrules::healRate(false, 0, 1.0f));
}

DP_TEST(wounds_max_health_penalty)
{
    std::vector<Wound> wounds = {woundrules::make(WoundType::Cut, BodyPart::Torso),
                                 woundrules::make(WoundType::Fracture, BodyPart::LeftLeg)};
    DP_CHECK_NEAR(woundrules::maxHealth(100.0f, wounds), 85.0f, 0.01);
}

DP_TEST(wounds_an_old_dressing_gets_dirty_at_twelve_hours)
{
    WoundWorld w;
    WoundSystem system;
    Wound cut = woundrules::make(WoundType::Cut, BodyPart::Torso, WoundDepth::Deep);
    woundrules::apply(woundrules::Treatment::Dress, cut);
    // Deep and unstitched: 30 hours to heal, so the dressing goes first.
    w.wounds().push_back(cut);
    w.hours(11.5f, system);
    DP_CHECK(!w.wounds().empty());
    DP_CHECK(w.wounds().front().dressed);
    w.hours(1.0f, system);
    DP_CHECK(!w.wounds().empty());
    DP_CHECK(!w.wounds().front().dressed);
    DP_CHECK(w.wounds().front().contaminated);
}

DP_TEST(wounds_dirty_bite_infects_and_only_infection_drives_illness_past_sixty)
{
    WoundWorld w;
    WoundSystem system;
    w.wounds().push_back(woundrules::make(WoundType::Bite, BodyPart::LeftArm, WoundDepth::Shallow, true));
    w.hours(3.5f, system);
    DP_CHECK(w.wounds().front().infected);
    DP_CHECK(w.world.getComponent<Needs>(w.player).illness > 0.0f);
    DP_CHECK(woundrules::feverDegrees(40.0f) > 0.5f);

    // Antibiotics cure it.
    DP_CHECK(woundrules::canTreat(woundrules::Treatment::CureInfection, w.wounds().front()));
    woundrules::apply(woundrules::Treatment::CureInfection, w.wounds().front());
    DP_CHECK(!w.wounds().front().infected);
}

DP_TEST(wounds_turning_only_in_hopeless)
{
    WoundWorld w;
    WoundSystem system;
    Wound bite = woundrules::make(WoundType::Bite, BodyPart::Torso, WoundDepth::Deep, true);
    w.wounds().push_back(bite);
    WoundContext grim;
    w.hours(30.0f, system, grim);
    DP_CHECK(!w.wounds().empty());
    DP_CHECK(!w.wounds().front().turning);

    WoundWorld h;
    WoundSystem hopelessSystem;
    h.wounds().push_back(bite);
    WoundContext hopeless;
    hopeless.hopeless = true;
    bool died = false;
    for (int i = 0; i < 60 * 60 && !died; ++i)
    {
        hopelessSystem.update(h.world, h.player, 1.0f, 12.0, hopeless);
        died = hopelessSystem.turnedToDeath();
    }
    DP_CHECK(!h.wounds().empty());
    DP_CHECK(h.wounds().front().turning);
    DP_CHECK(died); // within 3 + 24 + at most 24 hours
    DP_CHECK(!woundrules::canTreat(woundrules::Treatment::CureInfection, h.wounds().front()));
}

DP_TEST(wounds_treatment_picks_the_worst_it_can_help)
{
    std::vector<Wound> wounds = {woundrules::make(WoundType::Bruise, BodyPart::Torso),
                                 woundrules::make(WoundType::Cut, BodyPart::LeftArm, WoundDepth::Deep),
                                 woundrules::make(WoundType::Fracture, BodyPart::RightLeg)};
    DP_CHECK_EQ(woundrules::worstFor(woundrules::Treatment::Splint, wounds), 2);
    DP_CHECK_EQ(woundrules::worstFor(woundrules::Treatment::Stitch, wounds), 1);
    DP_CHECK_EQ(woundrules::worstFor(woundrules::Treatment::Dress, wounds), 1);
    DP_CHECK_EQ(woundrules::worstFor(woundrules::Treatment::CureInfection, wounds), -1);
    DP_CHECK(woundrules::treatmentFromName("stitch") == woundrules::Treatment::Stitch);
    DP_CHECK(woundrules::treatSeconds(woundrules::Treatment::Splint, 0) > woundrules::treatSeconds(woundrules::Treatment::Splint, 3));
}

DP_TEST(wounds_hud_names_worst_first)
{
    std::vector<Wound> wounds = {woundrules::make(WoundType::Bruise, BodyPart::Torso),
                                 woundrules::make(WoundType::Fracture, BodyPart::LeftLeg)};
    Wound bite = woundrules::make(WoundType::Bite, BodyPart::LeftArm);
    bite.infected = true;
    wounds.push_back(bite);
    const std::vector<std::string> labels = woundrules::hudLabels(wounds, true, 30.0f);
    DP_CHECK(labels.size() >= 3u);
    DP_CHECK(labels[0] == "Bleeding");
    DP_CHECK(labels[1] == "Infected bite");
    DP_CHECK(std::find(labels.begin(), labels.end(), "Broken leg") != labels.end());
    DP_CHECK(std::find(labels.begin(), labels.end(), "Fever") != labels.end());
    DP_CHECK(woundrules::label(wounds[1]) == "Broken left leg");
}
