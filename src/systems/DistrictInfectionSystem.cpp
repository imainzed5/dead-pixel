#include "systems/DistrictInfectionSystem.h"

#include <algorithm>
#include <cmath>

#include "components/Health.h"
#include "components/Transform.h"
#include "components/ZombieAI.h"

void DistrictInfectionSystem::initialize(const LayoutData& layout)
{
    mDistricts.clear();
    mLastUpdateDay = -1;

    for (const auto& dd : layout.districts)
    {
        DistrictInfection di;
        di.districtId = dd.id;
        di.infection = 0.0f;
        di.overwhelmed = false;
        mDistricts.push_back(di);
    }
}

void DistrictInfectionSystem::dailyUpdate(World& world, const LayoutData& layout, int currentDay)
{
    if (currentDay == mLastUpdateDay) return;
    mLastUpdateDay = currentDay;

    if (mDistricts.empty()) return;

    // Count living zombies per district
    std::vector<int> zombieCounts(mDistricts.size(), 0);
    std::vector<float> districtArea(mDistricts.size(), 1.0f);

    for (std::size_t i = 0; i < layout.districts.size() && i < mDistricts.size(); ++i)
    {
        const auto& dd = layout.districts[i];
        const float w = static_cast<float>(dd.maxX - dd.minX + 1);
        const float h = static_cast<float>(dd.maxY - dd.minY + 1);
        districtArea[i] = std::max(1.0f, w * h);
    }

    world.forEach<ZombieAI, Transform, Health>(
        [&](Entity, ZombieAI& zai, Transform& t, Health& h)
        {
            if (h.current <= 0.0f) return;

            // Find which district this zombie is in
            const int tileX = static_cast<int>(t.x / 32.0f);
            const int tileY = static_cast<int>(t.y / 32.0f);
            const int idx = tileY * layout.width + tileX;

            if (idx >= 0 && idx < static_cast<int>(layout.district.size()))
            {
                const int distId = layout.district[static_cast<std::size_t>(idx)];
                for (std::size_t d = 0; d < mDistricts.size(); ++d)
                {
                    if (mDistricts[d].districtId == distId)
                    {
                        zombieCounts[d]++;
                        break;
                    }
                }
            }
        });

    // Calculate infection spread
    std::vector<float> newInfection(mDistricts.size(), 0.0f);

    for (std::size_t i = 0; i < mDistricts.size(); ++i)
    {
        // Zombie density contribution
        const float density = static_cast<float>(zombieCounts[i]) / (districtArea[i] * 0.01f);
        newInfection[i] += density * kZombieDensityWeight;

        // Adjacency spread
        if (i < layout.districts.size())
        {
            for (const int adjId : layout.districts[i].adjacentDistricts)
            {
                for (std::size_t j = 0; j < mDistricts.size(); ++j)
                {
                    if (mDistricts[j].districtId == adjId)
                    {
                        newInfection[i] += mDistricts[j].infection * kAdjacencySpreadFactor * 0.01f;
                        break;
                    }
                }
            }
        }
    }

    // Apply infection changes
    for (std::size_t i = 0; i < mDistricts.size(); ++i)
    {
        mDistricts[i].infection = std::clamp(
            mDistricts[i].infection + kBaseSpreadRate + newInfection[i],
            0.0f, 100.0f);

        if (mDistricts[i].infection >= kOverwhelmedThreshold)
        {
            mDistricts[i].overwhelmed = true;
        }
    }
}

float DistrictInfectionSystem::getInfection(int districtId) const
{
    for (const auto& di : mDistricts)
    {
        if (di.districtId == districtId)
            return di.infection;
    }
    return 0.0f;
}

bool DistrictInfectionSystem::isOverwhelmed(int districtId) const
{
    for (const auto& di : mDistricts)
    {
        if (di.districtId == districtId)
            return di.overwhelmed;
    }
    return false;
}
