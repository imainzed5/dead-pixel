#pragma once

#include <cstdint>

// The town's power (phase 6): on until day 5 at 19:00, moved by up to 12
// hours either way by the run's seed. From day 4 it dips a few times a day
// for a few seconds. Each life starts in a town with power (per run, not
// world memory). Hours here are absolute game hours: day * 24 + hour.
namespace power
{
constexpr int kFailDay = 5;
constexpr double kFailHourOfDay = 19.0;
constexpr double kMaxShiftHours = 12.0;
constexpr int kFlickerFromDay = 4;
constexpr int kFlickersPerDay = 3;
// A dip lasts about five real seconds (one real second is a game minute).
constexpr double kFlickerHours = 5.0 / 60.0;

// Batteries: game hours of use in one.
constexpr float kFlashlightHours = 8.0f;
constexpr float kRadioHours = 20.0f;

// When the power goes for good this run.
[[nodiscard]] double failHour(std::uint32_t runSeed);
// The start of the k-th dip of `day` (k < kFlickersPerDay), or a negative
// number when that day has no dips (before day 4).
[[nodiscard]] double flickerStart(std::uint32_t runSeed, int day, int k);
// The lights are dipping at `hour` (only before the power fails).
[[nodiscard]] bool flickering(double hour, std::uint32_t runSeed);
// The power is on at `hour`: before it fails and not in a dip.
[[nodiscard]] bool on(double hour, std::uint32_t runSeed);
// A harness or dev override: -1 none, 0 forced off, 1 forced on.
[[nodiscard]] bool onWith(double hour, std::uint32_t runSeed, int forced);
} // namespace power
