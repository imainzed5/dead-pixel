#include "systems/ThreatDirectorSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "components/Health.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"
#include "systems/DistrictInfectionSystem.h"
#include "systems/ZombieAISystem.h"

namespace
{
constexpr float kSpriteSize = 32.0f;
constexpr float kTwoPi = 6.2831853f;

glm::vec2 centerOf(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}
} // namespace

void ThreatDirectorSystem::reset(std::uint32_t seed, const Tuning& tuning)
{
    mRng.seed(seed ^ 0x7E7E1234u);
    mTuning = tuning;
    mElapsed = 0.0f;
    mMigrationTimer = 0.0f;
    mSpawnTimer = 0.0f;
    mInitialPopulation = -1;
    mInitialInfection = -1.0f;
    mPopulationTarget = 0;
    mMigrationsSent = 0;
}

float ThreatDirectorSystem::uniform(float lo, float hi)
{
    return std::uniform_real_distribution<float>(lo, hi)(mRng);
}

bool ThreatDirectorSystem::findWalkableNear(const TileMap& tileMap, const glm::vec2& center,
                                            float minTiles, float maxTiles, glm::vec2& outWorld)
{
    const float tile = static_cast<float>(tileMap.tileWidth());
    for (int attempt = 0; attempt < 24; ++attempt)
    {
        const float angle = uniform(0.0f, kTwoPi);
        const float radius = uniform(minTiles, maxTiles) * tile;
        const glm::vec2 p = center + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
        const int tx = static_cast<int>(std::floor(p.x / tile));
        const int ty = static_cast<int>(std::floor(p.y / tile));
        if (tileMap.inBounds(tx, ty) && !tileMap.isSolid(tx, ty))
        {
            outWorld = glm::vec2((static_cast<float>(tx) + 0.5f) * tile, (static_cast<float>(ty) + 0.5f) * tile);
            return true;
        }
    }
    return false;
}

