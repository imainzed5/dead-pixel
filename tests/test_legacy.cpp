// Legacy rules: who rises, how leads are ordered and described.
#include "TestFramework.h"

#include <string>

#include "legacy/LegacyRules.h"

DP_TEST(legacy_zombie_deaths_and_infected_bites_rise)
{
    DP_CHECK(legacy::shouldRise("Killed by zombies on Day 3", false));
    DP_CHECK(legacy::shouldRise("Overrun at the corner store", false));
    DP_CHECK(legacy::shouldRise("Succumbed to infection on Day 5", false));
    DP_CHECK(!legacy::shouldRise("Starved on Day 9", false));
    DP_CHECK(!legacy::shouldRise("Bled out on Day 2", false));
    DP_CHECK(!legacy::shouldRise("Burned alive on Day 4", false));
    // An infected bite turns you whatever finished you off.
    DP_CHECK(legacy::shouldRise("Bled out on Day 2", true));
}

DP_TEST(legacy_screen_directions_match_the_iso_view)
{
    const glm::vec2 o(1000.0f, 1000.0f);
    // World -x and -y together is straight up on screen.
    DP_CHECK_EQ(std::string(legacy::screenDirection(o, o + glm::vec2(-100.0f, -100.0f))), std::string("N"));
    DP_CHECK_EQ(std::string(legacy::screenDirection(o, o + glm::vec2(100.0f, 100.0f))), std::string("S"));
    DP_CHECK_EQ(std::string(legacy::screenDirection(o, o + glm::vec2(100.0f, -100.0f))), std::string("E"));
    DP_CHECK_EQ(std::string(legacy::screenDirection(o, o + glm::vec2(-100.0f, 100.0f))), std::string("W"));
    // World +x alone runs down-right on screen.
    DP_CHECK_EQ(std::string(legacy::screenDirection(o, o + glm::vec2(100.0f, 0.0f))), std::string("SE"));
    DP_CHECK_EQ(std::string(legacy::screenDirection(o, o + glm::vec2(0.0f, -100.0f))), std::string("NE"));
    DP_CHECK_EQ(std::string(legacy::screenDirection(o, o)), std::string("here"));
    DP_CHECK_EQ(legacy::metresBetween(o, o + glm::vec2(96.0f, 128.0f), 32.0f), 5);
}

DP_TEST(legacy_leads_put_the_risen_first_then_packs_graves_survivors)
{
    std::vector<GraveRecord> graves(3);
    graves[0].name = "Old Bones";
    graves[0].runIndex = 1;
    graves[1].name = "Has Pack";
    graves[1].runIndex = 2;
    graves[1].items = {{0, 1}};
    graves[2].name = "Walker";
    graves[2].runIndex = 3;
    graves[2].risen = true;
    std::vector<RetirementRecord> retirements(1);
    retirements[0].name = "Quitter";
    retirements[0].runIndex = 4;

    const auto leads = legacy::collectLeads(graves, retirements);
    DP_CHECK_EQ(leads.size(), std::size_t{4});
    DP_CHECK(leads[0].kind == legacy::LeadKind::Risen);
    DP_CHECK_EQ(leads[0].graveIndex, 2);
    DP_CHECK(leads[1].kind == legacy::LeadKind::Pack);
    DP_CHECK(leads[2].kind == legacy::LeadKind::Grave);
    DP_CHECK(leads[3].kind == legacy::LeadKind::Survivor);
    DP_CHECK_EQ(leads[3].graveIndex, -1);
}

DP_TEST(legacy_lead_lines_read_naturally)
{
    legacy::Lead lead;
    lead.kind = legacy::LeadKind::Risen;
    lead.name = "Hayden Stone";
    lead.world = glm::vec2(0.0f, -320.0f);
    DP_CHECK_EQ(legacy::describeLead(lead, glm::vec2(0.0f), 32.0f), std::string("Hayden Stone walks again - 10 m NE"));
    lead.kind = legacy::LeadKind::Pack;
    lead.world = glm::vec2(16.0f, 0.0f);
    DP_CHECK_EQ(legacy::describeLead(lead, glm::vec2(0.0f), 32.0f),
                std::string("Hayden Stone's grave, pack still there - right here"));
    DP_CHECK_EQ(legacy::runHeadline(4, 3), std::string("Run 4 - three came before you"));
    DP_CHECK_EQ(legacy::runHeadline(14, 13), std::string("Run 14 - 13 came before you"));
}
