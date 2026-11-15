// Fighting (phase 3): being held by the dead, throwing, reloading, wound
// clocks and burning flares, and how a swing, shove or shot shows on screen.
// The rules live in world/CombatRules; the player's swings in CombatSystem.
#include "core/GameShared.h"

#include "world/CombatRules.h"

using namespace gameshared;

namespace
{
// The view pulls in a little while the dead hold on.
constexpr float kHeldZoom = 1.06f;
// A held body shakes with the struggle (camera trauma kept at least this).
constexpr float kHeldTrauma = 0.30f;
// A throw costs a little breath.
constexpr float kThrowStamina = 4.0f;
// A ringing clock goes off this often; a lure pulls the dead again this often.
constexpr float kClockNoiseEverySeconds = 0.7f;
constexpr float kRepullSeconds = 5.0f;
constexpr float kShotLineSeconds = 0.06f;
constexpr float kShoveArcSeconds = 0.14f;
} // namespace

bool Game::playerHeld() const
{
    if (mPlayerEntity == kInvalidEntity) return false;
    // Asked of the zombies themselves: a grab made this tick counts before
    // the combat system has counted it.
    bool held = false;
    const_cast<World&>(mWorld).forEach<ZombieAI, Health>(
        [&](Entity, ZombieAI& ai, Health& h)
        {
            if (ai.holding && h.current > 0.0f) held = true;
        });
    return held;
}

