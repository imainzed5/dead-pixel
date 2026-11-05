#include "TestFramework.h"

#include "components/Health.h"
#include "components/MentalState.h"
#include "components/Needs.h"
#include "components/SurvivorAI.h"
#include "components/Traits.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "ecs/World.h"
#include "noise/NoiseModel.h"
#include "systems/MentalStateSystem.h"

namespace
{
struct MentalFixture
{
    World world;
    NoiseModel noise;
    MentalStateSystem system;
    Entity player = kInvalidEntity;
    double hours = 12.0;
    int day = 1;

    MentalFixture()
    {
        world.registerComponent<Transform>();
        world.registerComponent<Health>();
        world.registerComponent<Needs>();
        world.registerComponent<MentalState>();
        world.registerComponent<Traits>();
        world.registerComponent<ZombieAI>();
        world.registerComponent<SurvivorAI>();

        player = world.createEntity();
        world.addComponent(player, Transform{});
        world.addComponent(player, Health{});
        world.addComponent(player, Needs{});
        world.addComponent(player, MentalState{});
        world.addComponent(player, Traits{});
    }

    MentalState& mental() { return world.getComponent<MentalState>(player); }

    // Advance one real second (1/60 game hour).
    void step()
    {
        system.update(world, noise, player, 1.0f, hours, day);
        hours += 1.0 / 60.0;
        if (hours >= 24.0)
        {
            hours -= 24.0;
            ++day;
        }
    }
};
} // namespace

DP_TEST(mental_crisis_starts_at_cap_and_retires_after_window)
{
    MentalFixture f;
    bool retired = false;
    int secondsToRetire = 0;
    for (int s = 0; s < 26 * 60 && !retired; ++s)
    {
        f.mental().depression = 100.0f; // held at the cap
        f.step();
        DP_CHECK(f.mental().inCrisis);
        retired = f.system.shouldForceRetire();
        secondsToRetire = s;
    }
    DP_CHECK(retired);
    // 24 game hours = 1440 real seconds.
    DP_CHECK(secondsToRetire >= 24 * 60 - 2 && secondsToRetire <= 24 * 60 + 2);
}

DP_TEST(mental_crisis_ends_when_depression_drops)
{
    MentalFixture f;
    f.mental().depression = 100.0f;
    f.step();
    DP_CHECK(f.mental().inCrisis);

    f.mental().depression = 70.0f;
    f.step();
    DP_CHECK(!f.mental().inCrisis);
    DP_CHECK(!f.system.shouldForceRetire());
}
