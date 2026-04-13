#include "audio/AudioManager.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace
{

const char* soundFileName(SoundId id)
{
    switch (id)
    {
    case SoundId::FootstepSoft:   return "footstep_soft.wav";
    case SoundId::FootstepNormal: return "footstep_normal.wav";
    case SoundId::FootstepLoud:   return "footstep_loud.wav";
    case SoundId::WeaponSwing:    return "weapon_swing.wav";
    case SoundId::WeaponHit:      return "weapon_hit.wav";
    case SoundId::ZombieGrowl:    return "zombie_growl.wav";
    case SoundId::ZombieAttack:   return "zombie_attack.wav";
    case SoundId::FoodEat:        return "food_eat.wav";
    case SoundId::PlayerHurt:     return "player_hurt.wav";
    case SoundId::Death:          return "death.wav";
    default:                      return "";
    }
}

const char* ambientFileName(AmbientId id)
{
    switch (id)
    {
    case AmbientId::Wind: return "ambient_wind.wav";
    default:              return "";
    }
}

} // namespace

AudioManager::~AudioManager()
{
    shutdown();
}

bool AudioManager::initialize(const std::string& assetsRoot)
{
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        std::cerr << "SDL_mixer init failed: " << Mix_GetError() << "\n";
        return false;
    }

    Mix_AllocateChannels(16);
    mInitialized = true;

    const std::filesystem::path soundsDir = std::filesystem::path(assetsRoot) / "sounds";

    mSounds.resize(static_cast<std::size_t>(SoundId::Count), nullptr);
    for (int i = 0; i < static_cast<int>(SoundId::Count); ++i)
    {
        const auto id = static_cast<SoundId>(i);
        const auto path = soundsDir / soundFileName(id);
        if (std::filesystem::exists(path))
        {
            mSounds[static_cast<std::size_t>(i)] = Mix_LoadWAV(path.string().c_str());
            if (!mSounds[static_cast<std::size_t>(i)])
            {
                std::cerr << "Failed to load sound " << path.string() << ": " << Mix_GetError() << "\n";
            }
        }
    }

    mAmbient.resize(static_cast<std::size_t>(AmbientId::Count), nullptr);
    for (int i = 0; i < static_cast<int>(AmbientId::Count); ++i)
    {
        const auto id = static_cast<AmbientId>(i);
        const auto path = soundsDir / ambientFileName(id);
        if (std::filesystem::exists(path))
        {
            mAmbient[static_cast<std::size_t>(i)] = Mix_LoadMUS(path.string().c_str());
            if (!mAmbient[static_cast<std::size_t>(i)])
            {
                std::cerr << "Failed to load ambient " << path.string() << ": " << Mix_GetError() << "\n";
            }
        }
    }

    return true;
}

void AudioManager::shutdown()
{
    if (!mInitialized)
    {
        return;
    }

    Mix_HaltChannel(-1);
    Mix_HaltMusic();

    for (Mix_Chunk* chunk : mSounds)
    {
        if (chunk)
        {
            Mix_FreeChunk(chunk);
        }
    }
    mSounds.clear();

    for (Mix_Music* music : mAmbient)
    {
        if (music)
        {
            Mix_FreeMusic(music);
        }
    }
    mAmbient.clear();

    Mix_CloseAudio();
    mInitialized = false;
}

void AudioManager::playSound(SoundId id, float volume, float pan)
{
    if (!mInitialized)
    {
        return;
    }

    const auto index = static_cast<std::size_t>(id);
    if (index >= mSounds.size() || !mSounds[index])
    {
        return;
    }

    const int channel = Mix_PlayChannel(-1, mSounds[index], 0);
    if (channel < 0)
    {
        return;
    }

    const int vol = static_cast<int>(std::clamp(volume * mMasterVolume, 0.0f, 1.0f) * MIX_MAX_VOLUME);
    Mix_Volume(channel, vol);

    // Stereo panning: pan -1.0 = full left, 0.0 = center, 1.0 = full right
    const float clampedPan = std::clamp(pan, -1.0f, 1.0f);
    const Uint8 right = static_cast<Uint8>((clampedPan + 1.0f) * 0.5f * 254.0f);
    const Uint8 left = 254 - right;
    Mix_SetPanning(channel, left, right);
}

void AudioManager::playSoundAtWorld(SoundId id, float worldX, float cameraX, float viewportWidth, float volume)
{
    if (viewportWidth <= 0.0f)
    {
        playSound(id, volume, 0.0f);
        return;
    }

    const float cameraCenterX = cameraX + viewportWidth * 0.5f;
    const float offset = worldX - cameraCenterX;
    const float pan = std::clamp(offset / (viewportWidth * 0.5f), -1.0f, 1.0f);

    playSound(id, volume, pan);
}

void AudioManager::playAmbient(AmbientId id, float volume)
{
    if (!mInitialized)
    {
        return;
    }

    const auto index = static_cast<std::size_t>(id);
    if (index >= mAmbient.size() || !mAmbient[index])
    {
        return;
    }

    Mix_VolumeMusic(static_cast<int>(std::clamp(volume * mMasterVolume, 0.0f, 1.0f) * MIX_MAX_VOLUME));
    Mix_PlayMusic(mAmbient[index], -1);
}

void AudioManager::stopAmbient()
{
    if (mInitialized)
    {
        Mix_HaltMusic();
    }
}

void AudioManager::setMasterVolume(float volume)
{
    mMasterVolume = std::clamp(volume, 0.0f, 1.0f);
}
