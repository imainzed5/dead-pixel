#include "dev/Autoplayer.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

#include "ai/Pathfinder.h"
#include "components/Bleeding.h"
#include "components/Combat.h"
#include "components/Health.h"
#include "components/Interactable.h"
#include "components/Inventory.h"
#include "components/Needs.h"
#include "components/Sleeping.h"
#include "components/Stamina.h"
#include "components/Wounds.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "core/Game.h"
#include "core/GameShared.h"
#include "systems/CombatSystem.h"
#include "systems/InventorySystem.h"
#include "world/FirstDays.h"

using namespace gameshared;

namespace
{
constexpr float kTile = 32.0f;
constexpr int kTapGapTicks = 8;     // a key goes up for a few ticks between presses
constexpr int kStuckGameHours = 3;
constexpr float kMinReach = 24.0f;  // a weapon reaching less than this (a broken knife) is not worth swinging
constexpr float kRunUntilFatigue = 70.0f; // sprinting tires fast and only standing still undoes it


bool isFood(const ItemDatabase& db, int itemId, float condition)
{
    const ItemDef* def = db.find(itemId);
    return def != nullptr && def->category == ItemCategory::Food && def->hungerRestore > 0.0f && condition >= 0.3f &&
           def->rawIllness <= 0.0f;
}

bool isDrink(const ItemDatabase& db, int itemId, float)
{
    const ItemDef* def = db.find(itemId);
    return def != nullptr && def->category == ItemCategory::Drink && def->thirstRestore > 0.0f;
}

bool stopsBleeding(const ItemDatabase& db, int itemId, float)
{
    const ItemDef* def = db.find(itemId);
    return def != nullptr && def->stopsBleeding;
}

bool treatsWounds(const ItemDatabase& db, int itemId, float)
{
    const ItemDef* def = db.find(itemId);
    return def != nullptr && def->woundTreatment > 0;
}

bool heals(const ItemDatabase& db, int itemId, float)
{
    const ItemDef* def = db.find(itemId);
    return def != nullptr && def->category == ItemCategory::Medical && def->healthRestore > 0.0f;
}

// What a weapon in a pack slot is worth in a fight: damage per swing, and
// nothing at all when its reach is inside a zombie's (a broken knife).
float weaponScore(const ItemDatabase& db, int itemId, float condition)
{
    const ItemDef* def = db.find(itemId);
    if (def == nullptr || def->category != ItemCategory::Weapon || def->weaponCategory < 0 ||
        def->weaponCategory == static_cast<int>(WeaponCategory::Ranged))
    {
        return 0.0f;
    }
    Combat combat{};
    combat.weapon = weaponStatsForItem(*def);
    combat.weapon.durability = std::clamp(
        static_cast<int>(std::round(static_cast<float>(combat.weapon.maxDurability) * condition)), 0,
        combat.weapon.maxDurability);
    combat.weapon.condition = weaponConditionFor(combat.weapon.durability, combat.weapon.maxDurability);
    if (CombatSystem::effectiveRangePixels(combat) < kMinReach) return 0.0f;
    return CombatSystem::effectiveDamage(combat);
}

bool isUsableWeapon(const ItemDatabase& db, int itemId, float condition)
{
    return weaponScore(db, itemId, condition) > 0.0f;
}

glm::vec2 tileCentre(const glm::ivec2& tile)
{
    return (glm::vec2(tile) + glm::vec2(0.5f)) * kTile;
}
} // namespace

double Autoplayer::clock(const Game& game)
{
    return static_cast<double>(game.mCurrentDay) * 24.0 + game.mGameHours;
}

bool Autoplayer::parseProfile(std::string_view name, Profile& out)
{
    if (name == "careful") out = Profile::Careful;
    else if (name == "normal") out = Profile::Normal;
    else if (name == "bold") out = Profile::Bold;
    else return false;
    return true;
}

void Autoplayer::start(Game& game, Profile profile, int days)
{
    *this = Autoplayer{};
    mRunning = true;
    mProfile = profile;
    mEndDay = game.mCurrentDay + std::max(1, days);
    mRng ^= static_cast<std::uint32_t>(game.mSaveManager.currentRunSeed());
    mLastMoveHour = clock(game);
    mStuckTile = playerTile(game);
}

void Autoplayer::stop(Game& game)
{
    mWant.clear();
    mClickNow = false;
    applyInput(game);
    mRunning = false;
}

bool Autoplayer::tap(SDL_Scancode key)
{
    if (mTapCooldown > 0)
    {
        return false;
    }
    mWant.push_back(key);
    mTapCooldown = kTapGapTicks;
    return true;
}

bool Autoplayer::click()
{
    if (mClickCooldown > 0)
    {
        return false;
    }
    mClickNow = true;
    mClickCooldown = kTapGapTicks;
    return true;
}

void Autoplayer::mouseAt(Game& game, const glm::vec2& screen)
{
    game.mInput.setMousePosition(static_cast<int>(screen.x), static_cast<int>(screen.y));
}

void Autoplayer::applyInput(Game& game)
{
    // Keys held last tick and not wanted now go up; wanted ones go down.
    for (SDL_Scancode key : mHeld)
    {
        if (std::find(mWant.begin(), mWant.end(), key) == mWant.end())
        {
            game.mInput.setKeyState(key, false);
        }
    }
    for (SDL_Scancode key : mWant)
    {
        game.mInput.setKeyState(key, true);
    }
    mHeld = mWant;
    mWant.clear();

    if (mMouseDown && !mClickNow)
    {
        game.mInput.setMouseButtonState(SDL_BUTTON_LEFT, false);
        mMouseDown = false;
    }
    else if (mClickNow && !mMouseDown)
    {
        game.mInput.setMouseButtonState(SDL_BUTTON_LEFT, true);
        mMouseDown = true;
    }
    mClickNow = false;
}

glm::vec2 Autoplayer::playerCentre(Game& game) const
{
    if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Transform>(game.mPlayerEntity))
    {
        return glm::vec2(0.0f);
    }
    const Transform& t = game.mWorld.getComponent<Transform>(game.mPlayerEntity);
    return glm::vec2(t.x + 16.0f, t.y + 16.0f);
}

glm::ivec2 Autoplayer::playerTile(Game& game) const
{
    const glm::vec2 c = playerCentre(game);
    return glm::ivec2(static_cast<int>(std::floor(c.x / kTile)), static_cast<int>(std::floor(c.y / kTile)));
}