void Game::updateFighting(float dtSeconds, bool playerAlive, bool drivingVehicle)
{
    mShotLineTimer = std::max(0.0f, mShotLineTimer - dtSeconds);
    mShoveArcTimer = std::max(0.0f, mShoveArcTimer - dtSeconds);

    const bool held = playerAlive && !drivingVehicle && playerHeld();

    // Held: you cannot walk away, the view closes in and shakes.
    if (held != mHeldZoomed)
    {
        mCamera.setZoom(held ? mCamera.zoom() * kHeldZoom : mCamera.zoom() / kHeldZoom);
        mHeldZoomed = held;
    }
    if (held)
    {
        if (mWorld.hasComponent<Velocity>(mPlayerEntity))
        {
            Velocity& vel = mWorld.getComponent<Velocity>(mPlayerEntity);
            vel.dx = 0.0f;
            vel.dy = 0.0f;
        }
        mShakeTrauma = std::max(mShakeTrauma, kHeldTrauma);
        mThrowAiming = false;
        // Hands are needed now: the pack and the crafting shut.
        mShowCrafting = false;
        mShowInventory = false;
        if (mContainerOpen) closeContainer();
        if (mTalk.open) closeTalk();
    }

    // A wound clock still in the pack.
    if (mArmedAlarm >= 0.0f && playerAlive)
    {
        mArmedAlarm -= dtSeconds;
        if (mArmedAlarm <= 0.0f)
        {
            mArmedAlarm = -1.0f;
            Lure lure{};
            lure.position = playerWorldCenter();
            lure.remaining = combatrules::kAlarmRingSeconds;
            lure.carried = true;
            lure.clockSurvives = true; // it never left the pack
            mLures.push_back(lure);
            mInteractionMessage = "The clock in your pack goes off.";
            mInteractionMessageTimer = 3.0f;
        }
    }

    // G: hold to aim a throw at the cursor, let go to throw.
    const bool canAct = playerAlive && !drivingVehicle && !held && !mShowCrafting && !mContainerOpen && !mTalk.open &&
                        mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity) &&
                        !mWorld.hasComponent<Sleeping>(mPlayerEntity);
    if (canAct)
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const InventorySlot& slot = inv.slots[static_cast<std::size_t>(std::clamp(inv.activeSlot, 0, Inventory::kMaxSlots - 1))];
        const ItemDef* def = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
        const bool throwable = def != nullptr && def->throwDef.throwable;
        if (!throwable)
        {
            mThrowAiming = false;
        }
        else if (mInput.pressed(Action::Throw))
        {
            mThrowAiming = true;
        }
        if (mThrowAiming && !mInput.down(Action::Throw))
        {
            mThrowAiming = false;
            if (throwSelected(mMouseWorld)) taught("throw");
            mInput.clearToggle(Action::Throw); // press once: thrown
        }
        handleReloadKey();
    }
    else
    {
        mThrowAiming = false;
    }

    // Clocks ringing and flares burning.
    const float tile = static_cast<float>(mTileMap.tileWidth());
    for (Lure& lure : mLures)
    {
        if (lure.carried) lure.position = playerWorldCenter();
        if (lure.delay > 0.0f)
        {
            lure.delay -= dtSeconds;
            continue;
        }
        lure.remaining -= dtSeconds;
        lure.noiseTimer -= dtSeconds;
        lure.pullTimer -= dtSeconds;
        if (lure.flare)
        {
            if (lure.noiseTimer <= 0.0f)
            {
                lure.noiseTimer = combatrules::kFlareNoiseEverySeconds;
                makeNoise(lure.position, NoiseTier::Medium, 1.0f, SoundId::FlareHiss);
                mParticleSystem.spawnEmbers(lure.position + glm::vec2(0.0f, -4.0f), 2);
            }
            // The light draws the eye: the dead that can see it come.
            if (lure.pullTimer <= 0.0f)
            {
                lure.pullTimer = kRepullSeconds;
                lureTo(lure.position, combatrules::kFlarePullTiles, kRepullSeconds + 1.0f, true);
            }
        }
        else
        {
            if (lure.noiseTimer <= 0.0f)
            {
                lure.noiseTimer = kClockNoiseEverySeconds;
                makeNoise(lure.position, NoiseTier::Loud, kClockNoiseEverySeconds, SoundId::ClockRing);
            }
            if (lure.pullTimer <= 0.0f)
            {
                lure.pullTimer = kRepullSeconds;
                lureTo(lure.position, static_cast<float>(NoiseModel::tierRadiusTiles(NoiseTier::Loud)),
                       combatrules::kLureSeconds, false);
            }
        }
        if (lure.remaining <= 0.0f && !lure.flare && !lure.carried && lure.clockSurvives)
        {
            dropOnFloor(worldToTile(mTileMap, lure.position), mItems.alarmClock, 1);
        }
    }
    for (Lure& lure : mLures)
    {
        if (lure.remaining <= 0.0f && lure.delay <= 0.0f && lure.marker != kInvalidEntity)
        {
            mWorld.destroyEntity(lure.marker);
            lure.marker = kInvalidEntity;
        }
    }
    mLures.erase(std::remove_if(mLures.begin(), mLures.end(),
                                [](const Lure& l) { return l.remaining <= 0.0f && l.delay <= 0.0f; }),
                 mLures.end());
    (void)tile;

    // Things in flight.
    std::vector<ThrowLanding> landed;
    mThrownSystem.update(mWorld, mTileMap, dtSeconds, landed);
    for (const ThrowLanding& landing : landed)
    {
        onThrowLanded(landing);
    }
}

bool Game::throwSelected(const glm::vec2& targetWorld)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const int slotIndex = std::clamp(inv.activeSlot, 0, Inventory::kMaxSlots - 1);
    InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    const ItemDef* def = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (def == nullptr || !def->throwDef.throwable) return false;

    const glm::vec2 from = playerWorldCenter();
    const float range = def->throwDef.rangeTiles * static_cast<float>(mTileMap.tileWidth());
    const glm::vec2 to = combatrules::throwLanding(from, targetWorld, range);

    // A wound clock goes with its time still running.
    float fuse = -1.0f;
    if (slot.itemId == mItems.alarmClock && mArmedAlarm >= 0.0f)
    {
        fuse = mArmedAlarm;
        mArmedAlarm = -1.0f;
    }
    ThrownSystem::launch(mWorld, slot.itemId, from, to, mPlayerEntity, fuse, mSpriteSheet.textureId(),
                         mSpriteSheet.uvRect(def->sprite));

    // A thrown thing leaves nothing behind in the pack (no empty bottle back).
    slot.count -= 1;
    if (slot.count <= 0) slot = InventorySlot{};
    if (mWorld.hasComponent<Stamina>(mPlayerEntity))
    {
        Stamina& st = mWorld.getComponent<Stamina>(mPlayerEntity);
        st.current = std::max(0.0f, st.current - kThrowStamina);
    }
    mAudioManager.play(SoundId::ThrowWhoosh);
    return true;
}

