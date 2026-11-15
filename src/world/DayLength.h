#pragma once

// Day length (phase 13): 24, 36 or 48 real minutes per game day, set per
// world when it is made. The game clock and everything written "per second"
// that means "per game minute" run on game seconds: real seconds times
// scale(). Real-time feel (combat, movement, noise, stamina) does not scale.
// The two groups are listed in core/Tuning.h.
namespace daylength
{
constexpr int kDefaultMinutes = 24;
constexpr int kChoices[3] = {24, 36, 48};

[[nodiscard]] constexpr int clampMinutes(int minutes)
{
    return minutes == 36 || minutes == 48 ? minutes : kDefaultMinutes;
}

// Game seconds per real second: 1 at 24 minutes, 0.5 at 48.
[[nodiscard]] constexpr float scale(int dayMinutes)
{
    return 24.0f / static_cast<float>(clampMinutes(dayMinutes));
}

// Real seconds in a game hour: 60, 90 or 120 (kGameHourSeconds).
[[nodiscard]] constexpr double gameHourSeconds(int dayMinutes)
{
    return static_cast<double>(clampMinutes(dayMinutes)) * 60.0 / 24.0;
}

// Game hours per real second. Exactly 1/60 at the default.
[[nodiscard]] constexpr double hoursPerSecond(int dayMinutes)
{
    return 1.0 / gameHourSeconds(dayMinutes);
}
} // namespace daylength