glm::ivec2 Autoplayer::homeTile(Game& game) const
{
    return game.mSpawnData.playerStart;
}

bool Autoplayer::standTileFor(Game& game, const glm::ivec2& tile, glm::ivec2& out) const
{
    std::vector<glm::ivec2> spots;
    for (const FurniturePiece& piece : game.mFurniture)
    {
        if (piece.tile == tile && furniture::info(piece.kind).oriented)
        {
            spots.push_back(furniture::frontTile(piece));
        }
    }
    for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(0, 1), glm::ivec2(-1, 0), glm::ivec2(0, -1)})
    {
        spots.push_back(tile + side);
    }
    const int building = game.mTileMap.getBuildingId(tile.x, tile.y);
    for (const glm::ivec2& spot : spots)
    {
        if (spot.x < 0 || spot.y < 0 || spot.x >= game.mTileMap.width() || spot.y >= game.mTileMap.height()) continue;
        if (game.mTileMap.isSolid(spot.x, spot.y)) continue;
        if (game.mTileMap.doorState(spot.x, spot.y) != DoorState::None) continue;
        if (building > 0 && game.mTileMap.getBuildingId(spot.x, spot.y) != building) continue;
        if (mUnreachable.count(Tile{spot.x, spot.y}) != 0) continue;
        out = spot;
        return true;
    }
    return false;
}

// Picks the WASD combination closest to a world direction (the keys move in
// screen directions: W is world (-1,-1), D is (1,-1)).
void Autoplayer::steer(Game&, const glm::vec2& worldDirection)
{
    if (glm::dot(worldDirection, worldDirection) < 1e-6f)
    {
        return;
    }
    struct Option
    {
        glm::vec2 dir;
        SDL_Scancode a;
        SDL_Scancode b;
    };
    static const Option kOptions[] = {
        {{-1.0f, -1.0f}, SDL_SCANCODE_W, SDL_SCANCODE_UNKNOWN}, {{1.0f, 1.0f}, SDL_SCANCODE_S, SDL_SCANCODE_UNKNOWN},
        {{-1.0f, 1.0f}, SDL_SCANCODE_A, SDL_SCANCODE_UNKNOWN},  {{1.0f, -1.0f}, SDL_SCANCODE_D, SDL_SCANCODE_UNKNOWN},
        {{-1.0f, 0.0f}, SDL_SCANCODE_W, SDL_SCANCODE_A},        {{0.0f, -1.0f}, SDL_SCANCODE_W, SDL_SCANCODE_D},
        {{0.0f, 1.0f}, SDL_SCANCODE_S, SDL_SCANCODE_A},         {{1.0f, 0.0f}, SDL_SCANCODE_S, SDL_SCANCODE_D},
    };
    const glm::vec2 want = glm::normalize(worldDirection);
    const Option* best = &kOptions[0];
    float bestDot = -2.0f;
    for (const Option& option : kOptions)
    {
        const float d = glm::dot(glm::normalize(option.dir), want);
        if (d > bestDot)
        {
            bestDot = d;
            best = &option;
        }
    }
    hold(best->a);
    if (best->b != SDL_SCANCODE_UNKNOWN) hold(best->b);
}

Autoplayer::Move Autoplayer::moveTo(Game& game, const glm::ivec2& goal, bool run)
{
    const glm::ivec2 here = playerTile(game);
    const glm::vec2 centre = playerCentre(game);
    if (here == goal && glm::length(tileCentre(goal) - centre) < 10.0f)
    {
        resetPath();
        return Move::Arrived;
    }

    if (goal != mPathGoal || mPath.empty())
    {
        mPath = Pathfinder::findPath(game.mTileMap, here, goal, Pathfinder::Mover::Living);
        mPathGoal = goal;
        mPathIndex = 0;
        mNoProgressTicks = 0;
        if (mPath.empty() && here != goal)
        {
            mUnreachable.insert(Tile{goal.x, goal.y});
            resetPath();
            return Move::Failed;
        }
        if (mPath.empty()) mPath.push_back(goal);
    }

    // Skip waypoints already reached.
    while (mPathIndex + 1 < mPath.size() && mPath[mPathIndex] == here)
    {
        ++mPathIndex;
    }
    for (std::size_t i = mPathIndex; i < mPath.size(); ++i)
    {
        if (mPath[i] == here && i + 1 < mPath.size()) mPathIndex = i + 1;
    }
    const glm::ivec2 next = mPath[std::min(mPathIndex, mPath.size() - 1)];

    // A closed door on the way: pry its boards off, then open it.
    if (game.mTileMap.doorState(next.x, next.y) == DoorState::Closed)
    {
        const float d = glm::length(tileCentre(next) - centre);
        if (d < kTile * 1.2f)
        {
            ++mDoorTicks;
            mActivity = "door";
            if (mDoorTicks > 600)
            {
                // Cannot get it open: try another way round.
                mUnreachable.insert(Tile{goal.x, goal.y});
                mDoorTicks = 0;
                resetPath();
                return Move::Failed;
            }
            if (game.mTileMap.doorBoards(next.x, next.y) > 0 && atHome(game) && isDark(game))
            {
                // Never undo the night's boards from the inside.
                mUnreachable.insert(Tile{goal.x, goal.y});
                mDoorTicks = 0;
                resetPath();
                return Move::Failed;
            }
            if (game.mTileMap.doorBoards(next.x, next.y) > 0) tap(SDL_SCANCODE_X);
            else tap(SDL_SCANCODE_E);
            steer(game, tileCentre(next) - centre); // lean in so the door is the nearest thing
            return Move::Walking;
        }
    }
    mDoorTicks = 0;

    // Progress watch: no tile change for a few seconds means a wall or a body
    // in the way. Repath, then sidestep, then give up on the goal.
    if (here != mLastTile)
    {
        mLastTile = here;
        mNoProgressTicks = 0;
        mRepaths = 0;
    }
    else if (++mNoProgressTicks > 240)
    {
        mNoProgressTicks = 0;
        if (++mRepaths > 3)
        {
            mUnreachable.insert(Tile{goal.x, goal.y});
            mRepaths = 0;
            resetPath();
            return Move::Failed;
        }
        mRng = mRng * 1664525u + 1013904223u;
        const float angle = static_cast<float>(mRng >> 8) * (6.2831853f / 16777216.0f);
        mNudgeDir = glm::vec2(std::cos(angle), std::sin(angle));
        mNudgeTicks = 30;
        resetPath();
        return Move::Walking;
    }

    if (mNudgeTicks > 0)
    {
        --mNudgeTicks;
        steer(game, mNudgeDir);
    }
    else
    {
        steer(game, tileCentre(next) - centre);
    }
    if (run) hold(SDL_SCANCODE_LSHIFT);
    return Move::Walking;
}

