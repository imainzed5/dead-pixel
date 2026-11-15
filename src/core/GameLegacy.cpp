// The legacy layer: past characters left in the world. The dead lie in graves
// with their packs beside them, or walk again near where they fell carrying
// what they had; the ones who gave up stay on as survivors. A new run opens
// with a card naming who came before and where their traces are.
#include "core/GameShared.h"

#include "legacy/LegacyRules.h"

using namespace gameshared;

namespace
{
constexpr float kLegacyCardSeconds = 11.0f;
constexpr float kLegacyCardFadeSeconds = 1.5f;
constexpr int kLegacyCardLines = 3;
constexpr float kRisenHealth = 140.0f;
constexpr float kRisenBaseSpeed = 104.0f;
constexpr float kRisenLeashTiles = 6.0f;
constexpr float kLabelRangeTiles = 7.0f;
const glm::vec4 kRisenTint(0.80f, 0.86f, 1.0f, 1.0f);

} // namespace

DistrictType Game::districtTypeAt(int tileX, int tileY) const
{
    const int districtId = mTileMap.getDistrictId(tileX, tileY);
    if (districtId == 0)
    {
        return DistrictType::Wilderness;
    }
    for (const auto& def : mMapGenerator.layoutData().districts)
    {
        if (def.id == districtId)
        {
            return static_cast<DistrictType>(def.type);
        }
    }
    return DistrictType::Wilderness;
}

void Game::spawnLegacy()
{
    mLegacyCardLines.clear();
    mLegacyCardTimer = 0.0f;
    // Fresh Start: nothing of earlier lives (phase 10).
    if (!livingWorld())
    {
        mLegacyGraves.clear();
        mLegacyPackSpawned.clear();
        mRisenTracks.clear();
        return;
    }
    mLegacyGraves = mSaveManager.graves();
    mLegacyPackSpawned.assign(mLegacyGraves.size(), false);
    mRisenTracks.clear();

    for (std::size_t i = 0; i < mLegacyGraves.size(); ++i)
    {
        const int index = static_cast<int>(i);
        const GraveRecord& grave = mLegacyGraves[i];
        if (grave.risen)
        {
            spawnRisen(index);
            continue;
        }
        spawnGrave(index);
        if (!grave.items.empty())
        {
            spawnLegacyPack(index, glm::vec2(grave.x + 22.0f, grave.y + 12.0f));
        }
    }

    const GLuint sheetTex = mSpriteSheet.textureId();
    for (const RetirementRecord& ret : mSaveManager.retirements())
    {
        Entity cairn = mWorld.createEntity();
        Transform t{};
        t.x = ret.x;
        t.y = ret.y;
        Sprite s{};
        s.textureId = sheetTex;
        s.uvRect = mSpriteSheet.uvRect("prop_cairn");
        s.layer = 5;
        s.drawSize = SpriteSizes::kHumanoid;
        Interactable inter{};
        inter.type = InteractableType::RetirementMarker;
        inter.prompt = "Read";
        inter.message = ret.name + " stopped here on Day " + std::to_string(ret.day) + ". They couldn't go on.";
        mWorld.addComponent<Transform>(cairn, t);
        mWorld.addComponent<Sprite>(cairn, s);
        mWorld.addComponent<Interactable>(cairn, inter);
    }

    // Run-start card: who came before and the most pressing traces, and how
    // long the town stood empty after a long life (phase 10).
    const auto leads = currentLegacyLeads();
    std::string empty;
    if (!mSaveManager.characterHistory().empty())
    {
        const CharacterHistoryRecord& last = mSaveManager.characterHistory().back();
        empty = journal::timePassingLine(std::max(1, last.dayEnded - last.dayStarted + 1));
    }
    if (!leads.empty() || !empty.empty())
    {
        // A companion's grave is not a life of the bloodline.
        const auto companionGraves = static_cast<std::size_t>(std::count_if(
            mLegacyGraves.begin(), mLegacyGraves.end(), [](const GraveRecord& g) { return !g.travelledWith.empty(); }));
        const int previous = static_cast<int>(std::max(mSaveManager.characterHistory().size(),
                                                       mLegacyGraves.size() - companionGraves + mSaveManager.retirements().size()));
        mLegacyCardTitle = legacy::runHeadline(mSaveManager.runNumber(), previous);
        const glm::vec2 player = playerWorldCenter();
        const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
        for (const legacy::Lead& lead : leads)
        {
            if (static_cast<int>(mLegacyCardLines.size()) >= kLegacyCardLines)
            {
                break;
            }
            mLegacyCardLines.push_back(legacy::describeLead(lead, player, tileSize));
        }
        if (!empty.empty()) mLegacyCardLines.push_back(empty);
        mLegacyCardTimer = kLegacyCardSeconds;
    }
}

