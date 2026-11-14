// The dead (overhaul phase 4): the game's side of kinds, senses, blood,
// hordes, threat modes and what the districts remember. The rules live in
// world/ZombieKinds, world/Senses, world/BloodTrail and world/Horde; the
// dead's own thinking in ZombieAISystem (ZombieSenses/Move/Attack.cpp).

#include "core/GameShared.h"
#include "world/WeatherRules.h"

#include "systems/InventorySystem.h"
#include "world/FirstDays.h"
#include "world/Horde.h"

#include <cmath>
#include <iostream>

using namespace gameshared;

namespace
{
glm::vec2 centreOf(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}

constexpr float kTwoPi = 6.2831853f;
// Seconds of frenzy after rain stops (The Unknown's "calm in rain").
constexpr float kRainFrenzySeconds = 120.0f;
// The Unknown: "drawn to light from 20 tiles".
constexpr float kLightDrawTiles = 20.0f;
// The follower is told where you are this often (one game hour).
constexpr float kFollowerSeconds = 60.0f;
// Decayed crowds: four or more within 3 tiles raise illness 1 a game hour.
constexpr float kDecayedTiles = 3.0f;
constexpr int kDecayedCrowd = 4;
constexpr float kDecayedIllnessPerSecond = 1.0f / 60.0f;
// A Bloated one bursts over 1.5 tiles.
constexpr float kBurstTiles = 1.5f;
constexpr float kBurstIllness = 8.0f;
constexpr float kBurstPanic = 10.0f;
constexpr float kCleanKillTiles = 2.0f;
// A kill lowers its district's infection this much.
constexpr float kKillCleansInfection = 0.15f;
// The district's infection falls this much between lives.
constexpr float kLifeCleansInfection = 10.0f;
// Kills of the last life thin the next start: half of them, at most 60 %.
constexpr float kKillsThinShare = 0.5f;
constexpr float kKillsThinCap = 0.6f;
// A rule of The Unknown counts as seen once a game hour, and is written
// down after three sightings.
constexpr float kUnknownSeenCooldown = 60.0f;
constexpr int kUnknownSeenToLearn = 3;
} // namespace

const zombiekinds::ModeRules& Game::modeRules() const
{
    return zombiekinds::activeTable().mode(std::clamp(mTitleModeIndex, 0, 5));
}

ZombieKind Game::rollKind(std::mt19937& rng, const glm::ivec2& tile) const
{
    const int district = mTileMap.inBounds(tile.x, tile.y) ? mTileMap.getDistrictId(tile.x, tile.y) : 0;
    const float infection = district > 0 ? mDistrictInfectionSystem.getInfection(district) : 0.0f;
    const auto shares = zombiekinds::shares(zombiekinds::activeTable(), modeRules(), mCurrentDay, infection);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    return zombiekinds::pick(shares, unit(rng));
}

EntityFactory::ZombieParams Game::zombieParams(ZombieKind kind, const glm::vec2& topLeft, std::mt19937& rng) const
{
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float healthRoll = unit(rng);
    const float speedRoll = unit(rng);
    const zombiekinds::Rolled rolled = zombiekinds::roll(zombiekinds::activeTable(), kind, healthRoll, speedRoll);
    EntityFactory::ZombieParams params;
    params.topLeftWorld = topLeft;
    params.kind = kind;
    params.health = rolled.health * modeRules().health *
                    toneTuning(saveTonePresetFromIndex(mTitleToneIndex)).zombieHealthScale;
    params.baseMoveSpeed = rolled.speed;
    params.spawnDay = mCurrentDay;
    return params;
}

void Game::resetDead()
{
    mBloodTrail.clear();
    mTrailLast = glm::vec2(-1.0f);
    mZombieEvents.clear();
    mPlayerBlows.clear();
    mPlayerDownTimer = 0.0f;
    mHobbleTimer = 0.0f;
    mLastBlowKind = ZombieKind::Shambler;
    mLastBlowFollower = false;
    mLastBlowBy.clear();
    mThunderTimer = 60.0f;
    mRainStoppedTimer = 0.0f;
    mWasRaining = false;
    mBruteShakeTimer = 0.0f;
    mHorde = Horde{};
    mHordeNightStarted = 0;
    mRuinLevels.clear();
    mRuinDistrictOf.clear();
    mDistrictKills.clear();
    mUnknownSeen.fill(0);
    mUnknownSeenCooldown.fill(0.0f);
    mUnknownRules = zombiekinds::unknownRules(mSaveManager.currentRunSeed());
    mFollowerTimer = 0.0f;
    mFollowerChosen = false;
    mShellSealed.clear();
    mShellTick = ~std::uint64_t{0};
    mDecayedExposure = 0.0f;
    mZombieAISystem.reset();
    // Noise is heard through a building's shell, one tier down when it is shut.
    mNoiseModel.setShellResolver(
        [this](const glm::vec2& world)
        {
            const senses::Shell shell = shellAt(worldToTile(mTileMap, world));
            return std::pair<int, bool>(shell.building, shell.sealed);
        });
}

// --- Buildings: shut or open ------------------------------------------------

senses::Shell Game::shellAt(const glm::ivec2& tile)
{
    if (!mTileMap.inBounds(tile.x, tile.y)) return senses::Shell{};
    const int building = mTileMap.getBuildingId(tile.x, tile.y);
    if (building <= 0) return senses::Shell{};
    return senses::Shell{building, buildingSealed(building)};
}

bool Game::buildingSealed(int building)
{
    if (building <= 0) return false;
    // Worked out once a tick per building, when first asked.
    if (mShellTick != mZombieAISystem.ticks())
    {
        mShellTick = mZombieAISystem.ticks();
        std::fill(mShellSealed.begin(), mShellSealed.end(), static_cast<std::int8_t>(-1));
    }
    if (static_cast<std::size_t>(building) >= mShellSealed.size())
    {
        mShellSealed.resize(static_cast<std::size_t>(building) + 1, -1);
    }
    std::int8_t& cached = mShellSealed[static_cast<std::size_t>(building)];
    if (cached >= 0) return cached == 1;

    bool sealed = true;
    // Every door of it closed (boarded or not); a broken one is a hole.
    static const glm::ivec2 kSides[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const Doors::Door& door : mDoors.all())
    {
        bool ours = mTileMap.getBuildingId(door.tile.x, door.tile.y) == building;
        for (const glm::ivec2& side : kSides)
        {
            const glm::ivec2 n = door.tile + side;
            if (mTileMap.inBounds(n.x, n.y) && mTileMap.getBuildingId(n.x, n.y) == building) ours = true;
        }
        if (!ours) continue;
        if (mTileMap.doorState(door.tile.x, door.tile.y) != DoorState::Closed && mTileMap.doorBoards(door.tile.x, door.tile.y) == 0)
        {
            sealed = false;
            break;
        }
    }
    // Every window glazed or boarded.
    if (sealed)
    {
        for (const windows::Spot& spot : mWindowSpots)
        {
            if (spot.building != building) continue;
            if (mTileMap.windowState(spot.tile.x, spot.tile.y) == WindowState::Broken &&
                mTileMap.windowBoards(spot.tile.x, spot.tile.y) == 0)
            {
                sealed = false;
                break;
            }
        }
    }
    cached = sealed ? 1 : 0;
    return sealed;
}

