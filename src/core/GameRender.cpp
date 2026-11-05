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
    mStructureView.collect(mCamera, playerTile(), mStructureSprites, mStaticSprites, &mTileMap);
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

    // UI: unlit and zoom-independent, drawn in two layers (HUD, then panels
    // and menus). Each layer draws its shapes with the sprite sheet, then its
    // text with the font atlas, so a panel covers the HUD text beneath it.
    // Text a shape pass draws is deferred to that layer's text pass.
    mShader.setFloat("uAmbientLight", 1.0f);
    mShader.setVec3("uAmbientColor", glm::vec3(1.0f));
    mShader.setFloat("uSaturation", 1.0f);
    mShader.setInt("uLightCount", 0);
    mShader.setInt("uSpotEnabled", 0);

    const auto uiLayer = [&](const auto& drawShapes, const auto& drawText)
    {
        mFont.beginDeferred();
        mSpriteBatch.begin();
        drawShapes();
        mSpriteBatch.end();
        mSpriteSheet.bindTexture(GL_TEXTURE0);
        mSpriteBatch.flush();

        mSpriteBatch.begin();
        mFont.drawDeferred(mSpriteBatch);
        drawText();
        mSpriteBatch.end();
        mFont.bindTexture(GL_TEXTURE0);
        mSpriteBatch.flush();
    };

    uiLayer(
        [&]
        {
            renderTitleScreen();
            mHud.drawShapes(mSpriteBatch, mSpriteSheet, mFont, mHudState, viewport);
            renderMinimapOverlay();
            renderAttackArcOverlay();
            renderNoiseDebugOverlay();
            renderLegacyCardShapes();
        },
        [&]
        {
            renderLegacyText();
            mHud.drawText(mSpriteBatch, mFont, mHudState, viewport);
        });

    uiLayer(
        [&]
        {
            renderWorldMapOverlay();
            renderNotebookOverlay();
            renderRunEndOverlay();
            renderPauseOverlay();
            renderCraftingOverlay();
            renderInventoryPanel();
            renderContainerOverlay();
            if (!mShowCrafting)
            {
                renderControlsPanel();
            }
        },
        [&]
        {
            if (!mShowCrafting)
            {
                renderTextOverlays();
            }
            renderInventoryOverlay();
            renderBuildModeOverlay();
            if (!mShowCrafting)
            {
                renderControlsOverlay();
            }
        });
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

    // Same colours as the town map (M).
    for (const legacy::Lead& lead : currentLegacyLeads())
    {
        drawIsoPoint(lead.world, legacyLeadColor(lead.kind), 3.0f);
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

    // The town drawn the way the game sees it (isometric, N up), as a floor
    // plan: roads, yards, building outlines. On top: what earlier runs left.
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);
    const int tilesW = std::max(1, mTileMap.width());
    const int tilesH = std::max(1, mTileMap.height());
    const float tileW = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float tileH = static_cast<float>(std::max(1, mTileMap.tileHeight()));

    mSpriteBatch.draw(glm::vec2(0.0f), glm::vec2(w, h), uiUv, glm::vec4(0.02f, 0.025f, 0.03f, 1.0f));

    const float headerH = 54.0f;
    const float footerH = 58.0f;
    const float pad = 18.0f;
    const glm::vec2 areaPos(pad, headerH);
    const glm::vec2 areaSize(w - pad * 2.0f, std::max(64.0f, h - headerH - footerH));

    // Iso extent of the map in world pixels: x in [-H, W], y in [0, (W + H) / 2].
    const float worldW = static_cast<float>(tilesW) * tileW;
    const float worldH = static_cast<float>(tilesH) * tileH;
    const float isoMinX = -worldH;
    const float isoRangeX = worldW + worldH;
    const float isoRangeY = (worldW + worldH) * 0.5f;
    const float scale = std::min(areaSize.x / isoRangeX, areaSize.y / isoRangeY);
    const glm::vec2 origin(
        areaPos.x + (areaSize.x - isoRangeX * scale) * 0.5f,
        areaPos.y + (areaSize.y - isoRangeY * scale) * 0.5f);
    const auto toMap = [&](const glm::vec2& world)
    {
        const glm::vec2 iso = Camera::worldToIso(world);
        return origin + glm::vec2((iso.x - isoMinX) * scale, iso.y * scale);
    };

    // District infection and burn scars, looked up per tile below.
    int maxDistrictId = 0;
    for (const auto& district : mDistrictInfectionSystem.districts())
    {
        maxDistrictId = std::max(maxDistrictId, district.districtId);
    }
    std::vector<float> riskByDistrict(static_cast<std::size_t>(maxDistrictId + 1), 0.0f);
    for (const auto& district : mDistrictInfectionSystem.districts())
    {
        if (district.districtId >= 0)
        {
            riskByDistrict[static_cast<std::size_t>(district.districtId)] =
                district.overwhelmed ? 1.0f : std::clamp(district.infection / 100.0f, 0.0f, 1.0f);
        }
    }
    std::vector<std::uint8_t> burned(static_cast<std::size_t>(tilesW * tilesH), 0u);
    for (const auto& [tx, ty] : mBurnedTiles)
    {
        if (tx >= 0 && tx < tilesW && ty >= 0 && ty < tilesH)
        {
            burned[static_cast<std::size_t>(ty * tilesW + tx)] = 1u;
        }
    }

    // Each tile is a 2:1 brick centred on its iso centre; bricks tile the
    // diamond exactly (rows are half a tile apart, offset by half a brick).
    const glm::vec2 brick(2.0f * tileW * scale + 0.6f, 0.5f * tileH * scale + 0.6f);
    for (int ty = 0; ty < tilesH; ++ty)
    {
        for (int tx = 0; tx < tilesW; ++tx)
        {
            const int gid = mTileMap.groundGid(tx, ty);
            const int base = gid > 0 ? ((gid - 1) % TileGid::kPerRow) + 1 : 0;
            const bool solid = mTileMap.isSolid(tx, ty);
            const bool inBuilding = mTileMap.getBuildingId(tx, ty) > 0;

            glm::vec3 color(0.18f, 0.24f, 0.17f);
            if (solid && TileGid::isWallGid(base))
            {
                color = inBuilding ? glm::vec3(0.10f, 0.09f, 0.09f) : glm::vec3(0.07f, 0.07f, 0.08f);
            }
            else if (solid)
            {
                color = glm::vec3(0.11f, 0.17f, 0.11f); // hedges, trees, fences
            }
            else if (inBuilding)
            {
                color = base == TileGid::kCarpet ? glm::vec3(0.50f, 0.42f, 0.40f)
                      : base == TileGid::kWood   ? glm::vec3(0.52f, 0.44f, 0.34f)
                      : base == TileGid::kMetal  ? glm::vec3(0.40f, 0.42f, 0.44f)
                                                 : glm::vec3(0.50f, 0.48f, 0.43f);
            }
            else
            {
                switch (base)
                {
                case TileGid::kAsphalt: color = glm::vec3(0.22f, 0.22f, 0.24f); break;
                case TileGid::kSidewalk: color = glm::vec3(0.34f, 0.33f, 0.31f); break;
                case TileGid::kGravel: color = glm::vec3(0.30f, 0.29f, 0.25f); break;
                case TileGid::kDirt: color = glm::vec3(0.27f, 0.22f, 0.16f); break;
                case TileGid::kWater: color = glm::vec3(0.14f, 0.24f, 0.36f); break;
                case TileGid::kGrass2: color = glm::vec3(0.17f, 0.23f, 0.16f); break;
                default: break;
                }
            }

            const int districtId = mTileMap.getDistrictId(tx, ty);
            if (districtId > 0 && districtId < static_cast<int>(riskByDistrict.size()))
            {
                const float risk = riskByDistrict[static_cast<std::size_t>(districtId)];
                color = glm::mix(color, glm::vec3(0.62f, 0.16f, 0.14f), risk * 0.42f);
            }
            if (burned[static_cast<std::size_t>(ty * tilesW + tx)] != 0u)
            {
                color = glm::mix(color, glm::vec3(0.10f, 0.07f, 0.06f), 0.75f);
            }

            const glm::vec2 centre = toMap(glm::vec2((static_cast<float>(tx) + 0.5f) * tileW,
                                                     (static_cast<float>(ty) + 0.5f) * tileH));
            mSpriteBatch.draw(centre - brick * 0.5f, brick, uiUv, glm::vec4(color, 1.0f));
        }
    }

    // Labels: placed right of their marker, nudged down when they would
    // overlap one already placed.
    std::vector<glm::vec4> placed;
    const auto label = [&](const glm::vec2& at, const std::string& text, const glm::vec4& color, float textScale)
    {
        const glm::vec2 size = mFont.measureText(text, textScale);
        glm::vec2 pos = at;
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            bool clash = false;
            for (const glm::vec4& r : placed)
            {
                if (pos.x < r.x + r.z && pos.x + size.x > r.x && pos.y < r.y + r.w && pos.y + size.y > r.y)
                {
                    clash = true;
                    pos.y = r.y + r.w + 1.0f;
                }
            }
            if (!clash) break;
        }
        placed.emplace_back(pos.x, pos.y, size.x, size.y);
        mFont.drawText(mSpriteBatch, pos + glm::vec2(1.0f), text, glm::vec4(0.0f, 0.0f, 0.0f, 0.85f), textScale);
        mFont.drawText(mSpriteBatch, pos, text, color, textScale);
    };
    const auto marker = [&](const glm::vec2& at, const glm::vec4& color, float size)
    {
        mSpriteBatch.draw(at - glm::vec2(size * 0.5f + 1.5f), glm::vec2(size + 3.0f), uiUv, glm::vec4(0.0f, 0.0f, 0.0f, 0.9f));
        mSpriteBatch.draw(at - glm::vec2(size * 0.5f), glm::vec2(size), uiUv, color);
    };

    // Only districts worth worrying about get a label.
    for (const auto& district : mMapGenerator.layoutData().districts)
    {
        const float infection = mDistrictInfectionSystem.getInfection(district.id);
        const bool overrun = mDistrictInfectionSystem.isOverwhelmed(district.id);
        if (!overrun && infection < 35.0f) continue;
        const glm::vec2 centre = toMap(glm::vec2(
            (static_cast<float>(district.minX + district.maxX + 1) * 0.5f) * tileW,
            (static_cast<float>(district.minY + district.maxY + 1) * 0.5f) * tileH));
        const std::string text = overrun ? "OVERRUN" : std::to_string(static_cast<int>(std::round(infection))) + "% infected";
        const glm::vec2 size = mFont.measureText(text, 1.0f);
        mFont.drawText(mSpriteBatch, centre - size * 0.5f + glm::vec2(1.0f), text, glm::vec4(0.0f, 0.0f, 0.0f, 0.8f), 1.0f);
        mFont.drawText(mSpriteBatch, centre - size * 0.5f, text, glm::vec4(1.0f, 0.70f, 0.62f, 0.95f), 1.0f);
    }

    const glm::vec4 risenColor = legacyLeadColor(legacy::LeadKind::Risen);
    const glm::vec4 packColor = legacyLeadColor(legacy::LeadKind::Pack);
    const glm::vec4 graveColor = legacyLeadColor(legacy::LeadKind::Grave);
    const glm::vec4 survivorColor = legacyLeadColor(legacy::LeadKind::Survivor);
    const glm::vec4 objectiveColor(0.96f, 0.84f, 0.32f, 1.0f);
    const glm::vec4 youColor(0.98f, 0.97f, 0.86f, 1.0f);

    // You first, so your label keeps its place.
    const glm::vec2 you = toMap(playerWorldCenter());
    marker(you, youColor, 8.0f);
    label(you + glm::vec2(8.0f, -5.0f), "You", youColor, 1.0f);

    if (mCampaignObjective.active && !mCampaignObjective.completed)
    {
        const glm::vec2 at = toMap(glm::vec2((static_cast<float>(mCampaignObjective.tileX) + 0.5f) * tileW,
                                             (static_cast<float>(mCampaignObjective.tileY) + 0.5f) * tileH));
        marker(at, objectiveColor, 8.0f);
        label(at + glm::vec2(8.0f, -5.0f), "Objective", objectiveColor, 1.0f);
    }

    for (const legacy::Lead& lead : currentLegacyLeads())
    {
        const glm::vec4 color = legacyLeadColor(lead.kind);
        const glm::vec2 at = toMap(lead.world);
        marker(at, color, lead.kind == legacy::LeadKind::Grave ? 5.0f : 6.0f);
        label(at + glm::vec2(7.0f, -5.0f), lead.name, color, 1.0f);
    }

    // Header.
    mFont.drawText(mSpriteBatch, glm::vec2(pad, 14.0f), "TOWN MAP", glm::vec4(0.95f, 0.86f, 0.60f, 1.0f), 2.0f);
    const std::string sub = "Day " + std::to_string(mCurrentDay) + "  -  Run " + std::to_string(mSaveManager.runNumber());
    mFont.drawText(mSpriteBatch, glm::vec2(pad + 150.0f, 21.0f), sub, glm::vec4(0.74f, 0.74f, 0.70f, 0.96f), 1.0f);
    const std::string closeHint = "M to close";
    mFont.drawText(mSpriteBatch, glm::vec2(w - pad - mFont.measureText(closeHint, 1.0f).x, 21.0f), closeHint,
                   glm::vec4(0.66f, 0.68f, 0.64f, 0.96f), 1.0f);

    // Compass: north is up, as on screen.
    const glm::vec2 compass(w - pad - 14.0f, headerH + 18.0f);
    mSpriteBatch.draw(compass + glm::vec2(-1.0f, -2.0f), glm::vec2(2.0f, 16.0f), uiUv, glm::vec4(0.86f, 0.82f, 0.66f, 0.9f));
    mFont.drawText(mSpriteBatch, compass + glm::vec2(0.0f, -16.0f), "N", glm::vec4(0.94f, 0.88f, 0.66f, 1.0f), 1.0f, TextAlign::Center);

    // Legend.
    struct LegendEntry
    {
        glm::vec4 color;
        const char* text;
    };
    const LegendEntry legend[] = {
        {youColor, "You"},
        {objectiveColor, "Objective"},
        {risenColor, "Risen - put them down"},
        {packColor, "Grave with a pack"},
        {graveColor, "Grave"},
        {survivorColor, "Retired survivor"},
        {glm::vec4(0.62f, 0.16f, 0.14f, 1.0f), "Infected district"},
        {glm::vec4(0.10f, 0.07f, 0.06f, 1.0f), "Burned"},
    };
    float lx = pad;
    const float ly = h - footerH + 22.0f;
    for (const LegendEntry& entry : legend)
    {
        if (entry.text == std::string("Objective") && !mCampaignObjective.active) continue;
        marker(glm::vec2(lx + 4.0f, ly + 4.0f), entry.color, 7.0f);
        mFont.drawText(mSpriteBatch, glm::vec2(lx + 13.0f, ly), entry.text, glm::vec4(0.80f, 0.80f, 0.74f, 0.95f), 1.0f);
        lx += 13.0f + mFont.measureText(entry.text, 1.0f).x + 22.0f;
    }
}

void Game::renderDayNightOverlay()
{
    // Day/night is now handled by the shader lighting system.
    // This function is kept as a no-op for compatibility.
}

void Game::setupLightingUniforms()
{
    // Ambient: daylight to a dark, cold night. Lights carry the night.
    const ToneTuning tone = toneTuning(saveTonePresetFromIndex(mTitleToneIndex));
    const float night = nightFactor();
    const float ambientLevel = 1.0f - night * tone.nightDarkness;
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
    mShader.setFloat("uSaturation", tone.saturation);

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
