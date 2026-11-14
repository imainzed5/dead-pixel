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

void ParticleSystem::spawnBloodSpray(const glm::vec2& worldPosition, const glm::vec2& direction, int count, float force)
{
    for (int i = 0; i < count; ++i)
    {
        ParticleData p;
        p.position = worldPosition + glm::vec2(randFloat(-3.0f, 3.0f), randFloat(-3.0f, 3.0f));
        const glm::vec2 scatter(randFloat(-1.0f, 1.0f), randFloat(-1.0f, 1.0f));
        p.velocity = (direction * randFloat(30.0f, 150.0f) + scatter * 38.0f) * force;
        const float shade = randFloat(0.42f, 0.66f);
        p.color = glm::vec4(shade, 0.04f, 0.05f, 1.0f);
        p.size = randFloat(1.4f, 3.2f);
        p.lifetime = 3.0f;
        p.fadeRate = 0.0f;
        p.height = randFloat(12.0f, 24.0f);
        p.rise = randFloat(10.0f, 110.0f) * force;
        p.lands = true;
        p.groundSeconds = randFloat(28.0f, 50.0f);
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

void ParticleSystem::spawnDustRing(const glm::vec2& worldPosition, float speed, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const float a = 6.2831853f * (static_cast<float>(i) + randFloat(0.0f, 0.6f)) / static_cast<float>(count);
        ParticleData p;
        p.position = worldPosition;
        p.velocity = glm::vec2(std::cos(a), std::sin(a)) * speed * randFloat(0.7f, 1.1f);
        p.color = glm::vec4(0.58f, 0.53f, 0.44f, 0.6f);
        p.size = randFloat(1.5f, 2.5f);
        p.lifetime = randFloat(0.35f, 0.6f);
        p.fadeRate = 1.8f;
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

void ParticleSystem::spawnSmoke(const glm::vec2& worldPosition, int count)
{
    for (int i = 0; i < count; ++i)
    {
        ParticleData p;
        p.position = worldPosition + glm::vec2(randFloat(-10.0f, 10.0f), randFloat(-6.0f, 6.0f));
        p.velocity = glm::vec2(randFloat(-6.0f, 10.0f), randFloat(-26.0f, -14.0f));
        const float grey = randFloat(0.28f, 0.42f);
        p.color = glm::vec4(grey, grey, grey * 1.05f, 0.42f);
        p.size = randFloat(5.0f, 9.0f);
        p.lifetime = randFloat(2.6f, 4.0f);
        p.fadeRate = 0.35f;
        p.gravity = -4.0f;
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

void ParticleSystem::spawnLeaf(const glm::vec2& worldPosition, float height, const glm::vec2& drift,
                              const glm::vec4& color)
{
    ParticleData p;
    p.position = worldPosition + glm::vec2(randFloat(-10.0f, 10.0f), randFloat(-10.0f, 10.0f));
    p.velocity = drift * randFloat(0.7f, 1.2f) + glm::vec2(randFloat(-8.0f, 8.0f), randFloat(-8.0f, 8.0f));
    p.color = color;
    p.size = randFloat(1.6f, 2.4f);
    p.lifetime = 12.0f;
    p.fadeRate = 0.0f;
    p.height = height;
    p.rise = randFloat(-6.0f, 4.0f);
    p.fallScale = randFloat(0.035f, 0.07f);
    p.lands = true;
    p.stains = false;
    p.groundSeconds = randFloat(14.0f, 26.0f);
    spawn(p);
}

void ParticleSystem::spawnCasing(const glm::vec2& worldPosition, const glm::vec2& direction)
{
    ParticleData p;
    p.position = worldPosition;
    p.velocity = direction * randFloat(45.0f, 75.0f) + glm::vec2(randFloat(-10.0f, 10.0f), randFloat(-10.0f, 10.0f));
    p.color = glm::vec4(0.86f, 0.70f, 0.30f, 1.0f);
    p.size = 1.6f;
    p.lifetime = 4.0f;
    p.fadeRate = 0.0f;
    p.height = 16.0f;
    p.rise = randFloat(70.0f, 110.0f);
    p.lands = true;
    p.stains = false;
    p.groundSeconds = 45.0f;
    spawn(p);
}

void ParticleSystem::spawnSplash(const glm::vec2& worldPosition)
{
    ParticleData p;
    p.position = worldPosition;
    p.color = glm::vec4(0.78f, 0.86f, 0.94f, 0.55f);
    p.size = randFloat(2.0f, 3.4f);
    p.lifetime = 0.3f;
    p.fadeRate = 2.2f;
    p.grounded = true;
    spawn(p);
}

void ParticleSystem::update(float dtSeconds)
{
    for (auto& p : mParticles)
    {
        p.age += dtSeconds;
        if (p.grounded)
        {
            p.color.a = std::max(0.0f, p.color.a - p.fadeRate * dtSeconds);
            continue;
        }
        p.velocity.y += p.gravity * dtSeconds;
        p.position += p.velocity * dtSeconds;
        p.color.a = std::max(0.0f, p.color.a - p.fadeRate * dtSeconds);
        if (p.lands)
        {
            p.rise -= kFallGravity * p.fallScale * dtSeconds;
            p.height += p.rise * dtSeconds;
            if (p.height <= 0.0f)
            {
                // It has hit the ground: a stain that dries and fades.
                p.height = 0.0f;
                p.grounded = true;
                p.velocity = glm::vec2(0.0f);
                p.age = 0.0f;
                p.lifetime = p.groundSeconds;
                p.color.a = 0.85f;
                if (p.stains)
                {
                    p.color.r *= 0.8f;
                }
                p.fadeRate = p.color.a / p.groundSeconds;
            }
        }
    }

    // Remove dead particles
    mParticles.erase(
        std::remove_if(
            mParticles.begin(),
            mParticles.end(),
            [](const ParticleData& p) { return p.age >= p.lifetime || p.color.a <= 0.0f; }),
        mParticles.end());
}

void ParticleSystem::render(SpriteBatch& spriteBatch, const Camera& camera, const glm::vec4& solidWhiteUv,
                            bool groundPass) const
{
    const float zoom = camera.zoom();
    const glm::vec2 viewportSize = camera.viewportSize();

    for (const ParticleData& p : mParticles)
    {
        if (p.color.a <= 0.01f || p.grounded != groundPass)
        {
            continue;
        }

        glm::vec2 screenPos = camera.worldToScreen(p.position);
        screenPos.y -= p.height * zoom;
        const float scaledSize = p.size * zoom;

        // Frustum cull
        if (screenPos.x + scaledSize < 0.0f || screenPos.y + scaledSize < 0.0f ||
            screenPos.x - scaledSize > viewportSize.x || screenPos.y - scaledSize > viewportSize.y)
        {
            continue;
        }

        // A mark on the ground lies flat: twice as wide as it is tall.
        const glm::vec2 drawSize = p.grounded
            ? glm::vec2(scaledSize * 1.3f, scaledSize * 0.65f)
            : glm::vec2(scaledSize, scaledSize);
        spriteBatch.draw(
            glm::vec2(screenPos.x - drawSize.x * 0.5f, screenPos.y - drawSize.y * 0.5f),
            drawSize,
            solidWhiteUv,
            p.color);
    }
}
