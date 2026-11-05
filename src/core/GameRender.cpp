// World rendering: ground, entities, lighting, minimap, world map and text passes.
#include "core/GameShared.h"

using namespace gameshared;

void Game::render()
{
    glViewport(0, 0, mWindowWidth, mWindowHeight);
    glClearColor(0.08f, 0.11f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    mShader.use();
    mShader.setMat4("uProjection", mProjection);
    mShader.setInt("uTexture", 0);
    setupLightingUniforms();
    buildHudState();
    const glm::vec2 viewport(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight));

    // Pass 1: Map tiles (tileset texture)
    mSpriteBatch.begin();
    renderMap();
    mSpriteBatch.end();

    mTexture.bind(GL_TEXTURE0);
    mSpriteBatch.flush();

    // Pass 2: Entity sprites + particles (spritesheet texture, with lighting)
    mSpriteBatch.begin();
    mStaticSprites.clear();
    mStructureView.collect(mCamera, playerTile(), mStructureSprites, mStaticSprites);
    RenderExtras extras;
    extras.statics = &mStaticSprites;
    extras.isHiddenAt = [this](int tileX, int tileY) { return mStructureView.isHiddenAt(tileX, tileY); };
    mRenderSystem.render(mWorld, mSpriteBatch, mCamera, mInterpolationAlpha, &extras);

    // Particles (rendered in world space, affected by lighting)
    const glm::vec4 solidWhiteUv = mSpriteSheet.uvRect("solid_white");
    mParticleSystem.render(mSpriteBatch, mCamera, solidWhiteUv);

    // Build mode ghost preview
    if (mBuildSystem.isBuildMode())
    {
        const int recipeIdx = mBuildSystem.selectedRecipe();
        if (recipeIdx >= 0 && recipeIdx < BuildSystem::kRecipeCount)
        {
            const auto& recipe = BuildSystem::kRecipes[recipeIdx];
            const glm::vec4 ghostUv = mSpriteSheet.uvRect(recipe.sprite);
            const glm::vec2 ghostWorld = mBuildSystem.ghostPosition() + glm::vec2(kSpriteSize * 0.5f);
            const glm::vec2 ghostScreen = mCamera.worldToScreen(ghostWorld);
            const glm::vec4 ghostColor = mBuildSystem.ghostValid()
                ? glm::vec4(0.2f, 0.9f, 0.3f, 0.55f)
                : glm::vec4(0.9f, 0.2f, 0.2f, 0.55f);
            const float ghostSize = 32.0f * mCamera.zoom();
            mSpriteBatch.draw(
                glm::vec2(ghostScreen.x - ghostSize * 0.5f, ghostScreen.y - ghostSize * 0.5f),
                glm::vec2(ghostSize, ghostSize), ghostUv, ghostColor);
        }
    }

    mSpriteBatch.end();

    mSpriteSheet.bindTexture(GL_TEXTURE0);
    mSpriteBatch.flush();

    // Pass 2.5: HUD overlays (spritesheet texture, no lighting — zoom-independent)
    mShader.setFloat("uAmbientLight", 1.0f);
    mShader.setVec3("uAmbientColor", glm::vec3(1.0f));
    mShader.setInt("uLightCount", 0);
    mShader.setInt("uSpotEnabled", 0);

    mSpriteBatch.begin();
    renderTitleScreen();
    mHud.drawShapes(mSpriteBatch, mSpriteSheet, mFont, mHudState, viewport);
    renderMinimapOverlay();
    renderAttackArcOverlay();
    renderNoiseDebugOverlay();
    renderWorldMapOverlay();
    renderNotebookOverlay();
    renderDeathOverlay();
    renderRetirementOverlay();
    renderPauseOverlay();
    mCraftingRenderTextPass = false;
    renderCraftingOverlay();
    renderInventoryPanel();
    if (!mShowCrafting)
    {
        renderControlsPanel();
    }
    mSpriteBatch.end();

    mSpriteSheet.bindTexture(GL_TEXTURE0);
    mSpriteBatch.flush();

    // Pass 3: text overlays (font atlas texture, no lighting)
    mShader.setFloat("uAmbientLight", 1.0f);
    mShader.setVec3("uAmbientColor", glm::vec3(1.0f));
    mShader.setInt("uLightCount", 0);
    mShader.setInt("uSpotEnabled", 0);

    mSpriteBatch.begin();
    mHud.drawText(mSpriteBatch, mFont, mHudState, viewport);
    if (!mShowCrafting)
    {
        renderTextOverlays();
    }
    renderInventoryOverlay();
    renderContainerOverlay();
    renderBuildModeOverlay();
    mCraftingRenderTextPass = true;
    renderCraftingOverlay();
    if (!mShowCrafting)
    {
        renderControlsOverlay();
    }
    mSpriteBatch.end();

    mFont.bindTexture(GL_TEXTURE0);
    mSpriteBatch.flush();
}

