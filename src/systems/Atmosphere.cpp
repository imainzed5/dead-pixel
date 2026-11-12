#include "systems/Atmosphere.h"

#include <algorithm>
#include <cmath>

#include "map/TileMap.h"
#include "rendering/Camera.h"
#include "rendering/SpriteBatch.h"
#include "systems/ParticleSystem.h"

namespace
{
constexpr float kCloudField = 1500.0f; // clouds wrap inside a square this wide round the camera

float approach(float value, float target, float step)
{
    return value < target ? std::min(target, value + step) : std::max(target, value - step);
}

float wrap(float value, float centre, float span)
{
    const float half = span * 0.5f;
    float d = std::fmod(value - centre + half, span);
    if (d < 0.0f)
    {
        d += span;
    }
    return centre - half + d;
}
} // namespace

float Atmosphere::skyDimFor(WeatherCondition condition)
{
    switch (condition)
    {
    case WeatherCondition::Clear:
    case WeatherCondition::Heatwave:
        return 0.0f;
    case WeatherCondition::Cloudy:
        return 0.08f;
    case WeatherCondition::Fog:
        return 0.10f;
    case WeatherCondition::Rain:
        return 0.16f;
    case WeatherCondition::HeavyRain:
    case WeatherCondition::Blizzard:
        return 0.24f;
    case WeatherCondition::Thunderstorm:
        return 0.32f;
    }
    return 0.0f;
}

bool Atmosphere::takeThunder()
{
    const bool struck = mThunder;
    mThunder = false;
    return struck;
}

void Atmosphere::update(float dtSeconds, const WeatherState& weather, const Camera& camera, const TileMap& map,
                        ParticleSystem& particles)
{
    const float dt = std::max(0.0f, dtSeconds);
    mClock += dt;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    // Weather comes on and clears over a few seconds rather than at a cut.
    mRain = approach(mRain, weather.rainIntensity, dt * 0.35f);
    mSkyDim = approach(mSkyDim, skyDimFor(weather.condition), dt * 0.08f);
    mFog = approach(mFog, weather.condition == WeatherCondition::Fog ? 1.0f : 0.0f, dt * 0.2f);
    const bool broken = weather.condition == WeatherCondition::Clear || weather.condition == WeatherCondition::Cloudy ||
                        weather.condition == WeatherCondition::Heatwave;
    const float cover = !broken ? 0.0f : (weather.condition == WeatherCondition::Cloudy ? 1.0f : 0.55f);
    mCloudCover = approach(mCloudCover, cover, dt * 0.2f);

    // Wind: there is always a breath of it, and it comes in gusts.
    const float base = 0.12f + weather.windStrength * 0.88f;
    const float swell = 0.5f + 0.5f * std::sin(mClock * 0.37f) * std::sin(mClock * 0.11f + 1.3f);
    mGust = std::clamp(base * (0.55f + 0.45f * swell), 0.0f, 1.0f);
    const float heading = -0.65f + 0.35f * std::sin(mClock * 0.021f);
    mWindDir = glm::vec2(std::cos(heading), std::sin(heading));

    // Rain: drops live in screen space and wrap.
    const auto wanted = static_cast<std::size_t>(mRain * static_cast<float>(kMaxDrops));
    while (mDrops.size() < wanted)
    {
        mDrops.push_back(Drop{glm::vec2(unit(mRng), unit(mRng)), 1.5f + unit(mRng) * 0.9f, 10.0f + unit(mRng) * 14.0f});
    }
    if (mDrops.size() > wanted)
    {
        mDrops.resize(wanted);
    }
    const float slant = 0.10f + mGust * 0.55f;
    for (Drop& drop : mDrops)
    {
        drop.position.y += drop.speed * dt;
        drop.position.x += drop.speed * slant * dt;
        if (drop.position.y > 1.0f)
        {
            drop.position.y -= 1.0f;
            drop.position.x = unit(mRng);
        }
        if (drop.position.x > 1.0f)
        {
            drop.position.x -= 1.0f;
        }
    }

    // Splashes where drops meet open ground.
    const glm::vec2 viewport = camera.viewportSize();
    mSplashDebt += mRain * 140.0f * dt;
    while (mSplashDebt >= 1.0f)
    {
        mSplashDebt -= 1.0f;
        const glm::vec2 world = camera.screenToWorld(glm::vec2(unit(mRng) * viewport.x, unit(mRng) * viewport.y));
        const int tx = static_cast<int>(std::floor(world.x / static_cast<float>(map.tileWidth())));
        const int ty = static_cast<int>(std::floor(world.y / static_cast<float>(map.tileHeight())));
        if (map.inBounds(tx, ty) && !map.isIndoors(tx, ty) && !map.blocksSight(tx, ty))
        {
            particles.spawnSplash(world);
        }
    }

    // Clouds and fog banks drift with the wind and wrap round the camera.
    if (mClouds.empty())
    {
        for (int i = 0; i < 9; ++i)
        {
            mClouds.push_back(Cloud{glm::vec2(unit(mRng), unit(mRng)) * kCloudField, 380.0f + unit(mRng) * 420.0f,
                                    0.6f + unit(mRng) * 0.4f});
        }
        for (int i = 0; i < 10; ++i)
        {
            mFogBanks.push_back(Cloud{glm::vec2(unit(mRng), unit(mRng)) * kCloudField, 520.0f + unit(mRng) * 420.0f,
                                      0.5f + unit(mRng) * 0.5f});
        }
    }
    const glm::vec2 centre = camera.screenToWorld(viewport * 0.5f);
    const glm::vec2 drift = mWindDir * ((14.0f + mGust * 70.0f) * dt);
    for (Cloud& cloud : mClouds)
    {
        cloud.position += drift;
        cloud.position.x = wrap(cloud.position.x, centre.x, kCloudField);
        cloud.position.y = wrap(cloud.position.y, centre.y, kCloudField);
    }
    for (Cloud& bank : mFogBanks)
    {
        bank.position += drift * 0.5f + glm::vec2(4.0f, 2.0f) * dt;
        bank.position.x = wrap(bank.position.x, centre.x, kCloudField);
        bank.position.y = wrap(bank.position.y, centre.y, kCloudField);
    }

    // Lightning: a strike, and often a second flicker right behind it.
    mFlash = std::max(0.0f, mFlash - kFlashDecayPerSecond * dt);
    if (weather.condition == WeatherCondition::Thunderstorm)
    {
        mNextStrike -= dt;
        if (mNextStrike <= 0.0f)
        {
            mFlash = 1.0f;
            mThunder = true;
            mNextStrike = 7.0f + unit(mRng) * 14.0f;
            mSecondPulse = unit(mRng) < 0.6f ? 0.16f : -1.0f;
        }
        if (mSecondPulse > 0.0f)
        {
            mSecondPulse -= dt;
            if (mSecondPulse <= 0.0f)
            {
                mFlash = std::max(mFlash, 0.7f);
                mSecondPulse = -1.0f;
            }
        }
    }
    else
    {
        mNextStrike = std::min(mNextStrike, 5.0f);
    }
}