// --- Districts --------------------------------------------------------------

int Game::ruinLevel(int districtId) const
{
    if (districtId <= 0) return 0;
    if (static_cast<std::size_t>(districtId) < mRuinLevels.size()) return mRuinLevels[static_cast<std::size_t>(districtId)];
    const float infection = mDistrictInfectionSystem.getInfection(districtId);
    if (mDistrictInfectionSystem.isOverwhelmed(districtId) || infection >= 75.0f) return 3;
    if (infection >= 50.0f) return 2;
    if (infection >= 25.0f) return 1;
    return 0;
}

const char* Game::ruinWord(int level)
{
    switch (level)
    {
    case 1: return "uneasy";
    case 2: return "turning";
    case 3: return "overrun";
    default: return "quiet";
    }
}

void Game::refreshRuinLevels()
{
    mRuinDay = mCurrentDay;
    int maxId = 0;
    for (const DistrictInfection& d : mDistrictInfectionSystem.districts()) maxId = std::max(maxId, d.districtId);
    mRuinLevels.assign(static_cast<std::size_t>(maxId) + 1, 0);
    for (const DistrictInfection& d : mDistrictInfectionSystem.districts())
    {
        int level = 0;
        if (d.overwhelmed || d.infection >= 75.0f) level = 3;
        else if (d.infection >= 50.0f) level = 2;
        else if (d.infection >= 25.0f) level = 1;
        mRuinLevels[static_cast<std::size_t>(d.districtId)] = level;
    }
    buildRuinMarks();
}

namespace
{
std::uint32_t ruinHash(int x, int y, std::uint32_t salt)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093u ^ static_cast<std::uint32_t>(y) * 19349663u ^ salt;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}
float unit(std::uint32_t h) { return static_cast<float>(h & 0xFFFFu) / 65535.0f; }
} // namespace

void Game::buildRuinMarks()
{
    // Placed by position hash, so a district at the same level looks the
    // same each time; nothing of it is saved but the infection itself.
    constexpr int kMaxBodies = 12;
    constexpr float kLitter[4] = {0.0f, 0.025f, 0.05f, 0.08f};
    constexpr float kSmear[4] = {0.0f, 0.03f, 0.08f, 0.14f};
    constexpr int kBodies[4] = {0, 2, 6, kMaxBodies};
    const std::uint32_t salt = 0x9E3779B9u; // the same streets look the same each life
    std::vector<SceneryView::RuinMark> marks;
    const float tile = 32.0f;
    // Districts are numbered only inside their building boxes; the streets
    // between belong (for their ruin) to the nearest one. Found once a run
    // by a breadth-first fill from every numbered tile.
    const int mapW = mTileMap.width();
    const int mapH = mTileMap.height();
    if (mRuinDistrictOf.size() != static_cast<std::size_t>(mapW * mapH))
    {
        mRuinDistrictOf.assign(static_cast<std::size_t>(mapW * mapH), 0);
        std::vector<int> queue;
        queue.reserve(mRuinDistrictOf.size());
        for (int y = 0; y < mapH; ++y)
        {
            for (int x = 0; x < mapW; ++x)
            {
                const int id = mTileMap.getDistrictId(x, y);
                if (id <= 0) continue;
                mRuinDistrictOf[static_cast<std::size_t>(y * mapW + x)] = id;
                queue.push_back(y * mapW + x);
            }
        }
        for (std::size_t head = 0; head < queue.size(); ++head)
        {
            const int i = queue[head];
            const int x = i % mapW;
            const int y = i / mapW;
            const int dx[4] = {1, -1, 0, 0};
            const int dy[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; ++k)
            {
                const int nx = x + dx[k];
                const int ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= mapW || ny >= mapH) continue;
                int& to = mRuinDistrictOf[static_cast<std::size_t>(ny * mapW + nx)];
                if (to != 0) continue;
                to = mRuinDistrictOf[static_cast<std::size_t>(i)];
                queue.push_back(ny * mapW + nx);
            }
        }
    }
    std::vector<int> bodiesIn(mRuinLevels.size(), 0);
    for (int y = 1; y < mTileMap.height() - 1; ++y)
    {
        {
            for (int x = 1; x < mTileMap.width() - 1; ++x)
            {
                if (mTileMap.isSolid(x, y) || mTileMap.getBuildingId(x, y) != 0) continue;
                const int districtId = mRuinDistrictOf[static_cast<std::size_t>(y * mapW + x)];
                const int level = ruinLevel(districtId);
                if (level <= 0) continue;
                int dummy = 0;
                int& bodies = static_cast<std::size_t>(districtId) < bodiesIn.size()
                                  ? bodiesIn[static_cast<std::size_t>(districtId)] : dummy;
                const std::uint32_t h = ruinHash(x, y, salt);
                const float r = unit(h);
                const glm::vec2 centre((static_cast<float>(x) + 0.5f) * tile, (static_cast<float>(y) + 0.5f) * tile);
                const glm::vec2 jitter((unit(h >> 7) - 0.5f) * 14.0f, (unit(h >> 13) - 0.5f) * 14.0f);
                // A wall beside it (the building to the north or west).
                const bool wallN = mTileMap.getBuildingId(x, y - 1) > 0 && mTileMap.isSolid(x, y - 1);
                const bool wallW = mTileMap.getBuildingId(x - 1, y) > 0 && mTileMap.isSolid(x - 1, y);
                if ((wallN || wallW) && r < kSmear[level])
                {
                    SceneryView::RuinMark m;
                    m.position = centre + glm::vec2(wallW ? -12.0f : 0.0f, wallN ? -12.0f : 0.0f);
                    m.sprite = "fx_blood";
                    m.frame = static_cast<int>((h >> 3) & 1u);
                    m.anchor = glm::vec2(-1.0f);
                    m.alpha = 0.55f;
                    marks.push_back(m);
                    continue;
                }
                const float body = unit(h >> 9);
                if (bodies < kBodies[level] && body < 0.004f * static_cast<float>(level))
                {
                    ++bodies;
                    SceneryView::RuinMark m;
                    m.position = centre;
                    m.sprite = std::string("zombie") + std::to_string((h >> 5) % 3u) + (((h >> 2) & 1u) ? "_death_se" : "_death_ne");
                    m.frame = 5;
                    m.anchor = glm::vec2(32.0f, 47.0f);
                    marks.push_back(m);
                    continue;
                }
                if (r < kLitter[level])
                {
                    SceneryView::RuinMark m;
                    m.position = centre + jitter;
                    m.sprite = ((h >> 11) & 1u) ? "scn_paper" : "scn_can";
                    m.anchor = glm::vec2(-1.0f);
                    marks.push_back(m);
                }
            }
        }
    }
    mSceneryView.setRuin(std::move(marks));
}

