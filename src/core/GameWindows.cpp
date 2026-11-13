// Windows: glass that the dead see through and break, boards and curtains,
// climbing through an empty frame (both ways), and light from inside that
// shows through at night. The rules live in world/Windows.
#include "core/GameShared.h"

#include <random>

using namespace gameshared;

namespace
{
glm::vec2 tileCentre(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

constexpr float kBangHearingTiles = 18.0f;
constexpr float kMessageTiles = 14.0f;
constexpr float kWakeTiles = 12.0f;
// The dead within this of a lit window, outside, come to it.
constexpr float kLitWindowTiles = 10.0f;
// A cut from the glass while climbing through, unless something is laid over it.
constexpr float kClimbCutChance = 0.30f;
} // namespace

void Game::placeWindows()
{
    mWindowSpots = windows::place(mTileMap, mMapGenerator.layoutData());
    mWindows.reset(mTileMap, mWindowSpots);
}

void Game::resetWindows()
{
    mWindows.reset(mTileMap, mWindowSpots);
    restoreWindows(mSaveManager.windows());
    mWindowClimbs.clear();
}

void Game::restoreWindows(const std::vector<WindowRecord>& records)
{
    for (const Windows::Window& window : mWindows.all())
    {
        mWindows.restore(mTileMap, window.spot.tile, false, 0.0f, false);
    }
    for (const WindowRecord& record : records)
    {
        mWindows.restore(mTileMap, glm::ivec2(record.x, record.y), record.broken, record.boardHp, record.covered);
    }
}

std::vector<WindowRecord> Game::windowRecords() const
{
    std::vector<WindowRecord> records;
    for (const Windows::Window& window : mWindows.all())
    {
        const glm::ivec2 tile = window.spot.tile;
        const bool broken = mTileMap.windowState(tile.x, tile.y) == WindowState::Broken;
        const bool covered = mTileMap.windowCovered(tile.x, tile.y);
        if (broken || window.boardHp > 0.0f || covered)
        {
            records.push_back(WindowRecord{tile.x, tile.y, broken, window.boardHp, covered});
        }
    }
    return records;
}

bool Game::insideOf(const windows::Spot& spot, const glm::vec2& world) const
{
    const glm::vec2 in = tileCentre(mTileMap, spot.inside);
    const glm::vec2 out = tileCentre(mTileMap, spot.outside);
    return glm::dot(world - in, world - in) < glm::dot(world - out, world - out);
}

const Windows::Window* Game::windowInReach(const ProximityPrompt& entityPrompt) const
{
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const Windows::Window* window = mWindows.nearest(mTileMap, here, tileSize * 1.05f);
    if (window == nullptr)
    {
        return nullptr;
    }
    // Something else to use right here wins, unless you stand at the window.
    if (entityPrompt.entity != kInvalidEntity &&
        glm::length(tileCentre(mTileMap, window->spot.tile) - here) > tileSize * 0.75f)
    {
        const Transform& t = mWorld.getComponent<Transform>(entityPrompt.entity);
        const glm::vec2 other(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
        if (glm::length(other - here) < glm::length(tileCentre(mTileMap, window->spot.tile) - here))
        {
            return nullptr;
        }
    }
    return window;
}

bool Game::handleWindowInteraction(const ProximityPrompt& entityPrompt)
{
    const Windows::Window* window = windowInReach(entityPrompt);
    if (window == nullptr || mBaseAction.kind == BaseAction::Kind::Climb)
    {
        return false;
    }
    const glm::ivec2 tile = window->spot.tile;
    const windows::Spot spot = window->spot;
    const bool inside = insideOf(spot, playerWorldCenter());
    const bool broken = mTileMap.windowState(tile.x, tile.y) == WindowState::Broken;
    const bool covered = mTileMap.windowCovered(tile.x, tile.y);
    const int boards = mWindows.boards(tile);
    const bool alarm = alarmOn(tile);
    // Duct tape arrives with phase 6; until then there is no quiet break.
    const ItemDef* tape = mItemDatabase.findByKey("duct_tape");
    const int tapeId = tape != nullptr ? tape->id : -1;
    const bool crouched = mInput.isKeyDown(SDL_SCANCODE_C) || mInput.isKeyDown(SDL_SCANCODE_LCTRL);

    std::string prompt;
    if (alarm)
    {
        prompt = "E: Take down the alarm";
    }
    else if (boards > 0)
    {
        prompt = "Window boarded (" + std::to_string(boards) + "/" + std::to_string(Windows::kMaxBoards) + ")";
        if (inside)
        {
            prompt += std::string(boards < Windows::kMaxBoards ? "   R: Add a board" : "") + "   X: Pry one off";
        }
    }
    else if (broken)
    {
        prompt = "E: Climb through";
        if (inside) prompt += "   R: Board up";
    }
    else if (inside)
    {
        prompt = covered ? "E: Open the curtain" : "E: Draw the curtain";
        prompt += "   R: Board up";
        if (crouched && tapeId >= 0 && playerHasItem(tapeId)) prompt += "   (crouched) F: Tape and break";
    }
    else
    {
        prompt = covered ? "A window, curtained" : "A window";
    }
    mProximityPrompt = prompt;

    const glm::vec2 centre = tileCentre(mTileMap, tile);
    if (mInput.wasKeyPressed(SDL_SCANCODE_R))
    {
        if (!inside)
        {
            mInteractionMessage = "You can only board it from inside.";
            mInteractionMessageTimer = 2.0f;
        }
        else
        {
            boardUpWindow(tile);
        }
        return true;
    }
    if (mInput.wasKeyPressed(SDL_SCANCODE_X) && boards > 0)
    {
        if (!inside)
        {
            mInteractionMessage = "You can't get at the boards from out here.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
        if (mWindows.removeBoard(mTileMap, tile))
        {
            if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
                InventorySystem::addItem(inv, mItemDatabase, mItems.woodPlanks, 1);
                // Nails come out whole only with a crowbar.
                if (InventorySystem::countTagged(inv, mItemDatabase, "tool:pry") > 0)
                {
                    InventorySystem::addItem(inv, mItemDatabase, mItems.nails, 2);
                }
            }
            mNoiseModel.emitNoise(centre, NoiseTier::Medium, 0.8f, mPlayerEntity);
            mAudioManager.playSound(SoundId::WeaponHit, 0.45f);
            const int left = mWindows.boards(tile);
            mInteractionMessage = left > 0 ? "You pry a board off. " + std::to_string(left) + " left."
                                           : "You pry the last board off.";
            mInteractionMessageTimer = 2.5f;
        }
        return true;
    }
    // A quiet break: tape over the glass, then a sharp tap.
    if (mInput.wasKeyPressed(SDL_SCANCODE_F) && crouched && inside && !broken && boards == 0 && tapeId >= 0 &&
        playerHasItem(tapeId))
    {
        InventorySystem::removeItem(mWorld.getComponent<Inventory>(mPlayerEntity), tapeId, 1);
        breakWindow(tile, false);
        mInteractionMessage = "You tape the glass and knock it out. Hardly a sound.";
        mInteractionMessageTimer = 3.0f;
        return true;
    }
    if (!mInput.wasKeyPressed(SDL_SCANCODE_E))
    {
        return false;
    }

    if (alarm)
    {
        Entity hung = kInvalidEntity;
        mWorld.forEach<Structure>(
            [&](Entity e, Structure& s)
            {
                if (s.type == StructureType::CanAlarm && s.target == tile) hung = e;
            });
        if (hung != kInvalidEntity)
        {
            mWorld.destroyEntity(hung);
            if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                InventorySystem::addItem(mWorld.getComponent<Inventory>(mPlayerEntity), mItemDatabase, mItems.canAlarm, 1);
            }
            mInteractionMessage = "You take the cans down.";
            mInteractionMessageTimer = 2.0f;
        }
        return true;
    }
    if (boards > 0)
    {
        mInteractionMessage = "It's boarded up.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    if (broken)
    {
        const glm::ivec2 to = inside ? spot.outside : spot.inside;
        if (mTileMap.isSolid(to.x, to.y))
        {
            mInteractionMessage = "Something is in the way.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
        mBaseAction = BaseAction{};
        mBaseAction.kind = BaseAction::Kind::Climb;
        mBaseAction.tile = tile;
        mBaseAction.to = to;
        mBaseAction.total = Windows::kPlayerClimbSeconds;
        mBaseAction.label = "Climbing through";
        mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
        mNoiseModel.emitNoise(centre, NoiseTier::Soft, 0.6f, mPlayerEntity);

        // Something laid over the broken glass, or a chance of a cut.
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        if (InventorySystem::countItem(inv, mItems.rags) > 0)
        {
            InventorySystem::removeItem(inv, mItems.rags, 1);
            mInteractionMessage = "You lay rags over the glass and climb through.";
            mInteractionMessageTimer = 2.5f;
        }
        else if (InventorySystem::countItem(inv, mItems.jacket) > 0)
        {
            InventorySystem::removeItem(inv, mItems.jacket, 1);
            mInteractionMessage = "You lay your jacket over the glass and climb through.";
            mInteractionMessageTimer = 2.5f;
        }
        else
        {
            std::mt19937 rng(mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(mGameHours * 3600.0) ^
                             static_cast<std::uint32_t>(tile.x * 73856093 ^ tile.y * 19349663));
            if (std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < kClimbCutChance &&
                mWorld.hasComponent<Wounds>(mPlayerEntity))
            {
                Wound cut;
                cut.type = WoundType::Cut;
                cut.severity = 0.35f;
                mWorld.getComponent<Wounds>(mPlayerEntity).active.push_back(cut);
                if (mWorld.hasComponent<Health>(mPlayerEntity))
                {
                    Health& health = mWorld.getComponent<Health>(mPlayerEntity);
                    health.current = std::max(1.0f, health.current - 5.0f);
                }
                mInteractionMessage = "The glass catches you as you climb through.";
                mInteractionMessageTimer = 2.5f;
            }
        }
        return true;
    }
    if (inside)
    {
        mTileMap.setWindowCovered(tile.x, tile.y, !covered);
        mSaveManager.setWindows(windowRecords());
        mInteractionMessage = covered ? "You open the curtain." : "You draw the curtain.";
        mInteractionMessageTimer = 1.5f;
        return true;
    }
    mInteractionMessage = "The glass is in the way. You could break it.";
    mInteractionMessageTimer = 2.0f;
    return true;
}

void Game::boardUpWindow(const glm::ivec2& tile)
{
    if (!mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }
    Inventory& inventory = mWorld.getComponent<Inventory>(mPlayerEntity);
    std::string message;
    if (mWindows.boards(tile) >= Windows::kMaxBoards)
    {
        message = "It won't take another board.";
    }
    else if (InventorySystem::countTagged(inventory, mItemDatabase, "tool:hammer") == 0)
    {
        message = "You need a hammer to board it up.";
    }
    else if (InventorySystem::countItem(inventory, mItems.woodPlanks) == 0)
    {
        message = "You need planks to board it up.";
    }
    else if (InventorySystem::countItem(inventory, mItems.nails) < 2)
    {
        message = "You need nails to board it up (2 a board).";
    }
    else if (mWindows.addBoard(mTileMap, tile))
    {
        InventorySystem::removeItem(inventory, mItems.woodPlanks, 1);
        InventorySystem::removeItem(inventory, mItems.nails, 2);
        mNoiseModel.emitNoise(tileCentre(mTileMap, tile), NoiseTier::Loud, 1.0f, mPlayerEntity);
        mAudioManager.playSound(SoundId::WeaponHit, 0.7f);
        const int boards = mWindows.boards(tile);
        message = "You nail a board across the window (" + std::to_string(boards) + "/" +
                  std::to_string(Windows::kMaxBoards) + ").";
        mSaveManager.setWindows(windowRecords());
        noteAccomplishment();
    }
    mInteractionMessage = message;
    mInteractionMessageTimer = 2.5f;
}

void Game::breakWindow(const glm::ivec2& tile, bool loud)
{
    const Windows::Window* window = mWindows.find(tile);
    if (window == nullptr || !mWindows.breakGlass(mTileMap, tile))
    {
        return;
    }
    const glm::vec2 centre = tileCentre(mTileMap, tile);
    // The glass falls outside.
    dropOnFloor(window->spot.outside, mItems.glassShards, 1);
    mNoiseModel.emitNoise(centre, loud ? NoiseTier::Loud : NoiseTier::Soft, loud ? 1.5f : 0.6f);
    mParticleSystem.spawnDust(centre, loud ? 10 : 4);
    const float tiles = glm::length(centre - playerWorldCenter()) / static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float volume = std::clamp(1.0f - tiles / kBangHearingTiles, 0.0f, 1.0f);
    if (loud && volume > 0.0f) mAudioManager.playSound(SoundId::WeaponHit, 0.4f + 0.5f * volume);
    rattleAlarm(tile);
    mSaveManager.setWindows(windowRecords());
}

void Game::windowBlow(const glm::ivec2& tile, Entity zombie, float dtSeconds, float damageScale)
{
    const Windows::Window* window = mWindows.find(tile);
    if (window == nullptr)
    {
        return;
    }
    const windows::Spot spot = window->spot;
    const glm::vec2 centre = tileCentre(mTileMap, tile);
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float tiles = glm::length(centre - playerWorldCenter()) / tileSize;
    const float volume = std::clamp(1.0f - tiles / kBangHearingTiles, 0.0f, 1.0f);

    // An empty frame: one at a time, they climb through.
    if (mTileMap.windowOpen(tile.x, tile.y))
    {
        if (zombie == kInvalidEntity || !mWorld.hasComponent<Transform>(zombie)) return;
        auto climb = std::find_if(mWindowClimbs.begin(), mWindowClimbs.end(),
                                  [&](const WindowClimb& c) { return c.tile == tile; });
        if (climb == mWindowClimbs.end())
        {
            mWindowClimbs.push_back(WindowClimb{tile, zombie, 0.0f, 0.0f});
            climb = mWindowClimbs.end() - 1;
            rattleAlarm(tile);
        }
        if (climb->zombie != zombie)
        {
            return; // someone else is in the frame
        }
        climb->seen = 0.0f;
        climb->timer += dtSeconds;
        if (climb->timer < Windows::kZombieClimbSeconds)
        {
            return;
        }
        Transform& t = mWorld.getComponent<Transform>(zombie);
        const glm::vec2 here(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
        const glm::ivec2 to = insideOf(spot, here) ? spot.outside : spot.inside;
        const glm::vec2 landing = tileCentre(mTileMap, to);
        t.x = landing.x - kSpriteSize * 0.5f;
        t.y = landing.y - kSpriteSize * 0.5f;
        if (mWorld.hasComponent<ZombieAI>(zombie))
        {
            mWorld.getComponent<ZombieAI>(zombie).path.clear();
        }
        mWindowClimbs.erase(climb);
        mNoiseModel.emitNoise(centre, NoiseTier::Soft, 0.8f);
        if (tiles <= kMessageTiles)
        {
            mInteractionMessage = "Something climbs in through a window.";
            mInteractionMessageTimer = 3.0f;
        }
        return;
    }

    const Windows::Blow blow = mWindows.batter(mTileMap, tile, Doors::kZombieDamagePerSecond * damageScale * dtSeconds);
    if (blow == Windows::Blow::None || blow == Windows::Blow::Thud)
    {
        return;
    }
    rattleAlarm(tile);
    if (blow == Windows::Blow::Bang)
    {
        mNoiseModel.emitNoise(centre, NoiseTier::Medium, 1.0f);
        if (volume > 0.0f) mAudioManager.playSound(SoundId::WeaponHit, 0.2f + 0.45f * volume);
        wakeSleeper(centre, kWakeTiles, 2, "Something at the window woke you!");
        return;
    }
    if (blow == Windows::Blow::Splinters)
    {
        mNoiseModel.emitNoise(centre, NoiseTier::Loud, 1.0f);
        mParticleSystem.spawnDust(centre, 6);
        if (volume > 0.0f) mAudioManager.playSound(SoundId::WeaponHit, 0.35f + 0.6f * volume);
        wakeSleeper(centre, kWakeTiles, 3, "A board splinters at the window!");
        if (tiles <= kMessageTiles)
        {
            const int left = mWindows.boards(tile);
            mInteractionMessage = left > 0 ? "A window board splinters. " + std::to_string(left) + " left."
                                           : "The last board on a window splinters.";
            mInteractionMessageTimer = 3.0f;
        }
        mSaveManager.setWindows(windowRecords());
        return;
    }
    // Shatters: the glass is already broken by batter; drop it and make it heard.
    dropOnFloor(spot.outside, mItems.glassShards, 1);
    mNoiseModel.emitNoise(centre, NoiseTier::Loud, 1.5f);
    mParticleSystem.spawnDust(centre, 10);
    if (volume > 0.0f) mAudioManager.playSound(SoundId::ZombieAttack, 0.3f + 0.6f * volume);
    wakeSleeper(centre, kWakeTiles, 3, "Glass breaks somewhere close!");
    if (tiles <= kMessageTiles)
    {
        mInteractionMessage = "Glass breaks.";
        mInteractionMessageTimer = 3.0f;
    }
    mSaveManager.setWindows(windowRecords());
}

bool Game::buildingLit(int building) const
{
    if (building <= 0) return false;
    // The flashlight in your hand.
    if (mFlashlightOn && mPlayerEntity != kInvalidEntity)
    {
        const glm::ivec2 tile = playerTile();
        if (mTileMap.getBuildingId(tile.x, tile.y) == building) return true;
    }
    // A fire or a candle burning inside (a can lantern hides its light).
    bool lit = false;
    auto& world = const_cast<World&>(mWorld);
    world.forEach<Structure, Transform>(
        [&](Entity, Structure& s, Transform& t)
        {
            if (lit || !s.lit || s.hp <= 0.0f) return;
            if (s.type != StructureType::Campfire && !(s.type == StructureType::Light && !s.lantern)) return;
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            lit = mTileMap.getBuildingId(tile.x, tile.y) == building;
        });
    if (lit) return true;
    world.forEach<Fire, Transform>(
        [&](Entity, Fire&, Transform& t)
        {
            if (lit) return;
            const glm::ivec2 tile = worldToTile(mTileMap, glm::vec2(t.x + 16.0f, t.y + 16.0f));
            lit = mTileMap.getBuildingId(tile.x, tile.y) == building;
        });
    return lit;
}

void Game::updateWindows(float dtSeconds)
{
    mWindows.tick(dtSeconds);

    // A climber who stopped pushing at the frame gives it up.
    for (WindowClimb& climb : mWindowClimbs) climb.seen += dtSeconds;
    std::erase_if(mWindowClimbs, [](const WindowClimb& c) { return c.seen > 0.75f; });

    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity)) return;
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));

