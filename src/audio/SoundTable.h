#pragma once

#include <string>
#include <vector>

#include "audio/SoundId.h"

// assets/data/sounds.json, as tools/generate_sounds.py writes it: for each
// sound its variant files and their lengths, its audible range, priority,
// base volume, and whether it loops.
enum class SoundCategory
{
    Effects,
    Ambience,
    Music,
    Interface
};

struct SoundDef
{
    std::string key;
    std::vector<std::string> files;
    std::vector<float> seconds;   // per file
    std::vector<std::string> muffled; // the low-passed twin heard through a wall
    std::vector<std::string> far;     // the distant tail (guns)
    float rangeTiles = 0.0f;      // 0: not placed (interface, stings)
    int priority = 0;
    float volume = 1.0f;
    bool loop = false;
    SoundCategory category = SoundCategory::Effects;
    bool known = false;           // found in the table
};

class SoundTable
{
public:
    // False (with `error`) when the file is missing or malformed.
    bool load(const std::string& path, std::string& error);

    [[nodiscard]] const SoundDef& sound(SoundId id) const;
    [[nodiscard]] const SoundDef& layer(LayerId id) const;
    // Keys in the file that no id names (for the test).
    [[nodiscard]] const std::vector<std::string>& unknownKeys() const { return mUnknown; }
    [[nodiscard]] bool loaded() const { return mLoaded; }

private:
    std::vector<SoundDef> mSounds = std::vector<SoundDef>(static_cast<std::size_t>(kSoundCount));
    std::vector<SoundDef> mLayers = std::vector<SoundDef>(static_cast<std::size_t>(kLayerCount));
    std::vector<std::string> mUnknown;
    bool mLoaded = false;
    SoundDef mEmpty;
};
