// Weather and seasons in play (phase 7): lightning, snow on the roofs,
// frozen ponds, smoke in a burning house, footprints in mud and snow, and
// what the fog hides. The rules live in world/WeatherRules.
#include "core/GameShared.h"
#include "world/WeatherRules.h"

#include <cstdint>
#include <cstdio>
#include <iostream>

using namespace gameshared;

namespace
{
constexpr float kPrintStep = 14.0f;      // world pixels between prints
constexpr float kPrintSeconds = 10.0f;   // 10 game minutes
constexpr std::size_t kMaxPrints = 400;
constexpr float kPrintsNearTiles = 20.0f; // the dead leave prints this near the player
constexpr float kSmokePuffSeconds = 0.35f;

glm::vec2 centreOfTile(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}
} // namespace

bool Game::pondFrozen() const
{
    return weatherrules::pondsFrozen(mCurrentDay);
}

void Game::updateWeather(float dtSeconds)
{
    const WeatherCondition weather = mWeatherState.condition;
    mLightningFlash = std::max(0.0f, mLightningFlash - dtSeconds);
    mHoursSinceRain = weatherrules::rainy(weather) ? 0.0f : mHoursSinceRain + dtSeconds / 60.0f;

    // Winter: ponds freeze and bear your weight; the roofs whiten with snow.
    const bool frozen = pondFrozen();
    if (frozen != mTileMap.waterFrozen())
    {
        mTileMap.setWaterFrozen(frozen);
        std::cout << "[Weather] ponds " << (frozen ? "frozen" : "thawed") << "\n";
    }
    mStructureView.setSnow(mWeatherState.snowCover);

    // Every flash of the sky lights the town for the dead a moment.
    if (mAtmosphere.flash() >= 0.99f) mLightningFlash = weatherrules::kFlashSeconds;

    // A strike close by: Explosive thunder, now and then a tree alight.
    if (weather == WeatherCondition::Thunderstorm && mPlayerEntity != kInvalidEntity)
    {
        mThunderTimer -= dtSeconds;
        if (mThunderTimer <= 0.0f) lightningStrike();
    }
    else
    {
        mThunderTimer = std::max(mThunderTimer, 40.0f);
    }

    // Smoke: inside a building with 3 or more tiles alight you start to
    // cough after 20 s, and it hurts after 40 s.
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        const glm::ivec2 here = playerTile();
        const int building = mTileMap.inBounds(here.x, here.y) ? mTileMap.getBuildingId(here.x, here.y) : 0;
        if (building > 0 && burningIn(building) >= weatherrules::kSmokyTiles)
        {
            needs.smokeSeconds += dtSeconds;
        }
        else
        {
            needs.smokeSeconds = std::max(0.0f, needs.smokeSeconds - dtSeconds * 2.0f);
        }
        if (needs.smokeSeconds >= weatherrules::kSmokeHurtAfterSeconds && !mDevGodMode &&
            mWorld.hasComponent<Health>(mPlayerEntity))
        {
            Health& health = mWorld.getComponent<Health>(mPlayerEntity);
            health.current = std::max(0.0f, health.current - weatherrules::kSmokeDamagePerSecond * dtSeconds);
        }
    }

    // Smoke rising over whatever burns.
    mSmokePuffTimer += dtSeconds;
    if (mSmokePuffTimer >= kSmokePuffSeconds && mFireSystem.count() > 0)
    {
        mSmokePuffTimer = 0.0f;
        int n = 0;
        for (const FireSystem::Burning& b : mFireSystem.burningNear(playerTile(), 30))
        {
            if ((b.tile.x + b.tile.y + static_cast<int>(mZombieAISystem.ticks() / 20)) % 4 != 0) continue;
            mParticleSystem.spawnSmoke(centreOfTile(mTileMap, b.tile) - glm::vec2(0.0f, 20.0f), 1);
            if (++n >= 12) break;
        }
    }

    updateFootprints(dtSeconds);
}

void Game::lightningStrike()
{
    std::mt19937 rng(mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(mGameHours * 600.0) ^
                     static_cast<std::uint32_t>(mCurrentDay * 104729));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    mThunderTimer = 40.0f + unit(rng) * 80.0f;
    mLightningFlash = weatherrules::kFlashSeconds;
    mAtmosphere.strike();
    if (mPlayerEntity == kInvalidEntity) return;

    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 player = playerWorldCenter();
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        const float angle = unit(rng) * 6.2831853f;
        const float distance = unit(rng) * 40.0f * tile;
        const glm::vec2 at = player + glm::vec2(std::cos(angle), std::sin(angle)) * distance;
        const glm::ivec2 t = worldToTile(mTileMap, at);
        if (!mTileMap.inBounds(t.x, t.y) || mTileMap.isSolid(t.x, t.y) || mTileMap.isIndoors(t.x, t.y)) continue;
        mNoiseModel.emitNoise(at, NoiseTier::Explosive, 1.0f);
        mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.2f);
        break;
    }

    // One strike in twenty sets a tree alight.
    if (unit(rng) >= weatherrules::kLightningTreeChance) return;
    std::vector<glm::ivec2> near;
    for (const glm::ivec2& t : mSceneryView.treeTiles())
    {
        if (glm::length(centreOfTile(mTileMap, t) - player) <= 40.0f * tile) near.push_back(t);
    }
    if (near.empty()) return;
    const glm::ivec2 hit = near[static_cast<std::size_t>(unit(rng) * static_cast<float>(near.size())) % near.size()];
    if (igniteFire(hit, 1.0f, false)) std::cout << "[Weather] lightning set a tree alight at " << hit.x << "," << hit.y << "\n";
}

