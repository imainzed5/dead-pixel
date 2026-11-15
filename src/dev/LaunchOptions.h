#pragma once

#include <cstdint>
#include <optional>
#include <string>

// Command-line options. With no arguments the game launches normally.
//
//   --script FILE        run a dev-harness script (enables harness mode; no sound)
//   --shots DIR          screenshot output directory (default: shots)
//   --metrics FILE       write a CSV of player/world metrics
//   --world-seed N       seed for newly created save slots (deterministic worlds)
//   --hidden             keep the window hidden (screenshots still work)
//   --watch              render every tick to the window (watch the script run)
//   --max-ticks N        hard stop after N simulation ticks (with or without a script)
//   --mute               open no audio device (the mixer still decides what would play)
//   --demo               the demo: a life ends at dawn on day 7 (also: demo.txt beside the exe)
struct LaunchOptions
{
    std::string scriptPath;
    std::string shotDir = "shots";
    std::string metricsPath;
    std::optional<std::uint32_t> worldSeed;
    bool hidden = false;
    bool watch = false;
    bool mute = false;
    bool demo = false;
    int maxTicks = 0;

    [[nodiscard]] bool harnessEnabled() const { return !scriptPath.empty(); }

    // Returns false and fills `error` on invalid arguments.
    static bool parse(int argc, char* argv[], LaunchOptions& out, std::string& error);
};
