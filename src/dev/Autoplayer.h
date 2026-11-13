#pragma once

#include <SDL.h>

#include <glm/vec2.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ecs/Types.h"

class Game;
class ItemDatabase;

// A scripted player for soak runs (harness `bot PROFILE [DAYS]`). It reads the
// world the way the player could and presses the keys the player would,
// through Input: WASD along a Pathfinder route, E at containers, doors and
// beds, R at a tap or a door to board, F in the pack to eat, drink and
// bandage, the mouse to aim and swing. It is not clever; it is consistent.
class Autoplayer
{
public:
    enum class Profile
    {
        Careful, // flees at 2, stays within 25 tiles of home
        Normal,  // flees at 3, ranges 40 tiles
        Bold     // fights up to 3, ranges 60 tiles
    };

    static bool parseProfile(std::string_view name, Profile& out);

    void start(Game& game, Profile profile, int days);
    // Before each game tick: decides and sets this tick's input.
    void tick(Game& game);
    void stop(Game& game);

    [[nodiscard]] bool running() const { return mRunning; }
    // Set once when the bot has stood on one tile for 3 game hours awake.
    [[nodiscard]] const std::string& stuckMessage() const { return mStuck; }
    [[nodiscard]] int containersSearched() const { return static_cast<int>(mSearched.size()); }
    [[nodiscard]] const char* activity() const { return mActivity; }

private:
    enum class Move
    {
        Walking,
        Arrived,
        Failed
    };

    struct Tile
    {
        int x = 0;
        int y = 0;
        bool operator<(const Tile& o) const { return x != o.x ? x < o.x : y < o.y; }
    };

    // Input for this tick.
    void hold(SDL_Scancode key) { mWant.push_back(key); }
    bool tap(SDL_Scancode key);
    bool click();
    void mouseAt(Game& game, const glm::vec2& screen);
    void applyInput(Game& game);
    void steer(Game& game, const glm::vec2& worldDirection);

    Move moveTo(Game& game, const glm::ivec2& goal, bool run);
    void resetPath() { mPath.clear(); mPathGoal = glm::ivec2(-1); }

    // Behaviours, top priority first; each returns true when it acted.
    bool handleContainerPanel(Game& game);
    bool handlePendingUse(Game& game);
    bool treatBleeding(Game& game);
    bool handleDanger(Game& game);
    bool eatAndDrink(Game& game);
    bool armSelf(Game& game);
    bool retreatWhenHurt(Game& game);
    bool restWhenTired(Game& game);
    float fatigue(Game& game) const;
    float fitShare() const;
    bool withinReach(Game& game, const glm::ivec2& tile) const;
    bool isStocked(Game& game) const;
    double homeHour() const;
    bool isDark(Game& game) const;
    bool atHome(Game& game) const;
    float heldWeaponScore(Game& game) const;
    int bestWeaponSlot(Game& game, float* score) const;
    bool sleepAtHome(Game& game);
    bool eveningAtHome(Game& game);
    bool useTap(Game& game);
    bool searchContainers(Game& game);
    void wander(Game& game);

    // Asks for the pack item in `slot` to be used (Tab, hover, F, Tab).
    void useSlot(int slot);
    [[nodiscard]] int findSlot(Game& game, bool (*want)(const ItemDatabase&, int itemId, float condition)) const;
    [[nodiscard]] bool wantsFromContainer(Game& game, int itemId) const;
    [[nodiscard]] glm::ivec2 playerTile(Game& game) const;
    [[nodiscard]] glm::vec2 playerCentre(Game& game) const;
    [[nodiscard]] glm::ivec2 homeTile(Game& game) const;
    // A free tile to stand on to use the piece of furniture at `tile`.
    [[nodiscard]] bool standTileFor(Game& game, const glm::ivec2& tile, glm::ivec2& out) const;
    void checkStuck(Game& game);
    static double clock(const Game& game); // game hours since day 0

    bool mRunning = false;
    Profile mProfile = Profile::Normal;
    int mEndDay = 0;
    std::string mStuck;
    const char* mActivity = "idle";

    // Input bookkeeping.
    std::vector<SDL_Scancode> mWant;
    std::vector<SDL_Scancode> mHeld;
    bool mMouseDown = false;
    bool mClickNow = false;
    int mTapCooldown = 0;
    int mClickCooldown = 0;

    // Movement.
    std::vector<glm::ivec2> mPath;
    std::size_t mPathIndex = 0;
    glm::ivec2 mPathGoal{-1, -1};
    glm::ivec2 mLastTile{-1, -1};
    int mNoProgressTicks = 0;
    int mRepaths = 0;
    int mNudgeTicks = 0;
    glm::vec2 mNudgeDir{1.0f, 0.0f};
    int mDoorTicks = 0;

    // Goals.
    std::set<Tile> mSearched;
    std::set<Tile> mUnreachable;
    glm::ivec2 mTarget{-1, -1};
    Entity mTargetContainer = kInvalidEntity;
    int mOpenTries = 0;
    int mPanelTicks = 0;
    int mPanelClicks = 0;
    int mUseSlot = -1;
    int mUseTicks = 0;
    bool mUseDone = false;
    double mSleepRetryAt = -1.0;
    double mTapRetryAt = -1.0;
    double mBoardRetryAt = -1.0;
    double mTreatRetryAt = -1.0;
    double mForgetAt = 0.0;
    glm::ivec2 mWanderGoal{-1, -1};
    int mWanderTicks = 0;
    int mWanderOutdoors = 0; // > 0: the next picks may be outside home
    glm::ivec2 mFleeGoal{-1, -1};
    int mFleeTicks = 0;
    std::uint32_t mRng = 0x9E3779B9u;
    std::uint64_t mTicks = 0;
    Entity mFightTarget = kInvalidEntity;
    double mArmRetryAt = 0.0;
    std::uint64_t mCorneredUntil = 0;
    bool mResting = false;
    glm::ivec2 mFleeTile{-1, -1};
    int mFleeStillTicks = 0;
    float mFightHealth = 0.0f;
    int mFightTicks = 0;
    std::map<Entity, std::uint64_t> mIgnored; // zombie -> tick it counts again

    // Stuck detection: game time at the last tile change while awake.
    double mLastMoveHour = 0.0;
    glm::ivec2 mStuckTile{-1, -1};
};