void Game::renderMinimapOverlay()
{
    if (mRunState != RunState::Playing ||
        mShowWorldMap ||
        mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        return;
    }

    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");

    const float mapWidth = 220.0f;
    const float mapHeight = 140.0f;
    const float margin = 16.0f;
    const float mapX = static_cast<float>(mWindowWidth) - mapWidth - margin;
    const float mapY = static_cast<float>(mWindowHeight) - mapHeight - margin;

    mSpriteBatch.draw(glm::vec2(mapX, mapY), glm::vec2(mapWidth, mapHeight), uiUv, glm::vec4(0.02f, 0.03f, 0.05f, 0.78f));
    mSpriteBatch.draw(glm::vec2(mapX - 1.0f, mapY - 1.0f), glm::vec2(mapWidth + 2.0f, 1.0f), uiUv, glm::vec4(0.72f, 0.62f, 0.40f, 0.65f));
    mSpriteBatch.draw(glm::vec2(mapX - 1.0f, mapY + mapHeight), glm::vec2(mapWidth + 2.0f, 1.0f), uiUv, glm::vec4(0.72f, 0.62f, 0.40f, 0.45f));
    mSpriteBatch.draw(glm::vec2(mapX - 1.0f, mapY), glm::vec2(1.0f, mapHeight), uiUv, glm::vec4(0.72f, 0.62f, 0.40f, 0.45f));
    mSpriteBatch.draw(glm::vec2(mapX + mapWidth, mapY), glm::vec2(1.0f, mapHeight), uiUv, glm::vec4(0.72f, 0.62f, 0.40f, 0.65f));

    const int tileW = std::max(1, mTileMap.tileWidth());
    const int tileH = std::max(1, mTileMap.tileHeight());
    const int mapTilesW = std::max(1, mTileMap.width());
    const int mapTilesH = std::max(1, mTileMap.height());
    const float worldW = static_cast<float>(mapTilesW * tileW);
    const float worldH = static_cast<float>(mapTilesH * tileH);

    const float isoMinX = -worldH;
    const float isoMaxX = worldW;
    const float isoMinY = 0.0f;
    const float isoMaxY = (worldW + worldH) * 0.5f;

    const Transform& playerTransform = mWorld.getComponent<Transform>(mPlayerEntity);
    const glm::vec2 playerCenter(playerTransform.x + kSpriteSize * 0.5f, playerTransform.y + kSpriteSize * 0.5f);
    const glm::vec2 playerIso = Camera::worldToIso(playerCenter);

    const float isoRangeX = std::max(1.0f, isoMaxX - isoMinX);
    const float isoRangeY = std::max(1.0f, isoMaxY - isoMinY);
    const float safeZoom = std::clamp(mMinimapZoom, kMinimapZoomMin, kMinimapZoomMax);

    float viewMinX = playerIso.x - (isoRangeX * 0.5f) / safeZoom;
    float viewMaxX = playerIso.x + (isoRangeX * 0.5f) / safeZoom;
    float viewMinY = playerIso.y - (isoRangeY * 0.5f) / safeZoom;
    float viewMaxY = playerIso.y + (isoRangeY * 0.5f) / safeZoom;

    const float viewWidth = viewMaxX - viewMinX;
    const float viewHeight = viewMaxY - viewMinY;

    if (viewWidth >= isoRangeX)
    {
        viewMinX = isoMinX;
        viewMaxX = isoMaxX;
    }
    else
    {
        if (viewMinX < isoMinX)
        {
            viewMaxX += (isoMinX - viewMinX);
            viewMinX = isoMinX;
        }
        if (viewMaxX > isoMaxX)
        {
            viewMinX -= (viewMaxX - isoMaxX);
            viewMaxX = isoMaxX;
        }
    }

    if (viewHeight >= isoRangeY)
    {
        viewMinY = isoMinY;
        viewMaxY = isoMaxY;
    }
    else
    {
        if (viewMinY < isoMinY)
        {
            viewMaxY += (isoMinY - viewMinY);
            viewMinY = isoMinY;
        }
        if (viewMaxY > isoMaxY)
        {
            viewMinY -= (viewMaxY - isoMaxY);
            viewMaxY = isoMaxY;
        }
    }

    const float viewRangeX = std::max(1.0f, viewMaxX - viewMinX);
    const float viewRangeY = std::max(1.0f, viewMaxY - viewMinY);

    auto projectIsoToMinimap = [&](const glm::vec2& isoPosition)
    {
        const float u = std::clamp((isoPosition.x - viewMinX) / viewRangeX, 0.0f, 1.0f);
        const float v = std::clamp((isoPosition.y - viewMinY) / viewRangeY, 0.0f, 1.0f);
        return glm::vec2(mapX + u * mapWidth, mapY + v * mapHeight);
    };

    auto projectWorldToMinimap = [&](const glm::vec2& worldPosition)
    {
        const glm::vec2 iso = Camera::worldToIso(worldPosition);
        return projectIsoToMinimap(iso);
    };

    auto drawIsoPoint = [&](const glm::vec2& worldPosition, const glm::vec4& color, float size)
    {
        const glm::vec2 minimapPos = projectWorldToMinimap(worldPosition);
        const glm::vec2 drawSize(size, size);
        mSpriteBatch.draw(minimapPos - drawSize * 0.5f, drawSize, uiUv, color);
    };

    const LayoutData& layout = mMapGenerator.layoutData();
    int maxDistrictId = 0;
    for (const auto& district : layout.districts)
    {
        maxDistrictId = std::max(maxDistrictId, district.id);
    }
    for (const auto& districtState : mDistrictInfectionSystem.districts())
    {
        maxDistrictId = std::max(maxDistrictId, districtState.districtId);
    }

    std::vector<int> districtTypeById(static_cast<std::size_t>(maxDistrictId + 1), static_cast<int>(DistrictType::Wilderness));
    for (const auto& district : layout.districts)
    {
        if (district.id >= 0 && district.id < static_cast<int>(districtTypeById.size()))
        {
            districtTypeById[static_cast<std::size_t>(district.id)] = district.type;
        }
    }

    std::vector<float> infectionByDistrict(static_cast<std::size_t>(maxDistrictId + 1), 0.0f);
    std::vector<std::uint8_t> overwhelmedByDistrict(static_cast<std::size_t>(maxDistrictId + 1), 0u);
    for (const auto& districtState : mDistrictInfectionSystem.districts())
    {
        if (districtState.districtId >= 0 && districtState.districtId < static_cast<int>(infectionByDistrict.size()))
        {
            infectionByDistrict[static_cast<std::size_t>(districtState.districtId)] = districtState.infection;
            overwhelmedByDistrict[static_cast<std::size_t>(districtState.districtId)] = districtState.overwhelmed ? 1u : 0u;
        }
    }

    auto blendColor = [](const glm::vec4& a, const glm::vec4& b, float t)
    {
        const float w = std::clamp(t, 0.0f, 1.0f);
        return glm::vec4(
            a.r + (b.r - a.r) * w,
            a.g + (b.g - a.g) * w,
            a.b + (b.b - a.b) * w,
            a.a + (b.a - a.a) * w);
    };

    const int sampleStep = (safeZoom >= 2.2f) ? 1 : (safeZoom >= 1.15f ? 2 : 3);
    for (int py = 0; py < static_cast<int>(mapHeight); py += sampleStep)
    {
        for (int px = 0; px < static_cast<int>(mapWidth); px += sampleStep)
        {
            const float u = (static_cast<float>(px) + static_cast<float>(sampleStep) * 0.5f) / mapWidth;
            const float v = (static_cast<float>(py) + static_cast<float>(sampleStep) * 0.5f) / mapHeight;

            const glm::vec2 sampleIso(
                viewMinX + u * viewRangeX,
                viewMinY + v * viewRangeY);
            const glm::vec2 sampleWorld = Camera::isoToWorld(sampleIso);

            const int tx = static_cast<int>(std::floor(sampleWorld.x / static_cast<float>(tileW)));
            const int ty = static_cast<int>(std::floor(sampleWorld.y / static_cast<float>(tileH)));

            glm::vec4 color(0.03f, 0.04f, 0.06f, 0.76f);
            if (tx >= 0 && tx < mapTilesW && ty >= 0 && ty < mapTilesH)
            {
                switch (mTileMap.getSurfaceType(tx, ty))
                {
                case SurfaceType::Carpet: color = glm::vec4(0.17f, 0.14f, 0.16f, 0.74f); break;
                case SurfaceType::Gravel: color = glm::vec4(0.25f, 0.24f, 0.20f, 0.74f); break;
                case SurfaceType::Grass: color = glm::vec4(0.16f, 0.27f, 0.18f, 0.74f); break;
                case SurfaceType::Metal: color = glm::vec4(0.16f, 0.20f, 0.25f, 0.74f); break;
                case SurfaceType::Default:
                default:
                    color = glm::vec4(0.12f, 0.15f, 0.13f, 0.74f);
                    break;
                }

                const int districtId = mTileMap.getDistrictId(tx, ty);
                if (districtId > 0 && districtId < static_cast<int>(districtTypeById.size()))
                {
                    const int districtType = districtTypeById[static_cast<std::size_t>(districtId)];
                    if (districtType == static_cast<int>(DistrictType::Residential))
                    {
                        color = blendColor(color, glm::vec4(0.27f, 0.39f, 0.30f, 0.84f), 0.34f);
                    }
                    else if (districtType == static_cast<int>(DistrictType::Commercial))
                    {
                        color = blendColor(color, glm::vec4(0.28f, 0.32f, 0.44f, 0.84f), 0.34f);
                    }
                    else if (districtType == static_cast<int>(DistrictType::Industrial))
                    {
                        color = blendColor(color, glm::vec4(0.38f, 0.27f, 0.24f, 0.84f), 0.34f);
                    }

                    if (districtId < static_cast<int>(infectionByDistrict.size()))
                    {
                        const float infection = std::clamp(infectionByDistrict[static_cast<std::size_t>(districtId)], 0.0f, 100.0f);
                        const bool overwhelmed = overwhelmedByDistrict[static_cast<std::size_t>(districtId)] != 0u;
                        const glm::vec4 riskColor = overwhelmed
                            ? glm::vec4(0.94f, 0.32f, 0.26f, 0.90f)
                            : glm::vec4(0.90f, 0.60f, 0.24f, 0.86f);
                        const float riskMix = overwhelmed ? 0.68f : (infection / 100.0f) * 0.45f;
                        color = blendColor(color, riskColor, riskMix);
                    }
                }

                if (mTileMap.isSolid(tx, ty))
                {
                    color = blendColor(color, glm::vec4(0.07f, 0.08f, 0.11f, 0.88f), 0.52f);
                }
            }

            const float drawW = std::min(static_cast<float>(sampleStep) + 0.5f, mapWidth - static_cast<float>(px));
            const float drawH = std::min(static_cast<float>(sampleStep) + 0.5f, mapHeight - static_cast<float>(py));
            mSpriteBatch.draw(glm::vec2(mapX + static_cast<float>(px), mapY + static_cast<float>(py)), glm::vec2(drawW, drawH), uiUv, color);
        }
    }

    // Draw subtle world bounds in iso space so map edges remain legible when zoomed.
    const glm::vec2 top = projectIsoToMinimap(glm::vec2((isoMinX + isoMaxX) * 0.5f, isoMinY));
    const glm::vec2 right = projectIsoToMinimap(glm::vec2(isoMaxX, (isoMinY + isoMaxY) * 0.5f));
    const glm::vec2 bottom = projectIsoToMinimap(glm::vec2((isoMinX + isoMaxX) * 0.5f, isoMaxY));
    const glm::vec2 left = projectIsoToMinimap(glm::vec2(isoMinX, (isoMinY + isoMaxY) * 0.5f));
    const glm::vec2 edgeColorSize(1.0f, 1.0f);
    mSpriteBatch.draw(top - edgeColorSize, glm::vec2(2.0f, 2.0f), uiUv, glm::vec4(0.74f, 0.66f, 0.46f, 0.80f));
    mSpriteBatch.draw(right - edgeColorSize, glm::vec2(2.0f, 2.0f), uiUv, glm::vec4(0.74f, 0.66f, 0.46f, 0.80f));
    mSpriteBatch.draw(bottom - edgeColorSize, glm::vec2(2.0f, 2.0f), uiUv, glm::vec4(0.74f, 0.66f, 0.46f, 0.80f));
    mSpriteBatch.draw(left - edgeColorSize, glm::vec2(2.0f, 2.0f), uiUv, glm::vec4(0.74f, 0.66f, 0.46f, 0.80f));

    std::ostringstream minimapLabel;
    minimapLabel << "MINIMAP x" << std::fixed << std::setprecision(1) << safeZoom;
    mFont.drawText(mSpriteBatch, glm::vec2(mapX + 6.0f, mapY + 6.0f), minimapLabel.str(), glm::vec4(0.85f, 0.86f, 0.78f, 0.94f), 0.9f);
    mFont.drawText(mSpriteBatch, glm::vec2(mapX + 6.0f, mapY + mapHeight - 12.0f), "Ctrl+Wheel or +/-", glm::vec4(0.70f, 0.74f, 0.70f, 0.88f), 0.85f);

    for (const GraveRecord& grave : mSaveManager.graves())
    {
        drawIsoPoint(glm::vec2(grave.x, grave.y), glm::vec4(0.80f, 0.38f, 0.38f, 0.92f), 3.0f);
    }

    for (const RetirementRecord& retirement : mSaveManager.retirements())
    {
        drawIsoPoint(glm::vec2(retirement.x, retirement.y), glm::vec4(0.48f, 0.76f, 0.54f, 0.92f), 3.0f);
    }

    for (const auto& [tx, ty] : mBurnedTiles)
    {
        const glm::vec2 worldCenter(
            (static_cast<float>(tx) + 0.5f) * static_cast<float>(tileW),
            (static_cast<float>(ty) + 0.5f) * static_cast<float>(tileH));
        drawIsoPoint(worldCenter, glm::vec4(0.38f, 0.22f, 0.16f, 0.72f), 2.0f);
    }

    if (mCampaignObjective.active)
    {
        const glm::vec2 objectiveCenter(
            (static_cast<float>(mCampaignObjective.tileX) + 0.5f) * static_cast<float>(tileW),
            (static_cast<float>(mCampaignObjective.tileY) + 0.5f) * static_cast<float>(tileH));
        drawIsoPoint(objectiveCenter, glm::vec4(0.94f, 0.80f, 0.30f, 0.95f), 4.0f);
    }

    drawIsoPoint(playerCenter, glm::vec4(0.98f, 0.96f, 0.78f, 1.0f), 4.0f);
}

