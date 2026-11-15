#include "core/Settings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace settings
{
namespace
{
template <typename E, std::size_t N>
E parseEnum(const nlohmann::json& json, const char* key, const std::array<E, N>& values, E fallback)
{
    if (!json.contains(key) || !json[key].is_string()) return fallback;
    const std::string text = json[key].get<std::string>();
    for (const E value : values)
    {
        if (text == name(value)) return value;
    }
    return fallback;
}

int level(const nlohmann::json& json, const char* key, int fallback)
{
    if (!json.contains(key) || !json[key].is_number()) return fallback;
    return std::clamp(json[key].get<int>(), 0, kLevelMax);
}

bool flag(const nlohmann::json& json, const char* key, bool fallback)
{
    if (!json.contains(key) || !json[key].is_boolean()) return fallback;
    return json[key].get<bool>();
}

// Phase 12 wrote volumes as 0..1; phase 13 writes 0-10 under "sound".
int volumeLevel(const nlohmann::json& json, const char* key, int fallback)
{
    if (!json.contains(key) || !json[key].is_number()) return fallback;
    const float v = json[key].get<float>();
    return std::clamp(static_cast<int>(std::lround(v * static_cast<float>(kLevelMax))), 0, kLevelMax);
}

constexpr std::array<Fullscreen, 3> kFullscreens{Fullscreen::Off, Fullscreen::Borderless, Fullscreen::Exclusive};
constexpr std::array<Resolution, 4> kResolutions{Resolution::R1280x720, Resolution::R1600x900, Resolution::R1920x1080,
                                                 Resolution::Native};
constexpr std::array<UiSize, 4> kUiSizes{UiSize::Auto, UiSize::X1, UiSize::X1_5, UiSize::X2};
constexpr std::array<ScreenFx, 3> kScreenFxs{ScreenFx::Full, ScreenFx::Reduced, ScreenFx::Off};
} // namespace

bool Settings::operator==(const Settings& o) const
{
    return master == o.master && effects == o.effects && ambience == o.ambience && music == o.music &&
           fullscreen == o.fullscreen && resolution == o.resolution && vsync == o.vsync && uiSize == o.uiSize &&
           screenFx == o.screenFx && shake == o.shake && autoPause == o.autoPause && crouchToggle == o.crouchToggle &&
           runToggle == o.runToggle && teaching == o.teaching && pressOnce == o.pressOnce && keys == o.keys &&
           taught == o.taught && hintsShown == o.hintsShown;
}

float uiScaleFor(UiSize size, int windowHeight)
{
    switch (size)
    {
    case UiSize::X1: return 1.0f;
    case UiSize::X1_5: return 1.5f;
    case UiSize::X2: return 2.0f;
    case UiSize::Auto: break;
    }
    if (windowHeight <= 899) return 1.0f;
    if (windowHeight <= 1299) return 1.5f;
    return 2.0f;
}

std::pair<int, int> resolutionSize(Resolution resolution)
{
    switch (resolution)
    {
    case Resolution::R1280x720: return {1280, 720};
    case Resolution::R1600x900: return {1600, 900};
    case Resolution::R1920x1080: return {1920, 1080};
    case Resolution::Native: break;
    }
    return {0, 0};
}

float volume(int value)
{
    return static_cast<float>(std::clamp(value, 0, kLevelMax)) / static_cast<float>(kLevelMax);
}

float screenFxLevel(ScreenFx fx)
{
    switch (fx)
    {
    case ScreenFx::Full: return 1.0f;
    case ScreenFx::Reduced: return 0.5f;
    case ScreenFx::Off: break;
    }
    return 0.0f;
}

float shakeScale(int value)
{
    return volume(value);
}

const char* name(Fullscreen value)
{
    switch (value)
    {
    case Fullscreen::Off: return "off";
    case Fullscreen::Borderless: return "borderless";
    case Fullscreen::Exclusive: return "exclusive";
    }
    return "off";
}

const char* name(Resolution value)
{
    switch (value)
    {
    case Resolution::R1280x720: return "1280x720";
    case Resolution::R1600x900: return "1600x900";
    case Resolution::R1920x1080: return "1920x1080";
    case Resolution::Native: return "native";
    }
    return "1280x720";
}

const char* name(UiSize value)
{
    switch (value)
    {
    case UiSize::Auto: return "auto";
    case UiSize::X1: return "1x";
    case UiSize::X1_5: return "1.5x";
    case UiSize::X2: return "2x";
    }
    return "auto";
}

const char* name(ScreenFx value)
{
    switch (value)
    {
    case ScreenFx::Full: return "full";
    case ScreenFx::Reduced: return "reduced";
    case ScreenFx::Off: return "off";
    }
    return "full";
}

nlohmann::json toJson(const Settings& s)
{
    nlohmann::json json;
    json["version"] = 1;
    json["sound"] = {{"master", s.master}, {"effects", s.effects}, {"ambience", s.ambience}, {"music", s.music}};
    // Phase 12's readers looked here; kept so an older build still hears the levels.
    json["volume"] = {{"master", volume(s.master)},
                      {"effects", volume(s.effects)},
                      {"ambience", volume(s.ambience)},
                      {"music", volume(s.music)}};
    json["screen"] = {{"fullscreen", name(s.fullscreen)}, {"resolution", name(s.resolution)},
                      {"vsync", s.vsync},                 {"interfaceSize", name(s.uiSize)},
                      {"effects", name(s.screenFx)},      {"shake", s.shake}};
    json["screenEffects"] = name(s.screenFx);
    json["game"] = {{"autoPause", s.autoPause}, {"crouchToggle", s.crouchToggle}, {"runToggle", s.runToggle},
                    {"teaching", s.teaching},   {"pressOnce", s.pressOnce}};
    json["keys"] = bindings::toJson(s.keys);
    nlohmann::json taught = nlohmann::json::object();
    for (const auto& [slot, seen] : s.taught)
    {
        taught[slot] = nlohmann::json(std::vector<std::string>(seen.begin(), seen.end()));
    }
    json["taught"] = taught;
    nlohmann::json hints = nlohmann::json::object();
    for (const auto& [slot, counts] : s.hintsShown)
    {
        hints[slot] = nlohmann::json(counts);
    }
    json["deathHints"] = hints;
    return json;
}

Settings fromJson(const nlohmann::json& json)
{
    Settings s;
    if (!json.is_object()) return s;
    if (json.contains("sound") && json["sound"].is_object())
    {
        const nlohmann::json& v = json["sound"];
        s.master = level(v, "master", s.master);
        s.effects = level(v, "effects", s.effects);
        s.ambience = level(v, "ambience", s.ambience);
        s.music = level(v, "music", s.music);
    }
    else if (json.contains("volume") && json["volume"].is_object())
    {
        const nlohmann::json& v = json["volume"];
        s.master = volumeLevel(v, "master", s.master);
        s.effects = volumeLevel(v, "effects", s.effects);
        s.ambience = volumeLevel(v, "ambience", s.ambience);
        s.music = volumeLevel(v, "music", s.music);
    }
    s.screenFx = parseEnum(json, "screenEffects", kScreenFxs, s.screenFx);
    if (json.contains("screen") && json["screen"].is_object())
    {
        const nlohmann::json& v = json["screen"];
        s.fullscreen = parseEnum(v, "fullscreen", kFullscreens, s.fullscreen);
        s.resolution = parseEnum(v, "resolution", kResolutions, s.resolution);
        s.vsync = flag(v, "vsync", s.vsync);
        s.uiSize = parseEnum(v, "interfaceSize", kUiSizes, s.uiSize);
        s.screenFx = parseEnum(v, "effects", kScreenFxs, s.screenFx);
        s.shake = level(v, "shake", s.shake);
    }
    if (json.contains("game") && json["game"].is_object())
    {
        const nlohmann::json& v = json["game"];
        s.autoPause = flag(v, "autoPause", s.autoPause);
        s.crouchToggle = flag(v, "crouchToggle", s.crouchToggle);
        s.runToggle = flag(v, "runToggle", s.runToggle);
        s.teaching = flag(v, "teaching", s.teaching);
        s.pressOnce = flag(v, "pressOnce", s.pressOnce);
    }
    if (json.contains("keys")) s.keys = bindings::fromJson(json["keys"]);
    if (json.contains("taught") && json["taught"].is_object())
    {
        for (const auto& [slot, list] : json["taught"].items())
        {
            if (!list.is_array()) continue;
            for (const nlohmann::json& id : list)
            {
                if (id.is_string()) s.taught[slot].insert(id.get<std::string>());
            }
        }
    }
    if (json.contains("deathHints") && json["deathHints"].is_object())
    {
        for (const auto& [slot, counts] : json["deathHints"].items())
        {
            if (!counts.is_object()) continue;
            for (const auto& [id, count] : counts.items())
            {
                if (count.is_number_integer()) s.hintsShown[slot][id] = std::max(0, count.get<int>());
            }
        }
    }
    return s;
}

Settings load(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) return Settings{};
    std::stringstream buffer;
    buffer << file.rdbuf();
    const nlohmann::json json = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (json.is_discarded())
    {
        std::cerr << path << ": unreadable, using the default settings\n";
        return Settings{};
    }
    return fromJson(json);
}

bool save(const std::string& path, const Settings& settings)
{
    std::error_code ec;
    const std::filesystem::path target(path);
    if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), ec);
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << toJson(settings).dump(2);
        out.flush();
        if (!out.good()) return false;
    }
    std::string backup = path;
    if (backup.size() > 5 && backup.compare(backup.size() - 5, 5, ".json") == 0) backup.resize(backup.size() - 5);
    backup += ".backup.json";
    ec.clear();
    if (std::filesystem::exists(path, ec))
    {
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, ec);
        ec.clear();
        std::filesystem::remove(path, ec);
        ec.clear();
    }
    std::filesystem::rename(temp, path, ec);
    if (ec)
    {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}
} // namespace settings
