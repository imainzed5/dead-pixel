// The journal (phase 10): the words in journal_lines.json, how one is chosen,
// the three-a-day rule, the retirement's summary and the line of time passing.
#include "TestFramework.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "legacy/Journal.h"

namespace
{
journal::Lines shippedLines()
{
    journal::Lines lines;
    std::string error;
    const bool loaded = journal::loadLines(DEAD_PIXEL_ASSETS_DIR "/data/journal_lines.json", lines, &error);
    if (!loaded) std::printf("  journal_lines.json: %s\n", error.c_str());
    DP_CHECK(loaded);
    return lines;
}

journal::Entry entry(int day, journal::Kind kind, int rank, const char* text)
{
    return journal::Entry{day, kind, rank, text};
}
} // namespace

DP_TEST(journal_lines_file_has_three_or_four_ways_to_say_each_thing)
{
    const journal::Lines lines = shippedLines();
    for (int k = 0; k < journal::kKindCount; ++k)
    {
        const auto& pool = lines.lines[static_cast<std::size_t>(k)];
        DP_CHECK(pool.size() >= 3 && pool.size() <= 4);
        DP_CHECK(lines.rank[static_cast<std::size_t>(k)] > 0);
        for (const std::string& line : pool)
        {
            // The voice: no exclamation marks, no "you".
            DP_CHECK(line.find('!') == std::string::npos);
            DP_CHECK(line.find(" you ") == std::string::npos && line.rfind("You ", 0) != 0);
        }
    }
    // The ends outrank everything a day can hold.
    DP_CHECK(lines.rank[static_cast<std::size_t>(journal::Kind::EndSudden)] >
             lines.rank[static_cast<std::size_t>(journal::Kind::Bitten)]);
}

DP_TEST(journal_writes_fill_in_names_and_numbers)
{
    const journal::Lines lines = shippedLines();
    for (std::uint32_t seed = 0; seed < 40; ++seed)
    {
        const std::string met = journal::write(journal::Event{2, journal::Kind::SurvivorMet, "Ana", "", 0}, seed, lines);
        DP_CHECK(met.find("Ana") != std::string::npos);
        DP_CHECK(met.find('{') == std::string::npos);
        const std::string days = journal::write(journal::Event{10, journal::Kind::Days, "", "", 10}, seed, lines);
        DP_CHECK(days.find("Ten") != std::string::npos || days.find("10") != std::string::npos);
        const std::string bite = journal::write(journal::Event{3, journal::Kind::Bitten, "arm", "", 0}, seed, lines);
        DP_CHECK(bite.find("arm") != std::string::npos);
    }
    // A world and a day choose the same words again; others vary.
    const journal::Event kill{2, journal::Kind::FirstKill, "", "", 0};
    DP_CHECK_EQ(journal::write(kill, 77u, lines), journal::write(kill, 77u, lines));
    std::set<std::string> seen;
    for (std::uint32_t seed = 0; seed < 64; ++seed) seen.insert(journal::write(kill, seed, lines));
    DP_CHECK(seen.size() >= 3);
}

DP_TEST(journal_keeps_three_a_day_and_the_most_important)
{
    std::vector<journal::Entry> book;
    DP_CHECK(journal::add(book, entry(1, journal::Kind::Start, 2, "start")));
    DP_CHECK(journal::add(book, entry(1, journal::Kind::Boarded, 4, "boards")));
    DP_CHECK(journal::add(book, entry(1, journal::Kind::FirstKill, 6, "kill")));
    // A fourth of lower rank than all: left out.
    DP_CHECK(!journal::add(book, entry(1, journal::Kind::SurvivorTraded, 1, "trade")));
    DP_CHECK_EQ(book.size(), std::size_t{3});
    // A fourth that matters more: the least of the day goes.
    DP_CHECK(journal::add(book, entry(1, journal::Kind::Bitten, 8, "bite")));
    DP_CHECK_EQ(book.size(), std::size_t{3});
    for (const journal::Entry& e : book) DP_CHECK(e.text != "start");
    // Another day has its own three.
    DP_CHECK(journal::add(book, entry(2, journal::Kind::FirstNight, 5, "night")));
    DP_CHECK_EQ(book.size(), std::size_t{4});
    // The last entry always stays, whatever the day holds.
    DP_CHECK(journal::add(book, entry(1, journal::Kind::EndSudden, 9, "stops")));
    DP_CHECK_EQ(book.size(), std::size_t{5});
    DP_CHECK(book.back().text == "stops");
    // Empty text is never written.
    DP_CHECK(!journal::add(book, entry(3, journal::Kind::Days, 5, "")));
}

DP_TEST(journal_number_words_and_the_retirement_summary)
{
    DP_CHECK_EQ(journal::numberWord(11), std::string("eleven"));
    DP_CHECK_EQ(journal::numberWord(31), std::string("thirty-one"));
    DP_CHECK_EQ(journal::numberWord(40), std::string("forty"));
    DP_CHECK_EQ(journal::numberWord(143), std::string("143"));
    DP_CHECK_EQ(journal::lifeSummary(journal::LifeStats{12, 4, 2, 31}),
                std::string("I stopped on day 12. I built four things, met two people and killed thirty-one of them."));
    DP_CHECK_EQ(journal::lifeSummary(journal::LifeStats{3, 0, 1, 1}),
                std::string("I stopped on day 3. I built nothing, met one person and killed one of them."));
    DP_CHECK_EQ(journal::timePassingLine(11), std::string("Eleven days since anyone lived here."));
    DP_CHECK(journal::timePassingLine(4).empty());
}

DP_TEST(journal_defaults_cover_every_kind_without_the_file)
{
    const journal::Lines lines = journal::defaults();
    for (int k = 0; k < journal::kKindCount; ++k)
    {
        DP_CHECK(!lines.lines[static_cast<std::size_t>(k)].empty());
        DP_CHECK(std::string(journal::key(static_cast<journal::Kind>(k))).size() > 2);
    }
    journal::Lines missing;
    DP_CHECK(!journal::loadLines("no/such/file.json", missing));
    DP_CHECK(!missing.lines[0].empty()); // the defaults stand
}