void Game::renderWorldMapOverlay()
{
    if (mRunState != RunState::Playing || !mShowWorldMap)
    {
        return;
    }

    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);

    mSpriteBatch.draw(glm::vec2(0.0f, 0.0f), glm::vec2(w, h), uiUv, glm::vec4(0.01f, 0.02f, 0.03f, 0.90f));

    const float panelPad = 24.0f;
    const float mapX = panelPad;
    const float mapY = panelPad + 36.0f;
    const float mapW = w - panelPad * 2.0f;
    const float mapH = h - panelPad * 2.0f - 48.0f;

    mSpriteBatch.draw(glm::vec2(mapX, mapY), glm::vec2(mapW, mapH), uiUv, glm::vec4(0.04f, 0.05f, 0.07f, 0.96f));

    const float scaleX = mapW / static_cast<float>(std::max(1, mTileMap.width()));
    const float scaleY = mapH / static_cast<float>(std::max(1, mTileMap.height()));

    for (const auto& district : mMapGenerator.layoutData().districts)
    {
        const float x = mapX + static_cast<float>(district.minX) * scaleX;
        const float y = mapY + static_cast<float>(district.minY) * scaleY;
        const float dw = std::max(1.0f, static_cast<float>(district.maxX - district.minX + 1) * scaleX);
        const float dh = std::max(1.0f, static_cast<float>(district.maxY - district.minY + 1) * scaleY);

        glm::vec4 color(0.20f, 0.23f, 0.25f, 0.58f);
        if (district.type == static_cast<int>(DistrictType::Residential)) color = glm::vec4(0.22f, 0.30f, 0.24f, 0.58f);
        if (district.type == static_cast<int>(DistrictType::Commercial)) color = glm::vec4(0.24f, 0.26f, 0.34f, 0.58f);
        if (district.type == static_cast<int>(DistrictType::Industrial)) color = glm::vec4(0.31f, 0.23f, 0.21f, 0.58f);

        const float infection = std::clamp(mDistrictInfectionSystem.getInfection(district.id), 0.0f, 100.0f);
        const bool overwhelmed = mDistrictInfectionSystem.isOverwhelmed(district.id);
        const float infectionT = infection / 100.0f;
        const glm::vec4 riskColor = overwhelmed
            ? glm::vec4(0.92f, 0.28f, 0.24f, 0.78f)
            : glm::vec4(0.88f, 0.62f, 0.24f, 0.72f);
        const float blend = overwhelmed ? 0.78f : (infectionT * 0.62f);
        color.r = color.r * (1.0f - blend) + riskColor.r * blend;
        color.g = color.g * (1.0f - blend) + riskColor.g * blend;
        color.b = color.b * (1.0f - blend) + riskColor.b * blend;
        color.a = std::max(color.a, 0.60f);

        mSpriteBatch.draw(glm::vec2(x, y), glm::vec2(dw, dh), uiUv, color);

        if (infection >= 35.0f)
        {
            const std::string pctLabel = std::to_string(static_cast<int>(std::round(infection))) + "%";
            mFont.drawText(
                mSpriteBatch,
                glm::vec2(x + dw * 0.5f, y + dh * 0.5f - 4.0f),
                pctLabel,
                overwhelmed ? glm::vec4(1.0f, 0.88f, 0.84f, 0.95f) : glm::vec4(0.92f, 0.90f, 0.74f, 0.90f),
                0.9f,
                TextAlign::Center);
        }
    }

    // Infection legend
    const float legendX = mapX + 10.0f;
    const float legendY = mapY + 10.0f;
    mSpriteBatch.draw(glm::vec2(legendX - 6.0f, legendY - 6.0f), glm::vec2(264.0f, 40.0f), uiUv, glm::vec4(0.02f, 0.03f, 0.04f, 0.72f));

    auto drawLegendSwatch = [&](float x, const glm::vec4& color, const std::string& label)
    {
        mSpriteBatch.draw(glm::vec2(x, legendY + 4.0f), glm::vec2(10.0f, 10.0f), uiUv, color);
        mFont.drawText(mSpriteBatch, glm::vec2(x + 14.0f, legendY + 2.0f), label, glm::vec4(0.84f, 0.86f, 0.80f, 0.95f), 0.85f);
    };

    mFont.drawText(mSpriteBatch, glm::vec2(legendX, legendY - 8.0f), "Infection", glm::vec4(0.94f, 0.86f, 0.60f, 0.96f), 0.9f);
    drawLegendSwatch(legendX, glm::vec4(0.26f, 0.70f, 0.30f, 0.90f), "Low");
    drawLegendSwatch(legendX + 58.0f, glm::vec4(0.78f, 0.70f, 0.30f, 0.90f), "Medium");
    drawLegendSwatch(legendX + 136.0f, glm::vec4(0.90f, 0.54f, 0.24f, 0.92f), "High");
    drawLegendSwatch(legendX + 196.0f, glm::vec4(0.92f, 0.28f, 0.24f, 0.94f), "Over");

    for (const auto& event : mSaveManager.worldMemoryEvents())
    {
        glm::vec4 color(0.62f, 0.62f, 0.62f, 0.85f);
        switch (event.type)
        {
        case WorldMemoryEventType::BurnedTile: color = glm::vec4(0.55f, 0.28f, 0.22f, 0.90f); break;
        case WorldMemoryEventType::GravePlaced: color = glm::vec4(0.82f, 0.42f, 0.42f, 0.90f); break;
        case WorldMemoryEventType::RetirementPlaced: color = glm::vec4(0.46f, 0.82f, 0.48f, 0.90f); break;
        case WorldMemoryEventType::VehicleAbandoned: color = glm::vec4(0.54f, 0.68f, 0.84f, 0.90f); break;
        default: break;
        }

        mSpriteBatch.draw(
            glm::vec2(mapX + event.tileX * scaleX, mapY + event.tileY * scaleY),
            glm::vec2(std::max(2.0f, scaleX * 1.2f), std::max(2.0f, scaleY * 1.2f)),
            uiUv,
            color);
    }

    if (mCampaignObjective.active)
    {
        mSpriteBatch.draw(
            glm::vec2(mapX + mCampaignObjective.tileX * scaleX - 2.0f, mapY + mCampaignObjective.tileY * scaleY - 2.0f),
            glm::vec2(std::max(6.0f, scaleX * 2.4f), std::max(6.0f, scaleY * 2.4f)),
            uiUv,
            glm::vec4(0.95f, 0.78f, 0.25f, 0.95f));
    }

    mFont.drawText(mSpriteBatch, glm::vec2(mapX + 8.0f, mapY - 24.0f), "WORLD MAP", glm::vec4(0.95f, 0.86f, 0.60f, 1.0f), 2.0f);
    mFont.drawText(mSpriteBatch, glm::vec2(mapX + 180.0f, mapY - 24.0f), "M to close", glm::vec4(0.74f, 0.76f, 0.72f, 0.96f), 1.2f);
}