void Game::spawnGrave(int graveIndex)
{
    const GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(graveIndex)];
    const GLuint sheetTex = mSpriteSheet.textureId();

    Entity graveEntity = mWorld.createEntity();
    Transform t{};
    t.x = grave.x;
    t.y = grave.y;
    Sprite s{};
    s.textureId = sheetTex;
    // A stone in the cemetery, or a mound where they fell.
    s.uvRect = mSpriteSheet.uvRect(grave.buried ? "prop_headstone" : grave.laidToRest ? "prop_grave_cross" : "prop_grave");
    s.layer = 5;
    s.drawSize = SpriteSizes::kHumanoid;
    Interactable inter{};
    inter.type = InteractableType::GraveMarker;
    inter.prompt = "Read epitaph";
    inter.message = grave.laidToRest
        ? "Here lies " + grave.name + ", put to rest by another survivor. Fell on Day " + std::to_string(grave.day) + "."
        : "Here lies " + grave.name + ". " + grave.cause + ".";
    if (!grave.travelledWith.empty())
    {
        inter.message = "Here lies " + grave.name + ". Fell on Day " + std::to_string(grave.day) + ". Travelled with " +
                        grave.travelledWith + ".";
    }
    mWorld.addComponent<Transform>(graveEntity, t);
    mWorld.addComponent<Sprite>(graveEntity, s);
    mWorld.addComponent<Interactable>(graveEntity, inter);
    mWorld.addComponent<LegacyLink>(graveEntity, LegacyLink{LegacyLinkKind::Grave, graveIndex});
    // Their journal is read at the grave (E, phase 10: handleLegacyInteraction).
}

void Game::spawnLegacyPack(int graveIndex, const glm::vec2& topLeftWorld)
{
    const GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(graveIndex)];
    Entity pack = mWorld.createEntity();
    Transform t{};
    t.x = topLeftWorld.x;
    t.y = topLeftWorld.y;
    Sprite s{};
    s.textureId = mSpriteSheet.textureId();
    s.uvRect = mSpriteSheet.uvRect("prop_pack");
    s.layer = 5;
    s.drawSize = SpriteSizes::kHumanoid;
    Interactable inter{};
    inter.type = InteractableType::Container;
    inter.prompt = "Search " + grave.name + "'s pack";
    Inventory inv{};
    for (const auto& [itemId, count] : grave.items)
    {
        InventorySystem::addItem(inv, mItemDatabase, itemId, count);
    }
    mWorld.addComponent<Transform>(pack, t);
    mWorld.addComponent<Sprite>(pack, s);
    mWorld.addComponent<Interactable>(pack, inter);
    mWorld.addComponent<Inventory>(pack, inv);
    mWorld.addComponent<LegacyLink>(pack, LegacyLink{LegacyLinkKind::Pack, graveIndex});
    mLegacyPackSpawned[static_cast<std::size_t>(graveIndex)] = true;
}

void Game::spawnRisen(int graveIndex)
{
    const GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(graveIndex)];
    EntityFactory::ZombieParams params;
    params.topLeftWorld = glm::vec2(grave.x, grave.y);
    params.health = kRisenHealth;
    params.baseMoveSpeed = kRisenBaseSpeed;
    params.spawnDay = grave.day;
    const Entity risen = EntityFactory::spawnZombie(mWorld, mSpriteSheet, params);
    if (mWorld.hasComponent<ZombieAI>(risen))
    {
        ZombieAI& ai = mWorld.getComponent<ZombieAI>(risen);
        ai.leashHome = glm::vec2(grave.x + kSpriteSize * 0.5f, grave.y + kSpriteSize * 0.5f);
        ai.leashTiles = kRisenLeashTiles;
    }
    if (mWorld.hasComponent<Sprite>(risen))
    {
        mWorld.getComponent<Sprite>(risen).color = kRisenTint;
    }
    mWorld.addComponent<LegacyLink>(risen, LegacyLink{LegacyLinkKind::Risen, graveIndex});
    mRisenTracks.push_back(RisenTrack{risen, graveIndex, glm::vec2(grave.x, grave.y)});
}