void Game::drawBloodTrail()
{
    // The blood you leave: small dark drops, a pool where it gathered,
    // fading as they dry or the rain takes them.
    const SpriteRegion* blood = mSpriteSheet.find("fx_blood");
    if (blood == nullptr) return;
    const float zoom = mCamera.zoom();
    for (const bloodtrail::Marker& m : mBloodTrail.markers())
    {
        const float fresh = std::clamp(bloodtrail::Trail::freshness(m), 0.0f, 1.0f);
        if (fresh <= 0.02f) continue;
        const float px = m.pool ? 22.0f : 11.0f;
        const glm::vec2 ground = mCamera.worldToScreen(m.world);
        const glm::vec2 size(px * zoom, px * 0.6f * zoom);
        const glm::vec4 uv = mSpriteSheet.frameUv("fx_blood", static_cast<int>(m.seq & 1u));
        mSpriteBatch.draw(glm::round(ground - size * 0.5f), size, uv, glm::vec4(0.55f, 0.18f, 0.16f, 0.35f + 0.55f * fresh));
    }
}

void Game::loadDistrictMemory(bool newLife)
{
    const DistrictMemoryRecord& memory = mSaveManager.districtMemory();
    const std::uint32_t seed = mSaveManager.currentRunSeed();
    const bool firstLook = memory.appliedRunSeed != seed;

    int maxId = 0;
    for (const DistrictInfection& d : mDistrictInfectionSystem.districts()) maxId = std::max(maxId, d.districtId);
    mDistrictKills.assign(static_cast<std::size_t>(maxId) + 1, 0);

    // The districts stay turned between lives, a little burned down.
    for (const auto& [id, infection] : memory.infection)
    {
        const float carried = (newLife && firstLook) ? infection - kLifeCleansInfection : infection;
        mDistrictInfectionSystem.setInfection(id, std::max(carried, mDistrictInfectionSystem.getInfection(id)));
    }
    // Continuing this life: its kills so far.
    if (!firstLook)
    {
        for (const auto& [id, kills] : memory.kills)
        {
            if (id >= 0 && static_cast<std::size_t>(id) < mDistrictKills.size()) mDistrictKills[static_cast<std::size_t>(id)] = kills;
        }
    }
    mLearnedRules = memory.learnedRules;
    refreshRuinLevels();
    if (firstLook) saveDistrictMemory();
}

void Game::saveDistrictMemory() const
{
    DistrictMemoryRecord memory = mSaveManager.districtMemory();
    memory.infection.clear();
    for (const DistrictInfection& d : mDistrictInfectionSystem.districts())
    {
        memory.infection.emplace_back(d.districtId, d.infection);
    }
    memory.kills.clear();
    for (std::size_t id = 0; id < mDistrictKills.size(); ++id)
    {
        if (mDistrictKills[id] > 0) memory.kills.emplace_back(static_cast<int>(id), mDistrictKills[id]);
    }
    memory.appliedRunSeed = mSaveManager.currentRunSeed();
    memory.learnedRules = mLearnedRules;
    auto& save = const_cast<SaveManager&>(mSaveManager);
    save.setDistrictMemory(std::move(memory));
    save.saveDistrictMemory();
}

void Game::reduceStartingDead()
{
    // A district cleared in the last life stays thinner: half its kills
    // there come off its starting dead, at most 60 % of them.
    const DistrictMemoryRecord& memory = mSaveManager.districtMemory();
    if (memory.appliedRunSeed == mSaveManager.currentRunSeed()) return; // the same life, continued
    mStartKills.clear();
    for (const auto& [id, kills] : memory.kills)
    {
        if (id < 0) continue;
        if (static_cast<std::size_t>(id) >= mStartKills.size()) mStartKills.resize(static_cast<std::size_t>(id) + 1, 0);
        mStartKills[static_cast<std::size_t>(id)] = kills;
    }
    if (mStartKills.empty()) return;

    std::vector<int> perDistrict(mStartKills.size(), 0);
    for (const glm::ivec2& tile : mSpawnData.zombieSpawns)
    {
        const int id = mTileMap.getDistrictId(tile.x, tile.y);
        if (id >= 0 && static_cast<std::size_t>(id) < perDistrict.size()) ++perDistrict[static_cast<std::size_t>(id)];
    }
    std::vector<int> toRemove(mStartKills.size(), 0);
    for (std::size_t id = 1; id < mStartKills.size(); ++id)
    {
        const float cut = std::min(kKillsThinShare * static_cast<float>(mStartKills[id]),
                                   kKillsThinCap * static_cast<float>(perDistrict[id]));
        toRemove[id] = static_cast<int>(std::floor(cut));
    }
    std::mt19937 rng(mSaveManager.currentRunSeed() ^ 0xD15Cu);
    std::shuffle(mSpawnData.zombieSpawns.begin(), mSpawnData.zombieSpawns.end(), rng);
    std::erase_if(mSpawnData.zombieSpawns,
                  [&](const glm::ivec2& tile)
                  {
                      const int id = mTileMap.getDistrictId(tile.x, tile.y);
                      if (id <= 0 || static_cast<std::size_t>(id) >= toRemove.size()) return false;
                      int& left = toRemove[static_cast<std::size_t>(id)];
                      if (left <= 0) return false;
                      --left;
                      return true;
                  });
}

void Game::placeDormantCrawlers(std::mt19937& rng)
{
    // Three in ten Crawlers lie in wait under something, indoors.
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    mWorld.forEach<ZombieAI, Transform>(
        [&](Entity, ZombieAI& ai, Transform& t)
        {
            if (ai.kind != ZombieKind::Crawler) return;
            const glm::ivec2 at = worldToTile(mTileMap, centreOf(t));
            if (!mTileMap.isIndoors(at.x, at.y)) return;
            if (unit(rng) >= zombiekinds::kCrawlerDormantShare) return;
            // Beside a piece of furniture if there is one near.
            for (int r = 1; r <= 3; ++r)
            {
                for (int dy = -r; dy <= r; ++dy)
                {
                    for (int dx = -r; dx <= r; ++dx)
                    {
                        const glm::ivec2 n = at + glm::ivec2(dx, dy);
                        if (!mTileMap.inBounds(n.x, n.y) || !mTileMap.hasFurniture(n.x, n.y)) continue;
                        const glm::ivec2 by = n + glm::ivec2(dx == 0 ? 0 : (dx > 0 ? -1 : 1), dy == 0 ? 0 : (dy > 0 ? -1 : 1));
                        if (!mTileMap.inBounds(by.x, by.y) || mTileMap.isSolid(by.x, by.y) ||
                            mTileMap.getBuildingId(by.x, by.y) != mTileMap.getBuildingId(at.x, at.y))
                        {
                            continue;
                        }
                        t.x = (static_cast<float>(by.x) + 0.5f) * tile - kSpriteSize * 0.5f;
                        t.y = (static_cast<float>(by.y) + 0.5f) * tile - kSpriteSize * 0.5f;
                        ai.dormant = true;
                        return;
                    }
                }
            }
            ai.dormant = true;
        });
}

// --- Hordes -----------------------------------------------------------------

bool Game::isHordeNight(int night) const
{
    if (night <= 0) return false;
    return horde::isHordeNight(night, mSaveManager.currentRunSeed(), modeRules());
}

bool Game::hordeDay() const
{
    return mHorde.active || isHordeNight(mCurrentDay);
}