void Game::renderDayNightOverlay()
{
    // Day/night is now handled by the shader lighting system.
    // This function is kept as a no-op for compatibility.
}

void Game::setupLightingUniforms()
{
    // Ambient: daylight to a dark, cold night. Lights carry the night.
    const float night = nightFactor();
    const float ambientLevel = 1.0f - night * 0.80f;
    const glm::vec3 dayColor(1.0f, 1.0f, 1.0f);
    const glm::vec3 duskColor(1.0f, 0.78f, 0.62f);
    const glm::vec3 nightColor(0.55f, 0.62f, 0.95f);
    glm::vec3 ambientColor = night < 0.5f
        ? glm::mix(dayColor, duskColor, night * 2.0f)
        : glm::mix(duskColor, nightColor, (night - 0.5f) * 2.0f);
    if (mGameHours >= 5.0 && mGameHours < 6.0)
    {
        ambientColor = glm::mix(nightColor, dayColor, 1.0f - night); // cool dawn, no orange
    }

    mShader.setFloat("uAmbientLight", ambientLevel);
    mShader.setVec3("uAmbientColor", ambientColor);

    // A world radius in pixels becomes this many screen pixels on the
    // (un-squashed) iso ground at the current zoom.
    const float groundScale = 1.41421356f * mCamera.zoom();
    const float tilePx = static_cast<float>(mTileMap.tileWidth());

    constexpr int kMaxLights = 16;
    glm::vec2 lightPositions[kMaxLights];
    glm::vec3 lightColors[kMaxLights];
    float lightRadii[kMaxLights];
    int lightCount = 0;
    auto addLight = [&](const glm::vec2& worldCenter, const glm::vec3& color, float radiusTiles)
    {
        if (lightCount >= kMaxLights) return;
        lightPositions[lightCount] = mCamera.worldToScreen(worldCenter);
        lightColors[lightCount] = color;
        lightRadii[lightCount] = radiusTiles * tilePx * groundScale;
        ++lightCount;
    };

    const bool playerAlive = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity);
    glm::vec2 playerCenter(0.0f);
    if (playerAlive)
    {
        const Transform& pt = mWorld.getComponent<Transform>(mPlayerEntity);
        const float lerpX = pt.prevX + (pt.x - pt.prevX) * mInterpolationAlpha;
        const float lerpY = pt.prevY + (pt.y - pt.prevY) * mInterpolationAlpha;
        playerCenter = glm::vec2(lerpX + kSpriteSize * 0.5f, lerpY + kSpriteSize * 0.5f);

        // Eyes adjusted to the dark: a small, dim pool so the immediate
        // surroundings stay readable.
        if (night > 0.0f)
        {
            addLight(playerCenter, glm::vec3(0.30f, 0.30f, 0.36f) * night, 3.5f);
            // Spill from the flashlight lights the holder too. Centered a little
            // up-screen so the whole body is caught, not just the feet.
            if (mFlashlightOn)
            {
                addLight(playerCenter - glm::vec2(10.0f, 10.0f), glm::vec3(0.45f, 0.43f, 0.38f) * night, 1.6f);
            }
        }
    }

    // Firelight flickers a little.
    const float flicker = 0.92f + 0.08f * std::sin(static_cast<float>(SDL_GetTicks()) * 0.013f);
    mWorld.forEach<Structure, Transform>(
        [&](Entity, Structure& structure, Transform& transform)
        {
            if (structure.type != StructureType::Campfire || structure.hp <= 0.0f) return;
            addLight(glm::vec2(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f),
                     glm::vec3(1.0f, 0.62f, 0.28f) * flicker, 5.0f);
        });
    mWorld.forEach<Fire, Transform>(
        [&](Entity, Fire& fire, Transform& transform)
        {
            if (fire.intensity <= 0.05f) return;
            const float strength = std::clamp(0.55f + fire.intensity * 0.35f, 0.0f, 1.2f) * flicker;
            addLight(glm::vec2(transform.x + kSpriteSize * 0.5f, transform.y + kSpriteSize * 0.5f),
                     glm::vec3(1.0f, 0.55f, 0.22f) * strength, 3.0f + fire.intensity * 1.5f);
        });

    mShader.setInt("uLightCount", lightCount);
    if (lightCount > 0)
    {
        mShader.setVec2Array("uLightPositions[0]", lightPositions, lightCount);
        mShader.setVec3Array("uLightColors[0]", lightColors, lightCount);
        mShader.setFloatArray("uLightRadii[0]", lightRadii, lightCount);
    }

    // Flashlight beam along the facing direction.
    const bool beam = playerAlive && mFlashlightOn && mWorld.hasComponent<Facing>(mPlayerEntity);
    mShader.setInt("uSpotEnabled", beam ? 1 : 0);
    if (beam)
    {
        const float angle = mWorld.getComponent<Facing>(mPlayerEntity).angleRadians;
        const glm::vec2 worldDir(std::cos(angle), std::sin(angle));
        // World direction -> iso screen direction, then un-squash y.
        glm::vec2 groundDir(worldDir.x - worldDir.y, worldDir.x + worldDir.y);
        groundDir = glm::length(groundDir) > 0.0001f ? glm::normalize(groundDir) : glm::vec2(1.0f, 0.0f);
        mShader.setVec2("uSpotPosition", mCamera.worldToScreen(playerCenter));
        mShader.setVec2("uSpotDirection", groundDir);
        mShader.setFloat("uSpotCosHalfAngle", std::cos(glm::radians(kFlashlightHalfAngleDegrees)));
        mShader.setFloat("uSpotRange", kFlashlightRangeTiles * tilePx * groundScale);
        // Strong at night; barely visible in daylight.
        mShader.setVec3("uSpotColor", glm::vec3(1.0f, 0.95f, 0.82f) * (0.15f + 0.75f * night));
    }
}