void Game::updateFootprints(float dtSeconds)
{
    for (Footprint& print : mFootprints) print.life -= dtSeconds;
    std::erase_if(mFootprints, [](const Footprint& p) { return p.life <= 0.0f; });

    const WeatherCondition weather = mWeatherState.condition;
    const bool mud = weatherrules::rainy(weather) || mHoursSinceRain < 1.0f;
    const bool snow = mWeatherState.snowCover >= weatherrules::kSnowDeepEnough;
    if (!mud && !snow)
    {
        mLastPrint.clear();
        return;
    }
    if (mPlayerEntity == kInvalidEntity) return;
    const glm::vec2 player = playerWorldCenter();
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));

    const auto step = [&](Entity e, const Transform& t)
    {
        const glm::vec2 feet(t.x + 16.0f, t.y + 24.0f);
        const glm::ivec2 at = worldToTile(mTileMap, feet);
        if (!mTileMap.inBounds(at.x, at.y) || mTileMap.isIndoors(at.x, at.y)) return;
        const int base = ((std::max(1, mTileMap.groundGid(at.x, at.y)) - 1) % TileGid::kPerRow) + 1;
        const bool soft = TileGid::isGrass(base) || base == TileGid::kDirt;
        const bool printSnow = snow;
        if (!printSnow && !(mud && soft)) return;
        auto [it, fresh] = mLastPrint.try_emplace(e, feet);
        if (fresh) return;
        const glm::vec2 d = feet - it->second;
        if (glm::dot(d, d) < kPrintStep * kPrintStep) return;
        it->second = feet;
        if (mFootprints.size() >= kMaxPrints) mFootprints.erase(mFootprints.begin());
        mFootprints.push_back(Footprint{feet, std::atan2(d.y, d.x), kPrintSeconds, printSnow});
    };
    step(mPlayerEntity, mWorld.getComponent<Transform>(mPlayerEntity));
    mWorld.forEach<SurvivorAI, Transform>([&](Entity e, SurvivorAI&, Transform& t) { step(e, t); });
    mZombieAISystem.grid().query(player, kPrintsNearTiles * tile, SpatialGrid::Zombie,
                                 [&](const SpatialGrid::Item& item)
                                 {
                                     if (mWorld.hasComponent<Transform>(item.entity))
                                         step(item.entity, mWorld.getComponent<Transform>(item.entity));
                                 });
}

void Game::drawFootprints()
{
    if (mFootprints.empty()) return;
    const SpriteRegion* region = mSpriteSheet.find("fx_footstep");
    if (region == nullptr) return;
    const float zoom = mCamera.zoom();
    const glm::vec2 viewport = mCamera.viewportSize();
    for (std::size_t i = 0; i < mFootprints.size(); ++i)
    {
        const Footprint& print = mFootprints[i];
        if (!deadDrawnAt(print.position)) continue;
        const glm::vec2 ground = mCamera.worldToScreen(print.position);
        if (ground.x < -20.0f || ground.y < -20.0f || ground.x > viewport.x + 20.0f || ground.y > viewport.y + 20.0f) continue;
        const float fade = std::clamp(print.life / kPrintSeconds, 0.0f, 1.0f);
        const glm::vec2 size(6.0f * zoom, 4.0f * zoom);
        const glm::vec4 color = print.snow ? glm::vec4(0.55f, 0.60f, 0.70f, 0.55f * fade) : glm::vec4(0.20f, 0.14f, 0.09f, 0.55f * fade);
        mSpriteBatch.draw(glm::round(ground - size * 0.5f), size, mSpriteSheet.frameUv("fx_footstep", static_cast<int>(i & 1u)), color);
    }
}

