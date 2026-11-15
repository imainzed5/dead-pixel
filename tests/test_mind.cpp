// A mind that shows (phase 5): panic and depression bands, sleep bands and
// coffee, and the false signs, which never touch the simulation.
#include "TestFramework.h"

#include <cmath>

#include "components/Health.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "ecs/World.h"
#include "noise/NoiseModel.h"
#include "systems/HallucinationSystem.h"
#include "world/MindRules.h"
#include "world/SleepRules.h"

DP_TEST(mind_panic_bands_and_effects)
{
    DP_CHECK(mindrules::panicBand(10.0f) == mindrules::PanicBand::Calm);
    DP_CHECK(mindrules::panicBand(30.0f) == mindrules::PanicBand::Uneasy);
    DP_CHECK(mindrules::panicBand(60.0f) == mindrules::PanicBand::Shaken);
    DP_CHECK(mindrules::panicBand(80.0f) == mindrules::PanicBand::Panicking);
    DP_CHECK(mindrules::panicBand(95.0f) == mindrules::PanicBand::Breaking);
    // Panic 60: held actions (boarding) take 30 % longer.
    DP_CHECK_NEAR(mindrules::heldActionScale(mindrules::panicBand(60.0f)), 1.3f, 0.001);
    DP_CHECK_NEAR(mindrules::heldActionScale(mindrules::panicBand(30.0f)), 1.0f, 0.001);
    DP_CHECK_NEAR(mindrules::staminaCostScale(mindrules::PanicBand::Uneasy), 1.1f, 0.001);
    DP_CHECK(mindrules::canSleep(mindrules::PanicBand::Shaken));
    DP_CHECK(!mindrules::canSleep(mindrules::PanicBand::Panicking));
    DP_CHECK(mindrules::fumbleChance(mindrules::PanicBand::Panicking) > 0.0f);
    // Every other step is louder when Shaken.
    DP_CHECK(mindrules::louderStep(mindrules::PanicBand::Shaken, 1) != mindrules::louderStep(mindrules::PanicBand::Shaken, 2));
    DP_CHECK(!mindrules::louderStep(mindrules::PanicBand::Uneasy, 1));
}

DP_TEST(mind_breaking_loses_control_once_a_minute)
{
    using mindrules::PanicBand;
    DP_CHECK(mindrules::losesControl(PanicBand::Breaking, true, 0.9f, 61.0f));
    DP_CHECK(mindrules::losesControl(PanicBand::Breaking, false, 0.2f, 61.0f));
    DP_CHECK(!mindrules::losesControl(PanicBand::Breaking, false, 0.9f, 61.0f)); // neither grabbed nor hurt
    DP_CHECK(!mindrules::losesControl(PanicBand::Breaking, true, 0.9f, 30.0f));  // too soon
    DP_CHECK(!mindrules::losesControl(PanicBand::Panicking, true, 0.1f, 61.0f));
}

DP_TEST(mind_depression_bands_and_numbness)
{
    using mindrules::DepressionBand;
    DP_CHECK(mindrules::depressionBand(35.0f) == DepressionBand::Low);
    DP_CHECK(mindrules::depressionBand(55.0f) == DepressionBand::WornDown);
    DP_CHECK(mindrules::depressionBand(75.0f) == DepressionBand::Numb);
    DP_CHECK(mindrules::depressionBand(95.0f) == DepressionBand::GivingUp);
    DP_CHECK_NEAR(mindrules::restScale(DepressionBand::Low), 0.9f, 0.001);
    DP_CHECK_NEAR(mindrules::comfortScale(DepressionBand::WornDown), 0.5f, 0.001);
    DP_CHECK_NEAR(mindrules::moveScale(DepressionBand::Numb), 0.95f, 0.001);
    DP_CHECK(!mindrules::canBuild(DepressionBand::GivingUp));
    DP_CHECK(!mindrules::canLootBodies(DepressionBand::GivingUp));
    DP_CHECK(mindrules::canBuild(DepressionBand::Numb));
    // Kills weigh only past twenty.
    DP_CHECK_NEAR(mindrules::killWeight(10, 80.0f), 0.0f, 0.001);
    DP_CHECK_NEAR(mindrules::killWeight(25, 100.0f), 0.3f, 0.001);
    // Numbness fades a point a day once a day has passed without a kill.
    DP_CHECK_NEAR(mindrules::desensitisationFade(10.0f, 1.0f), 0.0f, 0.001);
    DP_CHECK_NEAR(mindrules::desensitisationFade(30.0f, 24.0f), 1.0f, 0.001);
    // Hardened (above 60): no first-kill fright, and one zombie cannot make you Uneasy.
    DP_CHECK(mindrules::firstKillFrightens(30.0f));
    DP_CHECK(!mindrules::firstKillFrightens(70.0f));
    DP_CHECK(mindrules::singleZombiePanicCap(70.0f) < 25.0f);
}

