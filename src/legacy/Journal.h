#pragma once

// The journal (phase 10): what a character writes in their notebook as the
// life goes on, one or two short sentences an event, in the past tense. The
// words are in assets/data/journal_lines.json, three or four ways of saying
// each thing. No engine dependencies, so the rules are unit-tested directly.

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace journal
{

enum class Kind
{
    Start,          // the first page
    FirstNight,     // the first night lived through
    PowerOut,       // the lights went
    TapsDry,        // the water stopped
    FirstKill,
    Bitten,         // a: where
    Wounded,        // a: where, b: what (a cut, a scratch)
    Boarded,        // a: "door" or "window"
    BaseBuilt,      // a: what was built
    SurvivorMet,    // a: their name
    SurvivorTraded, // a: their name
    SurvivorLost,   // a: their name
    HordePassed,
    ClueLearned,
    GraveFound,     // a: whose; n: the day they fell
    LaidToRest,     // a: whose
    StoneSet,       // a: whose marker was set in the cemetery
    Days,           // n: ten, twenty, ... days
    SeasonTurned,   // a: the season
    LowPoint,       // a crisis began
    // The last entry: always kept.
    EndSudden,      // killed outright
    EndSlow,        // hunger, thirst, fever
    EndRetired,     // gave up; the stats are written in code
    EndStayed,      // stayed for good
    EndEscaped,     // went out by the gate
    Count
};
constexpr int kKindCount = static_cast<int>(Kind::Count);
constexpr int kEntriesPerDay = 3;

// The key in journal_lines.json ("first_night").
[[nodiscard]] const char* key(Kind kind);
[[nodiscard]] bool isEnd(Kind kind);

struct Event
{
    int day = 1;
    Kind kind = Kind::Start;
    std::string a;
    std::string b;
    int n = 0;
};

struct Entry
{
    int day = 1;
    Kind kind = Kind::Start;
    int rank = 0;
    std::string text;
};

struct Lines
{
    std::array<std::vector<std::string>, kKindCount> lines;
    std::array<int, kKindCount> rank{};
};

// The built-in words (the same as journal_lines.json ships).
[[nodiscard]] Lines defaults();
// Reads journal_lines.json over the defaults. False (and `error`) when it cannot.
bool loadLines(const std::string& path, Lines& out, std::string* error = nullptr);

// One of the phrasings for the event, chosen by the seed and the day, with
// {a}, {b}, {n}, {day}, {nw} (n as a word) and {Nw} (capitalised) filled in.
[[nodiscard]] std::string write(const Event& event, std::uint32_t seed, const Lines& lines);
// The entry for an event.
[[nodiscard]] Entry entryFor(const Event& event, std::uint32_t seed, const Lines& lines);

// Adds an entry. At most kEntriesPerDay a day: past that the least important
// of the day goes (the newer when ranks tie); the last entry always stays.
// Returns false when the new entry was the one left out.
bool add(std::vector<Entry>& journal, const Entry& entry);

// "eleven", "thirty-one"; numerals past ninety-nine.
[[nodiscard]] std::string numberWord(int n);
[[nodiscard]] std::string capitalised(std::string text);

// The retirement's last entry, from the life's numbers: "I stopped on day
// 12. I built four things, met two people and killed thirty-one of them."
struct LifeStats
{
    int day = 1;
    int built = 0;
    int met = 0;
    int kills = 0;
};
[[nodiscard]] std::string lifeSummary(const LifeStats& stats);

// The run-start card's line of time passing, for a last life of five days or
// more: "Eleven days since anyone lived here." Empty under five.
[[nodiscard]] std::string timePassingLine(int daysLived);

} // namespace journal
