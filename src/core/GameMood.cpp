// Mood on the screen (phase 12): the character's state carried by the
// picture, and the small things that make the town a place. Nothing here
// changes the world: it reads it, keeps its own clocks and numbers, and
// draws. All of it is gated by the screen-effects level (mScreenEffects),
// which the harness starts at 0 (`fx on` raises it), so older screenshots
// do not change.
#include "core/GameShared.h"
#include "world/BuildingKinds.h"
#include "world/MoodRules.h"
#include "world/Power.h"
#include "world/SleepRules.h"
#include "world/WeatherRules.h"

#include <cmath>

using namespace gameshared;

namespace
{
constexpr float kBreathPuffSeconds = 2.2f;
constexpr float kFliesAfterSeconds = 1440.0f; // a game day
constexpr float kDetailTiles = 18.0f;     // flies, curtains: drawn this near
constexpr float kShadowFromLength = 0.9f; // only the long shadows of morning and evening

glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

// A stable 0-1 from three numbers (motes, flies): no generator, no state.
float hash3(int a, int b, int c)
{
    std::uint32_t h = static_cast<std::uint32_t>(a) * 73856093u ^ static_cast<std::uint32_t>(b) * 19349663u ^
                      static_cast<std::uint32_t>(c) * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return static_cast<float>(h & 0xFFFFu) / 65535.0f;
}
} // namespace

float Game::moodLevel() const
{
    if (mRunState != RunState::Playing && mRunState != RunState::Paused) return 0.0f;
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity)) return 0.0f;
    return std::clamp(mScreenEffects, 0.0f, 1.0f);
}

float Game::moodRoll()
{
    // xorshift on the mood's own seed: blinks never touch the simulation's numbers.
    mMoodSeed ^= mMoodSeed << 13;
    mMoodSeed ^= mMoodSeed >> 17;
    mMoodSeed ^= mMoodSeed << 5;
    return static_cast<float>(mMoodSeed & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
}

const char* Game::moodIdle() const
{
    if (moodLevel() <= 0.0f || !mWorld.hasComponent<Needs>(mPlayerEntity)) return "";
    const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
    const float depression = mWorld.hasComponent<MentalState>(mPlayerEntity)
                                 ? mWorld.getComponent<MentalState>(mPlayerEntity).depression
                                 : 0.0f;
    float stamina01 = 1.0f;
    if (mWorld.hasComponent<Stamina>(mPlayerEntity))
    {
        const Stamina& s = mWorld.getComponent<Stamina>(mPlayerEntity);
        stamina01 = s.maximum > 0.0f ? s.current / s.maximum : 1.0f;
    }
    const float debt = sleeprules::shownDebt(needs.sleepDebt, needs.coffeeHours);
    return moodrules::idleName(moodrules::idleFor(depression, needs.bodyTemperature, debt, stamina01));
}

void Game::updateMood(float dtSeconds)
{
    mMoodClock += dtSeconds;
    const float level = moodLevel();
    if (level <= 0.0f)
    {
        mMoodParams = moodrules::Params{};
        return;
    }

    moodrules::Facts f;
    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        f.panic = mental.panic;
        f.depression = mental.depression;
    }
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        f.sleepDebt = sleeprules::shownDebt(needs.sleepDebt, needs.coffeeHours);
        f.bodyTemperature = needs.bodyTemperature;
        f.illness = needs.illness;
    }
    {
        // Breathing smoke: inside a building that is burning.
        const glm::ivec2 tile = playerTile();
        const int building = mTileMap.inBounds(tile.x, tile.y) ? mTileMap.getBuildingId(tile.x, tile.y) : 0;
        f.smoke = building > 0 && burningIn(building) > 0;
    }
    if (mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& h = mWorld.getComponent<Health>(mPlayerEntity);
        f.health = h.maximum > 0.0f ? h.current / h.maximum : 1.0f;
    }
    f.held = playerHeld();
    f.listening = mListening;

    // The heart, seen: the same rate the heartbeat is heard at.
    const float bpm = std::max(f.health < moodrules::kPulseBelowHealth ? 100.0f : 60.0f,
                               60.0f + 80.0f * std::clamp(f.panic / 100.0f, 0.0f, 1.0f));
    mBeatClock += dtSeconds * bpm / 60.0f;
    mBeatClock -= std::floor(mBeatClock);
    mHeartbeatSeen = std::max(0.0f, 1.0f - mBeatClock * 5.0f);
    f.heartbeat = mHeartbeatSeen;

    // Blinks, when very tired: every 20 to 40 s.
    if (f.sleepDebt >= moodrules::kDebtBlinkFrom)
    {
        mBlinkTimer -= dtSeconds;
        if (mBlinkTimer <= 0.0f && mBlinkLeft <= 0.0f)
        {
            mBlinkLeft = moodrules::kBlinkSeconds;
            mBlinkTimer = moodrules::nextBlinkSeconds(moodRoll());
        }
    }
    if (mBlinkLeft > 0.0f)
    {
        mBlinkLeft = std::max(0.0f, mBlinkLeft - dtSeconds);
        f.blink = moodrules::blinkShut(moodrules::kBlinkSeconds - mBlinkLeft);
    }
    mMoodParams = moodrules::params(f, level);

    const glm::vec2 here = playerWorldCenter();
    const bool outdoors = !mTileMap.isIndoorsAtWorld(here.x, here.y);
    const bool paused = mRunState != RunState::Playing;
    if (paused) return;

    // Breath in the cold, outdoors.
    if (outdoors && mWeatherState.airTemperature < moodrules::kBreathPuffBelow)
    {
        mBreathPuffTimer -= dtSeconds;
        if (mBreathPuffTimer <= 0.0f)
        {
            mBreathPuffTimer = kBreathPuffSeconds * (0.8f + 0.4f * moodRoll());
            float face = 1.0f;
            if (mWorld.hasComponent<Sprite>(mPlayerEntity) && mWorld.getComponent<Sprite>(mPlayerEntity).flipX) face = -1.0f;
            mParticleSystem.spawnSmoke(here + glm::vec2(5.0f * face, -14.0f), 1);
        }
    }

    // (Chimney smoke from homes with someone in them is phase 8's:
    // updateSurvivorSmoke.)

    // The town clock: it shows the hour while the power is on, and the
    // minute it failed after.
    if (powerOn()) mClockShows = absoluteHour();
    else if (mClockShows < 0.0) mClockShows = std::min(absoluteHour(), power::failHour(mSaveManager.currentRunSeed()));
}

