#pragma once

#include <array>

#include "audio/SoundId.h"
#include "components/Weather.h"

// Ambience that tells the truth (phase 12): the layers' volumes from what is
// around you, set every half second. Birds stop when something is near.
namespace ambience
{
inline constexpr float kUpdateSeconds = 0.5f;
inline constexpr float kQuietTiles = 12.0f;  // birds and insects stop with the dead this near
inline constexpr float kCrowdTiles = 30.0f;  // the crowd layer counts the dead this near
inline constexpr float kHumTiles = 6.0f;     // a lamp or fridge this near hums
inline constexpr float kFireTiles = 12.0f;   // tiles alight this near crackle
inline constexpr float kTinnitusSeconds = 3.0f;
inline constexpr float kTinnitusTiles = 4.0f; // a Screamer this near rings your ears
inline constexpr float kDuck = 0.6f;          // the other layers lose 60 % while it rings
inline constexpr float kBirdsBelowInfection = 50.0f;
inline constexpr float kInsectsAboveCelsius = 8.0f;
inline constexpr int kCrowsFromRuin = 2;

struct Facts
{
    bool playing = true;
    bool out = false;              // got out by the gate: wind, and nothing else
    bool day = true;
    WeatherCondition condition = WeatherCondition::Clear;
    float windStrength = 0.0f;     // 0-1
    bool indoors = false;
    bool openToOutside = false;    // a door or window of this building stands open
    bool closedIn = false;         // a shut room with no windows: no weather at all
    bool treesNear = false;
    float infection = 0.0f;        // the district's, 0-100
    int ruin = 0;                  // the district's ruin level, 0-3
    int deadNear = 0;              // within kQuietTiles
    int deadAround = 0;            // within kCrowdTiles
    float temperature = 20.0f;     // the air outdoors, Celsius
    bool hum = false;              // power on, a lamp or fridge within kHumTiles
    int burningNear = 0;           // tiles alight within kFireTiles
    float tinnitusSeconds = 0.0f;  // left of the ringing
};

using Mix = std::array<float, kLayerCount>;

// The target volume (0-1, before the layer's own volume) of each ambience
// layer and the ringing; other layers stay 0. The duck under the ringing
// is applied by the mixer at once (duck()), not faded.
[[nodiscard]] Mix mix(const Facts& facts);
// The ambience layers (the mixer ducks these under the ringing).
[[nodiscard]] bool isAmbience(LayerId layer);
// How much of every other layer is left while the ears ring.
[[nodiscard]] float duck(float tinnitusSeconds);
// How much of the outdoor world reaches you: 1 outdoors, less inside.
[[nodiscard]] float outdoorShare(const Facts& facts);
} // namespace ambience

// Music: almost none (phase 12). A pad when it has been calm a long while,
// by the tone preset; never in danger.
namespace music
{
enum class Tone
{
    Hopeless,
    GrimHuman,
    BrutalArcade
};

inline constexpr float kCalmTiles = 20.0f;
inline constexpr double kGrimCalmHours = 3.0;   // calm this long before a pad
inline constexpr double kGrimGapHours = 6.0;    // at most one pad in this long
inline constexpr double kBrutalCalmHours = 1.5;
inline constexpr double kBrutalGapHours = 3.0;
inline constexpr double kPadHours = 1.15;       // one loop of a pad (about 69 real seconds)
inline constexpr int kFightForPulse = 4;        // Brutal Arcade: a pulse under fights this big
inline constexpr float kFightTiles = 8.0f;
inline constexpr float kPadVolume = 0.55f;
inline constexpr float kPulseVolume = 0.6f;

struct Facts
{
    Tone tone = Tone::GrimHuman;
    bool playing = true;
    double hours = 0.0;        // game hours since the run began (any steady clock)
    bool calm = true;          // none of the dead within kCalmTiles
    bool indoors = false;
    bool dawnOrDusk = false;
    int fight = 0;             // the dead within kFightTiles that are after you
};

// Stings (dusk, a caller's first bang, dawn survived) are music too.
[[nodiscard]] bool stingsAllowed(Tone tone);

class Director
{
public:
    void update(const Facts& facts);
    void reset();

    // The pad playing (or LayerId::Count), its target volume, the pulse's.
    [[nodiscard]] LayerId pad() const { return mPad; }
    [[nodiscard]] float padTarget() const { return mPadOn ? kPadVolume : 0.0f; }
    [[nodiscard]] float pulseTarget() const { return mPulseOn ? kPulseVolume : 0.0f; }
    // How many pads have begun (the harness counts them).
    [[nodiscard]] int starts() const { return mStarts; }

private:
    double mCalmSince = -1.0;
    double mLastStart = -1.0e9;
    double mPadEnds = -1.0;
    int mNext = 0;
    LayerId mPad = LayerId::Count;
    bool mPadOn = false;
    bool mPulseOn = false;
    int mStarts = 0;
};
} // namespace music