void Atmosphere::renderCloudShadows(SpriteBatch& batch, const Camera& camera, const glm::vec4& cloudUv,
                                    float daylight) const
{
    const float strength = mCloudCover * std::clamp(daylight, 0.0f, 1.0f);
    if (strength <= 0.01f)
    {
        return;
    }
    const float zoom = camera.zoom();
    const glm::vec2 viewport = camera.viewportSize();
    for (const Cloud& cloud : mClouds)
    {
        const glm::vec2 size(cloud.size * zoom, cloud.size * 0.5f * zoom);
        const glm::vec2 topLeft = camera.worldToScreen(cloud.position) - size * 0.5f;
        if (topLeft.x + size.x < 0.0f || topLeft.x > viewport.x || topLeft.y + size.y < 0.0f || topLeft.y > viewport.y)
        {
            continue;
        }
        batch.draw(topLeft, size, cloudUv, glm::vec4(0.02f, 0.03f, 0.08f, 0.26f * strength * cloud.strength));
    }
}

void Atmosphere::renderRain(SpriteBatch& batch, const Camera& camera, const glm::vec4& whiteUv, bool sheltered) const
{
    if (mDrops.empty())
    {
        return;
    }
    const glm::vec2 viewport = camera.viewportSize();
    const float alpha = (sheltered ? 0.30f : 0.62f) * (0.6f + 0.4f * mRain);
    const float slant = 0.10f + mGust * 0.55f;
    const glm::vec4 color(0.84f, 0.90f, 1.0f, alpha);
    for (const Drop& drop : mDrops)
    {
        // A streak is three short pieces stepped sideways: the batch draws
        // no slanted quads.
        const glm::vec2 head(drop.position.x * viewport.x, drop.position.y * viewport.y);
        const float piece = drop.length / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float back = static_cast<float>(i) * piece;
            batch.draw(glm::vec2(std::round(head.x - back * slant), std::round(head.y - back - piece)),
                       glm::vec2(1.0f, piece), whiteUv, color);
        }
    }
}

void Atmosphere::renderFog(SpriteBatch& batch, const Camera& camera, const glm::vec4& cloudUv,
                           const glm::vec4& whiteUv) const
{
    // Heavy rain hangs a haze of its own.
    const float haze = std::max(mFog, std::max(0.0f, mRain - 0.5f) * 0.6f);
    if (haze <= 0.01f)
    {
        return;
    }
    const glm::vec2 viewport = camera.viewportSize();
    const glm::vec3 grey(0.70f, 0.74f, 0.78f);
    batch.draw(glm::vec2(0.0f), viewport, whiteUv, glm::vec4(grey, 0.30f * haze));
    const float zoom = camera.zoom();
    for (const Cloud& bank : mFogBanks)
    {
        const glm::vec2 size(bank.size * zoom, bank.size * 0.5f * zoom);
        const glm::vec2 topLeft = camera.worldToScreen(bank.position) - size * 0.5f;
        if (topLeft.x + size.x < 0.0f || topLeft.x > viewport.x || topLeft.y + size.y < 0.0f || topLeft.y > viewport.y)
        {
            continue;
        }
        batch.draw(topLeft, size, cloudUv, glm::vec4(grey + 0.12f, 0.30f * haze * bank.strength));
    }
}