void Game::renderTextOverlays()
{
    constexpr float kTextScale = 2.0f;
    constexpr float kSmallScale = 1.5f;

    // F3 infection debug: top infected districts for tuning and verification.
    if (mShowDistrictDebug && mRunState == RunState::Playing)
    {
        std::vector<DistrictInfection> sortedDistricts = mDistrictInfectionSystem.districts();
        std::sort(
            sortedDistricts.begin(),
            sortedDistricts.end(),
            [](const DistrictInfection& a, const DistrictInfection& b)
            {
                return a.infection > b.infection;
            });

        const float debugX = Hud::kMargin + Hud::kVitalsWidth + 16.0f;
        float debugY = Hud::kMargin;
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(debugX, debugY),
            "INFECTION DEBUG (F3)",
            glm::vec4(0.95f, 0.85f, 0.55f, 1.0f),
            1.5f);
        debugY += 18.0f;

        const auto& defs = mMapGenerator.layoutData().districts;
        const std::size_t listCount = std::min<std::size_t>(sortedDistricts.size(), 8);
        for (std::size_t i = 0; i < listCount; ++i)
        {
            const auto& entry = sortedDistricts[i];
            int districtType = static_cast<int>(DistrictType::Wilderness);
            for (const auto& def : defs)
            {
                if (def.id == entry.districtId)
                {
                    districtType = def.type;
                    break;
                }
            }

            std::ostringstream row;
            row << "#" << entry.districtId << " " << districtTypeLabel(districtType)
                << " " << static_cast<int>(std::round(entry.infection)) << "%";
            if (entry.overwhelmed)
            {
                row << " OVERWHELMED";
            }

            const glm::vec4 rowColor = entry.overwhelmed
                ? glm::vec4(0.95f, 0.45f, 0.35f, 1.0f)
                : glm::vec4(0.75f, 0.75f, 0.68f, 0.92f);

            mFont.drawText(mSpriteBatch, glm::vec2(debugX, debugY), row.str(), rowColor, 1.0f);
            debugY += 14.0f;
        }
    }

    // Death overlay text
    if (mRunState == RunState::Dead)
    {
        const float centerX = static_cast<float>(mWindowWidth) * 0.5f;
        const float centerY = static_cast<float>(mWindowHeight) * 0.5f;

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 40.0f), "YOU DIED",
                        glm::vec4(0.9f, 0.2f, 0.2f, 1.0f), 3.0f, TextAlign::Center);

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 4.0f), mCurrentCharacterName,
                        glm::vec4(0.8f, 0.82f, 0.9f, 1.0f), kTextScale, TextAlign::Center);

        // Traits on death screen
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            std::string traitLine;
            if (traits.positive != PositiveTrait::None) traitLine += traits.positiveName();
            if (traits.negative != NegativeTrait::None)
            {
                if (!traitLine.empty()) traitLine += " / ";
                traitLine += traits.negativeName();
            }
            if (!traitLine.empty())
            {
                mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 12.0f), traitLine,
                                glm::vec4(0.65f, 0.65f, 0.55f, 0.8f), 1.0f, TextAlign::Center);
            }

            if (traits.bloodline != BloodlineTrait::None)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 22.0f),
                                std::string("Bloodline: ") + traits.bloodlineName(),
                                glm::vec4(0.70f, 0.78f, 0.62f, 0.86f), 0.95f, TextAlign::Center);
            }
        }

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 32.0f), mDeathCause,
                        glm::vec4(0.75f, 0.55f, 0.55f, 1.0f), kSmallScale, TextAlign::Center);

        // Run stats
        {
            const int distMeters = static_cast<int>(mRunStats.distanceWalked / 32.0f);
            std::string statsLine = "Day " + std::to_string(mCurrentDay)
                + " | " + std::to_string(mRunStats.kills) + " kills"
                + " | " + std::to_string(distMeters) + "m walked";
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 48.0f), statsLine,
                            glm::vec4(0.65f, 0.65f, 0.7f, 0.9f), 1.0f, TextAlign::Center);
        }

        if (mDeathStateTimer >= kDeathRestartDelaySeconds)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 66.0f), "Press Enter to begin again",
                            glm::vec4(0.5f, 0.85f, 0.5f, 1.0f), kSmallScale, TextAlign::Center);
        }
        else
        {
            const int remaining = static_cast<int>(std::ceil(kDeathRestartDelaySeconds - mDeathStateTimer));
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 66.0f),
                            "Wait " + std::to_string(remaining) + "...",
                            glm::vec4(0.6f, 0.4f, 0.4f, 0.8f), kSmallScale, TextAlign::Center);
        }
    }

    // Retirement overlay text
    if (mRunState == RunState::Retired)
    {
        const float centerX = static_cast<float>(mWindowWidth) * 0.5f;
        const float centerY = static_cast<float>(mWindowHeight) * 0.5f;

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 40.0f), "GAVE UP",
                        glm::vec4(0.5f, 0.5f, 0.7f, 1.0f), 3.0f, TextAlign::Center);

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY - 4.0f), mCurrentCharacterName,
                        glm::vec4(0.7f, 0.7f, 0.8f, 1.0f), kTextScale, TextAlign::Center);

        // Traits on retirement screen
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            std::string traitLine;
            if (traits.positive != PositiveTrait::None) traitLine += traits.positiveName();
            if (traits.negative != NegativeTrait::None)
            {
                if (!traitLine.empty()) traitLine += " / ";
                traitLine += traits.negativeName();
            }
            if (!traitLine.empty())
            {
                mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 12.0f), traitLine,
                                glm::vec4(0.55f, 0.55f, 0.5f, 0.8f), 1.0f, TextAlign::Center);
            }

            if (traits.bloodline != BloodlineTrait::None)
            {
                mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 22.0f),
                                std::string("Bloodline: ") + traits.bloodlineName(),
                                glm::vec4(0.66f, 0.78f, 0.66f, 0.86f), 0.95f, TextAlign::Center);
            }
        }

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 32.0f), mDeathCause,
                        glm::vec4(0.5f, 0.5f, 0.65f, 1.0f), kSmallScale, TextAlign::Center);

        // Run stats
        {
            const int distMeters = static_cast<int>(mRunStats.distanceWalked / 32.0f);
            std::string statsLine = "Day " + std::to_string(mCurrentDay)
                + " | " + std::to_string(mRunStats.kills) + " kills"
                + " | " + std::to_string(distMeters) + "m walked";
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 48.0f), statsLine,
                            glm::vec4(0.55f, 0.55f, 0.65f, 0.9f), 1.0f, TextAlign::Center);
        }

        if (mDeathStateTimer >= kDeathRestartDelaySeconds)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 66.0f), "Press Enter to try again",
                            glm::vec4(0.45f, 0.65f, 0.75f, 1.0f), kSmallScale, TextAlign::Center);
        }
        else
        {
            const int remaining = static_cast<int>(std::ceil(kDeathRestartDelaySeconds - mDeathStateTimer));
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, centerY + 66.0f),
                            "Wait " + std::to_string(remaining) + "...",
                            glm::vec4(0.4f, 0.4f, 0.5f, 0.8f), kSmallScale, TextAlign::Center);
        }
    }

    // Pause overlay text
    if (mRunState == RunState::Paused)
    {
        const PauseLayout layout = computePauseLayout(mWindowWidth, mWindowHeight);
        const float centerX = layout.panelPos.x + layout.panelSize.x * 0.5f;

        mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.titleY), "PAUSED",
                        glm::vec4(0.9f, 0.9f, 0.85f, 1.0f), 3.0f, TextAlign::Center);

        // Who you are and how the run is going.
        mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.nameY), mCurrentCharacterName,
                        glm::vec4(0.94f, 0.88f, 0.70f, 1.0f), 2.0f, TextAlign::Center);
        const std::string runLine = std::string(kTitleGameModeNames[static_cast<std::size_t>(
                wrapIndex(mTitleGameModeIndex, static_cast<int>(kTitleGameModeNames.size())))])
            + " run  |  Threat: "
            + kTitleModeNames[static_cast<std::size_t>(wrapIndex(mTitleModeIndex, static_cast<int>(kTitleModeNames.size())))]
            + "  |  Tone: "
            + kTitleToneNames[static_cast<std::size_t>(wrapIndex(mTitleToneIndex, static_cast<int>(kTitleToneNames.size())))];
        mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.runLineY), runLine,
                        glm::vec4(0.66f, 0.72f, 0.80f, 0.95f), 1.0f, TextAlign::Center);
        const std::string statsLine = "Day " + std::to_string(mCurrentDay)
            + "  |  Kills " + std::to_string(mRunStats.kills)
            + "  |  Items looted " + std::to_string(mRunStats.itemsLooted);
        mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.statsLineY), statsLine,
                        glm::vec4(0.66f, 0.72f, 0.80f, 0.95f), 1.0f, TextAlign::Center);
        if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Traits>(mPlayerEntity))
        {
            const Traits& traits = mWorld.getComponent<Traits>(mPlayerEntity);
            std::string traitLine;
            auto append = [&](const std::string& part)
            {
                if (!traitLine.empty()) traitLine += "  |  ";
                traitLine += part;
            };
            if (traits.positive != PositiveTrait::None) append(traits.positiveName());
            if (traits.negative != NegativeTrait::None) append(traits.negativeName());
            if (traits.bloodline != BloodlineTrait::None) append(std::string("Bloodline: ") + traits.bloodlineName());
            mFont.drawText(mSpriteBatch, glm::vec2(centerX, layout.traitsLineY), traitLine,
                            glm::vec4(0.62f, 0.82f, 0.58f, 0.95f), 1.0f, TextAlign::Center);
        }

        static const char* options[] = {"Resume", "Save", "Back to Menu", "Restart", "Quit"};
        for (int i = 0; i < PauseLayout::kOptionCount; ++i)
        {
            const bool selected = (i == mPauseSelection);
            const glm::vec4 color = selected
                ? glm::vec4(1.0f, 1.0f, 0.7f, 1.0f)
                : glm::vec4(0.6f, 0.6f, 0.6f, 0.8f);
            mFont.drawText(mSpriteBatch,
                glm::vec2(centerX, layout.optionsY + static_cast<float>(i) * PauseLayout::kOptionH + 5.0f),
                options[i], color, kTextScale, TextAlign::Center);
        }
    }

    // Title screen briefing hub text
    if (mRunState == RunState::TitleScreen)
    {
        const TitlePanelLayout layout = computeTitlePanelLayout(mWindowWidth, mWindowHeight);
        const float leftX = layout.leftTextX;
        const float rightX = layout.rightTextX;
        const std::size_t menuCharBudget = approximateCharBudget(layout.menuRowW - 20.0f, 8.0f);
        const std::size_t rightCharBudget = approximateCharBudget(layout.rightCardTopW - 24.0f, 8.0f);
        const std::size_t rightBottomBudget = approximateCharBudget(layout.rightCardBottomW - 24.0f, 8.0f);

        const auto& graveHistory = mSaveManager.graves();
        const auto& retirementHistory = mSaveManager.retirements();
        const GraveRecord* lastGrave = graveHistory.empty() ? nullptr : &graveHistory.back();
        const RetirementRecord* lastRetirement = retirementHistory.empty() ? nullptr : &retirementHistory.back();
        const LatestLegacyEvent latestEvent = pickLatestLegacyEvent(graveHistory, retirementHistory);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(leftX, layout.panelY + 16.0f),
            "DEAD PIXEL SURVIVAL",
            glm::vec4(0.95f, 0.36f, 0.30f, 1.0f),
            3.0f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(leftX, layout.panelY + 48.0f),
            "FIELD BRIEFING // PRE-RUN STAGING",
            glm::vec4(0.84f, 0.78f, 0.66f, 0.95f),
            1.2f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(leftX, layout.panelY + 75.0f),
            "Set slot, review intel, then deploy.",
            glm::vec4(0.72f, 0.74f, 0.69f, 0.92f),
            1.15f);

        float menuY = layout.menuRowY + 3.0f;
        for (int i = 0; i < static_cast<int>(kTitleMenuItems.size()); ++i)
        {
            std::string itemText = kTitleMenuItems[static_cast<std::size_t>(i)];
            if (i == 3)
            {
                itemText += "  < " + std::string(kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)]) + " >";
            }
            else if (i == 4)
            {
                itemText += "  < " + std::string(kTitleGameModeNames[static_cast<std::size_t>(mTitleGameModeIndex)]) + " >";
            }
            else if (i == 5)
            {
                itemText += "  < " + std::string(kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)]) + " >";
            }
            else if (i == 6)
            {
                itemText += "  < " + std::string(kTitleToneNames[static_cast<std::size_t>(mTitleToneIndex)]) + " >";
            }

            glm::vec4 color(0.83f, 0.83f, 0.80f, 0.96f);
            if (i == 0 && !mTitleHasCheckpoint)
            {
                color = glm::vec4(0.72f, 0.44f, 0.44f, 0.95f);
                itemText += " [NO CHECKPOINT]";
            }

            if (i == mTitleMenuSelection)
            {
                color = glm::vec4(1.0f, 0.94f, 0.72f, 1.0f);
                itemText = ">> " + itemText;
            }
            else
            {
                itemText = "   " + itemText;
            }

            mFont.drawText(
                mSpriteBatch,
                glm::vec2(leftX, menuY),
                truncateHudText(itemText, menuCharBudget),
                color,
                i == mTitleMenuSelection ? 1.65f : 1.45f);
            menuY += layout.menuRowStep;
        }

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(leftX, layout.panelY + layout.panelH - 44.0f),
            "W/S Move  A/D Change  Enter Select  Esc Close/Quit",
            glm::vec4(0.74f, 0.75f, 0.71f, 0.95f),
            1.15f);

        float topY = layout.rightCardTopY + 10.0f;

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY),
            "ACTIVE SLOT",
            glm::vec4(0.93f, 0.83f, 0.56f, 0.98f),
            1.15f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 19.0f),
            truncateHudText(std::string(kTitleSaveLabels[static_cast<std::size_t>(mTitleSaveSlotIndex)]) + " [" + mActiveSaveSlot + "]", rightCharBudget),
            glm::vec4(0.92f, 0.92f, 0.86f, 0.98f),
            1.25f);

        const int completedRuns = std::max(0, mSaveManager.runNumber() - 1);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 42.0f),
            "Runs Completed: " + std::to_string(completedRuns),
            glm::vec4(0.77f, 0.79f, 0.75f, 0.95f),
            1.08f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 58.0f),
            "Best Streak: Day " + std::to_string(mSaveManager.longestRunDays()),
            glm::vec4(0.77f, 0.79f, 0.75f, 0.95f),
            1.08f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 74.0f),
            truncateHudText("Total Kills: " + std::to_string(mSaveManager.totalKills()) +
                "  |  Days Survived: " + std::to_string(mSaveManager.totalDaysSurvived()), rightCharBudget),
            glm::vec4(0.77f, 0.79f, 0.75f, 0.95f),
            1.08f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 90.0f),
            truncateHudText("Graves: " + std::to_string(graveHistory.size()) +
                "  |  Retirements: " + std::to_string(retirementHistory.size()), rightCharBudget),
            glm::vec4(0.77f, 0.79f, 0.75f, 0.95f),
            1.08f);

        std::string checkpointLine = "Checkpoint: none";
        std::string checkpointLoc = "Location: --";
        if (mTitleHasCheckpoint)
        {
            checkpointLine = "Checkpoint Day " + std::to_string(mTitleCheckpointDay) +
                " // " + truncateHudText(mTitleCheckpointName, 16);
            checkpointLoc = "Location: " + formatTileLocation(mTitleCheckpointX, mTitleCheckpointY, mTileMap);
        }

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 114.0f),
            truncateHudText(checkpointLine, rightCharBudget),
            mTitleHasCheckpoint ? glm::vec4(0.68f, 0.90f, 0.70f, 0.98f) : glm::vec4(0.62f, 0.62f, 0.62f, 0.90f),
            1.08f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, topY + 130.0f),
            truncateHudText(checkpointLoc, rightCharBudget),
            mTitleHasCheckpoint ? glm::vec4(0.66f, 0.83f, 0.68f, 0.95f) : glm::vec4(0.60f, 0.60f, 0.60f, 0.85f),
            1.0f);

        float midY = layout.rightCardMidY + 10.0f;

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, midY),
            "MODE PROFILE",
            glm::vec4(0.93f, 0.83f, 0.56f, 0.98f),
            1.15f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, midY + 19.0f),
            kTitleGameModeNames[static_cast<std::size_t>(mTitleGameModeIndex)],
            glm::vec4(0.95f, 0.91f, 0.78f, 0.98f),
            1.25f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, midY + 37.0f),
            truncateHudText(kTitleGameModeDescriptions[static_cast<std::size_t>(mTitleGameModeIndex)], rightCharBudget),
            glm::vec4(0.74f, 0.77f, 0.71f, 0.95f),
            1.08f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, midY + 53.0f),
            truncateHudText(std::string("Threat: ") + kTitleModeNames[static_cast<std::size_t>(mTitleModeIndex)], rightCharBudget),
            glm::vec4(0.70f, 0.74f, 0.77f, 0.95f),
            1.02f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, midY + 67.0f),
            truncateHudText(std::string("Tone: ") + kTitleToneNames[static_cast<std::size_t>(mTitleToneIndex)], rightCharBudget),
            glm::vec4(0.70f, 0.74f, 0.77f, 0.95f),
            1.02f);

        const std::size_t briefingIndex =
            (static_cast<std::size_t>(mSaveManager.worldSeed()) +
             static_cast<std::size_t>(mSaveManager.runNumber()) +
             static_cast<std::size_t>(mTitlePulseTimer * 0.4f)) % kBriefingLines.size();

        float bottomY = layout.rightCardBottomY + 10.0f;

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY),
            "BRIEFING NOTE",
            glm::vec4(0.93f, 0.83f, 0.56f, 0.98f),
            1.1f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 18.0f),
            truncateHudText(kBriefingLines[briefingIndex], rightBottomBudget),
            glm::vec4(0.77f, 0.80f, 0.74f, 0.98f),
            1.05f);

        std::string latestHeader = "Latest Event: none";
        std::string latestLineA = "No completed runs in this slot.";
        std::string latestLineB = "Start or continue a run to build history.";
        glm::vec4 latestColor(0.63f, 0.63f, 0.63f, 0.9f);

        if (latestEvent.kind == LegacyEventKind::Death && latestEvent.death != nullptr)
        {
            latestHeader = "Latest Event: DEATH (D" + std::to_string(latestEvent.death->day) + ")";
            latestLineA = truncateHudText(
                latestEvent.death->name + " @ " + formatTileLocation(latestEvent.death->x, latestEvent.death->y, mTileMap),
                rightBottomBudget);
            latestLineB = "Cause: " + truncateHudText(latestEvent.death->cause, rightBottomBudget > 7 ? rightBottomBudget - 7 : rightBottomBudget);
            latestColor = glm::vec4(0.88f, 0.66f, 0.66f, 0.98f);
        }
        else if (latestEvent.kind == LegacyEventKind::Retirement && latestEvent.retirement != nullptr)
        {
            latestHeader = "Latest Event: RETIREMENT (D" + std::to_string(latestEvent.retirement->day) + ")";
            latestLineA = truncateHudText(
                latestEvent.retirement->name + " @ " + formatTileLocation(latestEvent.retirement->x, latestEvent.retirement->y, mTileMap),
                rightBottomBudget);
            latestLineB = "Status: Survivor exited run";
            latestColor = glm::vec4(0.76f, 0.86f, 0.74f, 0.98f);
        }

        std::string secondaryLine = "Previous Event: none";
        if (latestEvent.kind == LegacyEventKind::Death && lastRetirement != nullptr)
        {
            secondaryLine = "Previous: RETIRE D" + std::to_string(lastRetirement->day) +
                " " + truncateHudText(lastRetirement->name, 12);
        }
        else if (latestEvent.kind == LegacyEventKind::Retirement && lastGrave != nullptr)
        {
            secondaryLine = "Previous: DEATH D" + std::to_string(lastGrave->day) +
                " " + truncateHudText(lastGrave->name, 12);
        }

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 42.0f),
            "LATEST SLOT EVENT",
            glm::vec4(0.93f, 0.83f, 0.56f, 0.98f),
            1.1f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 60.0f),
            truncateHudText(latestHeader, rightBottomBudget),
            latestColor,
            1.05f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 76.0f),
            truncateHudText(latestLineA, rightBottomBudget),
            glm::vec4(0.76f, 0.80f, 0.74f, 0.95f),
            1.0f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 92.0f),
            truncateHudText(latestLineB, rightBottomBudget),
            glm::vec4(0.72f, 0.74f, 0.71f, 0.95f),
            1.0f);
        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, bottomY + 108.0f),
            truncateHudText(secondaryLine, rightBottomBudget),
            glm::vec4(0.68f, 0.70f, 0.67f, 0.92f),
            1.0f);

        mFont.drawText(
            mSpriteBatch,
            glm::vec2(rightX, layout.panelY + layout.panelH - 42.0f),
            truncateHudText("World Seed: " + std::to_string(mSaveManager.worldSeed()), rightCharBudget),
            glm::vec4(0.67f, 0.69f, 0.66f, 0.90f),
            1.05f);

        if (!mTitleStatusMessage.empty() && mTitleStatusTimer > 0.0f)
        {
            mFont.drawText(
                mSpriteBatch,
                glm::vec2(layout.panelX + layout.panelW * 0.5f, layout.panelY + layout.panelH - 16.0f),
                truncateHudText(mTitleStatusMessage, 64),
                glm::vec4(0.99f, 0.86f, 0.58f, 0.98f),
                1.25f,
                TextAlign::Center);
        }

        if (mShowTitleCredits)
        {
            const float cx = layout.panelX + layout.panelW * 0.5f;
            const float cy = layout.panelY + layout.panelH * 0.5f;
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy - 52.0f), "CREDITS",
                            glm::vec4(0.96f, 0.86f, 0.60f, 1.0f), 2.2f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy - 22.0f), "Dead Pixel Survival",
                            glm::vec4(0.92f, 0.92f, 0.86f, 1.0f), 1.4f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy - 4.0f), "Code + Design: In Progress Build",
                            glm::vec4(0.80f, 0.82f, 0.78f, 0.98f), 1.1f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy + 14.0f), "Engine: C++20 / SDL2 / OpenGL",
                            glm::vec4(0.80f, 0.82f, 0.78f, 0.98f), 1.1f, TextAlign::Center);
            mFont.drawText(mSpriteBatch, glm::vec2(cx, cy + 34.0f), "Press Credits again or Esc to close",
                            glm::vec4(0.72f, 0.74f, 0.70f, 0.95f), 1.05f, TextAlign::Center);
        }
    }
}