bool Game::beginPostFx()
{
    if (moodLevel() <= 0.0f || mMoodParams.neutral() || !mPostFx.ready()) return false;
    return mPostFx.begin(mPixelWidth, mPixelHeight, 0.08f, 0.11f, 0.14f);
}

void Game::endPostFx()
{
    if (!mPostFx.active()) return;
    mPostFx.end(mMoodParams, mMoodClock);
    // Back to the sprite program for the HUD.
    mShader.use();
    mShader.setMat4("uProjection", mProjection);
    mShader.setInt("uTexture", 0);
}

void Game::renderMoodWorld()
{
    if (moodLevel() <= 0.0f) return;
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const float zoom = mCamera.zoom();
    const float px = std::max(1.0f, std::round(zoom));
    const float tilePx = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const glm::vec2 viewport = mCamera.viewportSize();
    const auto onScreen = [&](const glm::vec2& s)
    { return s.x > -64.0f && s.y > -64.0f && s.x < viewport.x + 64.0f && s.y < viewport.y + 64.0f; };
    const auto dot = [&](const glm::vec2& s, const glm::vec4& colour, float size = 1.0f)
    { mSpriteBatch.draw(glm::round(s), glm::vec2(px * size), white, colour); };

    // Long shadows at the ends of the day: characters and trees stretch away
    // from a low sun, and swing with the hour. None at night.
    const moodrules::Sun sun = moodrules::sunAt(mGameHours, weatherrules::dawnHour(mCurrentDay),
                                                weatherrules::duskHour(mCurrentDay));
    const bool clearSky = mWeatherState.condition == WeatherCondition::Clear ||
                          mWeatherState.condition == WeatherCondition::Cloudy;
    if (sun.length > kShadowFromLength && clearSky)
    {
        const float strength = std::clamp((sun.length - kShadowFromLength) / 1.0f, 0.0f, 1.0f);
        const glm::vec2 dir = glm::normalize(glm::vec2(sun.dirX, sun.dirY * 0.5f));
        const auto shadow = [&](const glm::vec2& feetWorld, float height, float width)
        {
            if (mTileMap.isIndoorsAtWorld(feetWorld.x, feetWorld.y)) return;
            const glm::vec2 feet = mCamera.worldToScreen(feetWorld);
            if (!onScreen(feet)) return;
            const float length = height * sun.length * zoom;
            const int steps = std::max(2, static_cast<int>(length / (2.0f * px)));
            for (int i = 0; i < steps; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float w = std::max(px, std::round(width * zoom * (1.0f - 0.6f * t) / px) * px);
                const glm::vec2 at = feet + dir * (length * t) - glm::vec2(w * 0.5f, px);
                mSpriteBatch.draw(glm::round(at), glm::vec2(w, 2.0f * px), white,
                                  glm::vec4(0.02f, 0.02f, 0.05f, 0.34f * strength * (1.0f - 0.6f * t)));
            }
        };
        const float reach = 20.0f * tilePx;
        mWorld.forEach<Transform, Sprite>(
            [&](Entity e, Transform& t, Sprite& s)
            {
                if (!s.visible) return;
                const bool body = e == mPlayerEntity || mWorld.hasComponent<ZombieAI>(e) || mWorld.hasComponent<SurvivorAI>(e);
                if (!body) return;
                const glm::vec2 feet(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
                if (glm::length(feet - here) > reach) return;
                shadow(feet, 22.0f, 9.0f);
            });
        std::vector<SceneryCanopy> trees;
        mSceneryView.canopiesNear(here, reach, trees);
        for (const SceneryCanopy& tree : trees) shadow(tree.position, 48.0f, 18.0f);
    }

    // Wet through: drops running off the character.
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        if (needs.wetHours > 0.0f || needs.wetExposure > 5.0f)
        {
            const glm::vec2 body = mCamera.worldToScreen(here);
            for (int i = 0; i < 4; ++i)
            {
                const float phase = std::fmod(mMoodClock * 0.9f + static_cast<float>(i) * 0.27f, 1.0f);
                const float x = (hash3(i, 7, 1) - 0.5f) * 14.0f * zoom;
                const float y = (-26.0f + 22.0f * phase) * zoom;
                dot(body + glm::vec2(x, y), glm::vec4(0.12f, 0.16f, 0.26f, 0.85f * (1.0f - phase)));
            }
        }
    }

    // Flies over bodies a day old, and a faint buzz (the flies layer).
    mWorld.forEach<Corpse, Transform>(
        [&](Entity e, Corpse& c, Transform& t)
        {
            if (c.age < kFliesAfterSeconds) return;
            const glm::vec2 at(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
            if (glm::length(at - here) > kDetailTiles * tilePx) return;
            const glm::vec2 s = mCamera.worldToScreen(at);
            if (!onScreen(s)) return;
            for (int i = 0; i < 3; ++i)
            {
                const float a = mMoodClock * (3.0f + static_cast<float>(i)) + hash3(static_cast<int>(e), i, 3) * 6.28f;
                const glm::vec2 o(std::cos(a) * 7.0f, std::sin(a * 1.7f) * 4.0f - 8.0f);
                dot(s + o * zoom, glm::vec4(0.05f, 0.05f, 0.05f, 0.9f));
                // A wing catching the light, so they show on dark ground too.
                const float flicker = 0.35f + 0.35f * std::sin(mMoodClock * 40.0f + static_cast<float>(i) * 2.1f);
                dot(s + o * zoom - glm::vec2(0.0f, px), glm::vec4(0.82f, 0.84f, 0.88f, flicker));
            }
        });

    // Dust in the light: motes in the flashlight's beam, and in a lamp's pool indoors.
    {
        const auto motes = [&](const glm::vec2& centreWorld, const glm::vec2& spread, int count, int salt)
        {
            for (int i = 0; i < count; ++i)
            {
                const float rx = hash3(i, salt, 5) - 0.5f;
                const float ry = hash3(i, salt, 9) - 0.5f;
                const float drift = mMoodClock * (0.15f + 0.1f * hash3(i, salt, 11));
                const glm::vec2 w = centreWorld + glm::vec2(rx * spread.x + std::sin(drift * 2.0f + rx * 9.0f) * 5.0f,
                                                            ry * spread.y + std::cos(drift * 1.3f + ry * 7.0f) * 4.0f);
                const float glint = 0.55f + 0.35f * std::sin(mMoodClock * 2.0f + static_cast<float>(i));
                dot(mCamera.worldToScreen(w) - glm::vec2(0.0f, 10.0f * zoom), glm::vec4(1.0f, 0.95f, 0.8f, glint));
            }
        };
        if (mFlashlightOn)
        {
            // Along the beam, which follows the facing (GameRender's spot).
            const float angle = mWorld.hasComponent<Facing>(mPlayerEntity) ? mWorld.getComponent<Facing>(mPlayerEntity).angleRadians : 0.0f;
            const glm::vec2 dir(std::cos(angle), std::sin(angle));
            motes(here + dir * 2.5f * tilePx, glm::vec2(2.0f * tilePx, 1.2f * tilePx), 8, 1);
        }
        if (mTileMap.isIndoorsAtWorld(here.x, here.y))
        {
            int salt = 20;
            for (const glm::ivec2& lamp : mLitLamps)
            {
                const glm::vec2 at = tileCentre(mTileMap, lamp);
                if (glm::length(at - here) > 6.0f * tilePx) continue;
                motes(at, glm::vec2(2.0f * tilePx), 4, salt++);
            }
        }
    }

    // Curtains at a broken window lift in the wind.
    if (mWeatherState.windStrength > 0.2f)
    {
        for (const Windows::Window& window : mWindows.all())
        {
            const glm::ivec2 tile = window.spot.tile;
            if (!mTileMap.windowOpen(tile.x, tile.y) || !mTileMap.windowCovered(tile.x, tile.y)) continue;
            // On the wall's inner face (the window is the wall tile itself).
            const glm::vec2 at = 0.5f * (tileCentre(mTileMap, tile) + tileCentre(mTileMap, window.spot.inside));
            if (glm::length(at - here) > kDetailTiles * tilePx) continue;
            const glm::vec2 s = mCamera.worldToScreen(at) - glm::vec2(0.0f, 30.0f * zoom);
            if (!onScreen(s)) continue;
            const float lift = std::sin(mMoodClock * (2.0f + mWeatherState.windStrength * 4.0f) + static_cast<float>(tile.x)) *
                               mWeatherState.windStrength;
            for (int i = 0; i < 5; ++i)
            {
                const float sway = lift * static_cast<float>(i) * 1.6f;
                mSpriteBatch.draw(glm::round(s + glm::vec2(-4.0f * zoom + sway * zoom, static_cast<float>(i) * 3.0f * zoom)),
                                  glm::vec2(8.0f * zoom, 3.0f * zoom), white, glm::vec4(0.62f, 0.55f, 0.48f, 0.9f));
            }
        }
    }

    // The town clock on the town-centre building's front wall.
    if (mClockBuilding == -2)
    {
        // The largest shop, office or station nearest the middle of town.
        mClockBuilding = -1;
        const LayoutData& layout = mMapGenerator.layoutData();
        const glm::vec2 middle(static_cast<float>(mTileMap.width()) * 0.5f, static_cast<float>(mTileMap.height()) * 0.5f);
        float best = 1e9f;
        for (std::size_t i = 0; i < layout.buildings.size(); ++i)
        {
            const LayoutData::BuildingRect& r = layout.buildings[i];
            const int kind = r.kind;
            if (kind == static_cast<int>(BuildingKind::House) || kind == static_cast<int>(BuildingKind::GuardHut) || kind < 0)
            {
                continue;
            }
            const glm::vec2 c(static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f,
                              static_cast<float>(r.y) + static_cast<float>(r.h) * 0.5f);
            const float d = glm::length(c - middle);
            if (d < best)
            {
                best = d;
                mClockBuilding = static_cast<int>(i);
            }
        }
    }
    if (mClockBuilding >= 0 && mClockShows >= 0.0)
    {
        const LayoutData::BuildingRect& r = mMapGenerator.layoutData().buildings[static_cast<std::size_t>(mClockBuilding)];
        const glm::vec2 wall((static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f) * tilePx,
                             (static_cast<float>(r.y + r.h) - 0.02f) * tilePx);
        const glm::vec2 s = mCamera.worldToScreen(wall) - glm::vec2(0.0f, 46.0f * zoom);
        if (onScreen(s) && glm::length(wall - here) < 30.0f * tilePx)
        {
            const float size = 9.0f * px;
            mSpriteBatch.draw(glm::round(s - glm::vec2(size * 0.5f + px)), glm::vec2(size + 2.0f * px), white,
                              glm::vec4(0.16f, 0.13f, 0.11f, 1.0f));
            mSpriteBatch.draw(glm::round(s - glm::vec2(size * 0.5f)), glm::vec2(size), white, glm::vec4(0.86f, 0.83f, 0.74f, 1.0f));
            const double hour = std::fmod(mClockShows, 24.0);
            const float minuteAngle = static_cast<float>(std::fmod(hour, 1.0)) * 6.2831853f;
            const float hourAngle = static_cast<float>(std::fmod(hour, 12.0) / 12.0) * 6.2831853f;
            const auto hand = [&](float angle, float length)
            {
                for (int i = 1; i <= static_cast<int>(length); ++i)
                {
                    const glm::vec2 p(std::sin(angle) * static_cast<float>(i), -std::cos(angle) * static_cast<float>(i));
                    dot(s + glm::round(p) * px - glm::vec2(px * 0.5f), glm::vec4(0.08f, 0.07f, 0.06f, 1.0f));
                }
            };
            hand(hourAngle, 2.0f);
            hand(minuteAngle, 4.0f);
        }
    }
}
