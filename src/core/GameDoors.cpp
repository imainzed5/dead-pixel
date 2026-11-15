// Doors: opening and closing them, boarding them up, survivors passing
// through, and the dead battering them down.
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

// (How far bangs carry is the sound table's: assets/data/sounds.json.)
constexpr float kBreakMessageTiles = 14.0f;
// Asleep within this, the banging wakes you.
constexpr float kWakeTiles = 12.0f;
} // namespace

std::vector<BoardedDoorRecord> Game::boardedDoorRecords() const
{
    std::vector<BoardedDoorRecord> records;
    for (const Doors::Door& door : mDoors.all())
    {
        if (door.boardHp > 0.0f && mTileMap.isClosedDoor(door.tile.x, door.tile.y))
        {
            records.push_back(BoardedDoorRecord{door.tile.x, door.tile.y, door.boardHp, door.boardToughness});
        }
    }
    return records;
}

void Game::resetDoors()
{
    mDoors.reset(mTileMap, mMapGenerator.layoutData());
    for (const auto& [x, y] : mSaveManager.brokenDoors())
    {
        mDoors.setState(mTileMap, glm::ivec2(x, y), DoorState::Broken);
    }
    for (const BoardedDoorRecord& record : mSaveManager.boardedDoors())
    {
        mDoors.setBoards(mTileMap, glm::ivec2(record.x, record.y), record.boardHp, record.toughness);
    }
}

void Game::updateDoors(float dtSeconds)
{
    mDoors.tick(dtSeconds);

    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 player = playerWorldCenter();
    for (const BarrierBlow& barrier : mDoorBlows)
    {
        const glm::ivec2 tile = barrier.tile;
        // How hard this one hits (its kind; Mutating doubles it late on).
        const float scale = barrierDamageScale(barrier.zombie);
        // A window, or furniture across a doorway, takes the blow instead.
        if (mTileMap.hasWindow(tile.x, tile.y))
        {
            // Late in Mutating, the dead go straight through glass.
            const bool climbs = zombiekinds::climbsWindows(modeRules(), mCurrentDay) &&
                                mTileMap.windowState(tile.x, tile.y) == WindowState::Intact &&
                                mTileMap.windowBoards(tile.x, tile.y) == 0;
            windowBlow(tile, barrier.zombie, dtSeconds, climbs ? 1000.0f : scale);
            continue;
        }
        if (mTileMap.doorwayBlocked(tile.x, tile.y))
        {
            furnitureBlow(tile, Doors::kZombieDamagePerSecond * scale * dtSeconds);
            continue;
        }
        rattleAlarm(tile);
        const Doors::Blow blow = mDoors.batter(mTileMap, tile, Doors::kZombieDamagePerSecond * scale * dtSeconds);
        if (blow != Doors::Blow::Bang && blow != Doors::Blow::Splinters && blow != Doors::Blow::Breaks)
        {
            continue;
        }

        const glm::vec2 centre = doorCentre(mTileMap, tile);
        const float tiles = glm::length(centre - player) / tileSize;
        (void)tiles;
        // The first of the night's callers at your door: a sting (phase 12,
        // not in Hopeless).
        if (mCallerStingNight != mCurrentDay && music::stingsAllowed(musicTone()) &&
            mWorld.hasComponent<ZombieAI>(barrier.zombie) &&
            mWorld.getComponent<ZombieAI>(barrier.zombie).nightCaller)
        {
            mCallerStingNight = mCurrentDay;
            mAudioManager.play(SoundId::StingCaller);
        }
        // Sleeping behind boards, only a board giving way wakes you.
        wakeSleeper(centre, kWakeTiles, blow == Doors::Blow::Bang ? 2 : 3, "Banging at the door woke you!");
        mParticleSystem.spawnDust(centre, blow == Doors::Blow::Bang ? 3 : (blow == Doors::Blow::Splinters ? 6 : 10));
        if (blow == Doors::Blow::Splinters)
        {
            makeNoise(centre, NoiseTier::Loud, 1.0f, SoundId::DeadBoardCrack);
            if (tiles <= kBreakMessageTiles)
            {
                const int left = mDoors.boards(tile);
                mInteractionMessage = left > 0
                    ? "A board splinters. " + std::to_string(left) + (left == 1 ? " board holds." : " boards hold.")
                    : "The last board splinters.";
                mInteractionMessageTimer = 3.0f;
            }
        }
        else if (blow == Doors::Blow::Bang)
        {
            // Every bang draws more of them.
            makeNoise(centre, NoiseTier::Medium, 1.0f, SoundId::DeadDoorBang);
        }
        else
        {
            makeNoise(centre, NoiseTier::Loud, 1.5f, SoundId::FurnitureBreak);
            if (tiles <= kBreakMessageTiles)
            {
                mInteractionMessage = "A door gives way.";
                mInteractionMessageTimer = 3.0f;
            }
        }
    }
    mDoorBlows.clear();
    // Survivors open and shut doors themselves (GameSurvivors.cpp).
}

const Doors::Door* Game::doorInReach(const ProximityPrompt& entityPrompt) const
{
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = playerWorldCenter();
    const Doors::Door* door = mDoors.nearest(mTileMap, here, tileSize);
    if (door == nullptr)
    {
        return nullptr;
    }
    const DoorState state = mTileMap.doorState(door->tile.x, door->tile.y);
    if (state != DoorState::Open && state != DoorState::Closed)
    {
        return nullptr;
    }
    // Something else to use right here wins, unless you stand at the door.
    if (entityPrompt.entity != kInvalidEntity &&
        glm::length(doorCentre(mTileMap, door->tile) - here) > tileSize * 0.6f)
    {
        return nullptr;
    }
    return door;
}