void Game::collectFireSprites()
{
    if (mFireSystem.count() == 0) return;
    const bool flames = mSpriteSheet.find("fx_flame_0") != nullptr;
    const float zoom = mCamera.zoom();
    const glm::vec2 viewport = mCamera.viewportSize();
    const int frameBase = static_cast<int>(mHudClock * 10.0f);
    for (const FireSystem::Burning& b : mFireSystem.burningNear(playerTile(), 40))
    {
        const glm::vec2 ground = mCamera.worldToScreen(centreOfTile(mTileMap, b.tile));
        if (ground.x < -64.0f || ground.y < -64.0f || ground.x > viewport.x + 64.0f || ground.y > viewport.y + 96.0f) continue;
        const float scale = 0.6f + 0.4f * std::clamp(b.heat, 0.0f, 1.0f);
        const glm::vec2 size = glm::vec2(32.0f, 40.0f) * zoom * scale;
        const int frame = (frameBase + b.tile.x * 3 + b.tile.y) & 3;
        const glm::vec4 uv = flames ? mSpriteSheet.uvRect("fx_flame_" + std::to_string(frame)) : mSpriteSheet.uvRect("env_campfire_lit");
        // A few pixels off the middle of the tile, so a field of fire is not a grid.
        const std::uint32_t h = static_cast<std::uint32_t>(b.tile.x) * 73856093u ^ static_cast<std::uint32_t>(b.tile.y) * 19349663u;
        const glm::vec2 jitter(static_cast<float>(static_cast<int>(h % 13u) - 6), static_cast<float>(static_cast<int>((h >> 8) % 7u) - 3));
        const glm::vec2 topLeft(std::round(ground.x - size.x * 0.5f + jitter.x * zoom),
                                std::round(ground.y - size.y + (6.0f + jitter.y) * zoom));
        const float depth = static_cast<float>(b.tile.x + b.tile.y) * 32.0f + 1.0f;
        mStaticSprites.push_back(StaticSprite{topLeft, size, uv, glm::vec4(1.4f, 1.2f, 1.0f, 0.95f), depth});
    }
}

void Game::drawFuelShimmer()
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    const Traits* traits = playerTraits();
    if (traits == nullptr || traits->bloodline != BloodlineTrait::Firewise) return;
    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const int slot = std::clamp(inv.activeSlot, 0, Inventory::kMaxSlots - 1);
    const InventorySlot& held = inv.slots[static_cast<std::size_t>(slot)];
    if (held.count <= 0 || (held.itemId != mItems.molotov && held.itemId != mItems.matches)) return;
    const glm::vec4 uv = mSpriteSheet.uvRect("solid_white");
    const float zoom = mCamera.zoom();
    const glm::ivec2 centre = playerTile();
    constexpr int kReach = 6;
    for (int y = centre.y - kReach; y <= centre.y + kReach; ++y)
    {
        for (int x = centre.x - kReach; x <= centre.x + kReach; ++x)
        {
            const float fuel = tileFuel(x, y);
            if (fuel <= 0.0f) continue;
            const float wave = 0.6f + 0.4f * std::sin(mHudClock * 3.0f + static_cast<float>(x * 7 + y * 3));
            const glm::vec2 ground = mCamera.worldToScreen(centreOfTile(mTileMap, glm::ivec2(x, y)));
            const glm::vec2 size(18.0f * zoom, 9.0f * zoom);
            mSpriteBatch.draw(glm::round(ground - size * 0.5f), size, uv, glm::vec4(1.0f, 0.62f, 0.25f, 0.22f * fuel * wave));
        }
    }
}

bool Game::deadDrawnAt(const glm::vec2& world) const
{
    const float tiles = weatherrules::deadDrawTiles(mWeatherState.condition);
    if (tiles <= 0.0f || mPlayerEntity == kInvalidEntity) return true;
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    return glm::length(world - playerWorldCenter()) <= tiles * tile;
}

std::string Game::weatherInfoLine() const
{
    const WeatherCondition c = mWeatherState.condition;
    bool raincoat = false;
    float bodyTemp = 0.0f;
    float clothing = 0.0f;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        raincoat = needs.raincoat;
        bodyTemp = needs.bodyTemperature;
        clothing = needs.clothingInsulation;
    }
    const weatherrules::ViewFade fade = weatherrules::playerViewFade(c);
    char line[480];
    std::snprintf(line, sizeof(line),
                  "condition=%s season=%s noise=%.2f sight=%.2f view=%.2f fog=%.1f-%.1f body=%+.1f raincoat=%d fire=%.2f "
                  "grassfire=%.2f air=%.1f snow=%.2f wind=%.2f,%.2f@%.2f temp=%.2f clothing=%.1f",
                  weatherrules::label(c), weatherrules::seasonName(weatherrules::seasonOf(mCurrentDay)),
                  weatherrules::noiseScale(c), weatherrules::deadSightScale(c, false), weatherrules::playerViewScale(c),
                  fade.startTiles, fade.endTiles, weatherrules::bodyShift(c, raincoat), raincoat ? 1 : 0,
                  weatherrules::fireSpreadScale(c, false), weatherrules::fireSpreadScale(c, true), mWeatherState.airTemperature,
                  mWeatherState.snowCover, mWeatherState.windX, mWeatherState.windY, mWeatherState.windStrength, bodyTemp, clothing);
    return line;
}