void Game::startHorde()
{
    if (mPlayerEntity == kInvalidEntity) return;
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    glm::ivec2 home = mFirstDays.homeBuilding > 0 ? mSpawnData.playerStart : playerTile();
    if (home.x <= 0 && home.y <= 0) home = playerTile();

    horde::RouteInput in;
    in.width = mTileMap.width();
    in.height = mTileMap.height();
    in.home = home;
    in.seed = mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(mCurrentDay * 2654435761u);
    in.walkable = [this](int x, int y) { return mTileMap.inBounds(x, y) && !mTileMap.isSolid(x, y) && !mTileMap.isIndoors(x, y); };
    in.isRoad = [this](int x, int y)
    {
        if (!mTileMap.inBounds(x, y) || mTileMap.isSolid(x, y)) return false;
        const int base = ((std::max(1, mTileMap.groundGid(x, y)) - 1) % TileGid::kPerRow) + 1;
        return base == TileGid::kAsphalt;
    };
    std::vector<glm::ivec2> route = horde::route(in);
    if (route.size() < 2) return;

    mHorde = Horde{};
    mHorde.id = mNextHordeId++;
    mHorde.night = mCurrentDay;
    mHorde.route = route;
    mHorde.active = true;
    mHordeNightStarted = mCurrentDay;

    // Gathered from the nearest of the dead to where it sets out; the rest
    // come in fresh at the edge of the map.
    const int size = horde::hordeSize(mCurrentDay);
    const glm::vec2 start = (glm::vec2(route.front()) + glm::vec2(0.5f)) * tile;
    struct Candidate
    {
        Entity entity;
        float distance;
    };
    std::vector<Candidate> candidates;
    mWorld.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI& ai, Transform& t)
        {
            if (ai.horde != 0 || ai.follower || ai.nightCaller || ai.dormant || mWorld.hasComponent<LegacyLink>(e)) return;
            if (ai.state == ZombieState::Chase || ai.state == ZombieState::Attack) return;
            if (mWorld.hasComponent<Health>(e) && mWorld.getComponent<Health>(e).current <= 0.0f) return;
            const float d = glm::length(centreOf(t) - start) / tile;
            if (d < 30.0f) candidates.push_back({e, d});
        });
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });

    const glm::vec2 firstLeg = (glm::vec2(route[std::min<std::size_t>(1, route.size() - 1)]) + glm::vec2(0.5f)) * tile;
    int members = 0;
    for (const Candidate& c : candidates)
    {
        if (members >= size) break;
        ZombieAI& ai = mWorld.getComponent<ZombieAI>(c.entity);
        ai.horde = mHorde.id;
        ai.hordeLeg = 0;
        ZombieAISystem::sendTo(ai, start, true);
        ++members;
    }
    std::mt19937 rng(in.seed ^ 0x40D3u);
    std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
    for (int attempt = 0; members < size && attempt < size * 10; ++attempt)
    {
        const glm::ivec2 at = route.front() + glm::ivec2(static_cast<int>(unit(rng) * 3.0f), static_cast<int>(unit(rng) * 3.0f));
        if (!mTileMap.inBounds(at.x, at.y) || mTileMap.isSolid(at.x, at.y)) continue;
        const glm::vec2 topLeft = (glm::vec2(at) + glm::vec2(0.5f)) * tile - glm::vec2(kSpriteSize * 0.5f);
        const Entity zombie = EntityFactory::spawnZombie(mWorld, mSpriteSheet, zombieParams(rollKind(rng, at), topLeft, rng));
        ZombieAI& ai = mWorld.getComponent<ZombieAI>(zombie);
        ai.horde = mHorde.id;
        ai.hordeLeg = 1;
        ZombieAISystem::sendTo(ai, firstLeg, true);
        ++members;
    }
    mHorde.members = members;
    std::cout << "[Horde] night " << mCurrentDay << ": " << members << " set out, " << route.size() << " waypoints\n";
}

void Game::updateHordes(float dtSeconds)
{
    // It sets out at nine on a horde night.
    if (!mHorde.active && mHordeNightStarted != mCurrentDay && isHordeNight(mCurrentDay) &&
        mGameHours >= horde::kSetOutHour)
    {
        startHorde();
        mHordeNightStarted = mCurrentDay;
    }
    if (!mHorde.active) return;

    mHorde.checkTimer -= dtSeconds;
    if (mHorde.checkTimer > 0.0f) return;
    mHorde.checkTimer = 0.5f;

    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const int lastLeg = static_cast<int>(mHorde.route.size()) - 1;

    // A light in an uncovered window draws the ones passing near it.
    std::vector<glm::vec2> litWindows;
    for (const windows::Spot& spot : mWindowSpots)
    {
        if (!mTileMap.windowClear(spot.tile.x, spot.tile.y) || mTileMap.windowCovered(spot.tile.x, spot.tile.y)) continue;
        if (!nightNow() || !buildingLit(spot.building)) continue;
        litWindows.push_back((glm::vec2(spot.outside) + glm::vec2(0.5f)) * tile);
    }

    int alive = 0;
    glm::vec2 sum(0.0f);
    std::vector<Entity> atEnd;
    mWorld.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI& ai, Transform& t)
        {
            if (ai.horde != mHorde.id) return;
            if (mWorld.hasComponent<Health>(e) && mWorld.getComponent<Health>(e).current <= 0.0f) return;
            const glm::vec2 c = centreOf(t);
            for (const glm::vec2& window : litWindows)
            {
                if (glm::length(window - c) > 12.0f * tile) continue;
                ai.horde = 0;
                ZombieAISystem::sendTo(ai, window, false);
                return;
            }
            ++alive;
            sum += c;
            const glm::vec2 goal = (glm::vec2(mHorde.route[static_cast<std::size_t>(std::clamp(ai.hordeLeg, 0, lastLeg))]) +
                                    glm::vec2(0.5f)) * tile;
            const bool there = glm::length(goal - c) < 2.0f * tile ||
                               (ai.state != ZombieState::Investigate && ai.state != ZombieState::Chase &&
                                ai.state != ZombieState::Attack);
            if (!there) return;
            if (ai.hordeLeg >= lastLeg)
            {
                atEnd.push_back(e);
                return;
            }
            ++ai.hordeLeg;
            const glm::vec2 next = (glm::vec2(mHorde.route[static_cast<std::size_t>(ai.hordeLeg)]) + glm::vec2(0.5f)) * tile;
            ZombieAISystem::sendTo(ai, next, true);
        });

    // At the far edge (or by dawn) it breaks up into groups where it is,
    // and that district turns a little.
    const bool dawn = mGameHours >= weatherrules::dawnHour(mCurrentDay) && mGameHours < 12.0 && mCurrentDay > mHorde.night;
    if (alive == 0 || dawn || static_cast<int>(atEnd.size()) * 2 >= alive)
    {
        const glm::vec2 centre = alive > 0 ? sum / static_cast<float>(alive)
                                           : (glm::vec2(mHorde.route.back()) + glm::vec2(0.5f)) * tile;
        const glm::ivec2 endTile = worldToTile(mTileMap, centre);
        const int district = mTileMap.inBounds(endTile.x, endTile.y) ? mTileMap.getDistrictId(endTile.x, endTile.y) : 0;
        if (district > 0)
        {
            mDistrictInfectionSystem.setInfection(district,
                                                  mDistrictInfectionSystem.getInfection(district) + horde::kInfectionRise);
        }
        mWorld.forEach<ZombieAI>(
            [&](Entity, ZombieAI& ai)
            {
                if (ai.horde != mHorde.id) return;
                ai.horde = 0;
                ai.hordeLeg = 0;
                if (ai.state == ZombieState::Investigate)
                {
                    ai.state = ZombieState::Wander;
                    ai.migrating = false;
                    ai.path.clear();
                    ai.stateTimer = 0.0f;
                }
            });
        std::cout << "[Horde] dispersed (" << alive << " left) in district " << district << "\n";
        mHorde.active = false;
    }
}

