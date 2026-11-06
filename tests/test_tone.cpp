// Tone presets: each one leans the run a clear way from the GRIM HUMAN baseline.
#include "TestFramework.h"

#include "systems/ToneTuning.h"

DP_TEST(tone_grim_human_is_the_baseline)
{
    const ToneTuning grim = toneTuning(SaveTonePreset::GrimHuman);
    const ToneTuning defaults{};
    DP_CHECK_EQ(grim.suppliesScale, defaults.suppliesScale);
    DP_CHECK_EQ(grim.zombieHealthScale, defaults.zombieHealthScale);
    DP_CHECK_EQ(grim.nightDarkness, defaults.nightDarkness);
    DP_CHECK_EQ(grim.saturation, defaults.saturation);
}

DP_TEST(tone_hopeless_is_harsher_and_arcade_is_kinder)
{
    const ToneTuning hopeless = toneTuning(SaveTonePreset::Hopeless);
    const ToneTuning grim = toneTuning(SaveTonePreset::GrimHuman);
    const ToneTuning arcade = toneTuning(SaveTonePreset::BrutalArcade);

    DP_CHECK(hopeless.suppliesScale < grim.suppliesScale);
    DP_CHECK(grim.suppliesScale < arcade.suppliesScale);
    DP_CHECK(hopeless.zombieHealthScale > grim.zombieHealthScale);
    DP_CHECK(arcade.zombieHealthScale < grim.zombieHealthScale);
    DP_CHECK(hopeless.nightDarkness > grim.nightDarkness);
    DP_CHECK(arcade.nightDarkness < grim.nightDarkness);
    DP_CHECK(hopeless.saturation < 1.0f);
    // Arcade trades weaker dead for more of them.
    DP_CHECK(arcade.zombieCountScale > grim.zombieCountScale);
    DP_CHECK(arcade.weaponScale > grim.weaponScale);
}
