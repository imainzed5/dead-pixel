#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

// Named numbers that later phases tune, read from assets/data/tuning.json.
// Every caller passes its own default, so a missing file or key changes
// nothing. Only numbers that are tuned live here; everything else stays a
// named constant next to its rule.
//
// Keys in use (default):
//   night.callers.base            2     callers per night for nights 1-3, times the night
//   night.callers.growth          1     callers added each night after night 3
//   doors.board.hp                450   health of one board
//   threat.migration.dayInterval  70    real seconds between migration waves by day
//   threat.migration.nightInterval 35   ... and by night
//   threat.spawn.interval         90    real seconds between population top-ups
//   threat.growth.daily           0.18  population target growth per day
//   needs.hunger.perSecond        0.025 hunger lost per real second
//   needs.thirst.perSecond        0.045 thirst lost per real second
//   needs.sleepDebt.perSecond     0.058 sleep debt gained per real second awake
//   zombie.attack.damage          7     health a landed blow takes
//   zombie.attack.hitChance       0.6   chance a blow that reaches lands
//   loot.stocked.scale            1     multiplies every loot class's stockedChance
class Tuning
{
public:
    // Reads a flat JSON object of name -> number. A missing file is not an
    // error (all defaults); a malformed one is reported and ignored.
    static bool load(const std::string& path);
    [[nodiscard]] static float get(std::string_view key, float fallback);
    // Overrides one value for this process (harness `tune KEY VALUE`).
    static void set(std::string_view key, float value);
    static void clear();

private:
    static std::unordered_map<std::string, float>& values();
};