bool Game::nightNow() const
{
    return weatherrules::isNight(mCurrentDay, mGameHours);
}

// --- The Stalker's follower -------------------------------------------------

void Game::updateFollower()
{
    if (!modeRules().follower || mPlayerEntity == kInvalidEntity) return;
    if (mFollowerTimer > 0.0f) return;
    mFollowerTimer = kFollowerSeconds;

    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 player = playerWorldCenter();
    Entity follower = kInvalidEntity;
    mWorld.forEach<ZombieAI, Health>(
        [&](Entity e, ZombieAI& ai, Health& h)
        {
            if (ai.follower && h.current > 0.0f) follower = e;
        });
    if (follower == kInvalidEntity)
    {
        // One per run: the first of the dead that is not near (it is
        // someone; the look is how you know it again).
        float best = 1.0e9f;
        mWorld.forEach<ZombieAI, Transform>(
            [&](Entity e, ZombieAI& ai, Transform& t)
            {
                if (ai.horde != 0 || ai.dormant || ai.kind != ZombieKind::Shambler || mWorld.hasComponent<LegacyLink>(e)) return;
                if (mWorld.hasComponent<Health>(e) && mWorld.getComponent<Health>(e).current <= 0.0f) return;
                const float d = glm::length(centreOf(t) - player) / tile;
                if (d < 25.0f) return;
                if (d < best)
                {
                    best = d;
                    follower = e;
                }
            });
        if (follower == kInvalidEntity) return;
        ZombieAI& chosen = mWorld.getComponent<ZombieAI>(follower);
        // Once a run, or it would be a fresh one every time it dies.
        if (mFollowerChosen) return;
        mFollowerChosen = true;
        chosen.follower = true;
        // A great deal tougher than the rest: it keeps coming.
        if (mWorld.hasComponent<Health>(follower))
        {
            Health& h = mWorld.getComponent<Health>(follower);
            h.maximum = std::max(h.maximum, 90.0f);
            h.current = h.maximum;
        }
    }
    ZombieAI& ai = mWorld.getComponent<ZombieAI>(follower);
    if (ai.state == ZombieState::Chase || ai.state == ZombieState::Attack) return;
    // By day it knows the district you are in; at night it comes to you.
    glm::vec2 target = player;
    if (!nightNow())
    {
        std::mt19937 rng(mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(mGameHours * 10.0));
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        const float angle = unit(rng) * kTwoPi;
        const float distance = (6.0f + unit(rng) * 8.0f) * tile;
        const glm::vec2 near = player + glm::vec2(std::cos(angle), std::sin(angle)) * distance;
        const glm::ivec2 at = worldToTile(mTileMap, near);
        if (mTileMap.inBounds(at.x, at.y) && !mTileMap.isSolid(at.x, at.y)) target = near;
    }
    ZombieAISystem::sendTo(ai, target, true);
}

// --- Blood ------------------------------------------------------------------

void Game::updateBloodTrail(float dtSeconds)
{
    const bool raining = weatherrules::rainy(mWeatherState.condition);
    mBloodTrail.update(dtSeconds, raining, modeRules().trailScale, weatherrules::bloodOutdoorSeconds(mWeatherState.condition));
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity)) return;

    // Bleeding, or an undressed cut or bite: drops as you walk.
    float strength = 0.0f;
    if (mWorld.hasComponent<Bleeding>(mPlayerEntity)) strength = bloodtrail::kBleedingStrength;
    else if (mWorld.hasComponent<Wounds>(mPlayerEntity))
    {
        for (const Wound& wound : mWorld.getComponent<Wounds>(mPlayerEntity).active)
        {
            // A dressed wound stays on the body (phase 5) but drips no more.
            if ((wound.type == WoundType::Cut || wound.type == WoundType::Bite) && !wound.dressed)
            {
                strength = bloodtrail::kWoundStrength;
                break;
            }
        }
    }
    const glm::vec2 at = playerWorldCenter();
    if (strength <= 0.0f)
    {
        mTrailLast = glm::vec2(-1.0f);
        return;
    }
    if (mTrailLast.x < 0.0f) mTrailLast = at;
    const glm::ivec2 tile = worldToTile(mTileMap, at);
    bloodtrail::Place place = bloodtrail::Place::Outdoor;
    if (mTileMap.inBounds(tile.x, tile.y))
    {
        const int base = ((std::max(1, mTileMap.groundGid(tile.x, tile.y)) - 1) % TileGid::kPerRow) + 1;
        if (base == TileGid::kWater) place = bloodtrail::Place::Water;
        else if (mTileMap.isIndoors(tile.x, tile.y)) place = bloodtrail::Place::Indoor;
    }
    mBloodTrail.walk(mTrailLast, at, strength, place, static_cast<float>(mTileMap.tileWidth()));
    mTrailLast = at;
}

// --- What the dead did this tick --------------------------------------------

void Game::handleZombieEvents()
{
    const glm::vec2 player = playerWorldCenter();
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    for (const ZombieEvent& event : mZombieEvents)
    {
        const float distanceTiles = glm::length(event.position - player) / tile;
        const float near = std::clamp(1.0f - distanceTiles / 30.0f, 0.15f, 1.0f);
        switch (event.type)
        {
        case ZombieEvent::Type::Scream:
            mAudioManager.playSound(SoundId::ZombieGrowl, std::min(1.0f, 0.9f * near + 0.2f));
            if (distanceTiles < 12.0f) mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.15f);
            // A scream close by goes straight through you (phase 5).
            if (distanceTiles < mindrules::kScreamerPanicTiles) addPanic(mindrules::kScreamerPanic);
            break;
        case ZombieEvent::Type::Scrape:
            if (distanceTiles < 8.0f) mAudioManager.playSound(SoundId::FootstepSoft, 0.5f);
            break;
        case ZombieEvent::Type::StartRun:
            if (distanceTiles < 20.0f) mAudioManager.playSound(SoundId::ZombieGrowl, 0.55f * near);
            break;
        case ZombieEvent::Type::Groan:
            if (distanceTiles < 15.0f) mAudioManager.playSound(SoundId::ZombieGrowl, 0.25f * near);
            break;
        case ZombieEvent::Type::UnknownSeen:
        {
            const std::size_t rule = static_cast<std::size_t>(std::clamp(event.rule, 0, zombiekinds::kUnknownRuleCount - 1));
            if (mUnknownSeenCooldown[rule] > 0.0f) break;
            mUnknownSeenCooldown[rule] = kUnknownSeenCooldown;
            ++mUnknownSeen[rule];
            const std::string key = zombiekinds::unknownRuleKey(static_cast<zombiekinds::UnknownRule>(rule));
            if (mUnknownSeen[rule] >= kUnknownSeenToLearn &&
                std::find(mLearnedRules.begin(), mLearnedRules.end(), key) == mLearnedRules.end())
            {
                mLearnedRules.push_back(key);
                mInteractionMessage = std::string("You write it down: ") +
                                      zombiekinds::unknownRuleLine(static_cast<zombiekinds::UnknownRule>(rule));
                mInteractionMessageTimer = 5.0f;
                saveDistrictMemory();
            }
            break;
        }
        }
    }
    mZombieEvents.clear();
}

