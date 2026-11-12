#pragma once

#include <random>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "components/Weather.h"

class Camera;
class ParticleSystem;
class SpriteBatch;
class TileMap;

// What the weather looks like: rain and its splashes, drifting cloud shadows,
// fog, lightning, the wind that everything else leans on, and how much the
// sky dims the daylight. It only shows the weather; WeatherSystem decides it.
class Atmosphere
{
public:
    static constexpr int kMaxDrops = 420;
    static constexpr float kFlashDecayPerSecond = 3.5f;

    // How much each kind of weather takes off the daylight (0..1).
    [[nodiscard]] static float skyDimFor(WeatherCondition condition);

    void update(float dtSeconds, const WeatherState& weather, const Camera& camera, const TileMap& map,
                ParticleSystem& particles);

    // The push of the wind on loose things, world pixels per second, and its
    // strength from 0 (still) to 1 (gale), gusts included.
    [[nodiscard]] glm::vec2 wind() const { return mWindDir * (mGust * 90.0f); }
    [[nodiscard]] float gust() const { return mGust; }
    [[nodiscard]] float rain() const { return mRain; }
    [[nodiscard]] float skyDim() const { return mSkyDim; }
    // Lightning: 1 at the strike, gone in a third of a second.
    [[nodiscard]] float flash() const { return mFlash; }
    // True once for each lightning strike.
    [[nodiscard]] bool takeThunder();

    // `daylight` is 1 by day and 0 at night: clouds cast no shadow in the dark.
    void renderCloudShadows(SpriteBatch& batch, const Camera& camera, const glm::vec4& cloudUv, float daylight) const;
    // `sheltered` thins the rain while the player stands under a roof.
    void renderRain(SpriteBatch& batch, const Camera& camera, const glm::vec4& whiteUv, bool sheltered) const;
    void renderFog(SpriteBatch& batch, const Camera& camera, const glm::vec4& cloudUv, const glm::vec4& whiteUv) const;

private:
    struct Drop
    {
        glm::vec2 position{0.0f}; // fractions of the viewport
        float speed = 1.0f;       // viewport heights per second
        float length = 10.0f;     // screen pixels
    };
    struct Cloud
    {
        glm::vec2 position{0.0f}; // world pixels
        float size = 300.0f;      // screen pixels across at zoom 1
        float strength = 1.0f;
    };

    std::mt19937 mRng{20261113u};
    std::vector<Drop> mDrops;
    std::vector<Cloud> mClouds;
    std::vector<Cloud> mFogBanks;
    glm::vec2 mWindDir{0.8f, -0.6f};
    float mClock = 0.0f;
    float mGust = 0.0f;
    float mRain = 0.0f;
    float mSkyDim = 0.0f;
    float mCloudCover = 0.0f; // how strongly cloud shadows show
    float mFog = 0.0f;
    float mFlash = 0.0f;
    float mNextStrike = 6.0f;
    float mSecondPulse = -1.0f;
    float mSplashDebt = 0.0f;
    bool mThunder = false;
};
