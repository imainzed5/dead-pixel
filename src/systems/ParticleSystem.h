#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <vector>

#include "rendering/Camera.h"
#include "rendering/SpriteBatch.h"

struct ParticleData
{
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 velocity{0.0f, 0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float size = 2.0f;
    float lifetime = 1.0f;
    float age = 0.0f;
    float fadeRate = 1.0f;
    float gravity = 0.0f;
    // Height above the ground and its rate of change. A particle with
    // `lands` set falls until it meets the ground and then lies there as a
    // flat mark for `groundSeconds`.
    float height = 0.0f;
    float rise = 0.0f;
    bool lands = false;
    bool grounded = false;
    float groundSeconds = 0.0f;
    float fallScale = 1.0f;   // 1 drops like a stone; a leaf is nearer 0.05
    bool stains = true;       // darkens where it lands (blood); leaves and brass do not
};

class ParticleSystem
{
public:
    void spawnBlood(const glm::vec2& worldPosition, int count = 8);
    // Blood thrown from a blow along `direction` (unit, world space; zero
    // sprays all round). The drops arc, land and stain the ground.
    void spawnBloodSpray(const glm::vec2& worldPosition, const glm::vec2& direction, int count, float force = 1.0f);
    void spawnDust(const glm::vec2& worldPosition, int count = 3);
    // A ring of dust thrown out along the ground (a heavy blow, a body hitting the floor).
    void spawnDustRing(const glm::vec2& worldPosition, float speed, int count = 14);
    void spawnEmbers(const glm::vec2& worldPosition, int count = 4);
    void spawnHeal(const glm::vec2& worldPosition, int count = 6);
    // A leaf let go at `height` above the ground, carried by `drift` (world
    // pixels per second) until it settles.
    void spawnLeaf(const glm::vec2& worldPosition, float height, const glm::vec2& drift, const glm::vec4& color);
    // A spent case thrown out sideways from a gun, which lies where it falls.
    void spawnCasing(const glm::vec2& worldPosition, const glm::vec2& direction);
    // A raindrop bursting on the ground.
    void spawnSplash(const glm::vec2& worldPosition);

    void update(float dtSeconds);
    // Drawn in two passes: marks lying on the ground go under everything
    // that stands, the rest goes over.
    void render(SpriteBatch& spriteBatch, const Camera& camera, const glm::vec4& solidWhiteUv,
                bool groundPass = false) const;

    static constexpr float kFallGravity = 420.0f;

    [[nodiscard]] int activeCount() const { return static_cast<int>(mParticles.size()); }

    static constexpr int kMaxParticles = 2048;

private:
    void spawn(const ParticleData& particle);
    std::vector<ParticleData> mParticles;
};
