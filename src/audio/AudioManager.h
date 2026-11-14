#pragma once

#include <SDL_mixer.h>

#include <glm/vec2.hpp>

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "audio/AudioRules.h"
#include "audio/SoundId.h"
#include "audio/SoundTable.h"

// The mixer (phase 12). It decides what plays whether or not an audio
// device is open: under the harness (and --mute) nothing is heard, but the
// same voices, volumes and layers are worked out and logged, so a script
// can ask what would have played.
//
// One-shots are placed in the world (playAt: distance, side, walls) or go
// straight to the ear (play). Loops are layers, faded to a target.
class AudioManager
{
public:
    ~AudioManager();

    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // assets/data/sounds.json. Always loaded (the decisions need it).
    bool loadTable(const std::string& assetsRoot);
    // Opens the audio device and loads the WAVs. False: the game stays silent.
    bool openDevice(const std::string& assetsRoot);
    void shutdown();
    [[nodiscard]] bool deviceOpen() const { return mInitialized; }

    // Where you are, as the mixer hears it. `screenX` turns a world point
    // into a screen x (for the side a sound comes from).
    struct Listener
    {
        glm::vec2 world{0.0f};
        audiorules::Shell shell;
        bool listening = false;       // ear to a door: walls do not cut
        float tileSize = 32.0f;
        float screenX = 0.0f;         // the listener's own screen x
        float halfViewport = 640.0f;
        float rangeScale = 1.0f;      // weather: how far sounds carry outdoors
    };
    void setListener(const Listener& listener) { mListener = listener; }
    using ScreenX = std::function<float(const glm::vec2&)>;
    using ShellAt = std::function<audiorules::Shell(const glm::vec2&)>;
    void setProjection(ScreenX screenX) { mScreenX = std::move(screenX); }
    void setShellResolver(ShellAt resolver) { mShellAt = std::move(resolver); }

    // A sound from a place in the world. `volume` scales the table's.
    void playAt(SoundId id, const glm::vec2& world, float volume = 1.0f);
    // The same, with the shell it was made in already known.
    void playAt(SoundId id, const glm::vec2& world, float volume, const audiorules::Shell& source);
    // Straight to the ear: the body, the interface, stings.
    void play(SoundId id, float volume = 1.0f, float pan = 0.0f);

    // A loop's target volume (0-1, before the table's) and side; it fades there.
    void setLayer(LayerId id, float volume, float pan = 0.0f);
    [[nodiscard]] float layerVolume(LayerId id) const;   // where its fade is now
    [[nodiscard]] float layerTarget(LayerId id) const;
    // Everything but the ringing at this share (the duck after a gunshot).
    void setDuck(float share) { mDuck = share; }
    [[nodiscard]] float duck() const { return mDuck; }
    void silenceLayers();

    // Advances the clock, the fades and the voices.
    void update(float dtSeconds);

    enum class Bus
    {
        Master,
        Effects,
        Ambience,
        Music
    };
    void setVolume(Bus bus, float volume);
    [[nodiscard]] float volume(Bus bus) const;

    // What was played (or dropped) lately, for the harness's `sounds`.
    struct Played
    {
        double time = 0.0;
        SoundId id = SoundId::None;
        float volume = 0.0f;
        float pan = 0.0f;
        float distanceTiles = 0.0f;
        bool positional = false;
        bool muffled = false;
        bool far = false;
        bool dropped = false;
    };
    [[nodiscard]] std::vector<Played> recent(double seconds) const;
    [[nodiscard]] int activeVoices() const;
    [[nodiscard]] std::uint64_t playedCount(SoundId id) const;
    [[nodiscard]] double clock() const { return mClock; }
    [[nodiscard]] const SoundTable& table() const { return mTable; }

private:
    void start(SoundId id, float volume, float pan, float distanceTiles, bool positional, bool muffled, bool far);
    [[nodiscard]] float busVolume(SoundCategory category) const;
    void freeChunks();

    SoundTable mTable;
    Listener mListener;
    ScreenX mScreenX;
    ShellAt mShellAt;

    std::vector<audiorules::Voice> mVoices = std::vector<audiorules::Voice>(audiorules::kVoices);
    struct LayerState
    {
        float current = 0.0f;
        float target = 0.0f;
        float pan = 0.0f;
        bool playing = false;
        int lastVolume = -1;
    };
    std::array<LayerState, kLayerCount> mLayers{};
    float mDuck = 1.0f;

    std::array<float, 4> mBus{1.0f, 1.0f, 1.0f, 1.0f};
    double mClock = 0.0;
    std::deque<Played> mLog;
    std::vector<std::uint64_t> mCounts = std::vector<std::uint64_t>(static_cast<std::size_t>(kSoundCount), 0);
    std::vector<int> mLastVariant = std::vector<int>(static_cast<std::size_t>(kSoundCount), -1);
    std::mt19937 mRng{0x5EEDu}; // variants only: never the simulation's

    // Loaded with the device: per sound, its variants (plain, muffled, far).
    struct Chunks
    {
        std::vector<Mix_Chunk*> plain;
        std::vector<Mix_Chunk*> muffled;
        std::vector<Mix_Chunk*> far;
    };
    std::vector<Chunks> mChunks;
    std::vector<Mix_Chunk*> mLayerChunks;
    bool mInitialized = false;
};
