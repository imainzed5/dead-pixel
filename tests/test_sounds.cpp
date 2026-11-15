// The sound table (phase 12): every SoundId and LayerId has a row in
// assets/data/sounds.json with files that exist, the table names nothing the
// game does not know, every source in the noise table (03_content_tables.md
// section 10) has a sound, and the generated audio stays small.
#include "TestFramework.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "audio/AudioRules.h"
#include "audio/SoundTable.h"

namespace
{
const std::filesystem::path kAssets = DEAD_PIXEL_ASSETS_DIR;

SoundTable loadTable()
{
    SoundTable table;
    std::string error;
    DP_CHECK(table.load((kAssets / "data" / "sounds.json").string(), error));
    DP_CHECK(error.empty());
    return table;
}

bool exists(const std::string& name)
{
    return std::filesystem::is_regular_file(kAssets / "sounds" / name);
}
} // namespace

DP_TEST(sound_ids_round_trip_their_keys)
{
    for (int i = 0; i < kSoundCount; ++i)
    {
        const SoundId id = static_cast<SoundId>(i);
        DP_CHECK(soundByKey(soundKey(id)) == id);
    }
    for (int i = 0; i < kLayerCount; ++i)
    {
        const LayerId id = static_cast<LayerId>(i);
        DP_CHECK(layerByKey(layerKey(id)) == id);
    }
    DP_CHECK(soundByKey("no_such_sound") == SoundId::None);
}

DP_TEST(every_sound_resolves_to_files_that_exist)
{
    const SoundTable table = loadTable();
    DP_CHECK(table.loaded());
    for (int i = 0; i < kSoundCount; ++i)
    {
        const SoundDef& def = table.sound(static_cast<SoundId>(i));
        DP_CHECK(def.known);
        DP_CHECK(!def.loop);
        // Three to five variants of every one-shot.
        DP_CHECK(def.files.size() >= 3 && def.files.size() <= 5);
        DP_CHECK(def.volume > 0.0f && def.volume <= 1.0f);
        for (const std::string& f : def.files) DP_CHECK(exists(f));
        for (const std::string& f : def.muffled) DP_CHECK(exists(f));
        for (const std::string& f : def.far) DP_CHECK(exists(f));
    }
    for (int i = 0; i < kLayerCount; ++i)
    {
        const SoundDef& def = table.layer(static_cast<LayerId>(i));
        DP_CHECK(def.known);
        DP_CHECK(def.loop);
        DP_CHECK(def.files.size() == 1);
        for (const std::string& f : def.files) DP_CHECK(exists(f));
    }
    // Nothing in the table the game does not know.
    DP_CHECK(table.unknownKeys().empty());
}

DP_TEST(the_sounds_that_matter_have_a_muffled_twin_and_guns_a_far_tail)
{
    const SoundTable table = loadTable();
    for (SoundId id : {SoundId::ShamblerIdle, SoundId::ShamblerAlert, SoundId::ShamblerChase, SoundId::RunnerShriek,
                       SoundId::BruteGrowl, SoundId::ScreamerScream, SoundId::DeadDoorBang, SoundId::DeadBoardCrack,
                       SoundId::DeadWindowBang, SoundId::Pistol, SoundId::Shotgun})
    {
        DP_CHECK(!table.sound(id).muffled.empty());
    }
    DP_CHECK(!table.sound(SoundId::Pistol).far.empty());
    DP_CHECK(!table.sound(SoundId::Shotgun).far.empty());
    // Rain has its own indoor variant: the roof.
    DP_CHECK(table.layer(LayerId::RainRoof).known);
}

DP_TEST(sound_ranges_follow_their_noise_tier)
{
    DP_CHECK_NEAR(audiorules::tierRangeTiles(1), 4.0f, 0.01f);  // Whisper
    DP_CHECK_NEAR(audiorules::tierRangeTiles(2), 9.0f, 0.01f);  // Soft
    DP_CHECK_NEAR(audiorules::tierRangeTiles(3), 18.0f, 0.01f); // Medium
    DP_CHECK_NEAR(audiorules::tierRangeTiles(4), 32.0f, 0.01f); // Loud
    DP_CHECK_NEAR(audiorules::tierRangeTiles(5), 60.0f, 0.01f); // Explosive
    const SoundTable table = loadTable();
    DP_CHECK_NEAR(table.sound(SoundId::Pistol).rangeTiles, 32.0f, 0.01f);
    DP_CHECK_NEAR(table.sound(SoundId::Shotgun).rangeTiles, 60.0f, 0.01f);
    DP_CHECK_NEAR(table.sound(SoundId::ScreamerScream).rangeTiles, 32.0f, 0.01f);
}

// 03_content_tables.md section 10: every source of noise has a sound.
DP_TEST(every_row_of_the_noise_table_has_a_sound)
{
    const std::vector<std::pair<const char*, std::vector<SoundId>>> rows = {
        {"crouch-walk / walk / run", {SoundId::FootGrassCrouch, SoundId::FootWoodWalk, SoundId::FootAsphaltRun}},
        {"swing", {SoundId::SwingLight, SoundId::SwingHeavy}},
        {"shove", {SoundId::Shove}},
        {"frying pan hit", {SoundId::HitPan}},
        {"door opened / closed gently", {SoundId::DoorOpen, SoundId::DoorClose, SoundId::DoorCreak}},
        {"door battered, board hammered", {SoundId::DeadDoorBang, SoundId::Hammer}},
        {"window broken", {SoundId::WindowBreak}},
        {"furniture pushed / broken up", {SoundId::DragLight, SoundId::DragHeavy, SoundId::FurnitureBreak}},
        {"pistol / shotgun", {SoundId::Pistol, SoundId::Shotgun}},
        {"rock lands", {SoundId::RockLand}},
        {"alarm clock, can alarm, car alarm", {SoundId::ClockRing, SoundId::CanAlarm, SoundId::CarAlarm}},
        {"flare", {SoundId::FlareHiss}},
        {"scream (player, Screamer, survivor)", {SoundId::BodyScream, SoundId::ScreamerScream, SoundId::PersonHey}},
        {"zombie groan (chasing)", {SoundId::ShamblerChase, SoundId::BruteGrowl, SoundId::RunnerBreath, SoundId::CrawlerDrag}},
        {"survivor walking / fighting", {SoundId::FootAsphaltWalk, SoundId::FootAsphaltRun, SoundId::SwingLight}},
        {"generator", {SoundId::GateGrind}},
    };
    const SoundTable table = loadTable();
    for (const auto& [row, sounds] : rows)
    {
        (void)row;
        DP_CHECK(!sounds.empty());
        for (SoundId id : sounds)
        {
            DP_CHECK(id != SoundId::None);
            DP_CHECK(table.sound(id).known);
        }
    }
    // The generator's engine is a layer.
    DP_CHECK(table.layer(LayerId::Generator).known);
}

DP_TEST(generated_audio_stays_under_forty_megabytes)
{
    std::uintmax_t total = 0;
    for (const auto& entry : std::filesystem::directory_iterator(kAssets / "sounds"))
    {
        if (entry.is_regular_file()) total += entry.file_size();
    }
    DP_CHECK(total > 0);
    DP_CHECK(total < 40ull * 1024ull * 1024ull);
}