bool Game::handleDoorInteraction(const ProximityPrompt& entityPrompt)
{
    const Doors::Door* door = doorInReach(entityPrompt);
    if (door == nullptr)
    {
        return false;
    }
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const DoorState state = mTileMap.doorState(door->tile.x, door->tile.y);
    const glm::vec2 centre = doorCentre(mTileMap, door->tile);

    const int boards = mDoors.boards(door->tile);
    if (alarmOn(door->tile))
    {
        mProximityPrompt = "Cans hang across the door   E: Take them down";
        if (!mInput.pressed(Action::Use))
        {
            return false;
        }
        Entity hung = kInvalidEntity;
        mWorld.forEach<Structure>(
            [&](Entity e, Structure& s)
            {
                if (s.type == StructureType::CanAlarm && s.target == door->tile) hung = e;
            });
        if (hung != kInvalidEntity)
        {
            mWorld.destroyEntity(hung);
            if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                InventorySystem::addItem(mWorld.getComponent<Inventory>(mPlayerEntity), mItemDatabase, mItems.canAlarm, 1);
            }
        }
        mInteractionMessage = "You take the cans down.";
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    // Locked (phase 6): E says so; X forces or breaks it; from inside it unlocks.
    if (state == DoorState::Closed && boards == 0 && doorLocked(door->tile))
    {
        return handleLockedDoor(door->tile);
    }
    if (state == DoorState::Open)
    {
        mProximityPrompt = "E: Close door";
    }
    else if (boards == 0)
    {
        mProximityPrompt = "E: Open door   R: Board up";
    }
    else
    {
        mProximityPrompt = "Boarded up (" + std::to_string(boards) + "/" + std::to_string(Doors::kMaxBoards) + ")   " +
                           (boards < Doors::kMaxBoards ? "R: Add a board   " : "") + "X: Pry one off";
    }

    if (state == DoorState::Closed && mInput.pressed(Action::Board))
    {
        startBoarding(door->tile, false);
        return true;
    }
    if (state == DoorState::Closed && boards > 0 && mInput.pressed(Action::Pry))
    {
        if (mDoors.removeBoard(mTileMap, door->tile))
        {
            if (mWorld.hasComponent<Inventory>(mPlayerEntity))
            {
                Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
                InventorySystem::addItem(inv, mItemDatabase, mItems.woodPlanks, 1);
                // The nails come out whole only with a crowbar.
                if (InventorySystem::countTagged(inv, mItemDatabase, "tool:pry") > 0)
                {
                    InventorySystem::addItem(inv, mItemDatabase, mItems.nails, 2);
                }
            }
            makeNoise(centre, NoiseTier::Medium, 0.8f, SoundId::BoardPry, mPlayerEntity);
            const int left = mDoors.boards(door->tile);
            mInteractionMessage = left > 0 ? "You pry a board off. " + std::to_string(left) + " left."
                                           : "You pry the last board off.";
            mInteractionMessageTimer = 2.5f;
        }
        return true;
    }

    if (!mInput.pressed(Action::Use))
    {
        return false;
    }
    if (boards > 0)
    {
        mInteractionMessage = "It's boarded up.";
        mInteractionMessageTimer = 2.0f;
        return true;
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
    // Crouched, you ease it: a creak, not a knock of the frame.
    const bool gently = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Facing>(mPlayerEntity) &&
                        mWorld.getComponent<Facing>(mPlayerEntity).crouching;
    const bool opened = state != DoorState::Open; // it was shut: now it is open
    makeNoise(centre, NoiseTier::Soft, 0.5f,
              gently ? SoundId::DoorCreak : (opened ? SoundId::DoorOpen : SoundId::DoorClose), mPlayerEntity);
    return true;
}

void Game::boardUpDoor(const glm::ivec2& tile)
{
    if (!mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }
    Inventory& inventory = mWorld.getComponent<Inventory>(mPlayerEntity);
    std::string message;
    if (mDoors.boards(tile) >= Doors::kMaxBoards)
    {
        message = "It won't take another board.";
    }
    // A plank per board, and something to drive the nails with.
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
    else
    {
        // Carpentry: a better-driven board holds longer; from level 3 a
        // second plank doubled behind it (reinforced) when you have one.
        const int carpentry = skillLevel(Skill::Carpentry);
        const bool reinforced = skillrules::canReinforceBoards(carpentry) &&
                                InventorySystem::countItem(inventory, mItems.woodPlanks) >= 2;
        const float toughness = skillrules::boardStrengthScale(carpentry) *
                                (reinforced ? skillrules::kReinforcedBoardScale : 1.0f);
        if (!mDoors.addBoard(mTileMap, tile, toughness))
        {
            mInteractionMessage = "It won't take another board.";
            mInteractionMessageTimer = 2.0f;
            return;
        }
        InventorySystem::removeItem(inventory, mItems.woodPlanks, reinforced ? 2 : 1);
        InventorySystem::removeItem(inventory, mItems.nails, 2);
        // Hammering carries: the dead will hear it.
        makeNoise(doorCentre(mTileMap, tile), NoiseTier::Loud, 1.0f, SoundId::Hammer, mPlayerEntity);
        const int boards = mDoors.boards(tile);
        message = "You nail a board across the door (" + std::to_string(boards) + "/" +
                  std::to_string(Doors::kMaxBoards) + ").";
        noteAccomplishment();
    }
    mInteractionMessage = message;
    mInteractionMessageTimer = 2.5f;
}