// Close enough to lean in and use a piece of furniture: within a tile and a
// half, and in the same building (not through a wall from next door).
bool Autoplayer::withinReach(Game& game, const glm::ivec2& tile) const
{
    const glm::ivec2 here = playerTile(game);
    if (game.mTileMap.getBuildingId(here.x, here.y) != game.mTileMap.getBuildingId(tile.x, tile.y)) return false;
    if (std::abs(here.x - tile.x) > 1 || std::abs(here.y - tile.y) > 1) return false;
    return glm::length(tileCentre(tile) - playerCentre(game)) < 44.0f;
}

void Autoplayer::useSlot(int slot)
{
    mUseSlot = slot;
    mUseTicks = 0;
    mUseDone = false;
}

int Autoplayer::findSlot(Game& game, bool (*want)(const ItemDatabase&, int itemId, float condition)) const
{
    if (game.mPlayerEntity == kInvalidEntity || !game.mWorld.hasComponent<Inventory>(game.mPlayerEntity)) return -1;
    const Inventory& inv = game.mWorld.getComponent<Inventory>(game.mPlayerEntity);
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const InventorySlot& slot = inv.slots[static_cast<std::size_t>(i)];
        if (slot.itemId >= 0 && slot.count > 0 && want(game.mItemDatabase, slot.itemId, slot.condition)) return i;
    }
    return -1;
}

bool Autoplayer::handlePendingUse(Game& game)
{
    if (mUseSlot < 0)
    {
        return false;
    }
    mActivity = "using";
    if (++mUseTicks > 120)
    {
        mUseSlot = -1;
        if (game.mShowInventory) tap(SDL_SCANCODE_TAB);
        return true;
    }
    if (mUseDone)
    {
        if (game.mShowInventory) tap(SDL_SCANCODE_TAB);
        else mUseSlot = -1;
        return true;
    }
    if (!game.mShowInventory)
    {
        tap(SDL_SCANCODE_TAB);
        return true;
    }
    const InventoryLayout layout = computeInventoryLayout(game.mWindowWidth, game.mWindowHeight);
    mouseAt(game, layout.slotPos(mUseSlot) + glm::vec2(InventoryLayout::kSlot * 0.5f));
    if (game.mInventoryCursor == mUseSlot && tap(SDL_SCANCODE_F))
    {
        mUseDone = true;
    }
    return true;
}

bool Autoplayer::treatBleeding(Game& game)
{
    const bool bleeding = game.mWorld.hasComponent<Bleeding>(game.mPlayerEntity);
    const bool wounded = game.mWorld.hasComponent<Wounds>(game.mPlayerEntity) &&
                         !game.mWorld.getComponent<Wounds>(game.mPlayerEntity).active.empty();
    if (!bleeding && !wounded) return false;
    if (clock(game) < mTreatRetryAt) return false; // the last try did nothing; wait a while
    const int slot = findSlot(game, bleeding ? stopsBleeding : treatsWounds);
    if (slot < 0) return false;
    mActivity = "bandage";
    mTreatRetryAt = clock(game) + 0.5;
    useSlot(slot);
    return true;
}

bool Autoplayer::eatAndDrink(Game& game)
{
    if (!game.mWorld.hasComponent<Needs>(game.mPlayerEntity)) return false;
    if (game.mWorld.hasComponent<Health>(game.mPlayerEntity))
    {
        const Health& health = game.mWorld.getComponent<Health>(game.mPlayerEntity);
        if (health.current < health.maximum * 0.5f)
        {
            const int slot = findSlot(game, heals);
            if (slot >= 0)
            {
                mActivity = "heal";
                useSlot(slot);
                return true;
            }
        }
    }
    const Needs& needs = game.mWorld.getComponent<Needs>(game.mPlayerEntity);
    if (needs.thirst < 60.0f)
    {
        const int slot = findSlot(game, isDrink);
        if (slot >= 0)
        {
            mActivity = "drink";
            useSlot(slot);
            return true;
        }
    }
    if (needs.hunger < 60.0f)
    {
        const int slot = findSlot(game, isFood);
        if (slot >= 0)
        {
            mActivity = "eat";
            useSlot(slot);
            return true;
        }
    }
    return false;
}

float Autoplayer::heldWeaponScore(Game& game) const
{
    if (!game.mWorld.hasComponent<Combat>(game.mPlayerEntity)) return 0.0f;
    const Combat& combat = game.mWorld.getComponent<Combat>(game.mPlayerEntity);
    if (combat.weapon.itemId < 0 || combat.weapon.category == WeaponCategory::Ranged) return 0.0f;
    if (CombatSystem::effectiveRangePixels(combat) < kMinReach) return 0.0f;
    return CombatSystem::effectiveDamage(combat);
}

