// Doors: opening and closing them, survivors passing through, and the dead
// battering them down.
#include "core/GameShared.h"

#include "systems/CollisionSystem.h"

using namespace gameshared;

namespace
{
glm::vec2 doorCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

// Bangs are heard across the street, louder the closer you are.
constexpr float kBangHearingTiles = 18.0f;
constexpr float kBreakMessageTiles = 14.0f;
} // namespace

void Game::updateDoors(float dtSeconds)
{
    mDoors.tick(dtSeconds);

    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 player = playerWorldCenter();
    for (const glm::ivec2& tile : mDoorBlows)
    {
        const Doors::Blow blow = mDoors.batter(mTileMap, tile, Doors::kZombieDamagePerSecond * dtSeconds);
        if (blow != Doors::Blow::Bang && blow != Doors::Blow::Breaks)
        {
            continue;
        }

        const glm::vec2 centre = doorCentre(mTileMap, tile);
        const float tiles = glm::length(centre - player) / tileSize;
        const float volume = std::clamp(1.0f - tiles / kBangHearingTiles, 0.0f, 1.0f);
        mParticleSystem.spawnDust(centre, blow == Doors::Blow::Breaks ? 10 : 3);
        if (blow == Doors::Blow::Bang)
        {
            // Every bang draws more of them.
            mNoiseModel.emitNoise(centre, NoiseTier::Medium, 1.0f);
            if (volume > 0.0f) mAudioManager.playSound(SoundId::WeaponHit, 0.25f + 0.5f * volume);
        }
        else
        {
            mNoiseModel.emitNoise(centre, NoiseTier::Loud, 1.5f);
            if (volume > 0.0f) mAudioManager.playSound(SoundId::ZombieAttack, 0.3f + 0.6f * volume);
            if (tiles <= kBreakMessageTiles)
            {
                mInteractionMessage = "A door gives way.";
                mInteractionMessageTimer = 3.0f;
            }
        }
    }
    mDoorBlows.clear();

    // Survivors open doors as they come to them (and leave them open).
    mWorld.forEach<SurvivorAI, Transform>(
        [&](Entity, SurvivorAI&, Transform& t)
        {
            const glm::vec2 centre(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
            const Doors::Door* door = mDoors.nearest(mTileMap, centre, tileSize * 0.8f);
            if (door != nullptr)
            {
                mDoors.open(mTileMap, door->tile);
            }
        });
}

bool Game::handleDoorInteraction(const ProximityPrompt& entityPrompt)
{
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const Doors::Door* door = mDoors.nearest(mTileMap, here, tileSize);
    if (door == nullptr)
    {
        return false;
    }
    const DoorState state = mTileMap.doorState(door->tile.x, door->tile.y);
    if (state != DoorState::Open && state != DoorState::Closed)
    {
        return false;
    }
    // Something else to use right here wins, unless you stand at the door.
    const glm::vec2 centre = doorCentre(mTileMap, door->tile);
    if (entityPrompt.entity != kInvalidEntity && glm::length(centre - here) > tileSize * 0.6f)
    {
        return false;
    }

    mProximityPrompt = state == DoorState::Closed ? "E: Open door" : "E: Close door";
    if (!mInput.wasKeyPressed(SDL_SCANCODE_E))
    {
        return false;
    }

    if (state == DoorState::Open)
    {
        // Nobody may be standing in the doorway when it shuts.
        const float tileLeft = static_cast<float>(door->tile.x) * tileSize;
        const float tileTop = static_cast<float>(door->tile.y) * static_cast<float>(mTileMap.tileHeight());
        bool blocked = false;
        mWorld.forEach<Transform, Velocity>(
            [&](Entity e, Transform& t, Velocity&)
            {
                const bool isPlayer = e == mPlayerEntity;
                const float halfW = (isPlayer ? CollisionSystem::kPlayerCollisionWidth : CollisionSystem::kCollisionWidth) * 0.5f;
                const float cx = t.x + kSpriteSize * 0.5f;
                const float cy = t.y + (isPlayer ? CollisionSystem::kPlayerCollisionCenterYOffset
                                                 : CollisionSystem::kCollisionCenterYOffset);
                if (cx + halfW > tileLeft && cx - halfW < tileLeft + tileSize &&
                    cy + halfW > tileTop && cy - halfW < tileTop + tileSize)
                {
                    blocked = true;
                }
            });
        if (blocked)
        {
            mInteractionMessage = "Something is in the way.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
    }

    mDoors.toggle(mTileMap, door->tile);
    mNoiseModel.emitNoise(centre, NoiseTier::Soft, 0.5f, mPlayerEntity);
    mAudioManager.playSound(SoundId::FootstepLoud, 0.35f);
    return true;
}