void Game::renderMap()
{
    const int tileW = mTileMap.tileWidth();
    const int tileH = mTileMap.tileHeight();

    if (tileW <= 0 || tileH <= 0)
    {
        return;
    }

    const int mapW = mTileMap.width();
    const int mapH = mTileMap.height();
    const glm::vec2 viewportSize = mCamera.viewportSize();
    const float zoom = mCamera.zoom();

    // Iso tile visual size (at zoom 1.0)
    constexpr float kIsoTileW = 64.0f;
    constexpr float kIsoTileH = 32.0f;
    const float scaledTileW = kIsoTileW * zoom;
    const float scaledTileH = kIsoTileH * zoom;
    const float margin = scaledTileW;

    // Render tiles back-to-front by sum of (col + row)
    for (int sum = 0; sum < mapW + mapH - 1; ++sum)
    {
        const int colStart = std::max(0, sum - (mapH - 1));
        const int colEnd = std::min(sum, mapW - 1);

        for (int col = colStart; col <= colEnd; ++col)
        {
            const int row = sum - col;

            int gid = mTileMap.groundGid(col, row);
            if (gid == 0 || mStructureView.coversGround(col, row))
            {
                continue;
            }

            // Pick one of the tileset's variants from the tile position.
            if (gid >= 1 && gid <= TileGid::kPerRow)
            {
                std::uint32_t h = static_cast<std::uint32_t>(col) * 73856093u ^ static_cast<std::uint32_t>(row) * 19349663u;
                h ^= h >> 13;
                gid += TileGid::kPerRow * static_cast<int>(h % static_cast<std::uint32_t>(TileGid::kVariants));
            }

            glm::vec4 uvRect;
            if (!mTileMap.gidToUvRect(gid, uvRect))
            {
                continue;
            }

            // World center of tile
            const glm::vec2 worldCenter(
                static_cast<float>(col * tileW) + static_cast<float>(tileW) * 0.5f,
                static_cast<float>(row * tileH) + static_cast<float>(tileH) * 0.5f);

            const glm::vec2 screenCenter = mCamera.worldToScreen(worldCenter);

            // Frustum cull
            if (screenCenter.x + scaledTileW * 0.5f < -margin ||
                screenCenter.y + scaledTileH * 0.5f < -margin ||
                screenCenter.x - scaledTileW * 0.5f > viewportSize.x + margin ||
                screenCenter.y - scaledTileH * 0.5f > viewportSize.y + margin)
            {
                continue;
            }

            // Tileset diamonds tile exactly; one extra screen pixel hides
            // rounding gaps at fractional zoom levels.
            mSpriteBatch.draw(
                glm::vec2(std::round(screenCenter.x - scaledTileW * 0.5f),
                          std::round(screenCenter.y - scaledTileH * 0.5f)),
                glm::vec2(scaledTileW + 1.0f, scaledTileH + 1.0f),
                uvRect,
                glm::vec4(1.0f));
        }
    }
}