void Game::noticeUnknownRules()
{
    // The Unknown: what the player can see each rule doing, checked once a
    // second. Each sighting counts toward writing the rule down (3, a minute
    // apart). DrawnToLight and IgnoreStill are noticed where they act.
    if (modeRules().unknownRules <= 0 || mPlayerEntity == kInvalidEntity) return;
    if (mZombieAISystem.ticks() % 60 != 0) return;
    const auto has = [&](zombiekinds::UnknownRule rule) { return mUnknownRules.first == rule || mUnknownRules.second == rule; };
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 player = playerWorldCenter();
    const bool raining = weatherrules::rainy(mWeatherState.condition);
    const bool night = nightNow();
    int nearCalm = 0;
    int nearHunting = 0;
    int nearListening = 0;
    mWorld.forEach<ZombieAI, Transform>(
        [&](Entity, ZombieAI& ai, Transform& t)
        {
            if (ai.dormant || ai.horde != 0) return;
            const float d = glm::length(centreOf(t) - player) / tile;
            if (d > 10.0f) return;
            const bool hunting = ai.state == ZombieState::Chase || ai.state == ZombieState::Attack;
            if (hunting) ++nearHunting;
            else if (ai.state == ZombieState::Investigate && !ai.seesPrey) ++nearListening;
            else if (d < 6.0f) ++nearCalm;
        });
    const auto seen = [&](zombiekinds::UnknownRule rule)
    {
        mZombieEvents.push_back(ZombieEvent{ZombieEvent::Type::UnknownSeen, kInvalidEntity, player, static_cast<int>(rule)});
    };
    using zombiekinds::UnknownRule;
    // Standing still close by in daylight; or running you down at night.
    if (has(UnknownRule::DormantByDay) && ((!night && nearCalm > 0) || (night && nearHunting > 0)))
        seen(UnknownRule::DormantByDay);
    // Coming to what they heard, not what they saw.
    if (has(UnknownRule::BlindHearing) && nearListening > 0) seen(UnknownRule::BlindHearing);
    // Quiet beside you in the rain; on you once it stops.
    if (has(UnknownRule::RainCalm) && ((raining && nearCalm > 0) || (mRainStoppedTimer > 0.0f && nearHunting > 0)))
        seen(UnknownRule::RainCalm);
}

std::string Game::zombieDeathCause()
{
    // What did it, so a death reads differently by mode: a crowd that closed
    // in, the one that followed you, or a kind of its own. Every phrase keeps
    // a word the legacy reads as "will rise" (zombie, bitten, overrun).
    // A person's blow last (phase 8): they killed you, and you stay down.
    if (!mLastBlowBy.empty()) return "Killed by " + mLastBlowBy;
    int near = 0;
    const glm::vec2 player = playerWorldCenter();
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    mWorld.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI&, Transform& t)
        {
            if (mWorld.hasComponent<Health>(e) && mWorld.getComponent<Health>(e).current <= 0.0f) return;
            if (glm::length(centreOf(t) - player) <= 3.0f * tile) ++near;
        });
    if (mLastBlowFollower) return "Hunted down by the zombie that followed";
    if (near >= 5) return "Overrun by the dead";
    switch (mLastBlowKind)
    {
    case ZombieKind::Runner: return "Run down by a zombie";
    case ZombieKind::Brute: return "Beaten down by a big zombie";
    case ZombieKind::Crawler: return "Bitten by a crawling zombie";
    default: return "Killed by zombies";
    }
}

void Game::applyPlayerBlows()
{
    if (mPlayerBlows.empty()) return;
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Velocity>(mPlayerEntity) ||
        !mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        mPlayerBlows.clear();
        return;
    }
    // A blow that lands shoves you, shakes the view and throws blood the way
    // it went; a Brute's puts you on the floor, a Crawler's takes your leg.
    Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
    const glm::vec2 centre = playerWorldCenter();
    const bool pinned = playerHeld(); // a bite while held does not throw you clear
    for (const PlayerBlow& blow : mPlayerBlows)
    {
        mLastBlowBy.clear();
        mLastBlowKind = blow.kind;
        mLastBlowFollower = blow.follower;
        const float push = blow.knockdownSeconds > 0.0f ? 340.0f : 230.0f;
        if (!pinned)
        {
            vel.dx += blow.direction.x * push;
            vel.dy += blow.direction.y * push;
        }
        mParticleSystem.spawnBloodSpray(centre, blow.direction, 9);
        if (blow.knockdownSeconds > 0.0f)
        {
            mPlayerDownTimer = std::max(mPlayerDownTimer, blow.knockdownSeconds);
            mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.3f);
            // Knocked down carrying more than you can: a fall that can break a leg (phase 5).
            if (mWorld.hasComponent<Inventory>(mPlayerEntity) && mWorld.hasComponent<Wounds>(mPlayerEntity) &&
                InventorySystem::totalWeight(mWorld.getComponent<Inventory>(mPlayerEntity), mItemDatabase) > playerCarryLimit() &&
                bodyRoll() < woundrules::kOverloadedFallFractureChance)
            {
                mWorld.getComponent<Wounds>(mPlayerEntity).active.push_back(
                    woundrules::make(WoundType::Fracture, woundrules::legPart(bodyRoll())));
                mInteractionMessage = "You land badly under the weight. Something snaps.";
                mInteractionMessageTimer = 3.0f;
            }
        }
        if (blow.hobbleSeconds > 0.0f) mHobbleTimer = std::max(mHobbleTimer, blow.hobbleSeconds);
    }
    mPlayerHurtTimer = 0.24f;
    mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.5f);
    mPlayerBlows.clear();
}

