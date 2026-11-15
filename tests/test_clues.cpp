// The way out's clues (phase 9): which source teaches which lead, what the
// living tell, an earlier life's notebook, the objective line and the
// notebook's page, the words in clues.json, and where the readables lie in
// real towns (and that each one can be reached).
#include "TestFramework.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "map/MapGenerator.h"
#include "world/BuildingKinds.h"
#include "world/Clues.h"
#include "world/Furniture.h"
#include "world/Gate.h"

namespace
{
LayoutData stampedTown(std::uint32_t seed)
{
    MapGenerator generator;
    LayoutData layout = generator.generateLayout(seed);
    gate::stamp(layout, 0);
    return layout;
}

bool hasSource(const std::vector<clues::Readable>& readables, clues::Source source)
{
    return std::any_of(readables.begin(), readables.end(), [&](const clues::Readable& r) { return r.source == source; });
}
} // namespace

DP_TEST(clue_sources_teach_their_leads)
{
    using clues::Source;
    DP_CHECK_EQ(clues::teaches(Source::FridgeNote), clues::bit(clues::WayOut));
    DP_CHECK_EQ(clues::teaches(Source::Broadcast), clues::bit(clues::WayOut));
    DP_CHECK_EQ(clues::teaches(Source::WallMap), clues::bit(clues::Where));
    DP_CHECK_EQ(clues::teaches(Source::Nearby), clues::bit(clues::Where));
    DP_CHECK_EQ(clues::teaches(Source::ControlBox), clues::bit(clues::Power));
    DP_CHECK(clues::knows(clues::teaches(Source::LogBook), clues::Power));
    DP_CHECK(clues::knows(clues::teaches(Source::LogBook), clues::Sergeant));
    DP_CHECK_EQ(clues::teaches(Source::DutyBoard), clues::bit(clues::Sergeant));
    DP_CHECK_EQ(clues::teaches(Source::OrderBook), clues::bit(clues::Fuse));
    DP_CHECK_EQ(clues::teaches(Source::GarageNote), clues::bit(clues::Fuse));
    DP_CHECK_EQ(clues::teaches(Source::Manual), clues::bit(clues::Fixable));
    // Every lead has somewhere to be learned.
    clues::Mask all = 0;
    for (int s = 0; s <= static_cast<int>(Source::Nearby); ++s) all |= clues::teaches(static_cast<Source>(s));
    DP_CHECK_EQ(all, clues::all());
    DP_CHECK_EQ(clues::count(clues::all()), clues::kLeadCount);
}

DP_TEST(clue_survivors_tell_what_they_heard)
{
    // Trust 10 or more: lead 1; a competent one (over 0.6) the gate's side too.
    DP_CHECK_EQ(clues::fromSurvivor(0, 5.0f, 0.9f), clues::Mask{0});
    DP_CHECK_EQ(clues::fromSurvivor(0, 10.0f, 0.5f), clues::bit(clues::WayOut));
    DP_CHECK_EQ(clues::fromSurvivor(0, 10.0f, 0.7f), static_cast<clues::Mask>(clues::bit(clues::WayOut) | clues::bit(clues::Where)));
    // Lead 1 known already: a competent one adds the side even at low trust.
    DP_CHECK_EQ(clues::fromSurvivor(clues::bit(clues::WayOut), 0.0f, 0.7f), clues::bit(clues::Where));
    // Nothing new: nothing told.
    DP_CHECK_EQ(clues::fromSurvivor(clues::all(), 50.0f, 1.0f), clues::Mask{0});
}

