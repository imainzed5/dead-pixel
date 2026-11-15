#include "audio/AudioManager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

#include <glm/geometric.hpp>

namespace
{
// Layers play on the channels after the voices.
int layerChannel(int layer)
{
    return audiorules::kVoices + layer;
}

constexpr std::size_t kLogSize = 512;
} // namespace

AudioManager::~AudioManager()
{
    shutdown();
}

bool AudioManager::loadTable(const std::string& assetsRoot)
{
    std::string error;
    const std::string path = (std::filesystem::path(assetsRoot) / "data" / "sounds.json").string();
    if (!mTable.load(path, error))
    {
        std::cerr << "Sound table: " << error << "\n";
        return false;
    }
    return true;
}

bool AudioManager::openDevice(const std::string& assetsRoot)
{
    if (mInitialized) return true;
    // 22 kHz like the files: SDL_mixer converts every chunk to the device's
    // format at load, and stereo is all the side of a sound needs.
    if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 1024) < 0)
    {
        std::cerr << "SDL_mixer init failed: " << Mix_GetError() << "\n";
        return false;
    }
    Mix_AllocateChannels(audiorules::kVoices + kLayerCount);
    mInitialized = true;

    const std::filesystem::path dir = std::filesystem::path(assetsRoot) / "sounds";
    const auto load = [&](const std::string& name) -> Mix_Chunk*
    {
        const std::string path = (dir / name).string();
        Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
        if (chunk == nullptr) std::cerr << "Failed to load sound " << path << ": " << Mix_GetError() << "\n";
        return chunk;
    };
    mChunks.assign(static_cast<std::size_t>(kSoundCount), Chunks{});
    for (int i = 0; i < kSoundCount; ++i)
    {
        const SoundDef& def = mTable.sound(static_cast<SoundId>(i));
        Chunks& c = mChunks[static_cast<std::size_t>(i)];
        for (const std::string& f : def.files) c.plain.push_back(load(f));
        for (const std::string& f : def.muffled) c.muffled.push_back(load(f));
        for (const std::string& f : def.far) c.far.push_back(load(f));
    }
    mLayerChunks.assign(static_cast<std::size_t>(kLayerCount), nullptr);
    for (int i = 0; i < kLayerCount; ++i)
    {
        const SoundDef& def = mTable.layer(static_cast<LayerId>(i));
        if (!def.files.empty()) mLayerChunks[static_cast<std::size_t>(i)] = load(def.files.front());
    }
    return true;
}

void AudioManager::freeChunks()
{
    for (Chunks& c : mChunks)
    {
        for (auto* list : {&c.plain, &c.muffled, &c.far})
        {
            for (Mix_Chunk* chunk : *list)
            {
                if (chunk) Mix_FreeChunk(chunk);
            }
        }
    }
    mChunks.clear();
    for (Mix_Chunk* chunk : mLayerChunks)
    {
        if (chunk) Mix_FreeChunk(chunk);
    }
    mLayerChunks.clear();
}

void AudioManager::shutdown()
{
    if (!mInitialized) return;
    Mix_HaltChannel(-1);
    freeChunks();
    Mix_CloseAudio();
    mInitialized = false;
}

float AudioManager::busVolume(SoundCategory category) const
{
    float bus = 1.0f;
    switch (category)
    {
    case SoundCategory::Ambience: bus = mBus[static_cast<std::size_t>(Bus::Ambience)]; break;
    case SoundCategory::Music: bus = mBus[static_cast<std::size_t>(Bus::Music)]; break;
    default: bus = mBus[static_cast<std::size_t>(Bus::Effects)]; break;
    }
    return bus * mBus[static_cast<std::size_t>(Bus::Master)];
}

void AudioManager::setVolume(Bus bus, float volume)
{
    mBus[static_cast<std::size_t>(bus)] = std::clamp(volume, 0.0f, 1.0f);
}

float AudioManager::volume(Bus bus) const
{
    return mBus[static_cast<std::size_t>(bus)];
}

void AudioManager::playAt(SoundId id, const glm::vec2& world, float volume)
{
    audiorules::Shell source;
    if (mShellAt) source = mShellAt(world);
    playAt(id, world, volume, source);
}

void AudioManager::playAt(SoundId id, const glm::vec2& world, float volume, const audiorules::Shell& source)
{
    const SoundDef& def = mTable.sound(id);
    if (!def.known) return;
    const float tile = std::max(1.0f, mListener.tileSize);
    const float distance = glm::length(world - mListener.world) / tile;
    // Rain and snow swallow what is made outdoors (the noise model's scale).
    const float range = def.rangeTiles * (source.building == 0 ? mListener.rangeScale : 1.0f);
    float gain = audiorules::distanceGain(distance, range);
    if (gain <= 0.0f) return;
    const bool muffled = audiorules::occluded(source, mListener.shell, mListener.listening);
    if (muffled) gain *= audiorules::kOcclusionGain;
    const bool far = !def.far.empty() && distance > audiorules::kFarTailTiles;
    const float pan = mScreenX ? audiorules::panFromScreen(mScreenX(world), mListener.screenX, mListener.halfViewport) : 0.0f;
    start(id, def.volume * volume * gain, pan, distance, true, muffled && !def.muffled.empty(), far);
}

void AudioManager::play(SoundId id, float volume, float pan)
{
    const SoundDef& def = mTable.sound(id);
    if (!def.known) return;
    start(id, def.volume * volume, pan, 0.0f, false, false, false);
}