void Game::onThrowLanded(const ThrowLanding& landing)
{
    mLastThrowLanding = landing.position;
    const ItemDef* def = mItemDatabase.find(landing.itemId);
    if (def == nullptr) return;
    const ThrowDef& t = def->throwDef;
    const glm::ivec2 tile = worldToTile(mTileMap, landing.position);

    if (landing.hitZombie != kInvalidEntity && mWorld.hasComponent<ZombieAI>(landing.hitZombie) &&
        mWorld.hasComponent<Health>(landing.hitZombie))
    {
        ZombieAI& ai = mWorld.getComponent<ZombieAI>(landing.hitZombie);
        Health& h = mWorld.getComponent<Health>(landing.hitZombie);
        const glm::vec2 from = landing.thrower != kInvalidEntity && mWorld.hasComponent<Transform>(landing.thrower)
                                   ? playerWorldCenter()
                                   : landing.position;
        const glm::vec2 d = landing.position - from;
        const glm::vec2 dir = glm::length(d) > 0.01f ? glm::normalize(d) : glm::vec2(1.0f, 0.0f);
        h.current -= t.damage > 0.0f ? t.damage : combatrules::kThrowHitDamage;
        ZombieBlow::land(ai, dir, 10.0f);
        ai.lastDamagedBy = landing.thrower;
        ai.lastBlow = static_cast<int>(WeaponCategory::Improvised);
        ai.lastBlowHeavy = false;
    }

    const auto makeMarker = [&](const char* spriteName)
    {
        const Entity e = mWorld.createEntity();
        Transform tr{};
        tr.x = tr.prevX = landing.position.x - 16.0f;
        tr.y = tr.prevY = landing.position.y - 16.0f;
        Sprite s{};
        s.textureId = mSpriteSheet.textureId();
        s.uvRect = mSpriteSheet.uvRect(spriteName);
        s.layer = 6;
        s.drawSize = SpriteSizes::kPickup;
        mWorld.addComponent<Transform>(e, tr);
        mWorld.addComponent<Sprite>(e, s);
        return e;
    };

    const NoiseTier tier = static_cast<NoiseTier>(std::clamp(t.noiseTier, 0, 5));
    if (t.effect == "fire")
    {
        // igniteTile makes its own (explosive) noise.
        const glm::vec2 thrownFrom = landing.thrower != kInvalidEntity && mWorld.hasComponent<Transform>(landing.thrower)
                                         ? playerWorldCenter()
                                         : landing.position - glm::vec2(1.0f, 0.0f);
        if (!igniteTile(tile, thrownFrom))
        {
            makeNoise(landing.position, NoiseTier::Loud, 0.8f, SoundId::BottleSmash);
        }
        return;
    }
    if (t.effect == "flare")
    {
        Lure lure{};
        lure.position = landing.position;
        lure.flare = true;
        lure.remaining = combatrules::flareSeconds(mWeatherState.rainIntensity > 0.05f);
        lure.marker = makeMarker(def->sprite.c_str());
        mLures.push_back(lure);
        return;
    }
    if (t.effect == "ring" && landing.fuseSeconds >= 0.0f)
    {
        // A wound clock: it lands, ticks out its time, then rings. One in
        // three is still whole when it stops.
        Lure lure{};
        lure.position = landing.position;
        lure.delay = landing.fuseSeconds;
        lure.remaining = combatrules::kAlarmRingSeconds;
        // Seeded by the run and where and when it fell, so a replay agrees.
        std::mt19937 rng(mSaveManager.currentRunSeed() ^ static_cast<std::uint32_t>(tile.x * 73856093) ^
                         static_cast<std::uint32_t>(tile.y * 19349663) ^ static_cast<std::uint32_t>(mGameHours * 3600.0));
        lure.clockSurvives = combatrules::alarmSurvives(std::uniform_real_distribution<float>(0.0f, 1.0f)(rng));
        lure.marker = makeMarker(def->sprite.c_str());
        mLures.push_back(lure);
        makeNoise(landing.position, NoiseTier::Soft, 0.4f, SoundId::RockLand);
        return;
    }

    // Everything else lands with a noise that draws the dead to it.
    if (tier != NoiseTier::None)
    {
        makeNoise(landing.position, tier, std::max(0.4f, t.noiseSeconds),
                  t.effect == "shatter" ? SoundId::BottleSmash : SoundId::RockLand);
        lureTo(landing.position, static_cast<float>(NoiseModel::tierRadiusTiles(tier)), combatrules::kLureSeconds, false);
    }
    if (t.effect == "shatter")
    {
        // A bottle breaks: what is left is glass.
        mParticleSystem.spawnDust(landing.position, 4);
        if (mItems.glassShards >= 0) dropOnFloor(tile, mItems.glassShards, 1);
        return;
    }
    // A rock, a can, an unwound clock: it lies where it fell.
    dropOnFloor(tile, landing.itemId, 1);
}

