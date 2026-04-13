#include "rendering/Camera.h"

#include <glm/common.hpp>

void Camera::setViewportSize(float width, float height)
{
    mViewportSize.x = width;
    mViewportSize.y = height;
}

void Camera::snapTo(const glm::vec2& worldTarget)
{
    mPosition = worldToIso(worldTarget);
}

void Camera::follow(const glm::vec2& worldTarget, float dtSeconds, float smoothSpeed)
{
    const glm::vec2 targetIso = worldToIso(worldTarget);
    const float blendFactor = glm::clamp(dtSeconds * smoothSpeed, 0.0f, 1.0f);
    mPosition = glm::mix(mPosition, targetIso, blendFactor);
}

void Camera::clampToBounds(const glm::vec2& worldSize)
{
    // Compute iso-space bounding box of the world rectangle
    // World corners: (0,0), (W,0), (0,H), (W,H)
    // Iso: (0,0), (W, W/2), (-H, H/2), (W-H, (W+H)/2)
    const float W = worldSize.x;
    const float H = worldSize.y;

    const float isoMinX = -H;
    const float isoMaxX = W;
    const float isoMinY = 0.0f;
    const float isoMaxY = (W + H) * 0.5f;

    const float halfViewW = mViewportSize.x * 0.5f / mZoom;
    const float halfViewH = mViewportSize.y * 0.5f / mZoom;

    const float clampMinX = isoMinX + halfViewW;
    const float clampMaxX = isoMaxX - halfViewW;
    const float clampMinY = isoMinY + halfViewH;
    const float clampMaxY = isoMaxY - halfViewH;

    if (clampMinX < clampMaxX)
    {
        mPosition.x = glm::clamp(mPosition.x, clampMinX, clampMaxX);
    }
    else
    {
        mPosition.x = (isoMinX + isoMaxX) * 0.5f;
    }

    if (clampMinY < clampMaxY)
    {
        mPosition.y = glm::clamp(mPosition.y, clampMinY, clampMaxY);
    }
    else
    {
        mPosition.y = (isoMinY + isoMaxY) * 0.5f;
    }
}

glm::vec2 Camera::worldToScreen(const glm::vec2& worldPosition) const
{
    const glm::vec2 iso = worldToIso(worldPosition);
    return (iso - mPosition) * mZoom + mViewportSize * 0.5f;
}

glm::vec2 Camera::screenToWorld(const glm::vec2& screenPosition) const
{
    const glm::vec2 iso = (screenPosition - mViewportSize * 0.5f) / mZoom + mPosition;
    return isoToWorld(iso);
}

glm::vec2 Camera::worldToIso(const glm::vec2& worldPosition)
{
    return glm::vec2(
        worldPosition.x - worldPosition.y,
        (worldPosition.x + worldPosition.y) * 0.5f);
}

glm::vec2 Camera::isoToWorld(const glm::vec2& isoPosition)
{
    return glm::vec2(
        isoPosition.y + isoPosition.x * 0.5f,
        isoPosition.y - isoPosition.x * 0.5f);
}

void Camera::setZoom(float zoom)
{
    mZoom = glm::clamp(zoom, kMinZoom, kMaxZoom);
}

void Camera::zoomToward(float newZoom, const glm::vec2& screenPivot)
{
    const float oldZoom = mZoom;
    mZoom = glm::clamp(newZoom, kMinZoom, kMaxZoom);

    // Adjust camera so the iso point under the screen pivot stays fixed
    const glm::vec2 pivotOffset = screenPivot - mViewportSize * 0.5f;
    mPosition += pivotOffset * (1.0f / oldZoom - 1.0f / mZoom);
}