void AudioManager::start(SoundId id, float volume, float pan, float distanceTiles, bool positional, bool muffled, bool far)
{
    const SoundDef& def = mTable.sound(id);
    const float heard = volume * busVolume(def.category);
    if (heard < audiorules::kInaudible) return;

    Played entry;
    entry.time = mClock;
    entry.id = id;
    entry.volume = heard;
    entry.pan = pan;
    entry.distanceTiles = distanceTiles;
    entry.positional = positional;
    entry.muffled = muffled;
    entry.far = far;

    const int sound = static_cast<int>(id);
    const int voice = audiorules::pickVoice(mVoices, sound, def.priority, heard, mClock);
    entry.dropped = voice < 0;
    mLog.push_back(entry);
    while (mLog.size() > kLogSize) mLog.pop_front();
    if (voice < 0) return;
    ++mCounts[static_cast<std::size_t>(sound)];

    // A variant, never the same one twice running.
    const std::vector<std::string>& files = far ? def.far : (muffled ? def.muffled : def.files);
    const int count = static_cast<int>(std::max<std::size_t>(1, files.size()));
    int variant = static_cast<int>(mRng() % static_cast<std::uint32_t>(count));
    int& last = mLastVariant[static_cast<std::size_t>(sound)];
    if (count > 1 && variant == last) variant = (variant + 1) % count;
    last = variant;
    const float seconds = def.seconds.empty() ? 0.5f : def.seconds[static_cast<std::size_t>(std::min(variant, static_cast<int>(def.seconds.size()) - 1))];
    // A distant tail rolls on longer than the shot.
    const float length = far ? seconds + 1.2f : seconds;

    audiorules::Voice& v = mVoices[static_cast<std::size_t>(voice)];
    v.sound = sound;
    v.priority = def.priority;
    v.volume = heard;
    v.endsAt = mClock + length;

    if (!mInitialized || mChunks.empty()) return;
    const Chunks& c = mChunks[static_cast<std::size_t>(sound)];
    const std::vector<Mix_Chunk*>& list = far ? c.far : (muffled ? c.muffled : c.plain);
    if (list.empty()) return;
    Mix_Chunk* chunk = list[static_cast<std::size_t>(variant) % list.size()];
    if (chunk == nullptr) return;
    Mix_HaltChannel(voice);
    if (Mix_PlayChannel(voice, chunk, 0) < 0) return;
    Mix_Volume(voice, static_cast<int>(std::clamp(heard, 0.0f, 1.0f) * MIX_MAX_VOLUME));
    Mix_SetPosition(voice, static_cast<Sint16>(audiorules::panAngle(pan)), 0);
}

void AudioManager::setLayer(LayerId id, float volume, float pan)
{
    const int i = static_cast<int>(id);
    if (i < 0 || i >= kLayerCount) return;
    mLayers[static_cast<std::size_t>(i)].target = std::clamp(volume, 0.0f, 1.0f);
    mLayers[static_cast<std::size_t>(i)].pan = std::clamp(pan, -1.0f, 1.0f);
}

float AudioManager::layerVolume(LayerId id) const
{
    const int i = static_cast<int>(id);
    return i >= 0 && i < kLayerCount ? mLayers[static_cast<std::size_t>(i)].current : 0.0f;
}

float AudioManager::layerTarget(LayerId id) const
{
    const int i = static_cast<int>(id);
    return i >= 0 && i < kLayerCount ? mLayers[static_cast<std::size_t>(i)].target : 0.0f;
}

void AudioManager::silenceLayers()
{
    for (LayerState& layer : mLayers) layer.target = 0.0f;
}

void AudioManager::update(float dtSeconds)
{
    mClock += dtSeconds;
    for (int i = 0; i < kLayerCount; ++i)
    {
        LayerState& layer = mLayers[static_cast<std::size_t>(i)];
        const LayerId id = static_cast<LayerId>(i);
        // The ringing comes with the shot, not over a crossfade; it then
        // follows its target down as the ears clear.
        const float fadeSeconds = id == LayerId::Tinnitus ? 0.1f : audiorules::kCrossfadeSeconds;
        layer.current = audiorules::fadeToward(layer.current, layer.target, dtSeconds, fadeSeconds);
        if (!mInitialized || mLayerChunks.empty()) continue;

        const SoundDef& def = mTable.layer(id);
        const float duck = id == LayerId::Tinnitus ? 1.0f : mDuck;
        const float heard = layer.current * def.volume * duck * busVolume(def.category);
        const int channel = layerChannel(i);
        Mix_Chunk* chunk = mLayerChunks[static_cast<std::size_t>(i)];
        if (heard >= 0.002f && chunk != nullptr)
        {
            if (!layer.playing)
            {
                Mix_PlayChannel(channel, chunk, -1);
                layer.playing = true;
                layer.lastVolume = -1;
            }
            const int vol = static_cast<int>(std::clamp(heard, 0.0f, 1.0f) * MIX_MAX_VOLUME);
            if (vol != layer.lastVolume)
            {
                Mix_Volume(channel, vol);
                Mix_SetPosition(channel, static_cast<Sint16>(audiorules::panAngle(layer.pan)), 0);
                layer.lastVolume = vol;
            }
        }
        else if (layer.playing && layer.target <= 0.0f)
        {
            Mix_HaltChannel(channel);
            layer.playing = false;
        }
    }
}

std::vector<AudioManager::Played> AudioManager::recent(double seconds) const
{
    std::vector<Played> out;
    for (const Played& p : mLog)
    {
        if (p.time >= mClock - seconds) out.push_back(p);
    }
    return out;
}

int AudioManager::activeVoices() const
{
    int n = 0;
    for (const audiorules::Voice& v : mVoices)
    {
        if (v.sound >= 0 && v.endsAt > mClock) ++n;
    }
    return n;
}

std::uint64_t AudioManager::playedCount(SoundId id) const
{
    const int i = static_cast<int>(id);
    return i >= 0 && i < kSoundCount ? mCounts[static_cast<std::size_t>(i)] : 0;
}