int Game::lureTo(const glm::vec2& where, float radiusTiles, float seconds, bool needSight)
{
    const float tile = static_cast<float>(mTileMap.tileWidth());
    const float radius = radiusTiles * tile;
    int pulled = 0;
    mWorld.forEach<ZombieAI, Transform, Health>(
        [&](Entity, ZombieAI& ai, Transform& t, Health& h)
        {
            if (h.current <= 0.0f || ai.holding || ZombieBlow::isDown(ai)) return;
            // Busy with the player: a noise elsewhere does not turn it.
            if (ai.state == ZombieState::Chase || ai.state == ZombieState::Attack) return;
            const glm::vec2 centre(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
            if (glm::length(centre - where) > radius) return;
            if (needSight && !ZombieAISystem::hasLineOfSight(mTileMap, centre, where)) return;
            const bool alreadyGoing = ai.state == ZombieState::Investigate && glm::length(ai.targetWorld - where) < tile;
            if (!alreadyGoing && glm::length(centre - where) > tile)
            {
                ZombieAISystem::sendTo(ai, where, false);
            }
            ai.lureTimer = std::max(ai.lureTimer, seconds);
            ++pulled;
        });
    return pulled;
}

bool Game::useFightingItem(int slotIndex)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return false;
    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    if (slotIndex < 0 || slotIndex >= Inventory::kMaxSlots) return false;
    const InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    if (slot.itemId < 0 || slot.count <= 0) return false;

    if (slot.itemId == mItems.alarmClock)
    {
        if (mArmedAlarm >= 0.0f)
        {
            mInteractionMessage = "It is already ticking.";
        }
        else
        {
            mArmedAlarm = combatrules::kAlarmDelaySeconds;
            mInteractionMessage = "Set for 8 seconds.";
        }
        mInteractionMessageTimer = 2.0f;
        return true;
    }
    if (slot.itemId == mItems.waterBottle)
    {
        // Water goes on a fire in front first; with none, it is drunk.
        return douseFireAhead();
    }
    return false;
}

