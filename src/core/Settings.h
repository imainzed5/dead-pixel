#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>

#include <nlohmann/json_fwd.hpp>

#include "core/Bindings.h"

// The player's settings (phase 13): one file, settings.json, next to the
// save slots. Sound, screen, game and keys, and the few things the game
// remembers per save about teaching (which prompts were shown, how often each
// death hint was). Pure data and rules; Game applies them.
namespace settings
{
enum class Fullscreen { Off, Borderless, Exclusive };
enum class Resolution { R1280x720, R1600x900, R1920x1080, Native };
// Auto picks by the window's height (uiScaleFor).
enum class UiSize { Auto, X1, X1_5, X2 };
enum class ScreenFx { Full, Reduced, Off };

constexpr int kLevelMax = 10; // sound levels and screen shake, 0-10

struct Settings
{
    // Sound, 0-10 each.
    int master = kLevelMax;
    int effects = kLevelMax;
    int ambience = kLevelMax;
    int music = kLevelMax;

    // Screen.
    Fullscreen fullscreen = Fullscreen::Off;
    Resolution resolution = Resolution::R1280x720;
    bool vsync = true;
    UiSize uiSize = UiSize::Auto;
    ScreenFx screenFx = ScreenFx::Full;
    int shake = kLevelMax;

    // Game.
    bool autoPause = true;     // pause when the window loses focus
    bool crouchToggle = false; // press once to crouch, again to stand
    bool runToggle = false;
    bool teaching = true;      // the one-line teaching prompts
    bool pressOnce = false;    // hold-to-act jobs start with one press and run on their own

    // Keys.
    bindings::Table keys = bindings::defaults();

    // Per save slot: teaching prompts already shown and death hints counted.
    std::map<std::string, std::set<std::string>> taught;
    std::map<std::string, std::map<std::string, int>> hintsShown;

    bool operator==(const Settings& other) const;
};

// The interface scale for a window this tall: up to 899 px 1x, up to 1299
// 1.5x, taller 2x; a chosen size wins.
[[nodiscard]] float uiScaleFor(UiSize size, int windowHeight);
// A resolution's size; Native is {0, 0} (the desktop's).
[[nodiscard]] std::pair<int, int> resolutionSize(Resolution resolution);
[[nodiscard]] float volume(int level);           // 0-10 -> 0..1
[[nodiscard]] float screenFxLevel(ScreenFx fx);  // full 1, reduced 0.5, off 0
[[nodiscard]] float shakeScale(int level);       // 0-10 -> 0..1

[[nodiscard]] const char* name(Fullscreen value);
[[nodiscard]] const char* name(Resolution value);
[[nodiscard]] const char* name(UiSize value);
[[nodiscard]] const char* name(ScreenFx value);

[[nodiscard]] nlohmann::json toJson(const Settings& settings);
// Missing or bad entries keep their defaults. Reads phase 12's file too
// (volumes 0..1 under "volume", "screenEffects").
[[nodiscard]] Settings fromJson(const nlohmann::json& json);

// The file: read (defaults when missing or unreadable) and written atomically
// (a side file renamed over it, the last one kept as settings.backup.json).
[[nodiscard]] Settings load(const std::string& path);
bool save(const std::string& path, const Settings& settings);
} // namespace settings
