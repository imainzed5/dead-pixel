#include "TestFramework.h"

#include "core/Settings.h"

#include <SDL.h>

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

DP_TEST(interface_size_follows_the_window_height)
{
    DP_CHECK_NEAR(settings::uiScaleFor(settings::UiSize::Auto, 720), 1.0f, 0.001f);
    DP_CHECK_NEAR(settings::uiScaleFor(settings::UiSize::Auto, 899), 1.0f, 0.001f);
    DP_CHECK_NEAR(settings::uiScaleFor(settings::UiSize::Auto, 900), 1.5f, 0.001f);
    DP_CHECK_NEAR(settings::uiScaleFor(settings::UiSize::Auto, 1080), 1.5f, 0.001f);
    DP_CHECK_NEAR(settings::uiScaleFor(settings::UiSize::Auto, 1299), 1.5f, 0.001f);
    DP_CHECK_NEAR(settings::uiScaleFor(settings::UiSize::Auto, 1440), 2.0f, 0.001f);
    // A chosen size wins over the height.
    DP_CHECK_NEAR(settings::uiScaleFor(settings::UiSize::X2, 720), 2.0f, 0.001f);
    DP_CHECK_NEAR(settings::uiScaleFor(settings::UiSize::X1, 2160), 1.0f, 0.001f);
}

DP_TEST(settings_survive_a_json_round_trip)
{
    settings::Settings s;
    s.master = 3;
    s.music = 0;
    s.fullscreen = settings::Fullscreen::Borderless;
    s.resolution = settings::Resolution::R1920x1080;
    s.vsync = false;
    s.uiSize = settings::UiSize::X1_5;
    s.screenFx = settings::ScreenFx::Reduced;
    s.shake = 4;
    s.autoPause = false;
    s.crouchToggle = true;
    s.teaching = false;
    s.pressOnce = true;
    (void)bindings::rebind(s.keys, Action::Shove, SDL_SCANCODE_F);
    s.taught["slot_1"] = {"push", "shove"};
    s.hintsShown["slot_1"]["thirst"] = 2;
    const settings::Settings back = settings::fromJson(settings::toJson(s));
    DP_CHECK(back == s);
}

DP_TEST(a_phase_twelve_settings_file_still_reads)
{
    const nlohmann::json old = {{"volume", {{"master", 0.5}, {"effects", 1.0}, {"ambience", 0.2}, {"music", 0.0}}},
                                {"screenEffects", "reduced"}};
    const settings::Settings s = settings::fromJson(old);
    DP_CHECK_EQ(s.master, 5);
    DP_CHECK_EQ(s.effects, 10);
    DP_CHECK_EQ(s.ambience, 2);
    DP_CHECK_EQ(s.music, 0);
    DP_CHECK(s.screenFx == settings::ScreenFx::Reduced);
}

DP_TEST(bad_settings_entries_keep_their_defaults)
{
    const nlohmann::json bad = {{"sound", {{"master", "loud"}, {"music", 99}}},
                                {"screen", {{"fullscreen", "sideways"}, {"shake", -4}}},
                                {"game", {{"teaching", 3}}}};
    const settings::Settings s = settings::fromJson(bad);
    DP_CHECK_EQ(s.master, settings::kLevelMax);
    DP_CHECK_EQ(s.music, settings::kLevelMax);
    DP_CHECK(s.fullscreen == settings::Fullscreen::Off);
    DP_CHECK_EQ(s.shake, 0);
    DP_CHECK(s.teaching);
}

DP_TEST(settings_file_is_written_and_read_back)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "dp_settings_test";
    std::filesystem::remove_all(dir);
    const std::string path = (dir / "settings.json").string();
    // Missing: the defaults.
    DP_CHECK(settings::load(path) == settings::Settings{});
    settings::Settings s;
    s.effects = 6;
    DP_CHECK(settings::save(path, s));
    s.effects = 7;
    DP_CHECK(settings::save(path, s)); // the second write keeps the first as the backup
    DP_CHECK_EQ(settings::load(path).effects, 7);
    DP_CHECK(std::filesystem::exists(dir / "settings.backup.json"));
    DP_CHECK(!std::filesystem::exists(dir / "settings.json.tmp"));
    // A broken file falls back to the defaults rather than failing.
    {
        std::ofstream broken(path, std::ios::trunc);
        broken << "{ not json";
    }
    DP_CHECK(settings::load(path) == settings::Settings{});
    std::filesystem::remove_all(dir);
}

DP_TEST(sound_levels_and_shake_map_to_zero_to_one)
{
    DP_CHECK_NEAR(settings::volume(0), 0.0f, 0.001f);
    DP_CHECK_NEAR(settings::volume(5), 0.5f, 0.001f);
    DP_CHECK_NEAR(settings::volume(12), 1.0f, 0.001f);
    DP_CHECK_NEAR(settings::shakeScale(3), 0.3f, 0.001f);
    DP_CHECK_NEAR(settings::screenFxLevel(settings::ScreenFx::Off), 0.0f, 0.001f);
}
