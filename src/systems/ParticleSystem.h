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
};

class ParticleSystem
{
public:
    void spawnBlood(const glm::vec2& worldPosition, int count = 8);
    void spawnDust(const glm::vec2& worldPosition, int count = 3);
    void spawnEmbers(const glm::vec2& worldPosition, int count = 4);
    void spawnHeal(const glm::vec2& worldPosition, int count = 6);

    void update(float dtSeconds);
    void render(SpriteBatch& spriteBatch, const Camera& camera, const glm::vec4& solidWhiteUv) const;

    [[nodiscard]] int activeCount() const { return static_cast<int>(mParticles.size()); }

    static constexpr int kMaxParticles = 2048;

private:
    void spawn(const ParticleData& particle);
    std::vector<ParticleData> mParticles;
};