void Game::handleReloadKey()
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Combat>(mPlayerEntity) ||
        !mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        return;
    }
    Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
    if (combat.weapon.category != WeaponCategory::Ranged) return;
    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);

    // Something in reach that takes R keeps it.
    const bool rTaken = mProximityPrompt.find("R: ") != std::string::npos;
    const int carried = CombatSystem::carriedRounds(combat, inv);
    const bool wantsLoad = combat.jammed || (combat.weapon.ammo < combat.weapon.magazine && carried > 0);
    if (!rTaken && mProximityPrompt.empty() && combat.reloadTimer <= 0.0f && wantsLoad &&
        (combat.jammed || combat.weapon.ammo == 0))
    {
        mProximityPrompt = combat.jammed ? "R: Clear the jam" : "R: Reload";
    }
    if (!rTaken && mInput.pressed(Action::Board) && combat.reloadTimer <= 0.0f)
    {
        if (CombatSystem::startReload(combat, inv))
        {
            mAudioManager.play(combat.jammed ? SoundId::Jam : SoundId::Reload);
            mInteractionMessage = combat.jammed ? "Clearing it." : "Reloading.";
        }
        else
        {
            mInteractionMessage = carried == 0 ? "Nothing to load." : "It is full.";
        }
        mInteractionMessageTimer = 1.5f;
    }
}

void Game::afterCombat()
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Combat>(mPlayerEntity) ||
        !mWorld.hasComponent<Facing>(mPlayerEntity))
    {
        return;
    }
    const Combat& combat = mWorld.getComponent<Combat>(mPlayerEntity);
    const float angle = mWorld.getComponent<Facing>(mPlayerEntity).angleRadians;
    const glm::vec2 aim(std::cos(angle), std::sin(angle));
    const glm::vec2 centre = playerWorldCenter();

    if (combat.shotEvent > 0)
    {
        // A flash at the muzzle, a spent case out to the side, and the shot's
        // line for a frame.
        mMuzzleFlash = 0.07f;
        mMuzzleWorld = centre + aim * 16.0f;
        mParticleSystem.spawnCasing(centre + aim * 6.0f, glm::vec2(-aim.y, aim.x));
        mShotLines = mCombatSystem.shotLines();
        mShotLineTimer = kShotLineSeconds;
        mShakeTrauma = std::min(1.0f, mShakeTrauma + (combat.shotEvent > 1 ? 0.45f : 0.28f));
        // The shot itself comes with its noise (CombatSystem); here the
        // case on the ground, and the ringing it leaves.
        playAt(SoundId::Casing, centre + glm::vec2(-aim.y, aim.x) * 20.0f, 0.7f);
        startTinnitus();
    }
    else if (combat.weapon.category == WeaponCategory::Ranged && combat.weapon.ammo <= 0 && !combat.jammed &&
             combat.reloadTimer <= 0.0f && mInput.pressed(Action::Attack) && !playerHeld())
    {
        mAudioManager.play(SoundId::DryClick);
        mInteractionMessage = "Empty. R to reload.";
        mInteractionMessageTimer = 1.6f;
    }
    if (combat.jamEvent)
    {
        mAudioManager.play(SoundId::Jam);
        mInteractionMessage = "It jams.";
        mInteractionMessageTimer = 1.8f;
    }
    if (combat.stuckEvent)
    {
        playAt(SoundId::StickPull, centre + aim * 20.0f);
        mInteractionMessage = "It sticks.";
        mInteractionMessageTimer = 1.4f;
    }
    if (combat.wallEvent)
    {
        playAt(SoundId::HitWall, centre + aim * 22.0f);
        mParticleSystem.spawnDust(centre + aim * 22.0f, 4);
        mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.15f);
    }
    if (combat.shoveEvent)
    {
        mShoveArcTimer = kShoveArcSeconds;
        mShoveArcAngle = angle;
        mParticleSystem.spawnDust(centre + aim * 20.0f, 3);
        ++mShovesThisLife; // for the run-end's hint (phase 13)
        taught("shove");
    }
    if (combat.brokeFree)
    {
        mAudioManager.play(SoundId::BodyGruntBig);
        mInteractionMessage = "You tear free.";
        mInteractionMessageTimer = 1.8f;
        mParticleSystem.spawnDustRing(centre + glm::vec2(0.0f, 8.0f), 60.0f, 12);
        mShakeTrauma = std::min(1.0f, mShakeTrauma + 0.3f);
    }
}