int Autoplayer::bestWeaponSlot(Game& game, float* score) const
{
    int best = -1;
    float bestScore = 0.0f;
    if (game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
    {
        const Inventory& inv = game.mWorld.getComponent<Inventory>(game.mPlayerEntity);
        for (int i = 0; i < Inventory::kMaxSlots; ++i)
        {
            const InventorySlot& slot = inv.slots[static_cast<std::size_t>(i)];
            if (slot.itemId < 0 || slot.count <= 0 || i == game.mEquippedWeaponSlot) continue;
            const float s = weaponScore(game.mItemDatabase, slot.itemId, slot.condition);
            if (s > bestScore)
            {
                bestScore = s;
                best = i;
            }
        }
    }
    if (score != nullptr) *score = bestScore;
    return best;
}

bool Autoplayer::armSelf(Game& game)
{
    if (!game.mWorld.hasComponent<Combat>(game.mPlayerEntity) || clock(game) < mArmRetryAt) return false;
    float score = 0.0f;
    const int slot = bestWeaponSlot(game, &score);
    // The best weapon carried goes in hand (a broken one is worth nothing).
    if (slot < 0 || score <= heldWeaponScore(game) + 1.0f) return false;
    mActivity = "arm";
    mArmRetryAt = clock(game) + 0.25;
    useSlot(slot);
    return true;
}

bool Autoplayer::handleDanger(Game& game)
{
    const glm::vec2 here = playerCentre(game);
    struct Near
    {
        Entity entity;
        glm::vec2 centre;
        float distance;
        bool windingUp;
        float health;
    };
    // Only the dead it can see count: one behind a wall is not a fight.
    const auto inSight = [&](const glm::vec2& to)
    {
        const glm::vec2 step = to - here;
        const int samples = std::max(1, static_cast<int>(glm::length(step) / 8.0f));
        for (int i = 1; i < samples; ++i)
        {
            const glm::vec2 p = here + step * (static_cast<float>(i) / static_cast<float>(samples));
            if (game.mTileMap.blocksView(static_cast<int>(p.x / kTile), static_cast<int>(p.y / kTile))) return false;
        }
        return true;
    };
    std::vector<Near> close;
    game.mWorld.forEach<ZombieAI, Transform, Health>(
        [&](Entity e, ZombieAI& ai, Transform& t, Health& h)
        {
            if (h.current <= 0.0f) return;
            const glm::vec2 c(t.x + 16.0f, t.y + 16.0f);
            const float d = glm::length(c - here);
            // One left alone for going nowhere still counts once it is close enough to bite.
            if (const auto it = mIgnored.find(e); it != mIgnored.end() && it->second > mTicks && d > 1.5f * kTile) return;
            // Only the ones coming for us: a shambler minding its own business is walked past.
            const bool engaged = ai.state == ZombieState::Chase || ai.state == ZombieState::Attack;
            if (engaged && d <= 8.0f * kTile && inSight(c)) close.push_back(Near{e, c, d, ai.attackWindup > 0.0f, h.current});
        });
    if (close.empty())
    {
        mFleeTicks = 0;
        return false;
    }
    if (game.mContainerOpen)
    {
        tap(SDL_SCANCODE_ESCAPE); // the dead first, the cupboard later
        return true;
    }

    int fleeAt = mProfile == Profile::Careful ? 2 : mProfile == Profile::Bold ? 4 : 3;
    if (game.mWorld.hasComponent<Health>(game.mPlayerEntity))
    {
        const Health& health = game.mWorld.getComponent<Health>(game.mPlayerEntity);
        if (health.current < health.maximum * 0.35f) fleeAt = 1; // hurt: run from anything
    }
    if (heldWeaponScore(game) <= 0.0f) fleeAt = 1; // nothing that outreaches them: do not trade blows
    if (mTicks < mCorneredUntil) fleeAt = 1000;      // running did not work: stand and fight
    if (game.mWorld.hasComponent<Stamina>(game.mPlayerEntity) &&
        game.mWorld.getComponent<Stamina>(game.mPlayerEntity).current < 20.0f && close.size() >= 2)
    {
        fleeAt = 1; // out of breath against more than one: get away
    }
    if (static_cast<int>(close.size()) >= fleeAt || mFleeTicks > 0)
    {
        // Run from the middle of them, to a tile about ten away.
        mActivity = "flee";
        if (mFleeTicks <= 0 || mFleeGoal.x < 0)
        {
            glm::vec2 middle(0.0f);
            for (const Near& n : close) middle += n.centre;
            middle /= static_cast<float>(close.size());
            glm::vec2 away = here - middle;
            if (glm::dot(away, away) < 1.0f) away = glm::vec2(1.0f, 0.0f);
            away = glm::normalize(away);
            mFleeGoal = glm::ivec2(-1);
            float bestScore = -1e9f;
            const glm::ivec2 tile = playerTile(game);
            const glm::vec2 toHome = glm::vec2(homeTile(game) - tile);
            const float homeAway = glm::dot(toHome, toHome) > 1.0f ? glm::dot(glm::normalize(toHome), away) : 0.0f;
            for (int dy = -10; dy <= 10; ++dy)
            {
                for (int dx = -10; dx <= 10; ++dx)
                {
                    const glm::ivec2 t = tile + glm::ivec2(dx, dy);
                    if (t.x < 0 || t.y < 0 || t.x >= game.mTileMap.width() || t.y >= game.mTileMap.height()) continue;
                    if (game.mTileMap.isSolid(t.x, t.y)) continue;
                    const glm::vec2 offset(static_cast<float>(dx), static_cast<float>(dy));
                    const float len = glm::length(offset);
                    if (len < 5.0f) continue;
                    float score = glm::dot(offset / len, away) * 10.0f - std::abs(len - 9.0f);
                    // Running home beats running anywhere, when home is not behind them.
                    if (homeAway > 0.0f) score -= 0.25f * glm::length(glm::vec2(t - homeTile(game)));
                    if (score > bestScore)
                    {
                        bestScore = score;
                        mFleeGoal = t;
                    }
                }
            }
            mFleeTicks = 180;
            resetPath();
        }
        --mFleeTicks;
        float closest = 1e9f;
        for (const Near& n : close) closest = std::min(closest, n.distance);
        const bool sprint = closest < 3.0f * kTile && fatigue(game) < kRunUntilFatigue;
        // Pinned (a wall, a corner, bodies) with one in reach: turn and fight.
        const glm::ivec2 nowTile = playerTile(game);
        if (nowTile != mFleeTile)
        {
            mFleeTile = nowTile;
            mFleeStillTicks = 0;
        }
        else if (++mFleeStillTicks > 45 && closest < 1.5f * kTile)
        {
            mFleeStillTicks = 0;
            mFleeTicks = 0;
            mFleeGoal = glm::ivec2(-1);
            mCorneredUntil = mTicks + 600;
            resetPath();
            return true;
        }
        const Move move = mFleeGoal.x < 0 ? Move::Failed : moveTo(game, mFleeGoal, sprint);
        if (move != Move::Walking)
        {
            mFleeTicks = 0;
            mFleeGoal = glm::ivec2(-1);
            if (move == Move::Failed)
            {
                // Nowhere to run: fight what comes close for a while, and
                // stop running from the ones further off.
                mCorneredUntil = mTicks + 600;
                for (const Near& n : close) mIgnored[n.entity] = mTicks + 1800;
            }
        }
        return true;
    }

    // Fight what is within two tiles.
    const auto nearest = std::min_element(close.begin(), close.end(),
                                          [](const Near& a, const Near& b) { return a.distance < b.distance; });
    if (nearest->distance > 2.0f * kTile)
    {
        return false;
    }
    mActivity = "fight";
    // A fight that goes nowhere (it cannot reach us, we cannot hit it) is
    // left for a minute.
    if (nearest->entity != mFightTarget || nearest->health < mFightHealth)
    {
        mFightTarget = nearest->entity;
        mFightHealth = nearest->health;
        mFightTicks = 0;
    }
    if (++mFightTicks > 600)
    {
        mIgnored[nearest->entity] = mTicks + 3600;
        mFightTicks = 0;
        return false;
    }
    mouseAt(game, game.mCamera.worldToScreen(nearest->centre));
    float reach = 44.0f;
    if (game.mWorld.hasComponent<Combat>(game.mPlayerEntity))
    {
        reach = std::max(24.0f, CombatSystem::effectiveRangePixels(game.mWorld.getComponent<Combat>(game.mPlayerEntity)) + 4.0f);
    }
    const glm::vec2 toward = nearest->centre - here;
    if (nearest->windingUp && nearest->distance < 40.0f)
    {
        steer(game, -toward); // step back from the lunge
        return true;
    }
    const bool winded = game.mWorld.hasComponent<Stamina>(game.mPlayerEntity) &&
                        game.mWorld.getComponent<Stamina>(game.mPlayerEntity).current < 15.0f;
    if (winded && nearest->distance < reach + 16.0f)
    {
        steer(game, -toward); // out of breath: back off rather than swing and miss
    }
    else if (nearest->distance <= reach)
    {
        click();
    }
    else
    {
        steer(game, toward);
    }
    return true;
}

bool Autoplayer::wantsFromContainer(Game& game, int itemId) const
{
    const ItemDef* def = game.mItemDatabase.find(itemId);
    if (def == nullptr) return false;
    if (def->category == ItemCategory::Food || def->category == ItemCategory::Drink ||
        def->category == ItemCategory::Medical)
    {
        return true;
    }
    if (itemId == game.mItems.woodPlanks || itemId == game.mItems.nails || itemId == game.mItems.emptyBottle ||
        def->hasTag("tool:hammer"))
    {
        return true;
    }
    if (def->category == ItemCategory::Weapon && def->weaponCategory != static_cast<int>(WeaponCategory::Ranged))
    {
        // Two good weapons to carry: knives break.
        if (!isUsableWeapon(game.mItemDatabase, itemId, 1.0f)) return false;
        int usable = 0;
        if (game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
        {
            for (const InventorySlot& slot : game.mWorld.getComponent<Inventory>(game.mPlayerEntity).slots)
            {
                if (slot.itemId >= 0 && slot.count > 0 && isUsableWeapon(game.mItemDatabase, slot.itemId, slot.condition)) ++usable;
            }
        }
        return usable < 2;
    }
    return false;
}

bool Autoplayer::handleContainerPanel(Game& game)
{
    if (!game.mContainerOpen)
    {
        mPanelTicks = 0;
        mPanelClicks = 0;
        return false;
    }
    mActivity = "loot";
    // Hold the panel still a moment, then take what is wanted, row by row.
    if (++mPanelTicks < 6)
    {
        return true;
    }
    const Entity box = game.mOpenContainerEntity;
    if (box != kInvalidEntity && game.mWorld.hasComponent<Inventory>(box) && mPanelClicks < 16 && mPanelTicks < 400)
    {
        const Inventory& inv = game.mWorld.getComponent<Inventory>(box);
        int row = 0;
        for (const InventorySlot& slot : inv.slots)
        {
            if (slot.itemId < 0 || slot.count <= 0) continue;
            if (wantsFromContainer(game, slot.itemId))
            {
                const ContainerLayout layout = game.containerLayout();
                mouseAt(game, glm::vec2(layout.boxX + 20.0f,
                                        ContainerLayout::kListY + static_cast<float>(row) * ContainerLayout::kRowH + 8.0f));
                hold(SDL_SCANCODE_LSHIFT);
                if (click()) ++mPanelClicks;
                return true;
            }
            ++row;
        }
    }
    if (box != kInvalidEntity && game.mWorld.hasComponent<Transform>(box))
    {
        const Transform& t = game.mWorld.getComponent<Transform>(box);
        const glm::ivec2 tile(static_cast<int>(t.x) / 32, static_cast<int>(t.y) / 32);
        mSearched.insert(Tile{tile.x, tile.y});
    }
    // The one walked to counts as searched even if a neighbour took the E.
    if (mTargetContainer != kInvalidEntity)
    {
        mSearched.insert(Tile{mTarget.x, mTarget.y});
    }
    tap(SDL_SCANCODE_ESCAPE);
    mTargetContainer = kInvalidEntity;
    return true;
}

bool Autoplayer::sleepAtHome(Game& game)
{
    const double hour = game.mGameHours;
    const bool night = hour >= 22.0 || hour < 6.0;
    if (!night || clock(game) < mSleepRetryAt) return false;

    // The home's bed (or the nearest one).
    const int home = game.mFirstDays.homeBuilding;
    const FurniturePiece* bed = nullptr;
    float bestD = 0.0f;
    const glm::vec2 homeAt = glm::vec2(homeTile(game));
    for (const FurniturePiece& piece : game.mFurniture)
    {
        if (piece.kind != FurnitureKind::Bed) continue;
        if (mUnreachable.count(Tile{piece.tile.x, piece.tile.y}) != 0) continue;
        float d = glm::length(glm::vec2(piece.tile) - homeAt);
        if (home > 0 && game.mTileMap.getBuildingId(piece.tile.x, piece.tile.y) != home) d += 1000.0f;
        if (bed == nullptr || d < bestD)
        {
            bed = &piece;
            bestD = d;
        }
    }
    if (bed == nullptr || bestD > 60.0f) return false;
    glm::ivec2 stand;
    if (!standTileFor(game, bed->tile, stand))
    {
        mUnreachable.insert(Tile{bed->tile.x, bed->tile.y});
        return false;
    }
    mActivity = "bed";
    const Move move = withinReach(game, bed->tile) ? Move::Arrived : moveTo(game, stand, false);
    if (move == Move::Failed)
    {
        mUnreachable.insert(Tile{bed->tile.x, bed->tile.y});
        return false;
    }
    if (move == Move::Arrived)
    {
        steer(game, tileCentre(bed->tile) - playerCentre(game));
        if (tap(SDL_SCANCODE_E))
        {
            ++mOpenTries;
            if (mOpenTries > 4)
            {
                // Not tired enough, or the dead too close: try again in a while.
                mOpenTries = 0;
                mSleepRetryAt = clock(game) + 0.75;
            }
        }
    }
    return true;
}

// In the home, its doorways included.
bool Autoplayer::atHome(Game& game) const
{
    const int home = game.mFirstDays.homeBuilding;
    if (home <= 0) return false;
    const glm::ivec2 at = playerTile(game);
    if (game.mTileMap.getBuildingId(at.x, at.y) == home) return true;
    return std::find(game.mFirstDays.homeDoors.begin(), game.mFirstDays.homeDoors.end(), at) != game.mFirstDays.homeDoors.end();
}

bool Autoplayer::isDark(Game& game) const
{
    return game.mGameHours >= homeHour() || game.mGameHours < 6.0;
}

double Autoplayer::homeHour() const
{
    return mProfile == Profile::Careful ? 16.0 : mProfile == Profile::Bold ? 17.0 : 16.5;
}

bool Autoplayer::eveningAtHome(Game& game)
{
    const double hour = game.mGameHours;
    if (hour < homeHour() && hour >= 6.0) return false;
    const int home = game.mFirstDays.homeBuilding;
    const bool inside = atHome(game);

    // Board the home's ways in while there are planks, nails and a hammer:
    // the weakest first, since that is where the night's callers go.
    if (inside && clock(game) >= mBoardRetryAt && game.mWorld.hasComponent<Inventory>(game.mPlayerEntity))
    {
        const Inventory& inv = game.mWorld.getComponent<Inventory>(game.mPlayerEntity);
        const bool canBoard = InventorySystem::countItem(inv, game.mItems.woodPlanks) > 0 &&
                              InventorySystem::countItem(inv, game.mItems.nails) >= 2 &&
                              InventorySystem::countTagged(inv, game.mItemDatabase, "tool:hammer") > 0;
        std::vector<Game::WayIn> ways = game.waysIn(home);
        std::stable_sort(ways.begin(), ways.end(),
                         [](const Game::WayIn& a, const Game::WayIn& b) { return a.strength < b.strength; });
        for (const Game::WayIn& way : ways)
        {
            const glm::ivec2 tile = way.tile;
            glm::ivec2 stand(-1);
            SDL_Scancode key = SDL_SCANCODE_R;
            if (way.window)
            {
                const Windows::Window* window = game.mWindows.find(tile);
                if (window == nullptr) continue;
                const bool full = game.mWindows.boards(tile) >= Windows::kMaxBoards;
                // No boards to spare: at least draw the curtain on whole glass.
                const bool curtain = !canBoard && game.mWindows.boards(tile) == 0 &&
                                     game.mTileMap.windowState(tile.x, tile.y) == WindowState::Intact &&
                                     !game.mTileMap.windowCovered(tile.x, tile.y);
                if (full || (!canBoard && !curtain)) continue;
                stand = window->spot.inside;
                key = canBoard ? SDL_SCANCODE_R : SDL_SCANCODE_E;
            }
            else
            {
                if (!canBoard) continue;
                const DoorState state = game.mTileMap.doorState(tile.x, tile.y);
                if (state == DoorState::Broken || state == DoorState::None) continue;
                if (game.mTileMap.doorBoards(tile.x, tile.y) >= Doors::kMaxBoards) continue;
                for (const glm::ivec2& side : {glm::ivec2(1, 0), glm::ivec2(-1, 0), glm::ivec2(0, 1), glm::ivec2(0, -1)})
                {
                    const glm::ivec2 t = tile + side;
                    if (!game.mTileMap.isSolid(t.x, t.y) && game.mTileMap.getBuildingId(t.x, t.y) == home)
                    {
                        stand = t;
                        break;
                    }
                }
                key = state == DoorState::Open ? SDL_SCANCODE_E : SDL_SCANCODE_R;
            }
            if (stand.x < 0 || game.mTileMap.isSolid(stand.x, stand.y) || mUnreachable.count(Tile{stand.x, stand.y}) != 0)
            {
                continue;
            }
            mActivity = way.window ? (canBoard ? "board window" : "curtain") : "board";
            const Move move = moveTo(game, stand, false);
            if (move == Move::Failed) continue;
            if (move == Move::Arrived)
            {
                steer(game, tileCentre(tile) - playerCentre(game));
                if (tap(key))
                {
                    if (++mOpenTries > 12)
                    {
                        mOpenTries = 0;
                        mBoardRetryAt = clock(game) + 2.0;
                    }
                }
            }
            return true;
        }
    }

    if (inside)
    {
        return false; // home: the rest (bed, wandering the rooms) follows
    }
    mActivity = "home";
    const Move move = moveTo(game, homeTile(game), false);
    return move == Move::Walking;
}

float Autoplayer::fatigue(Game& game) const
{
    return game.mWorld.hasComponent<Needs>(game.mPlayerEntity) ? game.mWorld.getComponent<Needs>(game.mPlayerEntity).fatigue
                                                               : 0.0f;
}

bool Autoplayer::restWhenTired(Game& game)
{
    // Worn out (running, fighting): stand still until the legs come back.
    const float tired = fatigue(game);
    if (!mResting && tired < 60.0f) return false;
    mResting = tired > 15.0f;
    if (!mResting) return false;
    mActivity = "rest";
    resetPath();
    return true;
}

bool Autoplayer::isStocked(Game& game) const
{
    // Enough food and water carried for a few days, and a weapon that works.
    if (!game.mWorld.hasComponent<Inventory>(game.mPlayerEntity)) return false;
    int food = 0;
    int drink = 0;
    int weapons = 0;
    for (const InventorySlot& slot : game.mWorld.getComponent<Inventory>(game.mPlayerEntity).slots)
    {
        if (slot.itemId < 0 || slot.count <= 0) continue;
        if (isFood(game.mItemDatabase, slot.itemId, slot.condition)) food += slot.count;
        if (isDrink(game.mItemDatabase, slot.itemId, slot.condition)) drink += slot.count;
        if (isUsableWeapon(game.mItemDatabase, slot.itemId, slot.condition)) ++weapons;
    }
    return food >= 6 && drink >= 4 && weapons >= 2;
}

float Autoplayer::fitShare() const
{
    return mProfile == Profile::Careful ? 0.8f : mProfile == Profile::Bold ? 0.6f : 0.7f;
}

bool Autoplayer::retreatWhenHurt(Game& game)
{
    if (!game.mWorld.hasComponent<Health>(game.mPlayerEntity) || game.mFirstDays.homeBuilding <= 0) return false;
    const Health& health = game.mWorld.getComponent<Health>(game.mPlayerEntity);
    if (health.current >= health.maximum * fitShare()) return false;
    if (atHome(game)) return false;
    mActivity = "home";
    return moveTo(game, homeTile(game), false) == Move::Walking;
}

bool Autoplayer::useTap(Game& game)
{
    if (!firstdays::tapsRunning(game.mCurrentDay, game.mGameHours) || clock(game) < mTapRetryAt) return false;
    if (!game.mWorld.hasComponent<Inventory>(game.mPlayerEntity)) return false;
    const Inventory& inv = game.mWorld.getComponent<Inventory>(game.mPlayerEntity);
    if (InventorySystem::countItem(inv, game.mItems.emptyBottle) <= 0) return false;
    const int home = game.mFirstDays.homeBuilding;
    for (const FurniturePiece& piece : game.mFurniture)
    {
        if (piece.kind != FurnitureKind::Counter && piece.kind != FurnitureKind::Sink) continue;
        if (home <= 0 || game.mTileMap.getBuildingId(piece.tile.x, piece.tile.y) != home) continue;
        glm::ivec2 stand;
        if (!standTileFor(game, piece.tile, stand)) continue;
        mActivity = "tap";
        const Move move = withinReach(game, piece.tile) ? Move::Arrived : moveTo(game, stand, false);
        if (move == Move::Failed) continue;
        if (move == Move::Arrived) steer(game, tileCentre(piece.tile) - playerCentre(game));
        if (move == Move::Arrived && tap(SDL_SCANCODE_R))
        {
            if (++mOpenTries > 3)
            {
                mOpenTries = 0;
                mTapRetryAt = clock(game) + 3.0;
            }
        }
        return true;
    }
    mTapRetryAt = clock(game) + 6.0;
    return false;
}

bool Autoplayer::searchContainers(Game& game)
{
    if (game.mWorld.hasComponent<Health>(game.mPlayerEntity))
    {
        const Health& health = game.mWorld.getComponent<Health>(game.mPlayerEntity);
        if (health.current < health.maximum * fitShare()) return false; // hurt: home, and heal first
    }
    const float range = mProfile == Profile::Careful ? 25.0f : mProfile == Profile::Bold ? 60.0f : 40.0f;
    const glm::vec2 home = glm::vec2(homeTile(game));
    const glm::vec2 here = glm::vec2(playerTile(game));

    // Keep the current target while it is still worth walking to.
    if (mTargetContainer != kInvalidEntity &&
        (!game.mWorld.hasComponent<Transform>(mTargetContainer) || mSearched.count(Tile{mTarget.x, mTarget.y}) != 0 ||
         mUnreachable.count(Tile{mTarget.x, mTarget.y}) != 0))
    {
        mTargetContainer = kInvalidEntity;
    }
    if (mTargetContainer == kInvalidEntity)
    {
        const bool stocked = isStocked(game);
        const bool daytime = game.mGameHours >= 6.0 && game.mGameHours < homeHour();
        // Where the dead stand: a cupboard with two or more of them nearby is left for later.
        std::vector<glm::vec2> dead;
        game.mWorld.forEach<ZombieAI, Transform, Health>(
            [&](Entity, ZombieAI&, Transform& t, Health& h)
            {
                if (h.current > 0.0f) dead.emplace_back((t.x + 16.0f) / kTile, (t.y + 16.0f) / kTile);
            });
        const float clearance = mProfile == Profile::Careful ? 8.0f : mProfile == Profile::Bold ? 6.0f : 7.0f;
        const int crowd = mProfile == Profile::Bold ? 2 : 1;
        const auto crowded = [&](const glm::ivec2& tile)
        {
            int count = 0;
            for (const glm::vec2& z : dead)
            {
                if (glm::length(z - (glm::vec2(tile) + glm::vec2(0.5f))) < clearance && ++count >= crowd) return true;
            }
            return false;
        };
        float bestD = 0.0f;
        game.mWorld.forEach<Interactable, Inventory, Transform>(
            [&](Entity e, Interactable& inter, Inventory&, Transform& t)
            {
                if (inter.type != InteractableType::Container || e == game.mPlayerEntity) return;
                const glm::ivec2 tile(static_cast<int>(t.x) / 32, static_cast<int>(t.y) / 32);
                if (mSearched.count(Tile{tile.x, tile.y}) != 0 || mUnreachable.count(Tile{tile.x, tile.y}) != 0) return;
                if (glm::length(glm::vec2(tile) - home) > range) return;
                if (crowded(tile)) return;
                if (!daytime && game.mTileMap.getBuildingId(tile.x, tile.y) != game.mFirstDays.homeBuilding) return;
                const bool atHome = game.mTileMap.getBuildingId(tile.x, tile.y) == game.mFirstDays.homeBuilding;
                if (stocked && !atHome) return; // enough put by: no need to go out
                // The home's own cupboards first (the kit is in them), then the nearest.
                const float d = glm::length(glm::vec2(tile) - here) + (atHome ? 0.0f : 1000.0f);
                if (mTargetContainer == kInvalidEntity || d < bestD)
                {
                    mTargetContainer = e;
                    mTarget = tile;
                    bestD = d;
                }
            });
        mOpenTries = 0;
    }
    if (mTargetContainer == kInvalidEntity) return false;

    glm::ivec2 stand;
    if (!standTileFor(game, mTarget, stand))
    {
        mUnreachable.insert(Tile{mTarget.x, mTarget.y});
        mTargetContainer = kInvalidEntity;
        return true;
    }
    mActivity = "search";
    const Move move = withinReach(game, mTarget) ? Move::Arrived : moveTo(game, stand, false);
    if (move == Move::Failed)
    {
        mUnreachable.insert(Tile{mTarget.x, mTarget.y});
        mTargetContainer = kInvalidEntity;
        return true;
    }
    if (move == Move::Arrived)
    {
        // Face it and press E; something else nearby may take the press, so
        // after a few tries count it as searched.
        const glm::vec2 boxCentre = tileCentre(mTarget);
        mouseAt(game, game.mCamera.worldToScreen(boxCentre));
        steer(game, boxCentre - playerCentre(game)); // lean in: a hand's reach is under a tile
        if (tap(SDL_SCANCODE_E) && ++mOpenTries > 4)
        {
            mSearched.insert(Tile{mTarget.x, mTarget.y});
            mTargetContainer = kInvalidEntity;
        }
    }
    return true;
}

void Autoplayer::wander(Game& game)
{
    // Home with nothing to fetch, or home after dark: keep still and quiet
    // (pacing the rooms makes noise and never lets the legs rest).
    if (atHome(game) && (isDark(game) || isStocked(game)))
    {
        mActivity = "hold";
        resetPath();
        return;
    }
    mActivity = "wander";
    if (mWanderGoal.x < 0 || --mWanderTicks <= 0)
    {
        const glm::ivec2 home = homeTile(game);
        mWanderGoal = home;
        // Hurt, or after dark: keep to the rooms of home.
        if (mWanderOutdoors > 0) --mWanderOutdoors;
        bool indoorsOnly = mWanderOutdoors <= 0 && game.mFirstDays.homeBuilding > 0 &&
                           (game.mGameHours >= homeHour() || game.mGameHours < 6.0 || isStocked(game));
        if (game.mWorld.hasComponent<Health>(game.mPlayerEntity))
        {
            const Health& health = game.mWorld.getComponent<Health>(game.mPlayerEntity);
            indoorsOnly = indoorsOnly || (mWanderOutdoors <= 0 && game.mFirstDays.homeBuilding > 0 &&
                                          health.current < health.maximum * fitShare());
        }
        for (int attempt = 0; attempt < 30; ++attempt)
        {
            mRng = mRng * 1664525u + 1013904223u;
            const int dx = static_cast<int>((mRng >> 8) % 13u) - 6;
            mRng = mRng * 1664525u + 1013904223u;
            const int dy = static_cast<int>((mRng >> 8) % 13u) - 6;
            const glm::ivec2 t = home + glm::ivec2(dx, dy);
            if (t.x < 0 || t.y < 0 || t.x >= game.mTileMap.width() || t.y >= game.mTileMap.height()) continue;
            if (game.mTileMap.isSolid(t.x, t.y) || game.mTileMap.doorState(t.x, t.y) != DoorState::None) continue;
            if (mUnreachable.count(Tile{t.x, t.y}) != 0) continue;
            if (indoorsOnly && game.mTileMap.getBuildingId(t.x, t.y) != game.mFirstDays.homeBuilding) continue;
            if (t == playerTile(game)) continue;
            mWanderGoal = t;
            break;
        }
        if (mWanderGoal == playerTile(game) && indoorsOnly)
        {
            // Nowhere else indoors (or all of it out of reach): anywhere near home.
            mWanderGoal = glm::ivec2(-1);
            mWanderTicks = 0;
            mWanderOutdoors = 2; // the next two picks
            return;
        }
        mWanderTicks = 900;
        resetPath();
    }
    if (moveTo(game, mWanderGoal, false) != Move::Walking)
    {
        mWanderTicks = std::min(mWanderTicks, 60); // stand a moment, then pick another spot
        if (playerTile(game) != mWanderGoal) mWanderGoal = glm::ivec2(-1);
    }
}

void Autoplayer::checkStuck(Game& game)
{
    const glm::ivec2 here = playerTile(game);
    const bool asleep = game.mWorld.hasComponent<Sleeping>(game.mPlayerEntity);
    // Keeping still on purpose (resting, holding the house) is not being stuck.
    const bool still = std::string_view(mActivity) == "hold" || std::string_view(mActivity) == "rest";
    if (asleep || still || here != mStuckTile)
    {
        mStuckTile = here;
        mLastMoveHour = clock(game);
        return;
    }
    if (mStuck.empty() && clock(game) - mLastMoveHour >= static_cast<double>(kStuckGameHours))
    {
        mStuck = "bot stuck at tile " + std::to_string(here.x) + "," + std::to_string(here.y) + " for " +
                 std::to_string(kStuckGameHours) + " game hours (doing: " + mActivity + ")";
    }
}

void Autoplayer::tick(Game& game)
{
    if (!mRunning)
    {
        return;
    }
    if (game.mRunState != Game::RunState::Playing || game.mPlayerEntity == kInvalidEntity ||
        !game.mWorld.hasComponent<Transform>(game.mPlayerEntity) || game.mCurrentDay >= mEndDay)
    {
        stop(game);
        return;
    }
    ++mTicks;
    mTapCooldown = std::max(0, mTapCooldown - 1);
    mClickCooldown = std::max(0, mClickCooldown - 1);

    // Doors open and the dead move on: what was out of reach may not be now.
    if (clock(game) >= mForgetAt)
    {
        mUnreachable.clear();
        mForgetAt = clock(game) + 2.0;
    }

    checkStuck(game);
    if (!mStuck.empty())
    {
        stop(game);
        return;
    }

    if (game.mWorld.hasComponent<Sleeping>(game.mPlayerEntity))
    {
        mActivity = "asleep";
        applyInput(game);
        return;
    }

    // Panels this bot does not use get closed.
    if (game.mShowCrafting || game.mShowWorldMap || game.mShowNotebook)
    {
        tap(SDL_SCANCODE_ESCAPE);
        applyInput(game);
        return;
    }

    const bool acted = handlePendingUse(game) || treatBleeding(game) || handleDanger(game) ||
                       handleContainerPanel(game) || eatAndDrink(game) || armSelf(game) || restWhenTired(game) || retreatWhenHurt(game) || sleepAtHome(game) ||
                       eveningAtHome(game) || useTap(game) || searchContainers(game);
    if (!acted)
    {
        wander(game);
    }
    applyInput(game);
}
