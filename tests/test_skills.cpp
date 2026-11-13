// Skills and traits (phase 5): XP and levels, forgetting, the per-level
// effects, the crafting soft gate, and trait points.
#include "TestFramework.h"

#include "components/Skills.h"
#include "components/Traits.h"
#include "world/SkillRules.h"
#include "world/TraitRules.h"

DP_TEST(skills_levels_follow_the_thresholds)
{
    DP_CHECK_EQ(skillrules::levelForXp(0.0f), 0);
    DP_CHECK_EQ(skillrules::levelForXp(99.0f), 0);
    DP_CHECK_EQ(skillrules::levelForXp(100.0f), 1);
    DP_CHECK_EQ(skillrules::levelForXp(250.0f), 2);
    DP_CHECK_EQ(skillrules::levelForXp(1500.0f), 5);
    DP_CHECK_EQ(skillrules::levelForXp(99999.0f), 5);
    DP_CHECK_NEAR(skillrules::xpForLevel(3), 500.0f, 0.01);
}

DP_TEST(skills_ten_boards_make_carpentry_one_and_boarding_faster)
{
    Skills skills;
    const float before = skillrules::boardSeconds(skills.of(Skill::Carpentry));
    bool leveled = false;
    for (int i = 0; i < 10; ++i)
    {
        const skillrules::XpResult result = skillrules::awardXp(skills, Skill::Carpentry, skillrules::kXpBoard, 10.0 + i);
        leveled = leveled || result.leveledUp;
    }
    DP_CHECK(leveled);
    DP_CHECK_EQ(skills.of(Skill::Carpentry), 1);
    const float after = skillrules::boardSeconds(skills.of(Skill::Carpentry));
    DP_CHECK(after < before);
    DP_CHECK_NEAR(before, 4.0f, 0.001);
    // Nine boards are not yet enough.
    Skills nine;
    for (int i = 0; i < 9; ++i) skillrules::awardXp(nine, Skill::Carpentry, skillrules::kXpBoard, 0.0);
    DP_CHECK_EQ(nine.of(Skill::Carpentry), 0);
}

DP_TEST(skills_forget_slowly_after_three_days_but_keep_the_level)
{
    Skills skills;
    skillrules::setLevel(skills, Skill::Fighting, 2, 0.0);
    skillrules::awardXp(skills, Skill::Fighting, 100.0f, 0.0); // 350 xp, level 2
    skillrules::decay(skills, 48.0, 1.0f);                       // two days: nothing
    DP_CHECK_NEAR(skills.xp[static_cast<std::size_t>(Skill::Fighting)], 350.0f, 0.01);
    for (int h = 73; h < 200; ++h) skillrules::decay(skills, static_cast<double>(h), 1.0f);
    DP_CHECK(skills.xp[static_cast<std::size_t>(Skill::Fighting)] < 350.0f);
    DP_CHECK_EQ(skills.of(Skill::Fighting), 2);
    DP_CHECK(skills.xp[static_cast<std::size_t>(Skill::Fighting)] >= skillrules::xpForLevel(2));
}

DP_TEST(skills_effects_are_rule_functions)
{
    DP_CHECK(!skillrules::canReinforceBoards(2));
    DP_CHECK(skillrules::canReinforceBoards(3));
    DP_CHECK(skillrules::salvagesNails(2));
    DP_CHECK_EQ(skillrules::extraPlanksFromFurniture(4), 1);
    DP_CHECK(skillrules::boardStrengthScale(5) > skillrules::boardStrengthScale(0));
    DP_CHECK(skillrules::swingStaminaScale(5) < 1.0f);
    DP_CHECK_NEAR(skillrules::doubleBlowChance(4), 0.0f, 0.001);
    DP_CHECK(skillrules::doubleBlowChance(5) > 0.0f);
    DP_CHECK(skillrules::seesInfectionEarly(2));
    DP_CHECK(!skillrules::antibioticsHalfDose(3));
    DP_CHECK(skillrules::antibioticsHalfDose(4));
}

DP_TEST(skills_names_parse)
{
    DP_CHECK(skillrules::parse("carpentry") == Skill::Carpentry);
    DP_CHECK(skillrules::parse("Medicine") == Skill::Medicine);
    DP_CHECK(skillrules::parse("land") == Skill::LivingOffTheLand);
    DP_CHECK(!skillrules::parse("juggling").has_value());
}

