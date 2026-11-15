// Teaching through play (phase 13): no tutorial screens. The prompt line
// names a verb the first time it can be used, once per save; the first life
// of a world opens with a look around; the run-end screen gives one plain
// line on what might have helped. The last ten messages are kept for the
// notebook.
#include "core/GameShared.h"

#include "world/FirstDays.h"

using namespace gameshared;

namespace
{
constexpr float kTeachSeconds = 7.0f;
constexpr std::size_t kMessageLog = 10;
constexpr int kHintTimes = 2;      // each death hint, at most this often per save
constexpr float kShoveTiles = 2.0f;
constexpr int kListenTiles = 1;

struct Prompt
{
    const char* id;
    const char* text; // written with the default keys; Input::text rebinds it
};
constexpr Prompt kPrompts[] = {
    {"push", "Hold E and walk to push."},
    {"shove", "Space to shove."},
    {"shove_night", "Space to shove."}, // night 1's caller, if Space has not been used yet
    {"listen", "Crouch near a door to listen."},
    {"throw", "Hold G to throw."},
};

const char* promptText(const std::string& id)
{
    for (const Prompt& p : kPrompts)
    {
        if (id == p.id) return p.text;
    }
    return "";
}

// The verb a prompt teaches: night 1's shove is the shove.
std::string usedKey(const std::string& id)
{
    return (id == "shove_night" ? std::string("shove") : id) + ".used";
}

glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}
} // namespace

bool Game::teachingOn() const
{
    return mSettings.teaching && mTeachAllowed;
}

bool Game::wasTaught(const std::string& id) const
{
    const auto slot = mSettings.taught.find(mActiveSaveSlot);
    return slot != mSettings.taught.end() && slot->second.count(usedKey(id)) > 0;
}

void Game::teach(const std::string& id)
{
    if (!teachingOn() || wasTaught(id) || id == mTeachId) return;
    const auto slot = mSettings.taught.find(mActiveSaveSlot);
    if (slot != mSettings.taught.end() && slot->second.count(id) > 0) return; // shown once already
    if (std::find(mTeachQueue.begin(), mTeachQueue.end(), id) != mTeachQueue.end()) return;
    mTeachQueue.push_back(id);
}

void Game::taught(const std::string& id)
{
    if (wasTaught(id)) return;
    mSettings.taught[mActiveSaveSlot].insert(usedKey(id));
    if (!mHarness) saveSettings();
    // Used: off the line and out of the queue.
    mTeachQueue.erase(std::remove_if(mTeachQueue.begin(), mTeachQueue.end(),
                                     [&](const std::string& q) { return usedKey(q) == usedKey(id); }),
                      mTeachQueue.end());
    if (!mTeachId.empty() && usedKey(mTeachId) == usedKey(id))
    {
        mTeachLine.clear();
        mTeachId.clear();
        mTeachTimer = 0.0f;
    }
}

bool Game::lookAroundStep() const
{
    return mLookAroundFirstLife && teachingOn();
}