void Game::updateLegacy(float dtSeconds)
{
    mLegacyCardTimer = std::max(0.0f, mLegacyCardTimer - dtSeconds);
    if (mShowNotebook || mShowWorldMap)
    {
        mLegacyCardTimer = 0.0f; // the notebook and map say the same, and more
    }

    for (std::size_t i = 0; i < mRisenTracks.size();)
    {
        RisenTrack& track = mRisenTracks[i];
        bool walking = mWorld.hasComponent<LegacyLink>(track.entity);
        if (walking)
        {
            const LegacyLink& link = mWorld.getComponent<LegacyLink>(track.entity);
            walking = link.kind == LegacyLinkKind::Risen && link.graveIndex == track.graveIndex &&
                      mWorld.hasComponent<Health>(track.entity) &&
                      mWorld.getComponent<Health>(track.entity).current > 0.0f;
        }
        if (walking)
        {
            if (mWorld.hasComponent<Transform>(track.entity))
            {
                const Transform& t = mWorld.getComponent<Transform>(track.entity);
                track.lastTopLeft = glm::vec2(t.x, t.y);
            }
            ++i;
            continue;
        }

        // Put down: the body becomes a grave where they fell, their pack
        // beside it. (Only a dead zombie is removed, in case the id was
        // already reused.)
        if (mWorld.hasComponent<LegacyLink>(track.entity))
        {
            mWorld.removeComponent<LegacyLink>(track.entity);
        }
        if (mWorld.hasComponent<ZombieAI>(track.entity) && mWorld.hasComponent<Health>(track.entity) &&
            mWorld.getComponent<Health>(track.entity).current <= 0.0f)
        {
            mWorld.destroyEntity(track.entity);
        }
        // The run-start card is out of date now.
        mLegacyCardTimer = std::min(mLegacyCardTimer, kLegacyCardFadeSeconds);
        GraveRecord& grave = mLegacyGraves[static_cast<std::size_t>(track.graveIndex)];
        grave.risen = false;
        grave.laidToRest = true;
        grave.restRun = mSaveManager.runNumber();
        // One of the line at rest: it lifts, twice as much for a Gravedigger (phase 10).
        {
            const Traits* traits = playerTraits();
            const bool digger = traits != nullptr && traits->has(BloodlineTrait::Gravedigger);
            addDepression(-legacy::kLaidToRestRelief * (digger ? legacy::kGravediggerRestScale : 1.0f));
            noteJournal(journal::Kind::LaidToRest, grave.name);
        }
        grave.x = track.lastTopLeft.x;
        grave.y = track.lastTopLeft.y;
        spawnGrave(track.graveIndex);
        if (!grave.items.empty())
        {
            spawnLegacyPack(track.graveIndex, track.lastTopLeft + glm::vec2(22.0f, 12.0f));
        }
        mInteractionMessage = grave.items.empty()
            ? grave.name + " is at rest."
            : grave.name + " is at rest. Their pack lies beside them.";
        mInteractionMessageTimer = 5.0f;
        mRisenTracks.erase(mRisenTracks.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

void Game::syncLegacyToSave()
{
    if (mLegacyGraves.empty())
    {
        return;
    }
    // A pack holds what is left in it now; a pack that is gone left nothing.
    for (std::size_t i = 0; i < mLegacyGraves.size(); ++i)
    {
        if (i < mLegacyPackSpawned.size() && mLegacyPackSpawned[i])
        {
            mLegacyGraves[i].items.clear();
        }
    }
    mWorld.forEach<LegacyLink, Inventory>(
        [&](Entity, LegacyLink& link, Inventory& inv)
        {
            if (link.kind == LegacyLinkKind::Pack && link.graveIndex >= 0 &&
                link.graveIndex < static_cast<int>(mLegacyGraves.size()))
            {
                auto items = InventorySystem::itemsUnloaded(inv, mItemDatabase);
                auto& target = mLegacyGraves[static_cast<std::size_t>(link.graveIndex)].items;
                target.insert(target.end(), items.begin(), items.end());
            }
        });
    for (std::size_t i = 0; i < mLegacyGraves.size(); ++i)
    {
        mSaveManager.updateGrave(i, mLegacyGraves[i]);
    }
    mSaveManager.flushLegacy();
}

glm::vec2 Game::playerWorldCenter() const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        return glm::vec2(0.0f);
    }
    const Transform& t = mWorld.getComponent<Transform>(mPlayerEntity);
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}

std::vector<legacy::Lead> Game::currentLegacyLeads()
{
    std::vector<legacy::Lead> leads = legacy::collectLeads(mLegacyGraves, mSaveManager.retirements());

    // Live positions: the risen move, and a retiree is wherever they are now
    // (or nowhere, if they did not make it).
    std::vector<legacy::Lead> out;
    for (legacy::Lead lead : leads)
    {
        if (lead.kind == legacy::LeadKind::Risen)
        {
            for (const RisenTrack& track : mRisenTracks)
            {
                if (track.graveIndex == lead.graveIndex)
                {
                    lead.world = track.lastTopLeft;
                }
            }
        }
        else if (lead.kind == legacy::LeadKind::Survivor)
        {
            bool found = false;
            mWorld.forEach<SurvivorAI, Transform, Health>(
                [&](Entity, SurvivorAI& ai, Transform& t, Health& h)
                {
                    if (!found && ai.alive && h.current > 0.0f && ai.name == lead.name)
                    {
                        lead.world = glm::vec2(t.x, t.y);
                        found = true;
                    }
                });
            if (!found)
            {
                continue;
            }
        }
        lead.world += glm::vec2(kSpriteSize * 0.5f);
        out.push_back(lead);
    }
    // The house an earlier life held, unless this life starts in it.
    const BaseRecord& base = mSaveManager.baseRecord();
    if (base.building > 0 && base.building != mFirstDays.homeBuilding)
    {
        legacy::Lead lead;
        lead.kind = legacy::LeadKind::Base;
        lead.world = glm::vec2(base.x, base.y);
        lead.runIndex = base.runIndex;
        out.push_back(lead);
    }
    return out;
}

void Game::renderLegacyCardShapes()
{
    if (mLegacyCardTimer <= 0.0f || mLegacyCardLines.empty() || mRunState != RunState::Playing)
    {
        return;
    }
    const float alpha = std::min(1.0f, mLegacyCardTimer / kLegacyCardFadeSeconds);
    const glm::vec4 uiUv = mSpriteSheet.uvRect("solid_white");
    const LegacyCardLayout layout = legacyCardLayout();
    mSpriteBatch.draw(layout.pos, layout.size, uiUv, glm::vec4(kPanelBack.r, kPanelBack.g, kPanelBack.b, 0.82f * alpha));
}

Game::LegacyCardLayout Game::legacyCardLayout() const
{
    constexpr float kTitleScale = 2.0f;
    constexpr float kLineScale = 2.0f;
    float width = mFont.measureText(mLegacyCardTitle, kTitleScale).x;
    for (const std::string& line : mLegacyCardLines)
    {
        width = std::max(width, mFont.measureText(line, kLineScale).x);
    }
    LegacyCardLayout layout;
    layout.size = glm::vec2(std::round(width + 40.0f),
                            std::round(20.0f + mFont.lineHeight(kTitleScale) + 12.0f +
                                       static_cast<float>(mLegacyCardLines.size()) * (mFont.lineHeight(kLineScale) + 6.0f) + 12.0f));
    layout.pos = glm::vec2(std::round((static_cast<float>(mWindowWidth) - layout.size.x) * 0.5f),
                           std::round(static_cast<float>(mWindowHeight) * 0.18f));
    // Clear of the objective panel (phase 13): slide left if there is room,
    // else drop below it.
    const glm::vec2 corner = Hud::objectiveCorner(mHudState, glm::vec2(static_cast<float>(mWindowWidth), static_cast<float>(mWindowHeight)), mFont);
    constexpr float kClear = 16.0f;
    if (layout.pos.x + layout.size.x > corner.x - kClear && layout.pos.y < corner.y + kClear)
    {
        const float slid = corner.x - kClear - layout.size.x;
        if (slid >= Hud::kEdge) layout.pos.x = std::round(slid);
        else layout.pos.y = std::round(corner.y + kClear);
    }
    layout.titleScale = kTitleScale;
    layout.lineScale = kLineScale;
    return layout;
}

void Game::renderLegacyText()
{
    if (mRunState != RunState::Playing)
    {
        return;
    }

    // Names over past characters' traces near the player.
    const glm::vec2 player = playerWorldCenter();
    const float tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const float zoom = uiZoom();
    const auto label = [&](const glm::vec2& topLeft, float heightPx, const std::string& text, const glm::vec4& color)
    {
        const glm::vec2 center = topLeft + glm::vec2(kSpriteSize * 0.5f);
        const float distTiles = glm::distance(center, player) / tileSize;
        if (distTiles > kLabelRangeTiles)
        {
            return;
        }
        const glm::ivec2 tile = worldToTile(mTileMap, center);
        if (mStructureView.isHiddenAt(tile.x, tile.y))
        {
            return;
        }
        const float fade = std::clamp((kLabelRangeTiles - distTiles) / 2.0f, 0.0f, 1.0f);
        const glm::vec2 screen = uiAt(center);
        const glm::vec2 pos(std::round(screen.x), std::round(screen.y - heightPx * zoom - 12.0f));
        mFont.drawText(mSpriteBatch, pos + glm::vec2(1.0f, 1.0f), text, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f * fade), 1.0f, TextAlign::Center);
        mFont.drawText(mSpriteBatch, pos, text, color * glm::vec4(1.0f, 1.0f, 1.0f, fade), 1.0f, TextAlign::Center);
    };

    mWorld.forEach<LegacyLink, Transform>(
        [&](Entity, LegacyLink& link, Transform& t)
        {
            if (link.graveIndex < 0 || link.graveIndex >= static_cast<int>(mLegacyGraves.size()))
            {
                return;
            }
            const std::string& name = mLegacyGraves[static_cast<std::size_t>(link.graveIndex)].name;
            switch (link.kind)
            {
            case LegacyLinkKind::Risen:
                label(glm::vec2(t.x, t.y), 56.0f, name, glm::vec4(0.92f, 0.52f, 0.46f, 1.0f));
                break;
            case LegacyLinkKind::Grave:
                label(glm::vec2(t.x, t.y), 34.0f, name, glm::vec4(0.82f, 0.82f, 0.78f, 1.0f));
                break;
            case LegacyLinkKind::Pack:
                break; // the prompt names it
            }
        });
    mWorld.forEach<SurvivorAI, Transform>(
        [&](Entity, SurvivorAI& ai, Transform& t)
        {
            if (ai.alive)
            {
                label(glm::vec2(t.x, t.y), 56.0f, ai.name, glm::vec4(0.58f, 0.78f, 0.98f, 1.0f));
            }
        });

    // Run-start card text.
    if (mLegacyCardTimer > 0.0f && !mLegacyCardLines.empty())
    {
        const float alpha = std::min(1.0f, mLegacyCardTimer / kLegacyCardFadeSeconds);
        const LegacyCardLayout layout = legacyCardLayout();
        const float cx = layout.pos.x + layout.size.x * 0.5f;
        float y = layout.pos.y + 20.0f;
        mFont.drawText(mSpriteBatch, glm::vec2(cx, y), mLegacyCardTitle,
                       glm::vec4(kPanelAmber.r, kPanelAmber.g, kPanelAmber.b, alpha), layout.titleScale, TextAlign::Center);
        y += mFont.lineHeight(layout.titleScale) + 12.0f;
        for (const std::string& line : mLegacyCardLines)
        {
            mFont.drawText(mSpriteBatch, glm::vec2(cx, y), line,
                           glm::vec4(kPanelInk.r, kPanelInk.g, kPanelInk.b, alpha), layout.lineScale, TextAlign::Center);
            y += mFont.lineHeight(layout.lineScale) + 6.0f;
        }
    }
}
