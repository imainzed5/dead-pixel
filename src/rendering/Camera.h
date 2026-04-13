#pragma once

#include <glm/vec2.hpp>

class Camera
{
public:
    void setViewportSize(float width, float height);
    void snapTo(const glm::vec2& worldTarget);
    void follow(const glm::vec2& worldTarget, float dtSeconds, float smoothSpeed);
    void clampToBounds(const glm::vec2& worldSize);

    [[nodiscard]] glm::vec2 worldToScreen(const glm::vec2& worldPosition) const;
    [[nodiscard]] glm::vec2 screenToWorld(const glm::vec2& screenPosition) const;

    // Isometric coordinate conversions
    [[nodiscard]] static glm::vec2 worldToIso(const glm::vec2& worldPosition);
    [[nodiscard]] static glm::vec2 isoToWorld(const glm::vec2& isoPosition);

    // Zoom
    void setZoom(float zoom);
    void zoomToward(float newZoom, const glm::vec2& screenPivot);
    [[nodiscard]] float zoom() const { return mZoom; }

    [[nodiscard]] const glm::vec2& position() const { return mPosition; }
    [[nodiscard]] const glm::vec2& viewportSize() const { return mViewportSize; }

    static constexpr float kMinZoom = 0.5f;
    static constexpr float kMaxZoom = 3.0f;

private:
    glm::vec2 mPosition{0.0f, 0.0f}; // Camera center in iso-space
    glm::vec2 mViewportSize{1280.0f, 720.0f};
    float mZoom = 1.0f;
};
