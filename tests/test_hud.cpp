// HUD conditions list: only problems are shown, worst first, and the
// thresholds line up with the needs model.
#include "TestFramework.h"

#include <algorithm>
#include <string>
#include <vector>

#include "systems/NeedsSystem.h"
#include "ui/Hud.h"

namespace
{
bool hasLabel(const std::vector<HudStatus>& statuses, const std::string& label)
{
    return std::any_of(statuses.begin(), statuses.end(),
                       [&](const HudStatus& s) { return s.label == label; });
}

HudSeverity severityOf(const std::vector<HudStatus>& statuses, const std::string& label)
{
    for (const HudStatus& s : statuses)
    {
        if (s.label == label)
        {
            return s.severity;
        }
    }
    return HudSeverity::Info;
}
} // namespace

DP_TEST(hud_healthy_character_shows_no_conditions)
{
    const HudConditionInputs healthy;
    DP_CHECK(Hud::buildConditions(healthy).empty());
}

DP_TEST(hud_thirst_escalates_with_needs_thresholds)
{
    HudConditionInputs in;
    in.thirst = 40.0f;
    DP_CHECK(hasLabel(Hud::buildConditions(in), "Thirsty"));

    in.thirst = NeedsSystem::kThirstyBelow - 1.0f;
    auto statuses = Hud::buildConditions(in);
    DP_CHECK(hasLabel(statuses, "Very thirsty"));
    DP_CHECK(!hasLabel(statuses, "Thirsty"));

    in.thirst = NeedsSystem::kParchedBelow - 1.0f;
    statuses = Hud::buildConditions(in);
    DP_CHECK(hasLabel(statuses, "Parched"));
    DP_CHECK(severityOf(statuses, "Parched") == HudSeverity::Critical);
}

DP_TEST(hud_temperature_bands_match_needs_model)
{
    HudConditionInputs in;
    in.bodyTemperature = NeedsSystem::kChillyBelow - 0.1f;
    DP_CHECK(hasLabel(Hud::buildConditions(in), "Chilly"));
    in.bodyTemperature = NeedsSystem::kColdBelow - 0.1f;
    DP_CHECK(hasLabel(Hud::buildConditions(in), "Cold"));
    in.bodyTemperature = NeedsSystem::kHypothermicBelow - 0.1f;
    DP_CHECK(hasLabel(Hud::buildConditions(in), "Hypothermic"));
    in.bodyTemperature = NeedsSystem::kOverheatedAbove + 0.1f;
    DP_CHECK(hasLabel(Hud::buildConditions(in), "Overheated"));
}

DP_TEST(hud_conditions_sorted_most_severe_first)
{
    HudConditionInputs in;
    in.hunger = 40.0f;   // Peckish: mild
    in.bleeding = true;  // critical
    in.panic = 55.0f;    // Scared: serious
    in.desensitisation = 60.0f; // Numb: info
    const auto statuses = Hud::buildConditions(in);
    DP_CHECK_EQ(statuses.size(), static_cast<std::size_t>(4));
    for (std::size_t i = 1; i < statuses.size(); ++i)
    {
        DP_CHECK(static_cast<int>(statuses[i - 1].severity) >= static_cast<int>(statuses[i].severity));
    }
    DP_CHECK(statuses.front().label == "Bleeding");
    DP_CHECK(statuses.back().label == "Numb");
}

DP_TEST(hud_sleeping_hides_tiredness)
{
    HudConditionInputs in;
    in.sleepDebt = 95.0f;
    DP_CHECK(hasLabel(Hud::buildConditions(in), "Exhausted"));
    in.sleeping = true;
    const auto statuses = Hud::buildConditions(in);
    DP_CHECK(!hasLabel(statuses, "Exhausted"));
    DP_CHECK(hasLabel(statuses, "Asleep"));
}

DP_TEST(hud_infection_replaces_bitten)
{
    HudConditionInputs in;
    in.bites = 1;
    DP_CHECK(hasLabel(Hud::buildConditions(in), "Bitten"));
    in.infected = true;
    const auto statuses = Hud::buildConditions(in);
    DP_CHECK(hasLabel(statuses, "Infected"));
    DP_CHECK(!hasLabel(statuses, "Bitten"));
}

DP_TEST(hud_crisis_overrides_depression_label)
{
    HudConditionInputs in;
    in.depression = 99.0f;
    DP_CHECK(hasLabel(Hud::buildConditions(in), "Despairing"));
    in.inCrisis = true;
    const auto statuses = Hud::buildConditions(in);
    DP_CHECK(hasLabel(statuses, "In crisis"));
    DP_CHECK(!hasLabel(statuses, "Despairing"));
}

DP_TEST(hud_weapon_slot_label_shows_wear_and_hand)
{
    HudSlot knife;
    knife.empty = false;
    knife.name = "Knife";
    knife.wear = 1.0f;
    knife.equipped = true;
    DP_CHECK(Hud::slotLabel(knife) == "Knife - in hand");
    knife.wear = 0.5f;
    DP_CHECK(Hud::slotLabel(knife) == "Knife - worn - in hand");
    knife.equipped = false;
    knife.wear = 0.0f;
    DP_CHECK(Hud::slotLabel(knife) == "Knife - broken");
    HudSlot food;
    food.empty = false;
    food.name = "Canned Food";
    DP_CHECK(Hud::slotLabel(food) == "Canned Food");
}