void Game::renderAttackArcOverlay()
{
    if (mPlayerEntity == kInvalidEntity ||
        !mWorld.hasComponent<Transform>(mPlayerEntity) ||
        !mWorld.hasComponent<Facing>(mPlayerEntity) ||
        !mWorld.hasComponent<Combat>(mPlayerEntity))
    {
        return;
    }

    const Transform& transform = mWorld.getComponent<Transform>(mPlayerEntity);
    const Facing& facing = mWorld.getComponent<Facing>(mPlayerEntity);
    const Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);

    if (combat.phase != AttackPhase::Windup)
    {
        return;
    }

    float arcDegrees = combat.weapon.arcDegrees;
    float rangePixels = combat.weapon.rangePixels;
    if (combat.weapon.condition == WeaponCondition::Broken)
    {
        arcDegrees *= 0.7f;
        rangePixels *= 0.6f;
    }
    else if (combat.weapon.condition == WeaponCondition::Damaged)
    {
        rangePixels *= 0.85f;
    }

    const glm::vec2 playerCenter(
        transform.x + kSpriteSize * 0.5f,
        transform.y + kSpriteSize * 0.5f);

    const glm::vec4 debugUvRect = mSpriteSheet.uvRect("solid_white");

    const float halfArcRadians = glm::radians(arcDegrees * 0.5f);
    constexpr int segments = 16;

    const glm::vec4 baseColor = combat.swingWillMiss
        ? glm::vec4(0.9f, 0.3f, 0.3f, 0.65f)
        : glm::vec4(1.0f, 0.95f, 0.2f, 0.75f);

    for (int index = 0; index <= segments; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(segments);
        const float angle = facing.angleRadians - halfArcRadians + (halfArcRadians * 2.0f * t);

        const glm::vec2 direction(std::cos(angle), std::sin(angle));
        const glm::vec2 arcPointWorld = playerCenter + direction * rangePixels;
        const glm::vec2 arcPointScreen = mCamera.worldToScreen(arcPointWorld);

        const float dotSize = 10.0f * mCamera.zoom();
        mSpriteBatch.draw(
            arcPointScreen - glm::vec2(dotSize * 0.5f, dotSize * 0.5f),
            glm::vec2(dotSize, dotSize),
            debugUvRect,
            baseColor);
    }
}

