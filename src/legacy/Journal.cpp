#include "legacy/Journal.h"

#include <algorithm>
#include <cctype>
#include <fstream>

#include <nlohmann/json.hpp>

namespace journal
{
namespace
{
constexpr std::array<const char*, kKindCount> kKeys = {
    "start",        "first_night",  "power_out",   "taps_dry",        "first_kill",     "bitten",
    "wounded",      "boarded",      "base_built",  "survivor_met",    "survivor_traded", "survivor_lost",
    "horde_passed", "clue_learned", "grave_found", "laid_to_rest",    "stone_set",      "days",
    "season_turned", "low_point",   "end_sudden",  "end_slow",        "end_retired",    "end_stayed",
    "end_escaped"};

void replaceAll(std::string& text, const std::string& from, const std::string& to)
{
    if (from.empty()) return;
    std::size_t at = 0;
    while ((at = text.find(from, at)) != std::string::npos)
    {
        text.replace(at, from.size(), to);
        at += to.size();
    }
}

std::uint32_t mix(std::uint32_t h)
{
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}
} // namespace

const char* key(Kind kind)
{
    const int i = static_cast<int>(kind);
    return i >= 0 && i < kKindCount ? kKeys[static_cast<std::size_t>(i)] : "";
}

bool isEnd(Kind kind)
{
    return kind == Kind::EndSudden || kind == Kind::EndSlow || kind == Kind::EndRetired || kind == Kind::EndStayed ||
           kind == Kind::EndEscaped;
}

Lines defaults()
{
    // One plain line each; the file has the rest.
    Lines out;
    const auto set = [&](Kind kind, int rank, const char* line)
    {
        out.rank[static_cast<std::size_t>(kind)] = rank;
        out.lines[static_cast<std::size_t>(kind)] = {line};
    };
    set(Kind::Start, 2, "Found this notebook in a drawer. I am going to write down what happens.");
    set(Kind::FirstNight, 5, "Got through the first night. Something tried the door around three.");
    set(Kind::PowerOut, 4, "The lights went. It is very dark.");
    set(Kind::TapsDry, 4, "The taps ran dry today. Every bottle counts now.");
    set(Kind::FirstKill, 6, "I killed one today. It was wearing a wedding ring.");
    set(Kind::Bitten, 8, "Bitten on the {a}. Cleaned it. We will see.");
    set(Kind::Wounded, 5, "A {b} on the {a}. Cleaned it as best I could.");
    set(Kind::Boarded, 4, "Nailed boards across the {a}. Now I will hear them coming.");
    set(Kind::BaseBuilt, 4, "Finished the {a} today. The place feels more like mine.");
    set(Kind::SurvivorMet, 5, "Met {a}. They would not come outside. We talked through the window.");
    set(Kind::SurvivorTraded, 3, "Traded with {a}. I think I got the worse of it.");
    set(Kind::SurvivorLost, 7, "{a} is dead. I keep thinking about it.");
    set(Kind::HordePassed, 6, "They went through in the night. I counted until I could not.");
    set(Kind::ClueLearned, 5, "There is a gate. Of course there is a gate.");
    set(Kind::GraveFound, 6, "Found {a}'s grave. I stood there a while.");
    set(Kind::LaidToRest, 7, "Found {a} walking. I did what they would have wanted.");
    set(Kind::StoneSet, 6, "Set {a}'s stone with the others. Said the name out loud.");
    set(Kind::Days, 5, "{Nw} days. I have stopped counting cans and started counting days.");
    set(Kind::SeasonTurned, 3, "It is {a} now. The air changed overnight.");
    set(Kind::LowPoint, 5, "I sat on the stairs all afternoon.");
    set(Kind::EndSudden, 9, "The writing stops here.");
    set(Kind::EndSlow, 9, "I am very tired. I will write more tomorrow.");
    set(Kind::EndRetired, 9, "I am not going on. I will stay here.");
    set(Kind::EndStayed, 9, "I am staying. The house is warm enough and the door holds.");
    set(Kind::EndEscaped, 9, "The gate opened. I went out.");
    return out;
}

bool loadLines(const std::string& path, Lines& out, std::string* error)
{
    out = defaults();
    std::ifstream in(path);
    if (!in)
    {
        if (error != nullptr) *error = "cannot open";
        return false;
    }
    try
    {
        nlohmann::json json;
        in >> json;
        const nlohmann::json& kinds = json.at("kinds");
        for (int i = 0; i < kKindCount; ++i)
        {
            const char* k = kKeys[static_cast<std::size_t>(i)];
            if (!kinds.contains(k)) continue;
            const nlohmann::json& kj = kinds[k];
            out.rank[static_cast<std::size_t>(i)] = kj.value("rank", out.rank[static_cast<std::size_t>(i)]);
            std::vector<std::string> lines;
            if (kj.contains("lines") && kj["lines"].is_array())
            {
                for (const auto& line : kj["lines"])
                {
                    if (line.is_string() && !line.get<std::string>().empty()) lines.push_back(line.get<std::string>());
                }
            }
            if (!lines.empty()) out.lines[static_cast<std::size_t>(i)] = std::move(lines);
        }
    }
    catch (const std::exception& e)
    {
        out = defaults();
        if (error != nullptr) *error = e.what();
        return false;
    }
    return true;
}

std::string numberWord(int n)
{
    static constexpr std::array<const char*, 20> kSmall = {
        "zero",    "one",     "two",       "three",    "four",     "five",    "six",
        "seven",   "eight",   "nine",      "ten",      "eleven",   "twelve",  "thirteen",
        "fourteen", "fifteen", "sixteen",  "seventeen", "eighteen", "nineteen"};
    static constexpr std::array<const char*, 10> kTens = {
        "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
    if (n < 0 || n > 99) return std::to_string(n);
    if (n < 20) return kSmall[static_cast<std::size_t>(n)];
    std::string word = kTens[static_cast<std::size_t>(n / 10)];
    if (n % 10 != 0) word += std::string("-") + kSmall[static_cast<std::size_t>(n % 10)];
    return word;
}

std::string capitalised(std::string text)
{
    if (!text.empty()) text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
    return text;
}

std::string write(const Event& event, std::uint32_t seed, const Lines& lines)
{
    const auto i = static_cast<std::size_t>(std::clamp(static_cast<int>(event.kind), 0, kKindCount - 1));
    const std::vector<std::string>& pool = lines.lines[i];
    if (pool.empty()) return {};
    const std::uint32_t h = mix(seed ^ mix(static_cast<std::uint32_t>(i) * 0x9E3779B9u + static_cast<std::uint32_t>(event.day) * 0x85EBCA6Bu));
    std::string text = pool[h % pool.size()];
    replaceAll(text, "{Nw}", capitalised(numberWord(event.n)));
    replaceAll(text, "{nw}", numberWord(event.n));
    replaceAll(text, "{n}", std::to_string(event.n));
    replaceAll(text, "{day}", std::to_string(event.day));
    replaceAll(text, "{A}", capitalised(event.a));
    replaceAll(text, "{a}", event.a);
    replaceAll(text, "{b}", event.b);
    return text;
}

Entry entryFor(const Event& event, std::uint32_t seed, const Lines& lines)
{
    Entry entry;
    entry.day = event.day;
    entry.kind = event.kind;
    entry.rank = lines.rank[static_cast<std::size_t>(std::clamp(static_cast<int>(event.kind), 0, kKindCount - 1))];
    entry.text = write(event, seed, lines);
    return entry;
}

bool add(std::vector<Entry>& journal, const Entry& entry)
{
    if (entry.text.empty()) return false;
    journal.push_back(entry);
    if (isEnd(entry.kind)) return true;
    // The day's entries, not counting a last entry.
    std::vector<std::size_t> today;
    for (std::size_t i = 0; i < journal.size(); ++i)
    {
        if (journal[i].day == entry.day && !isEnd(journal[i].kind)) today.push_back(i);
    }
    if (static_cast<int>(today.size()) <= kEntriesPerDay) return true;
    // The least important goes; of equals, the newest.
    std::size_t drop = today.front();
    for (const std::size_t i : today)
    {
        if (journal[i].rank <= journal[drop].rank) drop = i;
    }
    const bool newDropped = drop == journal.size() - 1;
    journal.erase(journal.begin() + static_cast<std::ptrdiff_t>(drop));
    return !newDropped;
}

std::string lifeSummary(const LifeStats& stats)
{
    const auto count = [](int n, const char* one, const char* many)
    {
        return n == 1 ? std::string("one ") + one : numberWord(n) + " " + many;
    };
    std::string text = "I stopped on day " + std::to_string(stats.day) + ".";
    std::vector<std::string> parts;
    parts.push_back(stats.built > 0 ? "built " + count(stats.built, "thing", "things") : "built nothing");
    parts.push_back(stats.met > 0 ? "met " + count(stats.met, "person", "people") : "met nobody");
    parts.push_back(stats.kills > 0 ? "killed " + (stats.kills == 1 ? std::string("one of them") : numberWord(stats.kills) + " of them")
                                    : "killed none of them");
    text += " I " + parts[0] + ", " + parts[1] + " and " + parts[2] + ".";
    return text;
}

std::string timePassingLine(int daysLived)
{
    if (daysLived < 5) return {};
    return capitalised(numberWord(daysLived)) + " days since anyone lived here.";
}

} // namespace journal