void Game::onZombieDeath(Entity zombie)
{
    if (!mWorld.hasComponent<ZombieAI>(zombie) || !mWorld.hasComponent<Transform>(zombie)) return;
    const ZombieAI& ai = mWorld.getComponent<ZombieAI>(zombie);
    const glm::vec2 at = centreOf(mWorld.getComponent<Transform>(zombie));
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::ivec2 t = worldToTile(mTileMap, at);

    // The district it fell in turns back a little.
    const int district = mTileMap.inBounds(t.x, t.y) ? mTileMap.getDistrictId(t.x, t.y) : 0;
    if (district > 0)
    {
        if (static_cast<std::size_t>(district) >= mDistrictKills.size()) mDistrictKills.resize(static_cast<std::size_t>(district) + 1, 0);
        ++mDistrictKills[static_cast<std::size_t>(district)];
        mDistrictInfectionSystem.setInfection(district, mDistrictInfectionSystem.getInfection(district) - kKillCleansInfection);
    }

    // A blade leaves a pool that smells for a while; a club does not.
    if (ai.lastBlow == static_cast<int>(WeaponCategory::Bladed))
    {
        bloodtrail::Place place = bloodtrail::Place::Outdoor;
        if (mTileMap.inBounds(t.x, t.y) && mTileMap.isIndoors(t.x, t.y)) place = bloodtrail::Place::Indoor;
        mBloodTrail.addMarker(at, bloodtrail::kPoolStrength, place, true);
    }

    // A Bloated one bursts, unless burned or killed from two tiles off.
    if (ai.decomp == DecompStage::Bloated && mPlayerEntity != kInvalidEntity)
    {
        const bool burned = ai.burnedLast;
        const float playerTiles = glm::length(playerWorldCenter() - at) / tile;
        mParticleSystem.spawnDustRing(at, 40.0f, 18);
        if (!burned && playerTiles < std::max(kBurstTiles, kCleanKillTiles) && playerTiles < kBurstTiles)
        {
            if (mWorld.hasComponent<Needs>(mPlayerEntity))
            {
                Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
                needs.illness = std::min(100.0f, needs.illness + kBurstIllness);
            }
            if (mWorld.hasComponent<MentalState>(mPlayerEntity))
            {
                MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
                mental.panic = std::min(100.0f, mental.panic + kBurstPanic);
            }
            mInteractionMessage = "It bursts. The smell gets into your mouth.";
            mInteractionMessageTimer = 3.0f;
        }
    }
}

float Game::barrierDamageScale(Entity zombie) const
{
    float scale = zombiekinds::doorDamageScale(modeRules(), mCurrentDay);
    if (mWorld.hasComponent<ZombieAI>(zombie)) scale *= mWorld.getComponent<ZombieAI>(zombie).doorDamage;
    return scale;
}

// --- Corpses ----------------------------------------------------------------

bool Game::searchNearestCorpse()
{
    if (mPlayerEntity == kInvalidEntity) return false;
    const glm::vec2 player = playerWorldCenter();
    Entity best = kInvalidEntity;
    float bestDistance = 40.0f; // within reach
    mWorld.forEach<Corpse, Transform>(
        [&](Entity e, Corpse&, Transform& t)
        {
            const float d = glm::length(centreOf(t) - player);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = e;
            }
        });
    if (best == kInvalidEntity) return false;

    // Giving up: you cannot bring yourself to (phase 5).
    if (mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        !mindrules::canLootBodies(mindrules::depressionBand(mWorld.getComponent<MentalState>(mPlayerEntity).depression)))
    {
        mInteractionMessage = "You cannot bring yourself to touch them.";
        mInteractionMessageTimer = 2.5f;
        return true;
    }
    Corpse& corpse = mWorld.getComponent<Corpse>(best);
    if (!corpse.searched)
    {
        // Rolled once, the first time its pockets are turned out.
        corpse.searched = true;
        Inventory pockets{};
        pockets.maxSlots = 6;
        std::vector<std::pair<int, int>> found;
        std::mt19937 rng(mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(best * 2654435761u) ^
                         static_cast<std::uint32_t>(mCurrentDay * 7919));
        mLoot.roll("corpse", rng, toneTuning(saveTonePresetFromIndex(mTitleToneIndex)).suppliesScale, found);
        for (const auto& [id, count] : found) InventorySystem::addItem(pockets, mItemDatabase, id, count);
        mWorld.addComponent<Inventory>(best, pockets);
    }
    if (!mWorld.hasComponent<Inventory>(best)) return false;
    const Inventory& pockets = mWorld.getComponent<Inventory>(best);
    bool any = false;
    for (const InventorySlot& slot : pockets.slots) any = any || (slot.itemId >= 0 && slot.count > 0);
    if (!any)
    {
        mInteractionMessage = "Nothing on them.";
        mInteractionMessageTimer = 2.5f;
        return true;
    }
    openContainer(best);
    return true;
}

// --- The tick ---------------------------------------------------------------

void Game::runZombieAI(float feelDt)
{
    const zombiekinds::ModeRules& mode = modeRules();
    ZombieTick tick;
    tick.tileMap = &mTileMap;
    tick.noise = &mNoiseModel;
    tick.player = mPlayerEntity;
    tick.dt = feelDt;
    tick.hours = mGameHours;
    tick.day = mCurrentDay;
    tick.infection = &mDistrictInfectionSystem;
    tick.blows = &mDoorBlows;
    tick.playerBlows = &mPlayerBlows;
    tick.events = &mZombieEvents;

    if (mPlayerEntity != kInvalidEntity)
    {
        const glm::ivec2 here = playerTile();
        const int building = mTileMap.inBounds(here.x, here.y) ? mTileMap.getBuildingId(here.x, here.y) : 0;
        // Lit: the flashlight, or beside a fire, or a light in the room.
        tick.playerLit = mFlashlightOn || (building > 0 && buildingLit(building));
        if (!tick.playerLit) tick.playerLit = !mFireSystem.burningNear(here, 3).empty();
        if (mWorld.hasComponent<Facing>(mPlayerEntity)) tick.playerCrouched = mWorld.getComponent<Facing>(mPlayerEntity).crouching;
        if (mWorld.hasComponent<Velocity>(mPlayerEntity))
        {
            const Velocity& v = mWorld.getComponent<Velocity>(mPlayerEntity);
            tick.playerMoving = v.dx * v.dx + v.dy * v.dy > 4.0f;
        }
    }
    tick.playerDown = playerKnockedDown();

    // The weather, as the rules read it (phase 7). A lightning flash shows
    // the dead everything for a moment.
    const WeatherCondition weather = mWeatherState.condition;
    tick.raining = weatherrules::rainy(weather);
    tick.hearingScale = weatherrules::noiseScale(weather);
    tick.visibility = weatherrules::deadSightScale(weather, mLightningFlash > 0.0f);
    tick.weatherSpeed = weatherrules::deadMoveScale(weather);
    tick.nightSpeed = weatherrules::deadNightScale(mWeatherState.season);
    tick.snowCover = mWeatherState.snowCover;
    // The wind turns slowly over the day; which way it blows is the run's.
    tick.windTo = glm::vec2(mWeatherState.windX, mWeatherState.windY);
    tick.windStrength = mWeatherState.windStrength;

    tick.speedScale = mode.speed;
    tick.giveUpSeconds = mode.giveUpSeconds;
    tick.awarenessDrains = mode.awarenessDrains;
    tick.clumpTiles = mode.clumpTiles;
    if (mode.unknownRules > 0)
    {
        const auto has = [&](zombiekinds::UnknownRule rule) { return mUnknownRules.first == rule || mUnknownRules.second == rule; };
        tick.dormantByDay = has(zombiekinds::UnknownRule::DormantByDay);
        tick.blindHearing = has(zombiekinds::UnknownRule::BlindHearing);
        tick.rainCalm = has(zombiekinds::UnknownRule::RainCalm);
        tick.frenzyAfterRain = tick.rainCalm && mRainStoppedTimer > 0.0f;
        tick.ignoreStill = has(zombiekinds::UnknownRule::IgnoreStill);
    }
    tick.trail = &mBloodTrail;
    tick.shellAt = [this](const glm::ivec2& tile) { return shellAt(tile); };

    mZombieAISystem.update(mWorld, tick);
}