    // A blow aimed at a window in reach breaks the glass (loudly).
    if (mInput.wasMouseButtonPressed(SDL_BUTTON_LEFT) && !mShowCrafting && !mContainerOpen && !mShowInventory &&
        !mBuildWheelOpen && mBuildChoice < 0 && !mWorld.hasComponent<Sleeping>(mPlayerEntity))
    {
        const glm::vec2 here = playerWorldCenter();
        const Windows::Window* window = mWindows.nearest(mTileMap, here, tileSize * 1.3f);
        if (window != nullptr && mTileMap.windowState(window->spot.tile.x, window->spot.tile.y) == WindowState::Intact &&
            mWindows.boards(window->spot.tile) == 0 &&
            glm::length(mMouseWorld - tileCentre(mTileMap, window->spot.tile)) < tileSize * 1.5f)
        {
            breakWindow(window->spot.tile, true);
            mInteractionMessage = "You smash the window.";
            mInteractionMessageTimer = 2.0f;
        }
    }

    // At night, light inside shows through uncovered windows: the dead
    // outside who can see the window come to it.
    mLitWindowTimer -= dtSeconds;
    if (mLitWindowTimer > 0.0f) return;
    mLitWindowTimer = 1.0f;
    if (nightFactor() < 0.5f) return;
    std::vector<int> litBuildings;
    for (const Windows::Window& window : mWindows.all())
    {
        const glm::ivec2 tile = window.spot.tile;
        if (!mTileMap.windowClear(tile.x, tile.y) && !mTileMap.windowOpen(tile.x, tile.y)) continue;
        if (mTileMap.windowCovered(tile.x, tile.y) || mWindows.boards(tile) > 0) continue;
        const int building = window.spot.building;
        const bool lit = std::find(litBuildings.begin(), litBuildings.end(), building) != litBuildings.end() ||
                         buildingLit(building);
        if (!lit) continue;
        if (std::find(litBuildings.begin(), litBuildings.end(), building) == litBuildings.end())
            litBuildings.push_back(building);
        const glm::vec2 outside = tileCentre(mTileMap, window.spot.outside);
        mWorld.forEach<ZombieAI, Transform>(
            [&](Entity, ZombieAI& ai, Transform& t)
            {
                if (ai.state != ZombieState::Wander && ai.state != ZombieState::Investigate) return;
                const glm::vec2 zc(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
                if (glm::length(zc - outside) > kLitWindowTiles * tileSize) return;
                const glm::ivec2 zt = worldToTile(mTileMap, zc);
                if (mTileMap.getBuildingId(zt.x, zt.y) > 0) return;
                if (!ZombieAISystem::hasLineOfSight(mTileMap, zc, outside)) return;
                if (ai.state == ZombieState::Investigate && glm::length(ai.targetWorld - outside) < tileSize) return;
                ZombieAISystem::sendTo(ai, outside, false);
            });
    }
}