DP_TEST(clue_notebook_carries_leads_and_the_newer_picture)
{
    clues::Mask known = clues::bit(clues::WayOut);
    clues::Seen seen;
    gate::State g;
    g.fuseFitted = true;
    g.fuel = 2;
    const clues::Seen written = clues::seenFrom(g, 2, 6);
    clues::transfer(known, seen, static_cast<clues::Mask>(clues::bit(clues::Where) | clues::bit(clues::Fuse)), written);
    DP_CHECK(clues::knows(known, clues::WayOut) && clues::knows(known, clues::Where) && clues::knows(known, clues::Fuse));
    DP_CHECK(seen.valid && seen.fuse);
    DP_CHECK_EQ(seen.fuel, 2);
    // An older picture does not replace a newer one.
    gate::State older;
    clues::transfer(known, seen, 0, clues::seenFrom(older, 1, 9));
    DP_CHECK_EQ(seen.run, 2);
    DP_CHECK_EQ(seen.fuel, 2);
    // The page says how old it is.
    const clues::Content content = clues::defaults();
    const clues::Names names = clues::names(content, 1234u, 0);
    const std::vector<std::string> page = clues::pageLines(known, seen, content, names, 3, 1);
    const bool aged = std::any_of(page.begin(), page.end(), [](const std::string& line)
                                  { return line.find("Fuel: 2 of 4, as of day 6 of an earlier life.") != std::string::npos; });
    DP_CHECK(aged);
}

DP_TEST(clue_objective_follows_what_is_known)
{
    const clues::Content content = clues::defaults();
    const clues::Names names = clues::names(content, 42u, 2);
    clues::Facts facts;
    clues::Seen seen;
    DP_CHECK_EQ(clues::objective(0, seen, names, facts), std::string("Find out why nobody has left."));
    DP_CHECK_EQ(clues::objective(clues::bit(clues::WayOut), seen, names, facts), "Find the service gate, " + names.side + " side.");
    const auto two = static_cast<clues::Mask>(clues::bit(clues::WayOut) | clues::bit(clues::Where));
    DP_CHECK_EQ(clues::objective(two, seen, names, facts), std::string("Look at the gate's control box."));
    gate::State g;
    seen = clues::seenFrom(g, 1, 1);
    const auto three = static_cast<clues::Mask>(two | clues::bit(clues::Power));
    DP_CHECK_EQ(clues::objective(three, seen, names, facts), std::string("Find a gate fuse."));
    DP_CHECK_EQ(clues::objective(static_cast<clues::Mask>(three | clues::bit(clues::Fuse)), seen, names, facts),
                std::string("Get a fuse from the hardware store."));
    g.fuseFitted = true;
    g.freed = true;
    g.fuel = 4;
    seen = clues::seenFrom(g, 1, 2);
    DP_CHECK_EQ(clues::objective(three, seen, names, facts), std::string("Find the gate key."));
    DP_CHECK_EQ(clues::objective(static_cast<clues::Mask>(three | clues::bit(clues::Sergeant)), seen, names, facts),
                "Find the Sergeant, " + names.street + ".");
    facts.keyInHand = true;
    DP_CHECK_EQ(clues::objective(three, seen, names, facts), std::string("Start the gate motor."));
}

DP_TEST(clue_words_load_and_follow_the_voice)
{
    clues::Content content;
    std::string error;
    const bool loaded = clues::loadContent(DEAD_PIXEL_ASSETS_DIR "/data/clues.json", content, &error);
    if (!loaded) std::printf("  clues.json: %s\n", error.c_str());
    DP_CHECK(loaded);
    DP_CHECK_EQ(content.broadcast.size(), std::size_t{3});
    DP_CHECK(content.sergeants.size() >= 6);
    const clues::Names names = clues::names(content, 777u, 1);
    DP_CHECK_EQ(names.side, std::string("west"));
    for (int s = 0; s <= static_cast<int>(clues::Source::Manual); ++s)
    {
        const std::string text = clues::fill(clues::readableText(content, static_cast<clues::Source>(s)), names);
        DP_CHECK(!text.empty());
        DP_CHECK(text.find('{') == std::string::npos);
        DP_CHECK(text.find('!') == std::string::npos);
    }
    for (const std::string& lead : content.leads)
    {
        DP_CHECK(!lead.empty());
        DP_CHECK(clues::fill(lead, names).find('!') == std::string::npos);
    }
    // The same world names the same Sergeant.
    DP_CHECK_EQ(clues::names(content, 777u, 1).sergeant, names.sergeant);
}