void Game::renderNoiseDebugOverlay()
{
    if (!mShowNoiseDebug)
    {
        return;
    }

    const auto& events = mNoiseModel.activeEvents();
    if (events.empty())
    {
        return;
    }

    const int tileWidth = mTileMap.tileWidth();
    const int tileHeight = mTileMap.tileHeight();
    if (tileWidth <= 0 || tileHeight <= 0)
    {
        return;
    }

    const glm::vec4 debugUvRect = mSpriteSheet.uvRect("solid_white");

    const glm::vec2 viewportSize = mCamera.viewportSize();

    for (const NoiseEvent& event : events)
    {
        const float centerTileX = event.worldPosition.x / static_cast<float>(tileWidth);
        const float centerTileY = event.worldPosition.y / static_cast<float>(tileHeight);

        const int minTileX = static_cast<int>(std::floor(centerTileX - static_cast<float>(event.radiusTiles) - 1.0f));
        const int maxTileX = static_cast<int>(std::ceil(centerTileX + static_cast<float>(event.radiusTiles) + 1.0f));
        const int minTileY = static_cast<int>(std::floor(centerTileY - static_cast<float>(event.radiusTiles) - 1.0f));
        const int maxTileY = static_cast<int>(std::ceil(centerTileY + static_cast<float>(event.radiusTiles) + 1.0f));

        glm::vec4 color = NoiseModel::tierColor(event.tier);
        const float lifeAlpha = event.totalDurationSeconds > 0.0f
            ? event.remainingSeconds / event.totalDurationSeconds
            : 0.0f;
        color.a *= lifeAlpha;

        for (int tileY = minTileY; tileY <= maxTileY; ++tileY)
        {
            for (int tileX = minTileX; tileX <= maxTileX; ++tileX)
            {
                if (tileX < 0 || tileX >= mTileMap.width() || tileY < 0 || tileY >= mTileMap.height())
                {
                    continue;
                }

                const float dx = (static_cast<float>(tileX) + 0.5f) - centerTileX;
                const float dy = (static_cast<float>(tileY) + 0.5f) - centerTileY;
                const float distance = std::sqrt(dx * dx + dy * dy);

                if (distance < static_cast<float>(event.radiusTiles) - 0.65f ||
                    distance > static_cast<float>(event.radiusTiles) + 0.35f)
                {
                    continue;
                }

                const glm::vec2 worldCenter(
                    static_cast<float>(tileX * tileWidth) + static_cast<float>(tileWidth) * 0.5f,
                    static_cast<float>(tileY * tileHeight) + static_cast<float>(tileHeight) * 0.5f);
                const glm::vec2 screenCenter = mCamera.worldToScreen(worldCenter);

                const float scaledW = static_cast<float>(tileWidth) * mCamera.zoom();
                const float scaledH = static_cast<float>(tileHeight) * mCamera.zoom();

                if (screenCenter.x + scaledW * 0.5f < 0.0f ||
                    screenCenter.y + scaledH * 0.5f < 0.0f ||
                    screenCenter.x - scaledW * 0.5f > viewportSize.x ||
                    screenCenter.y - scaledH * 0.5f > viewportSize.y)
                {
                    continue;
                }

                mSpriteBatch.draw(
                    glm::vec2(screenCenter.x - scaledW * 0.5f, screenCenter.y - scaledH * 0.5f),
                    glm::vec2(scaledW, scaledH),
                    debugUvRect,
                    color);
            }
        }
    }
}