void ThreatDirectorSystem::update(World& world,
                                  const TileMap& tileMap,
                                  Entity playerEntity,
                                  float dtSeconds,
                                  double gameHours,
                                  int currentDay,
                                  const DistrictInfectionSystem* infectionSystem,
                                  std::vector<SpawnRequest>& outSpawns)
{
    if (playerEntity == kInvalidEntity || !world.hasComponent<Transform>(playerEntity))
    {
        return;
    }

    const float tile = static_cast<float>(tileMap.tileWidth());
    const glm::vec2 playerCenter = centerOf(world.getComponent<Transform>(playerEntity));
    const bool isNight = gameHours >= 20.0 || gameHours < 5.0;

    // Census.
    struct Candidate
    {
        Entity entity;
        float distance;
    };
    std::vector<Candidate> farWanderers;
    int alive = 0;
    int nearCount = 0;
    world.forEach<ZombieAI, Transform>(
        [&](Entity e, ZombieAI& ai, Transform& t)
        {
            if (world.hasComponent<Health>(e) && world.getComponent<Health>(e).current <= 0.0f) return;
            ++alive;
            const float distance = glm::length(centerOf(t) - playerCenter) / tile;
            if (distance <= kNearRadiusTiles)
            {
                ++nearCount;
            }
            else if (distance >= kMinSourceDistanceTiles && ai.state == ZombieState::Wander && !ai.migrating)
            {
                farWanderers.push_back({e, distance});
            }
        });

    if (mInitialPopulation < 0)
    {
        mInitialPopulation = alive;
    }

    mElapsed += dtSeconds;
    if (mElapsed < kGracePeriodSeconds)
    {
        return;
    }

    float averageInfection = 0.0f;
    if (infectionSystem != nullptr && !infectionSystem->districts().empty())
    {
        for (const auto& district : infectionSystem->districts())
        {
            averageInfection += district.infection;
        }
        averageInfection /= static_cast<float>(infectionSystem->districts().size());
    }
    if (mInitialInfection < 0.0f)
    {
        mInitialInfection = averageInfection;
    }
    const float infectionGrowth = std::max(0.0f, averageInfection - mInitialInfection);
    const float dayFactor = static_cast<float>(std::max(0, currentDay - 1));

    // --- 1. Migration ---
    mMigrationTimer += dtSeconds * mTuning.migrationScale;
    const float interval = isNight ? kNightMigrationInterval : kDayMigrationInterval;
    if (mMigrationTimer >= interval)
    {
        mMigrationTimer = 0.0f;

        // How many bodies the area "should" hold: rises at night and with days.
        const int desiredNear = static_cast<int>(isNight
            ? kDesiredNearNight + dayFactor * kDesiredNearNightGrowth
            : kDesiredNearDay + dayFactor * kDesiredNearDayGrowth);

        if (nearCount < desiredNear && !farWanderers.empty())
        {
            // Draw from the closer half of the far wanderers so arrivals feel timely.
            std::sort(farWanderers.begin(), farWanderers.end(),
                [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
            const std::size_t pool = std::max<std::size_t>(1, std::min<std::size_t>(farWanderers.size(), 10));

            const int wave = std::min(desiredNear - nearCount, isNight ? 3 : 2);
            for (int i = 0; i < wave; ++i)
            {
                const std::size_t pick = static_cast<std::size_t>(uniform(0.0f, static_cast<float>(pool) - 0.001f));
                const Entity zombie = farWanderers[pick].entity;
                ZombieAI& ai = world.getComponent<ZombieAI>(zombie);
                if (ai.migrating) continue;

                glm::vec2 target;
                if (findWalkableNear(tileMap, playerCenter, kTargetScatterMinTiles, kTargetScatterMaxTiles, target))
                {
                    ZombieAISystem::sendTo(ai, target, true);
                    ++mMigrationsSent;
                }
            }
        }
    }

    // --- 2. Population growth ---
    mPopulationTarget = static_cast<int>(
        static_cast<float>(mInitialPopulation) *
        (1.0f + dayFactor * kDailyGrowth) *
        (1.0f + infectionGrowth / 100.0f) *
        mTuning.populationScale);
    mPopulationTarget = std::min(mPopulationTarget, kMaxPopulation);

    mSpawnTimer += dtSeconds;
    if (mSpawnTimer >= kSpawnInterval && alive < mPopulationTarget)
    {
        mSpawnTimer = 0.0f;
        const int batch = std::min(3, mPopulationTarget - alive);
        for (int i = 0; i < batch; ++i)
        {
            for (int attempt = 0; attempt < 40; ++attempt)
            {
                const int tx = static_cast<int>(uniform(0.0f, static_cast<float>(tileMap.width()) - 0.001f));
                const int ty = static_cast<int>(uniform(0.0f, static_cast<float>(tileMap.height()) - 0.001f));
                if (tileMap.isSolid(tx, ty)) continue;

                const glm::vec2 center((static_cast<float>(tx) + 0.5f) * tile, (static_cast<float>(ty) + 0.5f) * tile);
                if (glm::length(center - playerCenter) < kSpawnMinDistanceTiles * tile) continue;

                // Prefer infected districts: acceptance rises with local infection.
                const int districtId = tileMap.getDistrictId(tx, ty);
                const float infection = (infectionSystem != nullptr && districtId > 0)
                    ? infectionSystem->getInfection(districtId) : 0.0f;
                if (uniform(0.0f, 1.0f) > 0.35f + infection / 100.0f) continue;

                SpawnRequest request;
                request.topLeftWorld = glm::vec2(static_cast<float>(tx) * tile, static_cast<float>(ty) * tile);
                request.health = uniform(30.0f, 70.0f);
                request.baseMoveSpeed = 112.0f * uniform(0.8f, 1.2f);
                outSpawns.push_back(request);
                break;
            }
        }
    }
}
