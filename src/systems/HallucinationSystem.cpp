#include "systems/HallucinationSystem.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

void HallucinationSystem::reset(std::uint32_t seed)
{
    mState = seed != 0 ? seed : 0x9E3779B9u;
    mSigns.clear();
    mStarted.clear();
    mNextIn = -1.0f;
    mFirstOfSpell = true;
    mSilhouettesShown = 0;
    mSignsShown = 0;
}

float HallucinationSystem::roll()
{
    // xorshift32: its own stream, so the run's other rolls are untouched.
    mState ^= mState << 13;
    mState ^= mState >> 17;
    mState ^= mState << 5;
    return static_cast<float>(mState >> 8) / static_cast<float>(1u << 24);
}

std::vector<FalseSign> HallucinationSystem::takeStarted()
{
    std::vector<FalseSign> out;
    out.swap(mStarted);
    return out;
}

void HallucinationSystem::update(float dtSeconds, bool active, const HallucinationView& view, const Placer& place)
{
    // What is already showing plays out, whether or not the spell goes on.
    const float cosCone = std::cos(kLookConeDegrees * 3.14159265f / 180.0f);
    for (FalseSign& sign : mSigns)
    {
        sign.age += dtSeconds;
        if (sign.kind == FalseSign::Kind::Silhouette && !sign.fading)
        {
            const glm::vec2 to = sign.world - view.player;
            const float distance = glm::length(to);
            if (distance < kApproachFadePixels) sign.fading = true;
            else if (distance > 0.001f && glm::dot(to / distance, view.aim) >= cosCone)
            {
                sign.lookedAt += dtSeconds;
                if (sign.lookedAt >= kLookToFadeSeconds) sign.fading = true;
            }
        }
        if (sign.age >= sign.life) sign.fading = true;
        if (sign.fading) sign.alpha = std::max(0.0f, sign.alpha - dtSeconds / kFadeSeconds);
        else sign.alpha = std::min(1.0f, sign.alpha + dtSeconds / kFadeSeconds);
    }
    mSigns.erase(std::remove_if(mSigns.begin(), mSigns.end(),
                                [](const FalseSign& s) { return s.fading && s.alpha <= 0.0f; }),
                 mSigns.end());

    if (!active)
    {
        mNextIn = -1.0f;
        mFirstOfSpell = true;
        return;
    }
    if (mNextIn < 0.0f) mNextIn = kMinInterval + (kMaxInterval - kMinInterval) * roll();
    mNextIn -= dtSeconds;
    if (mNextIn > 0.0f) return;

    // The first sign of a spell is a figure, the plainest of them.
    FalseSign::Kind kind = FalseSign::Kind::Silhouette;
    if (!mFirstOfSpell)
    {
        const float r = roll();
        kind = r < 0.4f ? FalseSign::Kind::Silhouette
             : r < 0.6f ? FalseSign::Kind::Knock
             : r < 0.8f ? FalseSign::Kind::Groan
                        : FalseSign::Kind::MinimapDot;
    }
    const float a = roll();
    const float b = roll();
    const std::optional<glm::vec2> where = place ? place(kind, a, b) : std::nullopt;
    if (!where)
    {
        mNextIn = 1.0f; // nowhere fits yet: look again in a second
        return;
    }
    FalseSign sign;
    sign.kind = kind;
    sign.world = *where;
    sign.life = kind == FalseSign::Kind::Silhouette ? kSilhouetteSeconds
              : kind == FalseSign::Kind::MinimapDot ? kDotSeconds
                                                    : 0.1f;
    sign.alpha = kind == FalseSign::Kind::Silhouette ? 0.0f : 1.0f;
    mSigns.push_back(sign);
    mStarted.push_back(sign);
    ++mSignsShown;
    if (kind == FalseSign::Kind::Silhouette) ++mSilhouettesShown;
    mFirstOfSpell = false;
    mNextIn = kMinInterval + (kMaxInterval - kMinInterval) * roll();
}
