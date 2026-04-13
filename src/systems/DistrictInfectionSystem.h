#pragma once

#include <vector>

#include "ecs/World.h"
#include "map/LayoutData.h"

struct DistrictInfection
{
    int districtId = 0;
    float infection = 0.0f;   // 0-100 percentage
    bool overwhelmed = false;  // true once infection >= threshold
};

class DistrictInfectionSystem
{
public:
    void initialize(const LayoutData& layout);
    void dailyUpdate(World& world, const LayoutData& layout, int currentDay);

    [[nodiscard]] float getInfection(int districtId) const;
    [[nodiscard]] bool isOverwhelmed(int districtId) const;
    [[nodiscard]] const std::vector<DistrictInfection>& districts() const { return mDistricts; }

private:
    static constexpr float kOverwhelmedThreshold = 75.0f;
    static constexpr float kBaseSpreadRate = 2.0f;        // per-day base spread
    static constexpr float kAdjacencySpreadFactor = 0.3f;  // fraction of adjacent infection that spreads
    static constexpr float kZombieDensityWeight = 5.0f;    // infection gain per zombie density unit

    std::vector<DistrictInfection> mDistricts;
    int mLastUpdateDay = -1;
};