DP_TEST(clue_readables_lie_in_real_towns_within_reach)
{
    for (const std::uint32_t seed : {1234u, 42u, 777u})
    {
        const LayoutData layout = stampedTown(seed);
        DP_CHECK(layout.gate.placed);
        const auto pieces = furniture::furnish(layout, seed);
        // The first life's home always has its note.
        const int home = buildingkinds::buildingAt(layout, layout.playerStart);
        const std::vector<clues::Readable> readables = clues::place(pieces, layout, home);
        DP_CHECK(hasSource(readables, clues::Source::LogBook)); // in the hut
        DP_CHECK(hasSource(readables, clues::Source::FridgeNote));
        const bool homeNote = std::any_of(readables.begin(), readables.end(), [&](const clues::Readable& r)
                                          { return r.source == clues::Source::FridgeNote && r.building == home; });
        DP_CHECK(homeNote);
        // Each lead can be learned in this town, from something to read or the box and the radio.
        clues::Mask taught = static_cast<clues::Mask>(clues::teaches(clues::Source::ControlBox) | clues::teaches(clues::Source::Broadcast) |
                                                      clues::teaches(clues::Source::Nearby));
        for (const clues::Readable& r : readables) taught |= clues::teaches(r.source);
        const bool everyLead = clues::count(taught) == clues::kLeadCount;
        if (!everyLead) std::printf("  seed %u: only %d leads to learn\n", seed, clues::count(taught));
        // The Sergeant's lead (duty board or log book) and the box's are guaranteed; the
        // fuse and the generator need a hardware store and a garage.
        DP_CHECK(clues::knows(taught, clues::Sergeant) && clues::knows(taught, clues::Power));

        // Every readable can be stood beside: a free floor tile next to it,
        // reachable from the start on foot.
        const auto at = [&](const glm::ivec2& t) { return static_cast<std::size_t>(t.y * layout.width + t.x); };
        std::vector<std::uint8_t> taken(static_cast<std::size_t>(layout.width * layout.height), 0);
        for (const FurniturePiece& p : pieces) taken[at(p.tile)] = 1;
        std::vector<std::uint8_t> seen(taken.size(), 0);
        std::vector<glm::ivec2> stack{layout.playerStart};
        seen[at(layout.playerStart)] = 1;
        const glm::ivec2 dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        while (!stack.empty())
        {
            const glm::ivec2 t = stack.back();
            stack.pop_back();
            for (const glm::ivec2& d : dirs)
            {
                const glm::ivec2 n = t + d;
                if (n.x < 0 || n.y < 0 || n.x >= layout.width || n.y >= layout.height) continue;
                if (layout.collision[at(n)] != 0 || taken[at(n)] != 0 || seen[at(n)] != 0) continue;
                seen[at(n)] = 1;
                stack.push_back(n);
            }
        }
        for (const clues::Readable& r : readables)
        {
            bool reach = false;
            for (const glm::ivec2& d : dirs)
            {
                const glm::ivec2 n = r.tile + d;
                if (n.x >= 0 && n.y >= 0 && n.x < layout.width && n.y < layout.height && seen[at(n)] != 0) reach = true;
            }
            if (!reach) std::printf("  seed %u: %s at %d,%d cannot be reached\n", seed, clues::sourceKey(r.source), r.tile.x, r.tile.y);
            DP_CHECK(reach);
        }
        // The fuse cabinet, where there is a hardware store, is a piece that holds things.
        const glm::ivec2 cabinet = clues::fuseCabinet(pieces, layout);
        bool hardware = false;
        for (const auto& b : layout.buildings) hardware = hardware || buildingkinds::kindOf(b) == BuildingKind::Hardware;
        DP_CHECK_EQ(cabinet.x >= 0, hardware);
    }
}
