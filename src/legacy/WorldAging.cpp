#include "legacy/WorldAging.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace aging
{

Stage stageOf(float hpRatio)
{
    if (hpRatio <= 0.02f) return Stage::Gone;
    if (hpRatio > 0.66f) return Stage::Intact;
    if (hpRatio > 0.33f) return Stage::Damaged;
    return Stage::Breaking;
}

float ratioOf(Stage stage)
{
    switch (stage)
    {
    case Stage::Intact: return 1.0f;
    case Stage::Damaged: return 0.66f;
    case Stage::Breaking: return 0.33f;
    case Stage::Gone: return 0.0f;
    }
    return 0.0f;
}

Stage worse(Stage stage)
{
    switch (stage)
    {
    case Stage::Intact: return Stage::Damaged;
    case Stage::Damaged: return Stage::Breaking;
    default: return Stage::Gone;
    }
}

float wornBoards(float boardHp, float oneBoard)
{
    if (boardHp <= 0.0f || oneBoard <= 0.0f) return 0.0f;
    const float boards = std::ceil(boardHp / oneBoard - 1e-4f);
    const float left = boardHp - kBoardWear * oneBoard * boards;
    return left <= 0.5f ? 0.0f : left;
}

float agedInfection(float atEnd, float atStart, int daysLived)
{
    const float aged = atEnd + kInfectionPerDay * static_cast<float>(std::max(0, daysLived)) - kInfectionLifeCleans;
    return std::clamp(std::max(aged, atStart), 0.0f, 100.0f);
}

float populationScale(int livesBefore)
{
    return 1.0f + std::min(kPopulationCap, kPopulationPerLife * static_cast<float>(std::max(0, livesBefore)));
}

bool weathered(int graveRun, int tendedRun, int currentRun)
{
    return currentRun - std::max(graveRun, tendedRun) > kWeatherLives;
}

Report ageWorld(WorldState& world, int daysLived, std::uint32_t seed)
{
    Report report;
    std::mt19937 rng(seed ^ 0xA9E0u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const auto isFood = [&](int itemId)
    { return std::find(world.foodItems.begin(), world.foodItems.end(), itemId) != world.foodItems.end(); };
    const auto kept = [&](int building)
    {
        return building > 0 &&
               std::find(world.keptBuildings.begin(), world.keptBuildings.end(), building) != world.keptBuildings.end();
    };

    // Built things: a stage worse; a garden goes to seed or dies.
    std::vector<Built> next;
    for (Built b : world.built)
    {
        if (b.garden)
        {
            if (b.record.dead || b.record.sinceWaterHours >= kGardenWateredHours)
            {
                ++report.lost;
                continue;
            }
            b.record.growHours = 0.0f;
            b.record.sinceWaterHours = 0.0f;
            b.record.regrowing = false;
        }
        if (b.wears)
        {
            const Stage stage = worse(stageOf(b.record.hpRatio));
            if (stage == Stage::Gone)
            {
                ++report.lost;
                continue;
            }
            b.record.hpRatio = std::min(b.record.hpRatio, ratioOf(stage));
            b.record.condition = static_cast<int>(stage);
            ++report.worn;
        }
        // A crate nobody keeps: some of its food spoils or is taken, and now
        // and then a survivor finds it and takes the lot.
        if (b.crate && !kept(b.building) && !b.record.items.empty())
        {
            const float roll = unit(rng);
            if (roll < kStripChance)
            {
                for (const auto& [id, count] : b.record.items)
                {
                    report.strippedItems.push_back(ContainerItemRecord{id, count, 1.0f, 0});
                }
                b.record.items.clear();
                ++report.stripped;
                report.strippedAt.emplace_back(static_cast<int>(b.record.x), static_cast<int>(b.record.y));
            }
            else
            {
                std::vector<std::size_t> food;
                for (std::size_t i = 0; i < b.record.items.size(); ++i)
                {
                    if (isFood(b.record.items[i].first)) food.push_back(i);
                }
                // A fifth of the stacks, rounded to the nearest (at least one of
                // two or more; a single can stays four lives in five).
                int lose = static_cast<int>(std::floor(static_cast<float>(food.size()) * kCrateFoodLoss + 0.5f));
                if (food.size() == 1 && unit(rng) < kCrateFoodLoss) lose = 1;
                std::shuffle(food.begin(), food.end(), rng);
                food.resize(static_cast<std::size_t>(std::min<int>(lose, static_cast<int>(food.size()))));
                std::sort(food.rbegin(), food.rend());
                for (const std::size_t i : food)
                {
                    b.record.items.erase(b.record.items.begin() + static_cast<std::ptrdiff_t>(i));
                    ++report.foodLost;
                }
            }
        }
        next.push_back(std::move(b));
    }
    world.built = std::move(next);

    // Boards on doors and windows: a quarter of each board's health a life.
    std::vector<BoardedDoorRecord> doors;
    for (BoardedDoorRecord d : world.doors)
    {
        d.boardHp = wornBoards(d.boardHp, world.doorBoardHp);
        if (d.boardHp <= 0.0f)
        {
            ++report.boardsGone;
            continue;
        }
        doors.push_back(d);
    }
    world.doors = std::move(doors);
    for (WindowRecord& w : world.windows)
    {
        if (w.boardHp <= 0.0f) continue;
        w.boardHp = wornBoards(w.boardHp, world.windowBoardHp);
        if (w.boardHp <= 0.0f) ++report.boardsGone;
    }
    // A window with nothing left to remember goes from the list.
    std::erase_if(world.windows, [](const WindowRecord& w) { return !w.broken && !w.covered && w.boardHp <= 0.0f; });

    // The districts: worse for the days lived, a little burned down.
    for (auto& [id, value] : world.infection)
    {
        float start = 0.0f;
        for (const auto& [sid, svalue] : world.lifeStart)
        {
            if (sid == id) start = svalue;
        }
        value = agedInfection(value, start, daysLived);
    }

    // This life's dead are bones now; the oldest bones go past forty.
    for (const auto& body : world.bodies)
    {
        if (std::find(world.bones.begin(), world.bones.end(), body) == world.bones.end()) world.bones.push_back(body);
    }
    world.bodies.clear();
    if (world.bones.size() > kMaxBones)
    {
        world.bones.erase(world.bones.begin(), world.bones.end() - static_cast<std::ptrdiff_t>(kMaxBones));
    }
    return report;
}

} // namespace aging
