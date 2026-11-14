// Power (phase 6): the town's lights until day 5 at dusk, the dips before,
// floor lamps, the flashlight's battery and the radio. When it fails is
// world/Power's rule; this is where the game follows it.
#include "core/GameShared.h"

#include <iostream>

#include "world/Power.h"

using namespace gameshared;

namespace
{
glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

constexpr float kLampReachTiles = 1.3f;
// The radio: heard a little way off; music in shelter eases the mind.
constexpr float kRadioNoiseSeconds = 5.0f;
constexpr float kRadioReliefPerHour = 2.0f;
// One forecast a game hour (a game hour is a real minute).
constexpr float kForecastSeconds = 60.0f;
constexpr float kForecastHours = 12.0f;
} // namespace

bool Game::powerOn() const
{
    return power::onWith(absoluteHour(), mSaveManager.currentRunSeed(), mPowerForced);
}

void Game::updatePower(float)
{
    const std::uint32_t seed = mSaveManager.currentRunSeed();
    const double hour = absoluteHour();
    const bool on = powerOn();
    const bool failed = mPowerForced == 0 || (mPowerForced < 0 && hour >= power::failHour(seed));
    const bool asleep = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Sleeping>(mPlayerEntity);
    if (on != mPowerWasOn)
    {
        mPowerWasOn = on;
        applyFurnitureAlts();
        if (!on && !failed && !mDipAnnounced)
        {
            mDipAnnounced = true;
            if (!asleep)
            {
                mInteractionMessage = "The lights dip.";
                mInteractionMessageTimer = 3.0f;
            }
            std::cout << "[Power] a dip on day " << mCurrentDay << " at " << mGameHours << "\n";
        }
    }
    if (failed && !mPowerOutAnnounced)
    {
        mPowerOutAnnounced = true;
        if (asleep)
        {
            mPowerWentAsleep = true;
        }
        else
        {
            mInteractionMessage = "The power is out.";
            mInteractionMessageTimer = 4.0f;
        }
        std::cout << "[Power] out on day " << mCurrentDay << " at " << mGameHours << "\n";
    }
    if (mPowerWentAsleep && !asleep)
    {
        mPowerWentAsleep = false;
        mInteractionMessage = "The power went in the night.";
        mInteractionMessageTimer = 4.0f;
    }
}

bool Game::lampLit(const glm::ivec2& tile) const
{
    return powerOn() && std::find(mLitLamps.begin(), mLitLamps.end(), tile) != mLitLamps.end();
}

bool Game::handleFloorLamp(const ProximityPrompt& prompt)
{
    if (mPlayerEntity == kInvalidEntity) return false;
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const FurniturePiece* lamp = nullptr;
    float best = kLampReachTiles * tileSize;
    // Anything else in reach (a cupboard, a bottle on the floor) keeps E, as
    // it does from a bush: a lamp stands among the furniture of a room and
    // must not hide it. Step round to the lamp's free side to switch it.
    if (prompt.entity != kInvalidEntity) return false;
    for (const FurniturePiece& piece : mFurniture)
    {
        if (piece.kind != FurnitureKind::FloorLamp) continue;
        const float d = glm::length(tileCentre(mTileMap, piece.tile) - here);
        if (d < best)
        {
            best = d;
            lamp = &piece;
        }
    }
    if (lamp == nullptr) return false;
    const bool switchedOn = std::find(mLitLamps.begin(), mLitLamps.end(), lamp->tile) != mLitLamps.end();
    mProximityPrompt = std::string("Floor lamp   E: ") + (switchedOn ? "Switch it off" : "Switch it on") + "   Hold E: Push   X: Break up";
    if (!mInput.wasKeyPressed(SDL_SCANCODE_E)) return false;
    const glm::ivec2 tile = lamp->tile;
    if (switchedOn)
    {
        std::erase(mLitLamps, tile);
    }
    else
    {
        mLitLamps.push_back(tile);
    }
    applyFurnitureAlts();
    if (!powerOn() && !switchedOn)
    {
        mInteractionMessage = "Click. Nothing. The power is out.";
        mInteractionMessageTimer = 2.0f;
    }
    return true;
}

float Game::flashlightCharge() const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return -1.0f;
    float best = -1.0f;
    for (const InventorySlot& slot : mWorld.getComponent<Inventory>(mPlayerEntity).slots)
    {
        if (slot.itemId == mItems.flashlight && slot.count > 0) best = std::max(best, slot.condition);
    }
    return best;
}

void Game::updateFlashlightBattery(float dtSeconds)
{
    if (!mFlashlightOn || mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    InventorySlot* light = nullptr;
    for (InventorySlot& slot : inv.slots)
    {
        if (slot.itemId == mItems.flashlight && slot.count > 0 && (light == nullptr || slot.condition > light->condition)) light = &slot;
    }
    if (light == nullptr) return;
    light->condition -= static_cast<float>(dtSeconds * kGameHoursPerRealSecond) / power::kFlashlightHours;
    if (light->condition <= 0.0f)
    {
        light->condition = 0.0f;
        mFlashlightOn = false;
        mInteractionMessage = "The flashlight fades and dies.";
        mInteractionMessageTimer = 3.0f;
        std::cout << "[Power] the flashlight's battery is flat\n";
    }
}

void Game::updateRadio(float dtSeconds)
{
    if (!mRadioOn || mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    InventorySlot* radio = nullptr;
    for (InventorySlot& slot : inv.slots)
    {
        const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
        if (item != nullptr && item->hasTag("radio") && (radio == nullptr || slot.condition > radio->condition)) radio = &slot;
    }
    if (radio == nullptr)
    {
        mRadioOn = false;
        return;
    }
    const float hours = static_cast<float>(dtSeconds * kGameHoursPerRealSecond);
    radio->condition -= hours / power::kRadioHours;
    if (radio->condition <= 0.0f)
    {
        radio->condition = 0.0f;
        mRadioOn = false;
        mInteractionMessage = "The radio fades to hiss, then nothing.";
        mInteractionMessageTimer = 3.0f;
        return;
    }
    const float before = mRadioTimer;
    mRadioTimer -= dtSeconds;
    // It is heard a little way off.
    if (std::floor(before / kRadioNoiseSeconds) != std::floor(mRadioTimer / kRadioNoiseSeconds))
    {
        mNoiseModel.emitNoise(playerWorldCenter(), NoiseTier::Soft, 1.0f, mPlayerEntity);
    }
    if (mRadioTimer <= 0.0f)
    {
        mRadioTimer += kForecastSeconds;
        if (mRadioTimer <= 0.0f) mRadioTimer = kForecastSeconds;
        const std::string line = WeatherSystem::forecastLine(mWeatherSystem.forecast(mWeatherState, kForecastHours), mGameHours);
        mInteractionMessage = "Radio: " + line;
        mInteractionMessageTimer = 5.0f;
        std::cout << "[Power] radio: " << line << "\n";
    }
    // Between forecasts, a looped voice about a gate (phase 9).
    updateBroadcast(dtSeconds);
    // In shelter, faint music eases the mind (phase 5's depression).
    const glm::ivec2 me = playerTile();
    if (mTileMap.isIndoors(me.x, me.y)) addDepression(-kRadioReliefPerHour * hours);
}
