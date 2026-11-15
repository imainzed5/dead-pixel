#pragma once

#include <vector>

#include "audio/SoundId.h"

// The mixer's rules (phase 12), apart from SDL_mixer so they can be tested:
// how loud a sound is where you stand, which side it comes from, what a
// wall does to it, and which voice gives way when all are busy.
namespace audiorules
{
// Full volume within this distance; silent at the sound's range.
inline constexpr float kFullTiles = 2.0f;
// Through a shut shell (another building, or in and out): x0.45, muffled.
inline constexpr float kOcclusionGain = 0.45f;
// A gun heard beyond this plays its distant tail.
inline constexpr float kFarTailTiles = 15.0f;
// Voices for one-shots, and how many of one sound may play at once.
inline constexpr int kVoices = 24;
inline constexpr int kMaxSameSound = 3;
// Layers fade from one volume to another over this long.
inline constexpr float kCrossfadeSeconds = 1.5f;
// Below this a sound is not worth a voice.
inline constexpr float kInaudible = 0.01f;

// Audible range in tiles by noise tier (Whisper 1 .. Explosive 5).
[[nodiscard]] float tierRangeTiles(int tier);

// 1 within kFullTiles, falling linearly to 0 at `rangeTiles`.
[[nodiscard]] float distanceGain(float distanceTiles, float rangeTiles);

// Where the sound is made and where you are: building id (0 outdoors) and
// whether that building's doors and windows are all shut. With the ear to
// a door (`listening`) the cut is lifted.
struct Shell
{
    int building = 0;
    bool sealed = false;
};
[[nodiscard]] bool occluded(const Shell& source, const Shell& listener, bool listening);

// -1 full left .. 1 full right, from the sound's screen x against yours.
[[nodiscard]] float panFromScreen(float soundScreenX, float listenerScreenX, float halfViewportWidth);
// SDL_mixer's Mix_SetPosition angle (0 ahead, 90 right, 270 left) for a pan.
[[nodiscard]] int panAngle(float pan);

// A voice in use (or free once `endsAt` has passed).
struct Voice
{
    int sound = -1;          // SoundId as int; -1 free
    int priority = 0;
    float volume = 0.0f;
    double endsAt = 0.0;
};
// The voice a new sound takes, or -1 when it is dropped. A free voice if
// there is one; else the quietest of the lowest priority, if the new sound
// outranks it (higher priority, or the same and louder). At most
// kMaxSameSound of one sound: a fourth replaces the quietest of them if it
// is louder, else it is dropped.
[[nodiscard]] int pickVoice(const std::vector<Voice>& voices, int sound, int priority, float volume, double now,
                            int maxSame = kMaxSameSound);

// One step of a layer's fade toward its target.
[[nodiscard]] float fadeToward(float current, float target, float dtSeconds, float seconds = kCrossfadeSeconds);
} // namespace audiorules
