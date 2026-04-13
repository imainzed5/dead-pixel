#include "systems/ParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace
{
std::mt19937& rng()
{
    static std::mt19937 gen{42};
    return gen;
}

float randFloat(float minVal, float maxVal)
{
    std::uniform_real_distribution<float> dist(minVal, maxVal);
    return dist(rng());
}
} // namespace

void ParticleSystem::spawn(const ParticleData& particle)
{
    if (static_cast<int>(mParticles.size()) >= kMaxParticles)
    {
        return;
    }
    mParticles.push_back(particle);
}

void ParticleSystem::spawnBlood(const glm::vec2& worldPosition, int count)
{
    for (int i = 0; i < count; ++i)
    {
        ParticleData p;
        p.position = worldPosition + glm::vec2(randFloat(-4.0f, 4.0f), randFloat(-4.0f, 4.0f));
        p.velocity = glm::vec2(randFloat(-40.0f, 40.0f), randFloat(-60.0f, -10.0f));
        p.color = glm::vec4(0.7f + randFloat(-0.1f, 0.1f), 0.05f, 0.05f, 1.0f);
        p.size = randFloat(1.5f, 3.5f);
        p.lifetime = randFloat(0.4f, 1.0f);
        p.fadeRate = 1.2f;
        p.gravity = 120.0f;
        spawn(p);
    }
}

void ParticleSystem::spawnDust(const glm::vec2& worldPosition, int count)
{
    for (int i = 0; i < count; ++i)
    {
        ParticleData p;
        p.position = worldPosition + glm::vec2(randFloat(-6.0f, 6.0f), randFloat(-2.0f, 2.0f));
        p.velocity = glm::vec2(randFloat(-15.0f, 15.0f), randFloat(-20.0f, -5.0f));
        p.color = glm::vec4(0.55f, 0.5f, 0.4f, 0.5f);
        p.size = randFloat(1.0f, 2.0f);
        p.lifetime = randFloat(0.3f, 0.7f);
        p.fadeRate = 1.5f;
        p.gravity = 10.0f;
        spawn(p);
    }
}

void ParticleSystem::spawnEmbers(const glm::vec2& worldPosition, int count)
{
    for (int i = 0; i < count; ++i)
    {
        ParticleData p;
        p.position = worldPosition + glm::vec2(randFloat(-8.0f, 8.0f), randFloat(-4.0f, 4.0f));
        p.velocity = glm::vec2(randFloat(-10.0f, 10.0f), randFloat(-30.0f, -10.0f));
        p.color = glm::vec4(1.0f, randFloat(0.3f, 0.7f), 0.1f, 0.9f);
        p.size = randFloat(1.0f, 2.5f);
        p.lifetime = randFloat(0.5f, 1.5f);
        p.fadeRate = 0.8f;
        p.gravity = -15.0f; // float upward
        spawn(p);
    }
}

void ParticleSystem::spawnHeal(const glm::vec2& worldPosition, int count)
{
    for (int i = 0; i < count; ++i)
    {
        ParticleData p;
        p.position = worldPosition + glm::vec2(randFloat(-6.0f, 6.0f), randFloat(-2.0f, 2.0f));
        p.velocity = glm::vec2(randFloat(-5.0f, 5.0f), randFloat(-25.0f, -10.0f));
        p.color = glm::vec4(0.3f, 0.9f, 0.4f, 0.8f);
        p.size = randFloat(1.5f, 3.0f);
        p.lifetime = randFloat(0.6f, 1.2f);
        p.fadeRate = 1.0f;
        p.gravity = -20.0f; // float upward
        spawn(p);
    }
}

void ParticleSystem::update(float dtSeconds)
{
    for (auto& p : mParticles)
    {
        p.age += dtSeconds;
        p.velocity.y += p.gravity * dtSeconds;
        p.position += p.velocity * dtSeconds;
        p.color.a = std::max(0.0f, p.color.a - p.fadeRate * dtSeconds);
    }

    // Remove dead particles
    mParticles.erase(
        std::remove_if(
            mParticles.begin(),
            mParticles.end(),
            [](const ParticleData& p) { return p.age >= p.lifetime || p.color.a <= 0.0f; }),
        mParticles.end());
}

void ParticleSystem::render(SpriteBatch& spriteBatch, const Camera& camera, const glm::vec4& solidWhiteUv) const
{
    const float zoom = camera.zoom();
    const glm::vec2 viewportSize = camera.viewportSize();

    for (const ParticleData& p : mParticles)
    {
        if (p.color.a <= 0.01f)
        {
            continue;
        }

        const glm::vec2 screenPos = camera.worldToScreen(p.position);
        const float scaledSize = p.size * zoom;

        // Frustum cull
        if (screenPos.x + scaledSize < 0.0f || screenPos.y + scaledSize < 0.0f ||
            screenPos.x - scaledSize > viewportSize.x || screenPos.y - scaledSize > viewportSize.y)
        {
            continue;
        }

        spriteBatch.draw(
            glm::vec2(screenPos.x - scaledSize * 0.5f, screenPos.y - scaledSize * 0.5f),
            glm::vec2(scaledSize, scaledSize),
            solidWhiteUv,
            p.color);
    }
}
