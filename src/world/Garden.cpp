#include "world/Garden.h"

#include <algorithm>

namespace garden
{
float growScale(int livingSkill)
{
    return livingSkill >= 4 ? 1.25f : 1.0f;
}

float hoursToRipe(const Bed& bed)
{
    return bed.regrowing ? kRegrowHours : kFirstGrowHours;
}

int stage(const Bed& bed)
{
    const float total = hoursToRipe(bed);
    if (bed.growHours >= total) return kStages - 1;
    return std::clamp(static_cast<int>(bed.growHours / (total / 3.0f)), 0, kStages - 2);
}

bool ripe(const Bed& bed)
{
    return !bed.dead && bed.growHours >= hoursToRipe(bed);
}

bool thirsty(const Bed& bed)
{
    return bed.sinceWaterHours > kThirstyAfterHours;
}

void tick(Bed& bed, float hours, bool raining, int livingSkill)
{
    if (bed.dead || hours <= 0.0f) return;
    if (raining) bed.sinceWaterHours = 0.0f;
    bed.sinceWaterHours += hours;
    if (bed.sinceWaterHours > kDiesAfterHours)
    {
        bed.dead = true;
        return;
    }
    if (!thirsty(bed))
    {
        bed.growHours = std::min(hoursToRipe(bed), bed.growHours + hours * growScale(livingSkill));
    }
}

void water(Bed& bed)
{
    if (!bed.dead) bed.sinceWaterHours = 0.0f;
}

int harvest(Bed& bed)
{
    if (!ripe(bed)) return 0;
    bed.growHours = 0.0f;
    bed.regrowing = true;
    return kYield;
}

void trample(Bed& bed, float roll01)
{
    if (bed.dead || roll01 >= kTrampleChance) return;
    // Back to stage 1: seedlings.
    bed.growHours = 0.0f;
}

void carryOver(Bed& bed)
{
    bed = Bed{};
}
} // namespace garden
