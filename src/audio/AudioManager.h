#pragma once

#include <SDL_mixer.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "audio/SoundId.h"

class AudioManager
{
public:
    ~AudioManager();

    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool initialize(const std::string& assetsRoot);
    void shutdown();

    void playSound(SoundId id, float volume = 1.0f, float pan = 0.0f);
    void playSoundAtWorld(SoundId id, float worldX, float cameraX, float viewportWidth, float volume = 1.0f);

    void playAmbient(AmbientId id, float volume = 0.3f);
    void stopAmbient();

    void setMasterVolume(float volume);

private:
    std::vector<Mix_Chunk*> mSounds;
    std::vector<Mix_Music*> mAmbient;
    float mMasterVolume = 1.0f;
    bool mInitialized = false;
};
