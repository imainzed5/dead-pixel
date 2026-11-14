// Power (phase 6): on until day 5 at 19:00 give or take 12 hours by the
// run's seed, dips from day 4, and a harness override.
#include "TestFramework.h"

#include "world/Power.h"

DP_TEST(power_fails_on_day_five_near_dusk)
{
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        const double fail = power::failHour(seed);
        DP_CHECK(fail >= 5.0 * 24.0 + 19.0 - 12.0);
        DP_CHECK(fail <= 5.0 * 24.0 + 19.0 + 12.0);
        // The same seed, the same hour.
        DP_CHECK_NEAR(fail, power::failHour(seed), 0.0);
        // On the morning of day 1, on; a day after it fails, off.
        DP_CHECK(power::on(1.0 * 24.0 + 8.0, seed));
        DP_CHECK(!power::on(fail + 0.01, seed));
        DP_CHECK(!power::on(fail + 24.0, seed));
    }
    // Seeds really do move it.
    DP_CHECK(power::failHour(1) != power::failHour(2));
}

DP_TEST(power_dips_from_day_four_only)
{
    for (std::uint32_t seed = 1; seed <= 50; ++seed)
    {
        // Days 1 to 3: never a dip.
        for (double hour = 24.0; hour < 4.0 * 24.0; hour += 1.0 / 60.0)
        {
            DP_CHECK(!power::flickering(hour, seed));
        }
        // Day 4: three dips, each a few minutes, in waking hours.
        int dips = 0;
        for (int k = 0; k < power::kFlickersPerDay; ++k)
        {
            const double start = power::flickerStart(seed, 4, k);
            DP_CHECK(start >= 4.0 * 24.0 + 7.0);
            DP_CHECK(start + power::kFlickerHours <= 4.0 * 24.0 + 23.0 + 0.001);
            if (start < power::failHour(seed))
            {
                DP_CHECK(power::flickering(start + power::kFlickerHours * 0.5, seed));
                DP_CHECK(!power::on(start + power::kFlickerHours * 0.5, seed));
                ++dips;
            }
        }
        DP_CHECK(dips >= 1);
        DP_CHECK(power::flickerStart(seed, 3, 0) < 0.0);
    }
}

DP_TEST(power_override_wins)
{
    const std::uint32_t seed = 7;
    const double late = power::failHour(seed) + 48.0;
    DP_CHECK(power::onWith(late, seed, 1));
    DP_CHECK(!power::onWith(30.0, seed, 0));
    DP_CHECK(power::onWith(30.0, seed, -1) == power::on(30.0, seed));
    DP_CHECK_NEAR(power::kFlashlightHours, 8.0f, 0.0001);
    DP_CHECK_NEAR(power::kRadioHours, 20.0f, 0.0001);
}
