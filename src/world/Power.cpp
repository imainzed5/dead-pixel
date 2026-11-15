#include "world/Power.h"

#include <cmath>

namespace
{
std::uint32_t hash(std::uint32_t a, std::uint32_t b)
{
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

double unit(std::uint32_t h)
{
    return static_cast<double>(h >> 8) / 16777216.0;
}
} // namespace

namespace power
{
double failHour(std::uint32_t runSeed)
{
    const double shift = (unit(hash(runSeed, 0x504Fu)) * 2.0 - 1.0) * kMaxShiftHours;
    return static_cast<double>(kFailDay) * 24.0 + kFailHourOfDay + shift;
}

double flickerStart(std::uint32_t runSeed, int day, int k)
{
    if (day < kFlickerFromDay || k < 0 || k >= kFlickersPerDay) return -1.0;
    // Spread through the waking day, one in each third of 7:00 to 23:00.
    const double span = 16.0 / static_cast<double>(kFlickersPerDay);
    const double within = unit(hash(runSeed ^ static_cast<std::uint32_t>(day * 977), 0xF11Cu + static_cast<std::uint32_t>(k)));
    return static_cast<double>(day) * 24.0 + 7.0 + span * static_cast<double>(k) + within * (span - kFlickerHours);
}

bool flickering(double hour, std::uint32_t runSeed)
{
    if (hour >= failHour(runSeed)) return false;
    const int day = static_cast<int>(std::floor(hour / 24.0));
    for (int k = 0; k < kFlickersPerDay; ++k)
    {
        const double start = flickerStart(runSeed, day, k);
        if (start >= 0.0 && hour >= start && hour < start + kFlickerHours) return true;
    }
    return false;
}

bool on(double hour, std::uint32_t runSeed)
{
    return hour < failHour(runSeed) && !flickering(hour, runSeed);
}

bool onWith(double hour, std::uint32_t runSeed, int forced)
{
    if (forced == 0) return false;
    if (forced == 1) return true;
    return on(hour, runSeed);
}
} // namespace power
