#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

// Things that are not there (phase 5): with too little sleep, a long panic or
// a high fever, every 30 to 80 s one false sign: a figure at the edge of the
// view that fades when looked at or walked toward, a knock at an empty door,
// a groan from nowhere, a dot on the minimap that vanishes.
//
// It owns its signs and nothing else: it never touches the World, makes no
// noise in the simulation and the dead never react. The game draws the signs
// and plays their sounds; the tests check that is all that happens.
struct FalseSign
{
    enum class Kind
    {
        Silhouette,
        Knock,
        Groan,
        MinimapDot
    };
    Kind kind = Kind::Silhouette;
    glm::vec2 world{0.0f};
    float age = 0.0f;
    float life = 8.0f;     // seconds it lasts at most
    float lookedAt = 0.0f; // seconds the player has looked straight at it
    bool fading = false;
    float alpha = 0.0f;    // 0..1 for drawing
};

struct HallucinationView
{
    glm::vec2 player{0.0f}; // world centre of the player
    glm::vec2 aim{1.0f, 0.0f}; // unit direction the player looks
};

class HallucinationSystem
{
public:
    // Where a sign of this kind could appear, from two rolls; nothing if no
    // place fits (the game keeps figures out of the player's building).
    using Placer = std::function<std::optional<glm::vec2>(FalseSign::Kind, float, float)>;

    static constexpr float kMinInterval = 30.0f;
    static constexpr float kMaxInterval = 80.0f;
    static constexpr float kLookToFadeSeconds = 0.5f;
    static constexpr float kLookConeDegrees = 14.0f;
    static constexpr float kApproachFadePixels = 4.0f * 32.0f;
    static constexpr float kFadeSeconds = 0.6f;
    static constexpr float kSilhouetteSeconds = 8.0f;
    static constexpr float kDotSeconds = 3.0f;

    void reset(std::uint32_t seed);
    void update(float dtSeconds, bool active, const HallucinationView& view, const Placer& place);

    [[nodiscard]] const std::vector<FalseSign>& signs() const { return mSigns; }
    // Signs that began since the last call (for their sounds).
    [[nodiscard]] std::vector<FalseSign> takeStarted();
    [[nodiscard]] int silhouettesShown() const { return mSilhouettesShown; }
    [[nodiscard]] int signsShown() const { return mSignsShown; }
    [[nodiscard]] float nextIn() const { return mNextIn; }

private:
    float roll();

    std::uint32_t mState = 1;
    std::vector<FalseSign> mSigns;
    std::vector<FalseSign> mStarted;
    float mNextIn = -1.0f; // < 0: not counting yet
    bool mFirstOfSpell = true;
    int mSilhouettesShown = 0;
    int mSignsShown = 0;
};