void Game::updateTeaching(float dtSeconds)
{
    if (mPlayerEntity == kInvalidEntity || mRunState != RunState::Playing) return;
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));

    // Day 1's look around checks itself: ten tiles walked and one E.
    if (lookAroundStep() && mCurrentDay == 1)
    {
        mLookAroundTiles = mRunStats.distanceWalked / tile;
        if (mInput.pressed(Action::Use)) mLookAroundUsed = true;
    }

    // Used: never shown again. Space while held is the same key's other use
    // (struggling free), so it counts as the shove learned.
    if (mListening) taught("listen");
    if (mInput.pressed(Action::Shove) && mPrevHolders > 0) taught("shove");

    if (!teachingOn())
    {
        mTeachQueue.clear();
        mTeachLine.clear();
        mTeachId.clear();
        return;
    }

    // The first time each verb can be used.
    const bool asleep = mWorld.hasComponent<Sleeping>(mPlayerEntity);
    if (!asleep && mActiveVehicleEntity == kInvalidEntity)
    {
        if (mProximityPrompt.find("Hold E: Push") != std::string::npos) teach("push");
        if (mWorld.hasComponent<Inventory>(mPlayerEntity))
        {
            const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
            const InventorySlot& slot = inv.slots[static_cast<std::size_t>(std::clamp(inv.activeSlot, 0, Inventory::kMaxSlots - 1))];
            const ItemDef* def = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
            if (def != nullptr && def->throwDef.throwable) teach("throw");
        }
        const glm::ivec2 here = playerTile();
        for (const Doors::Door& door : mDoors.all())
        {
            if (std::abs(door.tile.x - here.x) > kListenTiles || std::abs(door.tile.y - here.y) > kListenTiles) continue;
            if (mTileMap.doorState(door.tile.x, door.tile.y) == DoorState::Closed)
            {
                teach("listen");
                break;
            }
        }
        if (mShovesThisLife == 0)
        {
            const glm::vec2 me = playerWorldCenter();
            const int night = firstdays::nightIndex(mCurrentDay, mGameHours);
            bool near = false;
            bool caller = false;
            mWorld.forEach<ZombieAI, Transform, Health>(
                [&](Entity, ZombieAI& ai, Transform& t, Health& h)
                {
                    if (h.current <= 0.0f || glm::length(centreOf(t) - me) > kShoveTiles * tile) return;
                    near = true;
                    if (ai.nightCaller && night == 1) caller = true;
                });
            if (caller) teach("shove_night");
            else if (near) teach("shove");
        }
    }

    // One on the line at a time; the rest wait.
    mTeachTimer = std::max(0.0f, mTeachTimer - dtSeconds);
    if (!mTeachLine.empty() && mTeachTimer <= 0.0f)
    {
        mTeachLine.clear();
        mTeachId.clear();
    }
    while (mTeachLine.empty() && !mTeachQueue.empty())
    {
        const std::string id = mTeachQueue.front();
        mTeachQueue.pop_front();
        if (wasTaught(id)) continue;
        mTeachId = id;
        mTeachLine = mInput.text(promptText(id));
        mTeachTimer = kTeachSeconds;
        mSettings.taught[mActiveSaveSlot].insert(id); // shown: once per save
        if (!mHarness) saveSettings();
        std::cout << "[Teach] " << id << "\n";
    }
}

void Game::noteMessage()
{
    // Messages stay three seconds or until replaced; the last ten are kept
    // for the notebook's Journal page.
    if (mInteractionMessage.empty() || mInteractionMessageTimer <= 0.0f)
    {
        mLastLogged.clear();
        return;
    }
    if (mInteractionMessage == mLastLogged) return;
    mLastLogged = mInteractionMessage;
    mMessageLog.push_back(mInput.text(mInteractionMessage));
    while (mMessageLog.size() > kMessageLog) mMessageLog.pop_front();
}

void Game::chooseDeathHint()
{
    mDeathHint.clear();
    if (!teachingOn() || mPlayerEntity == kInvalidEntity) return;

    aims::DeathFacts facts;
    facts.thirst = mDeathCause.rfind("Dehydrated", 0) == 0;
    facts.infection = mDeathCause.find("infection") != std::string::npos || mDeathCause.rfind("Turned", 0) == 0;
    if (firstdays::nightIndex(mCurrentDay, mGameHours) == 1 && playerAtHome())
    {
        for (const WayIn& way : waysIn(mFirstDays.homeBuilding))
        {
            if (!way.window && !way.boarded) facts.nightOneAtHomeUnboarded = true;
        }
    }
    facts.grabbedNoShoves = mPrevHolders > 0 && mShovesThisLife == 0;

    std::map<std::string, int>& shown = mSettings.hintsShown[mActiveSaveSlot];
    for (const std::string& id : aims::deathHintIds(facts))
    {
        if (shown[id] >= kHintTimes) continue;
        for (const aims::DeathHint& hint : mDeathHints)
        {
            if (hint.id != id) continue;
            ++shown[id];
            mDeathHint = mInput.text(hint.line);
            if (!mHarness) saveSettings();
            return;
        }
    }
}
