#include "audio/SoundTable.h"

#include <cstring>
#include <fstream>

#include <nlohmann/json.hpp>

namespace
{
constexpr const char* kSoundKeys[] = {
#define DP_SOUND_KEY(name, key) key,
    DP_SOUND_LIST(DP_SOUND_KEY)
#undef DP_SOUND_KEY
};
constexpr const char* kLayerKeys[] = {
#define DP_LAYER_KEY(name, key) key,
    DP_LAYER_LIST(DP_LAYER_KEY)
#undef DP_LAYER_KEY
};

SoundCategory parseCategory(const std::string& text)
{
    if (text == "ambience") return SoundCategory::Ambience;
    if (text == "music") return SoundCategory::Music;
    if (text == "interface") return SoundCategory::Interface;
    return SoundCategory::Effects;
}

std::vector<std::string> strings(const nlohmann::json& j, const char* field)
{
    std::vector<std::string> out;
    if (j.contains(field) && j[field].is_array())
    {
        for (const auto& s : j[field]) out.push_back(s.get<std::string>());
    }
    return out;
}
} // namespace

const char* soundKey(SoundId id)
{
    const int i = static_cast<int>(id);
    return i >= 0 && i < kSoundCount ? kSoundKeys[i] : "";
}

const char* layerKey(LayerId id)
{
    const int i = static_cast<int>(id);
    return i >= 0 && i < kLayerCount ? kLayerKeys[i] : "";
}

SoundId soundByKey(const char* key)
{
    for (int i = 0; i < kSoundCount; ++i)
    {
        if (std::strcmp(kSoundKeys[i], key) == 0) return static_cast<SoundId>(i);
    }
    return SoundId::None;
}

LayerId layerByKey(const char* key)
{
    for (int i = 0; i < kLayerCount; ++i)
    {
        if (std::strcmp(kLayerKeys[i], key) == 0) return static_cast<LayerId>(i);
    }
    return LayerId::Count;
}

bool SoundTable::load(const std::string& path, std::string& error)
{
    std::ifstream file(path);
    if (!file)
    {
        error = "cannot open " + path;
        return false;
    }
    nlohmann::json json;
    try
    {
        file >> json;
    }
    catch (const std::exception& e)
    {
        error = path + ": " + e.what();
        return false;
    }
    if (!json.contains("sounds") || !json["sounds"].is_array())
    {
        error = path + ": no sounds";
        return false;
    }
    mUnknown.clear();
    for (const auto& j : json["sounds"])
    {
        SoundDef def;
        def.key = j.value("id", std::string());
        def.files = strings(j, "files");
        def.muffled = strings(j, "muffled");
        def.far = strings(j, "far");
        if (j.contains("seconds") && j["seconds"].is_array())
        {
            for (const auto& s : j["seconds"]) def.seconds.push_back(s.get<float>());
        }
        def.rangeTiles = j.value("range", 0.0f);
        def.priority = j.value("priority", 0);
        def.volume = j.value("volume", 1.0f);
        def.loop = j.value("loop", false);
        def.category = parseCategory(j.value("category", std::string("effects")));
        def.known = true;
        const SoundId sound = soundByKey(def.key.c_str());
        const LayerId layer = layerByKey(def.key.c_str());
        if (sound != SoundId::None) mSounds[static_cast<std::size_t>(sound)] = std::move(def);
        else if (layer != LayerId::Count) mLayers[static_cast<std::size_t>(layer)] = std::move(def);
        else mUnknown.push_back(def.key);
    }
    mLoaded = true;
    return true;
}

const SoundDef& SoundTable::sound(SoundId id) const
{
    const int i = static_cast<int>(id);
    return i >= 0 && i < kSoundCount ? mSounds[static_cast<std::size_t>(i)] : mEmpty;
}

const SoundDef& SoundTable::layer(LayerId id) const
{
    const int i = static_cast<int>(id);
    return i >= 0 && i < kLayerCount ? mLayers[static_cast<std::size_t>(i)] : mEmpty;
}