DP_TEST(mind_sleep_bands_and_coffee)
{
    using sleeprules::Band;
    DP_CHECK(sleeprules::band(40.0f) == Band::Fine);
    DP_CHECK(sleeprules::band(60.0f) == Band::Drowsy);
    DP_CHECK(sleeprules::band(75.0f) == Band::Tired);
    DP_CHECK(sleeprules::band(90.0f) == Band::Exhausted);
    DP_CHECK(sleeprules::band(97.0f) == Band::DeadOnFeet);
    DP_CHECK(sleeprules::microsleeps(Band::DeadOnFeet));
    DP_CHECK(!sleeprules::microsleeps(Band::Exhausted));
    DP_CHECK_NEAR(sleeprules::staminaCapScale(Band::Exhausted), 0.75f, 0.001);
    const float interval = sleeprules::microsleepInterval(0.5f);
    DP_CHECK(interval >= 40.0f && interval <= 90.0f);
    // Coffee hides 20 for its hours; the real debt is untouched.
    DP_CHECK_NEAR(sleeprules::shownDebt(80.0f, 2.0f), 60.0f, 0.001);
    DP_CHECK_NEAR(sleeprules::shownDebt(80.0f, 0.0f), 80.0f, 0.001);
    DP_CHECK(sleeprules::doubleCoffee(10.0, 6.0));
    DP_CHECK(!sleeprules::doubleCoffee(20.0, 6.0));
    // A full night (6 h at 0.8) leaves you Rested; a sedative spoils a night.
    DP_CHECK(sleeprules::fullSleep(6.5f, 0.85f));
    DP_CHECK(!sleeprules::fullSleep(5.0f, 0.95f));
    DP_CHECK_NEAR(sleeprules::sleepQuality(0.9f, true), 0.75f, 0.001);
}

DP_TEST(mind_seeing_things_conditions)
{
    DP_CHECK(mindrules::seesThings(90.0f, 0.0f, 37.0f));
    DP_CHECK(mindrules::seesThings(0.0f, 61.0f, 37.0f));
    DP_CHECK(mindrules::seesThings(0.0f, 0.0f, 39.5f));
    DP_CHECK(!mindrules::seesThings(80.0f, 30.0f, 38.0f));
}

DP_TEST(mind_false_signs_come_and_never_touch_the_world)
{
    // A world with the dead in it and a noise model: neither is handed to the
    // hallucination system, and neither changes while it runs.
    World world;
    world.registerComponent<Transform>();
    world.registerComponent<Health>();
    world.registerComponent<ZombieAI>();
    NoiseModel noise;
    for (int i = 0; i < 3; ++i)
    {
        const Entity z = world.createEntity();
        Transform t{};
        t.x = 100.0f + 40.0f * static_cast<float>(i);
        t.y = 100.0f;
        world.addComponent(z, t);
        world.addComponent(z, Health{});
        world.addComponent(z, ZombieAI{});
    }
    const auto snapshot = [&]()
    {
        float sum = 0.0f;
        int count = 0;
        world.forEach<Transform, Health>(
            [&](Entity, Transform& t, Health& h)
            {
                sum += t.x + t.y + h.current;
                ++count;
            });
        return std::make_pair(count, sum);
    };
    const auto before = snapshot();
    const std::uint64_t noiseBefore = noise.emittedTotal();

    HallucinationSystem signs;
    signs.reset(42u);
    HallucinationView view;
    view.player = glm::vec2(500.0f, 500.0f);
    view.aim = glm::vec2(1.0f, 0.0f);
    const auto place = [&](FalseSign::Kind, float a, float b) -> std::optional<glm::vec2>
    {
        const float angle = a * 6.2831853f;
        return view.player + glm::vec2(std::cos(angle), std::sin(angle)) * (192.0f + 96.0f * b);
    };
    for (int i = 0; i < 120 * 60; ++i) signs.update(1.0f / 60.0f, true, view, place);
    DP_CHECK(signs.silhouettesShown() >= 1); // the first sign of a spell is a figure
    DP_CHECK(signs.signsShown() >= 2);       // every 30 to 80 s

    const auto after = snapshot();
    DP_CHECK_EQ(after.first, before.first);
    DP_CHECK_NEAR(after.second, before.second, 0.0001);
    DP_CHECK_EQ(noise.emittedTotal(), noiseBefore);

    // Not seeing things: nothing new comes.
    HallucinationSystem calm;
    calm.reset(42u);
    for (int i = 0; i < 200 * 60; ++i) calm.update(1.0f / 60.0f, false, view, place);
    DP_CHECK_EQ(calm.signsShown(), 0);
}

DP_TEST(mind_a_figure_fades_when_looked_at)
{
    HallucinationSystem signs;
    signs.reset(7u);
    HallucinationView view;
    view.player = glm::vec2(0.0f);
    view.aim = glm::vec2(1.0f, 0.0f);
    // Always straight ahead, 6 tiles off.
    const auto place = [](FalseSign::Kind, float, float) -> std::optional<glm::vec2> { return glm::vec2(192.0f, 0.0f); };
    bool sawFigure = false;
    bool fadedBeforeItsTime = false;
    for (int i = 0; i < 100 * 60; ++i)
    {
        signs.update(1.0f / 60.0f, true, view, place);
        for (const FalseSign& sign : signs.signs())
        {
            if (sign.kind != FalseSign::Kind::Silhouette) continue;
            sawFigure = true;
            if (sign.fading && sign.age < HallucinationSystem::kSilhouetteSeconds - 1.0f) fadedBeforeItsTime = true;
        }
    }
    DP_CHECK(sawFigure);
    DP_CHECK(fadedBeforeItsTime);
}
