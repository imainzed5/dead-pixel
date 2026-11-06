// Fire: lighting and putting out fires, burning, spreading and damage.
#include "core/GameShared.h"

using namespace gameshared;

// Fire: the L key (Molotov ignites, water suppresses) and the burn/spread simulation.
void Game::updateFire(float dtSeconds, bool playerAlive, bool drivingVehicle)
{
    // Fire interaction key (L): active-slot Molotov ignites, active-slot water suppresses.
    if (playerAlive &&
        !mShowCrafting &&
        !drivingVehicle &&
        !mContainerOpen &&
        mInput.wasKeyPressed(SDL_SCANCODE_L) &&
        mPlayerEntity != kInvalidEntity &&
        mWorld.hasComponent<Inventory>(mPlayerEntity) &&
        mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const int activeSlot = std::clamp(inv.activeSlot, 0, Inventory::kMaxSlots - 1);
        const InventorySlot& slot = inv.slots[static_cast<std::size_t>(activeSlot)];

        if (slot.itemId == 26 && slot.count > 0)
        {
            const glm::ivec2 targetTile = worldToTile(mTileMap, mMouseWorld);
            if (targetTile.x >= 0 && targetTile.x < mTileMap.width() &&
                targetTile.y >= 0 && targetTile.y < mTileMap.height() &&
                !mTileMap.blocksSight(targetTile.x, targetTile.y))
            {
                bool foundExisting = false;
                mWorld.forEach<Fire, Transform>(
                    [&](Entity, Fire& fire, Transform& ft)
                    {
                        const glm::ivec2 fireTile = worldToTile(mTileMap, glm::vec2(ft.x, ft.y));
                        if (fireTile.x == targetTile.x && fireTile.y == targetTile.y)
                        {
                            fire.intensity = std::min(2.0f, fire.intensity + 0.55f);
                            fire.lifeRemaining = std::max(fire.lifeRemaining, 24.0f);
                            foundExisting = true;
                        }
                    });

                if (!foundExisting)
                {
                    Entity fireEntity = mWorld.createEntity();
                    Transform ft{};
                    ft.x = static_cast<float>(targetTile.x * mTileMap.tileWidth());
                    ft.y = static_cast<float>(targetTile.y * mTileMap.tileHeight());

                    Sprite fs{};
                    fs.textureId = mSpriteSheet.textureId();
                    fs.uvRect = mSpriteSheet.uvRect("env_campfire_lit");
                    fs.color = glm::vec4(1.0f, 0.62f, 0.20f, 0.84f);
                    fs.layer = 6;

                    Fire fire{};
                    fire.intensity = 1.05f;
                    fire.lifeRemaining = 24.0f;
                    fire.spreadTimer = 0.0f;
                    fire.noiseTimer = 0.0f;

                    mWorld.addComponent<Transform>(fireEntity, ft);
                    mWorld.addComponent<Sprite>(fireEntity, fs);
                    mWorld.addComponent<Fire>(fireEntity, fire);
                }

                InventorySystem::removeItem(inv, 26, 1);
                const glm::vec2 noisePos(
                    (static_cast<float>(targetTile.x) + 0.5f) * static_cast<float>(mTileMap.tileWidth()),
                    (static_cast<float>(targetTile.y) + 0.5f) * static_cast<float>(mTileMap.tileHeight()));
                mNoiseModel.emitNoise(noisePos, NoiseTier::Explosive, 0.8f, mPlayerEntity);
                mInteractionMessage = "Molotov ignited.";
                mInteractionMessageTimer = 2.0f;
            }
            else
            {
                mInteractionMessage = "Cannot ignite that tile.";
                mInteractionMessageTimer = 1.8f;
            }
        }
        else if (slot.itemId == 3 && slot.count > 0)
        {
            const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
            const glm::vec2 playerCenter(pt.x + kSpriteSize * 0.5f, pt.y + kSpriteSize * 0.5f);

            Entity nearestFire = kInvalidEntity;
            float bestDistSq = 96.0f * 96.0f;
            mWorld.forEach<Fire, Transform>(
                [&](Entity fireEntity, Fire&, Transform& ft)
                {
                    const glm::vec2 fireCenter(ft.x + kSpriteSize * 0.5f, ft.y + kSpriteSize * 0.5f);
                    const glm::vec2 d = fireCenter - playerCenter;
                    const float distSq = glm::dot(d, d);
                    if (distSq < bestDistSq)
                    {
                        bestDistSq = distSq;
                        nearestFire = fireEntity;
                    }
                });

            if (nearestFire != kInvalidEntity)
            {
                Fire& fire = mWorld.getComponent<Fire>(nearestFire);
                fire.intensity = std::max(0.0f, fire.intensity - 0.9f);
                fire.lifeRemaining = std::max(0.0f, fire.lifeRemaining - 14.0f);
                InventorySystem::removeItem(inv, 3, 1);
                mInteractionMessage = "Doused nearby fire.";
                mInteractionMessageTimer = 1.8f;
            }
            else
            {
                mInteractionMessage = "No nearby fire to suppress.";
                mInteractionMessageTimer = 1.8f;
            }
        }
    }

    // Fire simulation update (M8 narrow slice).
    {
        auto structureFlammabilityBonus = [&](int tileX, int tileY)
        {
            float bonus = 0.0f;
            mWorld.forEach<Structure, Transform>(
                [&](Entity, Structure& structure, Transform& st)
                {
                    const glm::ivec2 structTile = worldToTile(mTileMap, glm::vec2(st.x, st.y));
                    if (structTile.x != tileX || structTile.y != tileY)
                    {
                        return;
                    }

                    switch (structure.type)
                    {
                    case StructureType::Barricade:
                    case StructureType::NoiseTrap:
                    case StructureType::SleepingArea:
                    case StructureType::SupplyCache:
                        bonus = std::max(bonus, 0.35f);
                        break;
                    case StructureType::Campfire:
                        bonus = std::max(bonus, 0.20f);
                        break;
                    case StructureType::RainCollector:
                        bonus = std::max(bonus, 0.02f);
                        break;
                    }
                });
            return bonus;
        };

        auto tileFlammability = [&](int tileX, int tileY)
        {
            if (tileX < 0 || tileX >= mTileMap.width() || tileY < 0 || tileY >= mTileMap.height())
            {
                return 0.0f;
            }
            if (mTileMap.blocksSight(tileX, tileY))
            {
                return 0.0f;
            }

            float value = fireBaseFlammability(mTileMap.getSurfaceType(tileX, tileY));
            value += structureFlammabilityBonus(tileX, tileY);
            if (mTileMap.hasFurniture(tileX, tileY))
            {
                value += 0.25f; // wood, cloth and paper
            }
            if (hasTilePair(mBurnedTiles, tileX, tileY))
            {
                value *= 0.35f;
            }
            return std::clamp(value, 0.0f, 1.0f);
        };

        auto tileAlreadyBurning = [&](int tileX, int tileY)
        {
            bool burning = false;
            mWorld.forEach<Fire, Transform>(
                [&](Entity, Fire&, Transform& ft)
                {
                    if (burning) return;
                    const glm::ivec2 ftile = worldToTile(mTileMap, glm::vec2(ft.x, ft.y));
                    burning = (ftile.x == tileX && ftile.y == tileY);
                });
            return burning;
        };

        std::vector<Entity> extinguish;
        std::vector<glm::ivec2> newIgnitions;

        const float rainSuppression = std::clamp(1.0f - mWeatherState.rainIntensity * 0.82f, 0.10f, 1.0f);
        const float windBoost = 1.0f + mWeatherState.windStrength * 0.32f;

        mWorld.forEach<Fire, Transform, Sprite>(
            [&](Entity fireEntity, Fire& fire, Transform& ft, Sprite& fs)
            {
                const glm::ivec2 fireTile = worldToTile(mTileMap, glm::vec2(ft.x, ft.y));
                if (fireTile.x < 0 || fireTile.x >= mTileMap.width() || fireTile.y < 0 || fireTile.y >= mTileMap.height())
                {
                    extinguish.push_back(fireEntity);
                    return;
                }

                fire.noiseTimer += dtSeconds;
                if (fire.noiseTimer >= 1.35f)
                {
                    fire.noiseTimer = 0.0f;
                    const glm::vec2 noisePos(
                        (static_cast<float>(fireTile.x) + 0.5f) * static_cast<float>(mTileMap.tileWidth()),
                        (static_cast<float>(fireTile.y) + 0.5f) * static_cast<float>(mTileMap.tileHeight()));
                    const NoiseTier tier = fire.intensity >= 1.2f ? NoiseTier::Loud : NoiseTier::Medium;
                    mNoiseModel.emitNoise(noisePos, tier, 0.85f, fireEntity);
                }

                fire.lifeRemaining -= dtSeconds * (1.0f + mWeatherState.rainIntensity * 1.5f);
                fire.intensity = std::clamp(
                    fire.intensity + dtSeconds * (0.05f - mWeatherState.rainIntensity * 0.28f),
                    0.0f,
                    2.0f);

                // Direct fire damage on entities occupying the burning tile.
                mWorld.forEach<Transform, Health>(
                    [&](Entity target, Transform& tt, Health& health)
                    {
                        if (health.current <= 0.0f) return;
                        const glm::ivec2 targetTile = worldToTile(mTileMap, glm::vec2(tt.x, tt.y));
                        if (targetTile.x != fireTile.x || targetTile.y != fireTile.y)
                        {
                            return;
                        }

                        const float damage = (target == mPlayerEntity) ? 14.0f : 18.0f;
                        health.current = std::max(0.0f, health.current - damage * fire.intensity * dtSeconds);
                        if (mWorld.hasComponent<ZombieAI>(target))
                        {
                            mWorld.getComponent<ZombieAI>(target).lastDamagedBy = fireEntity;
                        }
                    });

                // Structures on burning tiles take accelerated damage.
                mWorld.forEach<Structure, Transform>(
                    [&](Entity, Structure& structure, Transform& st)
                    {
                        const glm::ivec2 structureTile = worldToTile(mTileMap, glm::vec2(st.x, st.y));
                        if (structureTile.x != fireTile.x || structureTile.y != fireTile.y)
                        {
                            return;
                        }

                        float multiplier = 1.0f;
                        if (structure.type == StructureType::RainCollector)
                        {
                            multiplier = 0.25f;
                        }
                        else if (structure.type == StructureType::Campfire)
                        {
                            multiplier = 0.55f;
                        }
                        structure.hp = std::max(0.0f, structure.hp - dtSeconds * fire.intensity * 16.0f * multiplier);
                    });

                fire.spreadTimer += dtSeconds;
                const float spreadInterval = std::max(0.55f, 1.5f - fire.intensity * 0.45f);
                if (fire.spreadTimer >= spreadInterval)
                {
                    fire.spreadTimer = 0.0f;
                    static constexpr int kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& dir : kDirs)
                    {
                        const int nx = fireTile.x + dir[0];
                        const int ny = fireTile.y + dir[1];
                        if (nx < 0 || nx >= mTileMap.width() || ny < 0 || ny >= mTileMap.height())
                        {
                            continue;
                        }
                        if (tileAlreadyBurning(nx, ny))
                        {
                            continue;
                        }

                        const float flammability = tileFlammability(nx, ny);
                        if (flammability <= 0.02f)
                        {
                            continue;
                        }

                        const float chance = std::clamp(
                            flammability * (0.22f + fire.intensity * 0.32f) * rainSuppression * windBoost,
                            0.0f,
                            0.95f);
                        const int salt = static_cast<int>(std::floor(mGameHours * 120.0)) + mCurrentDay * 17;
                        if (deterministic01(nx, ny, salt) < chance)
                        {
                            newIgnitions.emplace_back(nx, ny);
                        }
                    }
                }

                fs.color = glm::vec4(
                    1.0f,
                    std::clamp(0.40f + fire.intensity * 0.30f, 0.0f, 1.0f),
                    std::clamp(0.12f + fire.intensity * 0.14f, 0.0f, 1.0f),
                    std::clamp(0.30f + fire.intensity * 0.34f, 0.0f, 1.0f));

                if (fire.lifeRemaining <= 0.0f || fire.intensity <= 0.05f)
                {
                    addUniqueTilePair(mBurnedTiles, fireTile.x, fireTile.y);
                    extinguish.push_back(fireEntity);
                }
            });

        for (Entity fireEntity : extinguish)
        {
            mWorld.destroyEntity(fireEntity);
        }

        for (const glm::ivec2& tile : newIgnitions)
        {
            if (tileAlreadyBurning(tile.x, tile.y))
            {
                continue;
            }

            Entity fireEntity = mWorld.createEntity();
            Transform ft{};
            ft.x = static_cast<float>(tile.x * mTileMap.tileWidth());
            ft.y = static_cast<float>(tile.y * mTileMap.tileHeight());

            Sprite fs{};
            fs.textureId = mSpriteSheet.textureId();
            fs.uvRect = mSpriteSheet.uvRect("env_campfire_lit");
            fs.color = glm::vec4(1.0f, 0.52f, 0.18f, 0.72f);
            fs.layer = 6;

            Fire fire{};
            fire.intensity = 0.55f;
            fire.lifeRemaining = 16.0f;
            fire.spreadTimer = 0.0f;
            fire.noiseTimer = 0.0f;

            mWorld.addComponent<Transform>(fireEntity, ft);
            mWorld.addComponent<Sprite>(fireEntity, fs);
            mWorld.addComponent<Fire>(fireEntity, fire);
        }
    }
}