void Game::renderFightingOverlay()
{
    if (mPlayerEntity == kInvalidEntity) return;
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const float zoom = uiZoom();
    const auto dot = [&](const glm::vec2& world, float lift, float size, const glm::vec4& color)
    {
        const glm::vec2 screen = uiAt(world) - glm::vec2(0.0f, lift * zoom);
        const float s = std::round(size * zoom);
        mSpriteBatch.draw(glm::round(screen - glm::vec2(s * 0.5f)), glm::vec2(s), white, color);
    };

    // The throw: its arc to where it will land, and a ring where it lands.
    if (mThrowAiming && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        const InventorySlot& slot = inv.slots[static_cast<std::size_t>(std::clamp(inv.activeSlot, 0, Inventory::kMaxSlots - 1))];
        const ItemDef* def = slot.itemId >= 0 ? mItemDatabase.find(slot.itemId) : nullptr;
        if (def != nullptr && def->throwDef.throwable)
        {
            const glm::vec2 from = playerWorldCenter();
            const glm::vec2 to = combatrules::throwLanding(from, mMouseWorld,
                                                           def->throwDef.rangeTiles * static_cast<float>(mTileMap.tileWidth()));
            const float distance = glm::length(to - from);
            const int steps = std::max(4, static_cast<int>(distance / 14.0f));
            for (int i = 1; i < steps; ++i)
            {
                const float p = static_cast<float>(i) / static_cast<float>(steps);
                dot(from + (to - from) * p, combatrules::throwArcHeight(p, distance), 2.0f,
                    glm::vec4(0.96f, 0.95f, 0.93f, 0.55f));
            }
            for (int i = 0; i < 12; ++i)
            {
                const float a = static_cast<float>(i) / 12.0f * 6.2831853f;
                dot(to + glm::vec2(std::cos(a), std::sin(a)) * 8.0f, 0.0f, 2.0f, glm::vec4(0.94f, 0.71f, 0.30f, 0.85f));
            }
        }
    }

    // A shot's line, for a frame or so.
    if (mShotLineTimer > 0.0f)
    {
        const float alpha = std::clamp(mShotLineTimer / kShotLineSeconds, 0.0f, 1.0f) * 0.8f;
        for (const auto& [a, b] : mShotLines)
        {
            const float length = glm::length(b - a);
            const int steps = std::max(2, static_cast<int>(length / 5.0f));
            for (int i = 0; i <= steps; ++i)
            {
                dot(a + (b - a) * (static_cast<float>(i) / static_cast<float>(steps)), 6.0f, 1.5f,
                    glm::vec4(1.0f, 0.92f, 0.70f, alpha));
            }
        }
    }

    // A shove: a brief white sweep in front.
    if (mShoveArcTimer > 0.0f)
    {
        const float alpha = std::clamp(mShoveArcTimer / kShoveArcSeconds, 0.0f, 1.0f) * 0.7f;
        const glm::vec2 centre = playerWorldCenter();
        const float half = glm::radians(combatrules::kShoveArcDegrees * 0.5f);
        for (int i = 0; i <= 10; ++i)
        {
            const float a = mShoveArcAngle - half + half * 2.0f * static_cast<float>(i) / 10.0f;
            dot(centre + glm::vec2(std::cos(a), std::sin(a)) * combatrules::kShoveRangePixels, 4.0f, 3.0f,
                glm::vec4(1.0f, 1.0f, 1.0f, alpha));
        }
    }
}
