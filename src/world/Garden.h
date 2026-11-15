#pragma once

// A garden bed (phase 6): planted from one Seed Packet, it gives 4
// Vegetables after 6 game days if watered (a Water Bottle, or rain) at least
// every 2 days. Dry for 3 days, it dies. After a harvest it grows again from
// its own seed every 5 days. The dead trample it. The next life finds it as
// seedlings. Plain rules, apart from the game.
namespace garden
{
constexpr float kFirstGrowHours = 6.0f * 24.0f;
constexpr float kRegrowHours = 5.0f * 24.0f;
constexpr float kThirstyAfterHours = 2.0f * 24.0f; // stops growing
constexpr float kDiesAfterHours = 3.0f * 24.0f;
constexpr int kYield = 4;
constexpr float kTrampleChance = 0.5f;
constexpr int kStages = 4; // seedlings, young, grown, ripe

struct Bed
{
    float growHours = 0.0f;
    float sinceWaterHours = 0.0f;
    bool regrowing = false; // harvested before: 5 days, not 6
    bool dead = false;
};

// Living off the land 4: crops grow 25 % faster.
[[nodiscard]] float growScale(int livingSkill);
[[nodiscard]] float hoursToRipe(const Bed& bed);
// 0 seedlings, 1 young, 2 grown, 3 ripe.
[[nodiscard]] int stage(const Bed& bed);
[[nodiscard]] bool ripe(const Bed& bed);
[[nodiscard]] bool thirsty(const Bed& bed);

// `hours` pass; rain waters it.
void tick(Bed& bed, float hours, bool raining, int livingSkill);
void water(Bed& bed);
// Ripe: the yield, and it starts again from its own seed. Otherwise 0.
int harvest(Bed& bed);
// One of the dead walks through it: on `roll01` < 0.5, back to seedlings.
void trample(Bed& bed, float roll01);
// Between lives: seedlings, freshly watered.
void carryOver(Bed& bed);
} // namespace garden