DP_TEST(skills_craft_soft_gate)
{
    // At or above the recipe's level: as written. Below: twice as long, 40 % fail.
    DP_CHECK_NEAR(skillrules::craftTimeScale(1, 1), 1.0f, 0.001);
    DP_CHECK_NEAR(skillrules::craftTimeScale(0, 1), 2.0f, 0.001);
    DP_CHECK_NEAR(skillrules::craftFailChance(2, 1), 0.0f, 0.001);
    DP_CHECK_NEAR(skillrules::craftFailChance(0, 1), 0.4f, 0.001);
}

DP_TEST(traits_points_and_limits)
{
    Traits traits;
    DP_CHECK_EQ(traitrules::points(traits), 0);
    DP_CHECK(traitrules::canBegin(traits));
    traitrules::toggle(traits, PositiveTrait::Tough); // -3
    DP_CHECK_EQ(traitrules::points(traits), -3);
    DP_CHECK(!traitrules::canBegin(traits)); // overspent
    traitrules::toggle(traits, NegativeTrait::Frail); // +3
    DP_CHECK(traitrules::canBegin(traits));
    // Toggling again takes it off.
    traitrules::toggle(traits, PositiveTrait::Tough);
    DP_CHECK(!traits.has(PositiveTrait::Tough));

    // Three of each, at most.
    Traits full;
    DP_CHECK(traitrules::toggle(full, NegativeTrait::Clumsy));
    DP_CHECK(traitrules::toggle(full, NegativeTrait::Hungry));
    DP_CHECK(traitrules::toggle(full, NegativeTrait::Loner));
    DP_CHECK(!traitrules::toggle(full, NegativeTrait::Nervous));
    DP_CHECK_EQ(full.negatives.size(), 3u);
}

DP_TEST(traits_starting_numbers)
{
    Traits traits;
    traits.positives = {PositiveTrait::Tough, PositiveTrait::StrongBack, PositiveTrait::Handy};
    DP_CHECK_NEAR(traitrules::maxHealth(traits), 120.0f, 0.01);
    DP_CHECK_NEAR(traitrules::carryLimitKg(traits, 0), 31.0f, 0.01);
    DP_CHECK_NEAR(traitrules::carryLimitKg(traits, 2), 34.0f, 0.01);
    const auto levels = traitrules::startingLevels(traits);
    DP_CHECK_EQ(levels[static_cast<std::size_t>(Skill::Carpentry)], 2);
    Traits frail;
    frail.negatives = {NegativeTrait::Frail};
    DP_CHECK_NEAR(traitrules::maxHealth(frail), 80.0f, 0.01);
}

DP_TEST(traits_quick_start_is_balanced_and_seeded)
{
    for (std::uint32_t seed = 1; seed < 200; seed += 7)
    {
        const Traits a = traitrules::quickStart(seed);
        const Traits b = traitrules::quickStart(seed);
        DP_CHECK_EQ(a.positives.size(), 1u);
        DP_CHECK_EQ(a.negatives.size(), 1u);
        DP_CHECK(traitrules::canBegin(a));
        DP_CHECK(a.positives == b.positives && a.negatives == b.negatives);
    }
}

DP_TEST(traits_old_saves_keep_their_pick_and_names_parse)
{
    // The seed pick from before the character screen: one of the first three each.
    const Traits old = traitrules::legacyPick(1234u);
    DP_CHECK_EQ(old.positives.size(), 1u);
    DP_CHECK(static_cast<int>(old.positives.front()) >= 1 && static_cast<int>(old.positives.front()) <= 3);
    DP_CHECK(static_cast<int>(old.negatives.front()) >= 1 && static_cast<int>(old.negatives.front()) <= 3);
    DP_CHECK(traitrules::parsePositive("strong_back") == PositiveTrait::StrongBack);
    DP_CHECK(traitrules::parsePositive("Light Sleeper") == PositiveTrait::LightSleeper);
    DP_CHECK(traitrules::parseNegative("weakstomach") == NegativeTrait::WeakStomach);
    DP_CHECK(!traitrules::parsePositive("clumsy").has_value());
}