void Game::updateDead(float dtSeconds, float feelDt)
{
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));

    // Your own state from what the dead did to you.
    mPlayerDownTimer = std::max(0.0f, mPlayerDownTimer - feelDt);
    mHobbleTimer = std::max(0.0f, mHobbleTimer - dtSeconds);
    mFollowerTimer = std::max(0.0f, mFollowerTimer - dtSeconds);
    for (float& cooldown : mUnknownSeenCooldown) cooldown = std::max(0.0f, cooldown - dtSeconds);
    // Each dawn the districts show how far they have gone.
    if (mCurrentDay != mRuinDay && mGameHours >= 6.0) refreshRuinLevels();

    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Velocity>(mPlayerEntity))
    {
        Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
        if (playerKnockedDown())
        {
            vel.dx = 0.0f;
            vel.dy = 0.0f;
        }
        else if (mHobbleTimer > 0.0f)
        {
            vel.dx *= 1.0f - zombiekinds::kCrawlerHobbleSlow;
            vel.dy *= 1.0f - zombiekinds::kCrawlerHobbleSlow;
        }
        // Wading through the skeletal: half speed; a decayed crowd makes you ill.
        const glm::vec2 player = playerWorldCenter();
        bool throughBones = false;
        int decayed = 0;
        mZombieAISystem.grid().query(player, kDecayedTiles * tile, SpatialGrid::Zombie,
                                     [&](const SpatialGrid::Item& item)
                                     {
                                         const Entity e = item.entity;
                                         const glm::vec2 at = item.position;
                                         if (!mWorld.hasComponent<ZombieAI>(e)) return;
                                         const ZombieAI& ai = mWorld.getComponent<ZombieAI>(e);
                                         if (ai.decomp == DecompStage::Skeletal && glm::length(at - player) < 0.6f * tile)
                                         {
                                             throughBones = true;
                                         }
                                         if (ai.decomp == DecompStage::Decayed) ++decayed;
                                     });
        if (throughBones)
        {
            vel.dx *= 0.5f;
            vel.dy *= 0.5f;
        }
        if (decayed >= kDecayedCrowd && mWorld.hasComponent<Needs>(mPlayerEntity))
        {
            Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
            needs.illness = std::min(100.0f, needs.illness + kDecayedIllnessPerSecond * dtSeconds);
            mDecayedExposure += dtSeconds;
        }
    }

    // Rain stopping (The Unknown's calm-in-rain rule turns to frenzy).
    const bool raining = weatherrules::rainy(mWeatherState.condition);
    if (mWasRaining && !raining) mRainStoppedTimer = kRainFrenzySeconds;
    mRainStoppedTimer = std::max(0.0f, mRainStoppedTimer - dtSeconds);
    mWasRaining = raining;

    // The Unknown: drawn to light from 20 tiles.
    const zombiekinds::ModeRules& mode = modeRules();
    if (mode.unknownRules > 0 && mPlayerEntity != kInvalidEntity &&
        (mUnknownRules.first == zombiekinds::UnknownRule::DrawnToLight ||
         mUnknownRules.second == zombiekinds::UnknownRule::DrawnToLight) &&
        nightNow() && mFlashlightOn && mZombieAISystem.ticks() % 120 == 0)
    {
        const glm::vec2 player = playerWorldCenter();
        bool drew = false;
        mWorld.forEach<ZombieAI, Transform>(
            [&](Entity, ZombieAI& ai, Transform& t)
            {
                if (ai.state != ZombieState::Wander || ai.horde != 0 || ai.dormant) return;
                const glm::vec2 c = centreOf(t);
                if (glm::length(c - player) > kLightDrawTiles * tile) return;
                ZombieAISystem::sendTo(ai, player, false);
                drew = true;
            });
        if (drew)
        {
            mZombieEvents.push_back(ZombieEvent{ZombieEvent::Type::UnknownSeen, kInvalidEntity, player,
                                                static_cast<int>(zombiekinds::UnknownRule::DrawnToLight)});
        }
    }

    runZombieAI(feelDt);
    noticeUnknownRules();
    applyPlayerBlows();
    handleZombieEvents();
    updateHordes(dtSeconds);
    updateFollower();
    updateBloodTrail(dtSeconds);

    // A Brute nearby shakes the ground as it comes.
    mBruteShakeTimer = std::max(0.0f, mBruteShakeTimer - dtSeconds);
    if (mBruteShakeTimer <= 0.0f && mPlayerEntity != kInvalidEntity)
    {
        mBruteShakeTimer = 0.45f;
        const glm::vec2 player = playerWorldCenter();
        bool near = false;
        mZombieAISystem.grid().query(player, zombiekinds::kBruteShakeTiles * tile, SpatialGrid::Zombie,
                                     [&](const SpatialGrid::Item& item)
                                     {
                                         const Entity e = item.entity;
                                         if (!mWorld.hasComponent<ZombieAI>(e) || !mWorld.hasComponent<Velocity>(e)) return;
                                         const ZombieAI& ai = mWorld.getComponent<ZombieAI>(e);
                                         const Velocity& v = mWorld.getComponent<Velocity>(e);
                                         if (ai.kind == ZombieKind::Brute && v.dx * v.dx + v.dy * v.dy > 100.0f) near = true;
                                     });
        if (near) mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.08f);
    }
}

// --- Checkpoints ------------------------------------------------------------

void Game::recordTheDead(RunCheckpoint& cp)
{
    // The walking dead (not the risen: the legacy keeps those), the bodies
    // (searched ones keep what is left on them) and the blood on the ground.
    cp.zombiesSaved = true;
    mWorld.forEach<ZombieAI, Transform, Health>(
        [&](Entity zombie, ZombieAI& ai, Transform& transform, Health& health)
        {
            if (health.current <= 0.0f || mWorld.hasComponent<LegacyLink>(zombie)) return;
            ZombieRecord record;
            record.x = transform.x;
            record.y = transform.y;
            record.health = health.current;
            record.maxHealth = health.maximum;
            record.baseMoveSpeed = ai.baseMoveSpeed;
            record.spawnDay = ai.spawnDay;
            record.kind = static_cast<int>(ai.kind);
            record.dormant = ai.dormant;
            record.follower = ai.follower;
            cp.zombies.push_back(record);
        });
    mWorld.forEach<Corpse, Transform, Sprite>(
        [&](Entity body, Corpse& corpse, Transform& transform, Sprite& sprite)
        {
            CorpseRecord record;
            record.x = transform.x;
            record.y = transform.y;
            record.flipX = sprite.flipX;
            record.look = corpse.look;
            record.dir = corpse.dir;
            record.kind = static_cast<int>(corpse.kind);
            record.searched = corpse.searched;
            if (corpse.searched && mWorld.hasComponent<Inventory>(body))
            {
                for (const InventorySlot& slot : mWorld.getComponent<Inventory>(body).slots)
                {
                    if (slot.itemId >= 0 && slot.count > 0) record.items.emplace_back(slot.itemId, slot.count);
                }
            }
            cp.corpses.push_back(record);
        });
    for (const bloodtrail::Marker& m : mBloodTrail.markers())
    {
        cp.blood.push_back(BloodMarkerRecord{m.world.x, m.world.y, m.strength, m.age, m.life,
                                             static_cast<int>(m.place), m.pool, m.seq});
    }
}
