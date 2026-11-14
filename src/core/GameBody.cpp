// Body and mind (phase 5): who the character is (traits, skills), what their
// wounds, tiredness and fear do to them, treating them, the BODY tab, and
// the things a tired mind sees. Rules in world/TraitRules, SkillRules,
// WoundRules, SleepRules and MindRules; the false signs in
// systems/HallucinationSystem.
#include "core/GameShared.h"
#include "world/WeatherRules.h"

using namespace gameshared;

namespace
{
constexpr float kTwoPi = 6.2831853f;
constexpr float kRunXpPixels = 20.0f * 32.0f; // 20 tiles run
constexpr float kRebleedRunSeconds = 10.0f;   // a deep leg wound: each 10 s of running, a chance
constexpr float kHiddenTiles = 12.0f;         // none of the dead hunting within this: out of sight

// Pack slots holding something that can treat this wound, one per kind.
std::vector<int> treatmentSlots(const Inventory& inv, const ItemDatabase& db, const Wound& wound)
{
    std::vector<int> slots;
    std::vector<int> seen;
    for (int i = 0; i < Inventory::kMaxSlots; ++i)
    {
        const InventorySlot& slot = inv.slots[static_cast<std::size_t>(i)];
        const ItemDef* def = slot.itemId >= 0 && slot.count > 0 ? db.find(slot.itemId) : nullptr;
        if (def == nullptr || def->treats.empty()) continue;
        if (std::find(seen.begin(), seen.end(), slot.itemId) != seen.end()) continue;
        if (!woundrules::canTreat(woundrules::treatmentFromName(def->treats), wound)) continue;
        seen.push_back(slot.itemId);
        slots.push_back(i);
    }
    return slots;
}

std::string lowerFirst(std::string text)
{
    if (!text.empty()) text[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
    return text;
}
} // namespace

// ---------------------------------------------------------------------------
// Who they are

float Game::bodyRoll()
{
    // The run's seed and a count: the same run rolls the same body.
    std::uint32_t h = mSaveManager.currentRunSeed() ^ 0xB0D1E5u ^ ((++mBodyRolls) * 0x9E3779B9u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
}

void Game::applyCharacter(const Traits& traits, bool fresh)
{
    if (mPlayerEntity == kInvalidEntity) return;
    if (!mWorld.hasComponent<Traits>(mPlayerEntity)) mWorld.addComponent<Traits>(mPlayerEntity, Traits{});
    if (!mWorld.hasComponent<Skills>(mPlayerEntity)) mWorld.addComponent<Skills>(mPlayerEntity, Skills{});

    // The bloodline is the family's, not a choice.
    Traits& own = mWorld.getComponent<Traits>(mPlayerEntity);
    const std::vector<BloodlineTrait> bloodlines = own.bloodlines;
    own = traits;
    own.bloodlines = bloodlines;

    if (fresh)
    {
        Skills& skills = mWorld.getComponent<Skills>(mPlayerEntity);
        skills = Skills{};
        const double now = absoluteHour();
        for (std::size_t i = 0; i < kSkillCount; ++i) skills.lastUsedHour[i] = now;
        const auto levels = traitrules::startingLevels(own);
        for (std::size_t i = 0; i < kSkillCount; ++i)
        {
            // What runs in the family (phase 10), where it is more.
            const int level = std::max(levels[i], mSkillEchoes[i]);
            if (level > 0) skillrules::setLevel(skills, static_cast<Skill>(i), level, now);
        }
        if (mWorld.hasComponent<Health>(mPlayerEntity))
        {
            Health& health = mWorld.getComponent<Health>(mPlayerEntity);
            health.baseMaximum = traitrules::maxHealth(own);
            health.maximum = health.baseMaximum;
            if (mWorld.hasComponent<Wounds>(mPlayerEntity))
            {
                health.maximum = woundrules::maxHealth(health.baseMaximum, mWorld.getComponent<Wounds>(mPlayerEntity).active);
            }
            health.current = health.maximum;
        }
    }
    if (mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        mWorld.getComponent<Inventory>(mPlayerEntity).maxWeight = playerCarryLimit();
    }
}

const Traits* Game::playerTraits() const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Traits>(mPlayerEntity)) return nullptr;
    return &mWorld.getComponent<Traits>(mPlayerEntity);
}

int Game::skillLevel(Skill skill) const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Skills>(mPlayerEntity)) return 0;
    return mWorld.getComponent<Skills>(mPlayerEntity).of(skill);
}

void Game::awardXp(Skill skill, float amount)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Skills>(mPlayerEntity) || amount <= 0.0f) return;
    const skillrules::XpResult result =
        skillrules::awardXp(mWorld.getComponent<Skills>(mPlayerEntity), skill, amount, absoluteHour());
    if (result.leveledUp)
    {
        // One quiet line.
        mInteractionMessage = std::string(skillrules::name(skill)) + " " + std::to_string(result.level);
        mInteractionMessageTimer = 3.0f;
    }
}

float Game::playerCarryLimit() const
{
    const Traits* traits = playerTraits();
    const float base = traits == nullptr ? Inventory::kMaxCarryWeight : traitrules::carryLimitKg(*traits, skillLevel(Skill::Fitness));
    // A backpack carries more (phase 6): the best one counts.
    float bonus = 0.0f;
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        for (const InventorySlot& slot : mWorld.getComponent<Inventory>(mPlayerEntity).slots)
        {
            const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
            if (item != nullptr) bonus = std::max(bonus, item->carryBonus);
        }
    }
    return base + bonus;
}

void Game::addPanic(float amount)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<MentalState>(mPlayerEntity)) return;
    MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
    if (amount > 0.0f)
    {
        const Traits* traits = playerTraits();
        if (traits != nullptr) amount *= traitrules::panicGainScale(*traits);
    }
    mental.panic = std::clamp(mental.panic + amount, 0.0f, 100.0f);
}

void Game::addDepression(float amount)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<MentalState>(mPlayerEntity)) return;
    MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
    // Hardened (phase 10): every gain is 0.7 of itself below 60.
    if (amount > 0.0f && mental.depression < MentalStateSystem::kHardenedBelow)
    {
        if (const Traits* traits = playerTraits(); traits != nullptr && traits->has(BloodlineTrait::Hardened))
        {
            amount *= MentalStateSystem::kHardenedDepressionFactor;
        }
    }
    mental.depression = std::clamp(mental.depression + amount, 0.0f, 100.0f);
}

WoundContext Game::woundContext() const
{
    WoundContext context;
    context.seed = mSaveManager.currentRunSeed();
    context.hopeless = mTitleToneIndex == 0;
    // In a bed (a good night's spot): rest heals faster.
    context.bedRest = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Sleeping>(mPlayerEntity) &&
                      mWorld.getComponent<Sleeping>(mPlayerEntity).quality >= 0.8f;
    context.medicineLevel = skillLevel(Skill::Medicine);
    const Traits* traits = playerTraits();
    context.healTimeScale = traits != nullptr ? traitrules::healTimeScale(*traits) : 1.0f;
    return context;
}

float Game::heldActionScale() const
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<MentalState>(mPlayerEntity)) return 1.0f;
    return mindrules::heldActionScale(mindrules::panicBand(mWorld.getComponent<MentalState>(mPlayerEntity).panic));
}

// ---------------------------------------------------------------------------
// Each tick

void Game::prepareBodyModifiers()
{
    if (mPlayerEntity == kInvalidEntity) return;
    const Traits* traits = playerTraits();
    static const std::vector<Wound> kNoWounds;
    const std::vector<Wound>& wounds =
        mWorld.hasComponent<Wounds>(mPlayerEntity) ? mWorld.getComponent<Wounds>(mPlayerEntity).active : kNoWounds;
    const woundrules::BodyEffects body = woundrules::effects(wounds);

    sleeprules::Band sleepBand = sleeprules::Band::Fine;
    bool rested = false;
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        sleepBand = sleeprules::band(sleeprules::shownDebt(needs.sleepDebt, needs.coffeeHours));
        rested = needs.restedHours > 0.0f;
    }
    mindrules::PanicBand panicBand = mindrules::PanicBand::Calm;
    mindrules::DepressionBand depressionBand = mindrules::DepressionBand::Fine;
    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        panicBand = mindrules::panicBand(mental.panic);
        depressionBand = mindrules::depressionBand(mental.depression);
    }
    const int fighting = skillLevel(Skill::Fighting);

    // The legs.
    MoveModifiers move;
    move.speedScale = (traits != nullptr ? traitrules::moveScale(*traits) : 1.0f) * body.moveScale *
                      mindrules::moveScale(depressionBand);
    move.runSpeedScale = skillrules::runSpeedScale(skillLevel(Skill::Fitness));
    move.canRun = body.canRun;
    move.runAtWalkSpeed = body.runAtWalkSpeed;
    // Weather (phase 7): a blizzard slows you outdoors; ice is slippery.
    {
        const glm::ivec2 at = playerTile();
        const bool outdoors = mTileMap.inBounds(at.x, at.y) && !mTileMap.isIndoors(at.x, at.y);
        move.speedScale *= weatherrules::playerMoveScale(mWeatherState.condition, outdoors);
        if (mTileMap.waterFrozen() && mTileMap.isWater(at.x, at.y)) move.accelScale = weatherrules::kIceAccelerationScale;
    }
    mInputSystem.setModifiers(move);

    // The arms.
    CombatBody combat;
    combat.staminaScale = mindrules::staminaCostScale(panicBand);
    combat.swingStaminaScale = skillrules::swingStaminaScale(fighting) * body.swingStaminaScale;
    combat.windupScale = skillrules::windupScale(fighting) * body.windupScale * sleeprules::windupScale(sleepBand);
    combat.hesitationChance = mindrules::hesitationChance(depressionBand);
    combat.wearScale = skillrules::wearScale(fighting);
    combat.missWear = body.missWearScale > 1.0f ? body.missWearScale : 0.0f;
    combat.fightingLevel = fighting;
    combat.doubleBlowChance = skillrules::doubleBlowChance(fighting);
    combat.shoveFellsWeakened = skillrules::shoveFellsWeakened(fighting);
    combat.extraSpreadDegrees = sleeprules::aimSpreadDegrees(sleepBand);
    combat.reloadScale = mindrules::heldActionScale(panicBand);
    combat.fumbleChance = mindrules::fumbleChance(panicBand);
    combat.regenScale = sleeprules::staminaRegenScale(sleepBand, rested);
    // Heat and smoke (phase 7): overheated -30 %, coughing -50 %.
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        if (needs.bodyTemperature > weatherrules::kOverheatedAbove) combat.regenScale *= weatherrules::kOverheatedRegenScale;
        if (needs.smokeSeconds >= weatherrules::kCoughAfterSeconds) combat.regenScale *= weatherrules::kCoughingRegenScale;
    }
    combat.noTwoHanded = body.noTwoHanded;
    mCombatSystem.setBody(combat);

    // What the mind takes in.
    MindInputs mind;
    if (traits != nullptr)
    {
        mind.panicGainScale = traitrules::panicGainScale(*traits);
        mind.companyScale = traitrules::companyScale(*traits);
    }
    if (mWorld.hasComponent<Transform>(mPlayerEntity))
    {
        const glm::vec2 here = playerWorldCenter();
        const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
        bool hunted = false;
        bool horde = false;
        bool heard = false;
        mWorld.forEach<ZombieAI, Transform, Health>(
            [&](Entity, ZombieAI& ai, Transform& t, Health& h)
            {
                if (h.current <= 0.0f) return;
                const float d = glm::length(glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f) - here) / tile;
                if (d < kHiddenTiles && (ai.state == ZombieState::Chase || ai.state == ZombieState::Attack)) hunted = true;
                if (d < mindrules::kHordePanicTiles && ai.horde > 0) horde = true;
                if (d < mindrules::kDarkIndoorsHeardTiles) heard = true;
            });
        const bool crouched = mInput.isKeyDown(SDL_SCANCODE_C) || mInput.isKeyDown(SDL_SCANCODE_LCTRL);
        bool still = true;
        if (mWorld.hasComponent<Velocity>(mPlayerEntity))
        {
            const Velocity& v = mWorld.getComponent<Velocity>(mPlayerEntity);
            still = v.dx * v.dx + v.dy * v.dy < 4.0f;
        }
        mind.crouchedStillHidden = crouched && still && !hunted;
        mind.hordeNear = horde;
        const glm::ivec2 me = playerTile();
        mind.darkIndoorsHeard = heard && mTileMap.isIndoors(me.x, me.y) && nightFactor() > 0.6f && !mFlashlightOn;
    }
    mMentalStateSystem.setInputs(mind);
}

void Game::updateBody(float dtSeconds)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Health>(mPlayerEntity)) return;
    const double now = absoluteHour();
    const float hours = static_cast<float>(static_cast<double>(dtSeconds) * kGameHoursPerRealSecond);
    const bool asleep = mWorld.hasComponent<Sleeping>(mPlayerEntity);
    mBodyMessageCooldown = std::max(0.0f, mBodyMessageCooldown - dtSeconds);

    // Skills unused for three days begin to slip; the pack follows Fitness.
    if (mWorld.hasComponent<Skills>(mPlayerEntity))
    {
        skillrules::decay(mWorld.getComponent<Skills>(mPlayerEntity), now, hours);
    }
    if (mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        inv.maxWeight = playerCarryLimit();
        // Fitness: carrying heavy, an hour at a time.
        if (InventorySystem::totalWeight(inv, mItemDatabase) >= skillrules::kHeavyCarryKg && !asleep)
        {
            mCarryXpHours += hours;
            if (mCarryXpHours >= 1.0f)
            {
                mCarryXpHours -= 1.0f;
                awardXp(Skill::Fitness, skillrules::kXpHeavyCarryPerHour);
            }
        }
    }

    // Fitness: running, twenty tiles at a time.
    static const std::vector<Wound> kNoWounds;
    const std::vector<Wound>& wounds =
        mWorld.hasComponent<Wounds>(mPlayerEntity) ? mWorld.getComponent<Wounds>(mPlayerEntity).active : kNoWounds;
    const woundrules::BodyEffects body = woundrules::effects(wounds);
    const bool running = mWorld.hasComponent<Facing>(mPlayerEntity) && mWorld.getComponent<Facing>(mPlayerEntity).running &&
                         mWorld.hasComponent<Velocity>(mPlayerEntity);
    if (running)
    {
        const Velocity& v = mWorld.getComponent<Velocity>(mPlayerEntity);
        const float pixels = std::sqrt(v.dx * v.dx + v.dy * v.dy) * dtSeconds;
        mRunXpPixels += pixels;
        if (mRunXpPixels >= kRunXpPixels)
        {
            mRunXpPixels -= kRunXpPixels;
            awardXp(Skill::Fitness, skillrules::kXpRunPer20Tiles);
        }
    }

    // Coffee, the crash after it, the shakes, and the rested glow.
    sleeprules::Band sleepBand = sleeprules::Band::Fine;
    float shownDebt = 0.0f;
    float bodyTemperature = 37.0f;
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        if (needs.coffeeHours > 0.0f)
        {
            needs.coffeeHours -= hours;
            if (needs.coffeeHours <= 0.0f)
            {
                needs.coffeeHours = 0.0f;
                needs.sleepDebt = std::min(needs.maxSleepDebt, needs.sleepDebt + sleeprules::kCoffeeCrash);
                mInteractionMessage = "The coffee wears off.";
                mInteractionMessageTimer = 2.5f;
            }
        }
        needs.coffeeJitterHours = std::max(0.0f, needs.coffeeJitterHours - hours);
        needs.restedHours = std::max(0.0f, needs.restedHours - hours);
        shownDebt = sleeprules::shownDebt(needs.sleepDebt, needs.coffeeHours);
        sleepBand = sleeprules::band(shownDebt);
        bodyTemperature = needs.bodyTemperature;
    }

    // Dead on your feet: the eyes close by themselves now and then.
    mMicrosleepLeft = std::max(0.0f, mMicrosleepLeft - dtSeconds);
    if (sleeprules::microsleeps(sleepBand) && !asleep)
    {
        if (mMicrosleepIn < 0.0f) mMicrosleepIn = sleeprules::microsleepInterval(bodyRoll());
        mMicrosleepIn -= dtSeconds;
        if (mMicrosleepIn <= 0.0f)
        {
            mMicrosleepLeft = sleeprules::kMicrosleepSeconds;
            mMicrosleepIn = sleeprules::microsleepInterval(bodyRoll());
        }
    }
    else
    {
        mMicrosleepIn = -1.0f;
    }

    const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
    int holders = 0;
    mWorld.forEach<ZombieAI, Health>(
        [&](Entity, ZombieAI& ai, Health& h)
        {
            if (ai.holding && h.current > 0.0f) ++holders;
        });

    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        const mindrules::PanicBand panicBand = mindrules::panicBand(mental.panic);

        // Grabbed: a fresh jolt each time one gets hold.
        if (holders > mPrevHolders) addPanic(mindrules::kGrabbedPanic);

        // A deep breath: C held, still, out of sight, three seconds.
        mental.deepBreathCooldown = std::max(0.0f, mental.deepBreathCooldown - dtSeconds);
        const bool crouched = mInput.isKeyDown(SDL_SCANCODE_C) || mInput.isKeyDown(SDL_SCANCODE_LCTRL);
        bool still = true;
        if (mWorld.hasComponent<Velocity>(mPlayerEntity))
        {
            const Velocity& v = mWorld.getComponent<Velocity>(mPlayerEntity);
            still = v.dx * v.dx + v.dy * v.dy < 4.0f;
        }
        bool hunted = false;
        {
            const glm::vec2 here = playerWorldCenter();
            const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
            mWorld.forEach<ZombieAI, Transform, Health>(
                [&](Entity, ZombieAI& ai, Transform& t, Health& h)
                {
                    if (h.current <= 0.0f) return;
                    const float d = glm::length(glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f) - here) / tile;
                    if (d < kHiddenTiles && (ai.state == ZombieState::Chase || ai.state == ZombieState::Attack)) hunted = true;
                });
        }
        if (crouched && still && !hunted && !asleep && mental.deepBreathCooldown <= 0.0f && mental.panic > 0.0f)
        {
            mental.deepBreathTimer += dtSeconds;
            if (mental.deepBreathTimer >= mindrules::kDeepBreathSeconds)
            {
                mental.panic = std::max(0.0f, mental.panic - mindrules::kDeepBreathRelief);
                mental.deepBreathTimer = 0.0f;
                mental.deepBreathCooldown = mindrules::kDeepBreathCooldownSeconds;
                mInteractionMessage = "You breathe. In, and out.";
                mInteractionMessageTimer = 2.5f;
            }
        }
        else
        {
            mental.deepBreathTimer = 0.0f;
        }

        // Breaking: grabbed or badly hurt, the legs run without you.
        mental.lostControlCooldown += dtSeconds;
        if (mental.lostControlTimer > 0.0f)
        {
            mental.lostControlTimer = std::max(0.0f, mental.lostControlTimer - dtSeconds);
        }
        else if (!asleep && mindrules::losesControl(panicBand, holders > 0, health.current / std::max(1.0f, health.maximum),
                                                    mental.lostControlCooldown))
        {
            const glm::vec2 here = playerWorldCenter();
            glm::vec2 nearest(0.0f);
            float best = 1e9f;
            mWorld.forEach<ZombieAI, Transform, Health>(
                [&](Entity, ZombieAI&, Transform& t, Health& h)
                {
                    if (h.current <= 0.0f) return;
                    const glm::vec2 at(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
                    const float d = glm::length(at - here);
                    if (d < best)
                    {
                        best = d;
                        nearest = at;
                    }
                });
            glm::vec2 away = here - nearest;
            if (best > 1e8f || glm::length(away) < 0.01f)
            {
                const float a = bodyRoll() * kTwoPi;
                away = glm::vec2(std::cos(a), std::sin(a));
            }
            mLostControlDir = glm::normalize(away);
            mental.lostControlTimer = mindrules::kLostControlSeconds;
            mental.lostControlCooldown = 0.0f;
            mInteractionMessage = "You run. You cannot stop yourself.";
            mInteractionMessageTimer = 2.5f;
        }

        // Numbness fades a day after the last kill.
        mental.hoursSinceKill += hours;
        mental.desensitisation =
            std::max(0.0f, mental.desensitisation - mindrules::desensitisationFade(mental.hoursSinceKill, hours));

        // Panic held high: the mind starts to play tricks.
        mental.panicHighSeconds = mental.panic >= mindrules::kSeeingPanic ? mental.panicHighSeconds + dtSeconds : 0.0f;

        // A week alive.
        if (mCurrentDay >= 8 && !mental.weekRelieved)
        {
            mental.weekRelieved = true;
            mental.depression = std::max(0.0f, mental.depression - mindrules::kWeekAliveRelief);
            mInteractionMessage = "A week.";
            mInteractionMessageTimer = 4.0f;
        }

        // Things that are not there. They touch nothing in the world.
        const bool seeing = !asleep && mindrules::seesThings(shownDebt, mental.panicHighSeconds, bodyTemperature);
        HallucinationView view;
        view.player = playerWorldCenter();
        const glm::vec2 look = mMouseWorld - view.player;
        view.aim = glm::length(look) > 0.01f ? glm::normalize(look) : glm::vec2(1.0f, 0.0f);
        mHallucinations.update(dtSeconds, seeing, view,
                               [this](FalseSign::Kind kind, float a, float b) { return placeFalseSign(kind, a, b); });
        for (const FalseSign& sign : mHallucinations.takeStarted())
        {
            const float distance = glm::length(sign.world - view.player) / 32.0f;
            const float volume = std::clamp(0.6f - distance * 0.03f, 0.15f, 0.6f);
            if (sign.kind == FalseSign::Kind::Knock) mAudioManager.playSound(SoundId::FootstepLoud, volume);
            if (sign.kind == FalseSign::Kind::Groan) mAudioManager.playSound(SoundId::ZombieGrowl, volume * 0.6f);
        }
    }
    mPrevHolders = holders;

    // Fighting: a shove that met one already lunging.
    if (mCombatSystem.shoveSavesThisTick() > 0)
    {
        awardXp(Skill::Fighting, skillrules::kXpShoveSave * static_cast<float>(mCombatSystem.shoveSavesThisTick()));
    }

    // Deep wounds not stitched open again: after a swing, after a run.
    const auto rebleed = [&](const char* line)
    {
        if (!mWorld.hasComponent<Bleeding>(mPlayerEntity))
        {
            Bleeding bleeding;
            bleeding.bleedRate = 1.0f;
            bleeding.remainingSeconds = 6.0f;
            mWorld.addComponent<Bleeding>(mPlayerEntity, bleeding);
        }
        mInteractionMessage = line;
        mInteractionMessageTimer = 2.5f;
        ++mRebleeds;
    };
    if (body.rebleedOnSwing && mCombatSystem.swungThisTick() && bodyRoll() < woundrules::kRebleedOnSwingChance)
    {
        rebleed("The wound in your arm opens again.");
    }
    if (body.rebleedOnRun && running)
    {
        mRebleedRunSeconds += dtSeconds;
        if (mRebleedRunSeconds >= kRebleedRunSeconds)
        {
            mRebleedRunSeconds = 0.0f;
            if (bodyRoll() < woundrules::kRebleedOnSwingChance) rebleed("The wound in your leg opens again.");
        }
    }

    // The wounds themselves: healing, dirt, infection, turning.
    mWoundSystem.update(mWorld, mPlayerEntity, dtSeconds, mGameHours, woundContext());
    if (mWoundSystem.turnedToDeath())
    {
        beginDeathState("Turned on Day " + std::to_string(mCurrentDay));
        return;
    }
    if (mWoundSystem.newlyTurning() > 0)
    {
        mInteractionMessage = "The bite has gone black. Nothing will help now.";
        mInteractionMessageTimer = 5.0f;
    }
    else if (mWoundSystem.newlyInfected() > 0)
    {
        mInteractionMessage = "A wound is hot and swollen. Infected.";
        mInteractionMessageTimer = 4.0f;
    }
    else if (mWoundSystem.dressingsDirtied() > 0 && mBodyMessageCooldown <= 0.0f)
    {
        mInteractionMessage = "A dressing has gone dirty. It wants changing.";
        mInteractionMessageTimer = 3.0f;
        mBodyMessageCooldown = 20.0f;
    }
}

void Game::onPlayerKill()
{
    awardXp(Skill::Fighting, skillrules::kXpKill);
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<MentalState>(mPlayerEntity)) return;
    MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
    const Traits* traits = playerTraits();
    // The first of the run frightens, unless you are already used to it.
    if (!mental.firstKillDone)
    {
        mental.firstKillDone = true;
        if (mindrules::firstKillFrightens(mental.desensitisation)) addPanic(mindrules::kFirstKillPanic);
    }
    if (traits != nullptr) addPanic(traitrules::killPanic(*traits)); // Squeamish
    const float gain = 1.5f * (traits != nullptr ? traitrules::desensitisationScale(*traits) : 1.0f);
    mental.desensitisation = std::min(100.0f, mental.desensitisation + gain);
    mental.hoursSinceKill = 0.0f;
    addDepression(mindrules::killWeight(mRunStats.kills, mental.desensitisation));
}

// ---------------------------------------------------------------------------
// Treatment

bool Game::treatWithItem(int slotIndex, int woundIndex)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity) ||
        !mWorld.hasComponent<Wounds>(mPlayerEntity) || slotIndex < 0 || slotIndex >= Inventory::kMaxSlots)
    {
        return false;
    }
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item == nullptr || item->treats.empty()) return false;
    const woundrules::Treatment treatment = woundrules::treatmentFromName(item->treats);
    if (treatment == woundrules::Treatment::None) return false;
    Wounds& wounds = mWorld.getComponent<Wounds>(mPlayerEntity);
    int index = woundIndex;
    if (index < 0 || index >= static_cast<int>(wounds.active.size()) ||
        !woundrules::canTreat(treatment, wounds.active[static_cast<std::size_t>(index)]))
    {
        index = woundrules::worstFor(treatment, wounds.active);
    }
    if (index < 0) return false;

    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::Treat;
    mBaseAction.slot = slotIndex;
    mBaseAction.wound = index;
    mBaseAction.treatment = static_cast<int>(treatment);
    mBaseAction.startPos = playerWorldCenter();
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
    float seconds = woundrules::treatSeconds(treatment, skillLevel(Skill::Medicine));
    if (seconds <= 0.0f)
    {
        finishTreatment(); // a dressing, a pill, a splash: at once
        return true;
    }
    // Numb: the hands are slow to start.
    if (mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        bodyRoll() < mindrules::hesitationChance(mindrules::depressionBand(mWorld.getComponent<MentalState>(mPlayerEntity).depression)))
    {
        seconds += mindrules::kHesitationSeconds;
    }
    mBaseAction.total = seconds;
    mBaseAction.label = treatment == woundrules::Treatment::Stitch ? "Stitching the wound" : "Setting the bone";
    if (mShowInventory) mShowInventory = false; // watch the bar, not the pack
    mBodyChoosing = false;
    return true;
}

void Game::finishTreatment()
{
    const int slotIndex = mBaseAction.slot;
    const int woundIndex = mBaseAction.wound;
    const auto treatment = static_cast<woundrules::Treatment>(mBaseAction.treatment);
    mBaseAction = BaseAction{};
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity) ||
        !mWorld.hasComponent<Wounds>(mPlayerEntity) || slotIndex < 0 || slotIndex >= Inventory::kMaxSlots)
    {
        return;
    }
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    Wounds& wounds = mWorld.getComponent<Wounds>(mPlayerEntity);
    if (item == nullptr || woundrules::treatmentFromName(item->treats) != treatment || woundIndex < 0 ||
        woundIndex >= static_cast<int>(wounds.active.size()) ||
        !woundrules::canTreat(treatment, wounds.active[static_cast<std::size_t>(woundIndex)]))
    {
        mInteractionMessage = "It's no longer to hand.";
        mInteractionMessageTimer = 2.0f;
        return;
    }

    // Panicking: the hands slip, and nothing is used.
    if (mWorld.hasComponent<MentalState>(mPlayerEntity) &&
        bodyRoll() < mindrules::fumbleChance(mindrules::panicBand(mWorld.getComponent<MentalState>(mPlayerEntity).panic)))
    {
        mInteractionMessage = "Your hands shake. It slips. Try again.";
        mInteractionMessageTimer = 2.5f;
        return;
    }

    const double now = absoluteHour();
    Wound& wound = wounds.active[static_cast<std::size_t>(woundIndex)];
    const std::string what = lowerFirst(woundrules::label(wound));
    const int medicine = skillLevel(Skill::Medicine);
    int treated = 0;
    if (treatment == woundrules::Treatment::CureInfection)
    {
        // Antibiotics: every infection at once, as before.
        for (Wound& w : wounds.active)
        {
            if (!woundrules::canTreat(treatment, w)) continue;
            woundrules::apply(treatment, w);
            ++treated;
        }
    }
    else
    {
        woundrules::apply(treatment, wound);
        treated = 1;
        // A Disinfected Bandage is big enough for a second wound.
        if (treatment == woundrules::Treatment::DressClean)
        {
            const int second = woundrules::worstFor(treatment, wounds.active);
            if (second >= 0)
            {
                woundrules::apply(treatment, wounds.active[static_cast<std::size_t>(second)]);
                ++treated;
            }
        }
    }

    // What it costs you.
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        if (treatment == woundrules::Treatment::Stitch && now - needs.lastPainkillerHour > woundrules::kPainkillerWindowHours)
        {
            addPanic(woundrules::kStitchPanic);
        }
    }
    if (treatment == woundrules::Treatment::Disinfect) addPanic(woundrules::kDisinfectPanic);
    if (item->stopsBleeding || treatment == woundrules::Treatment::Dress || treatment == woundrules::Treatment::DressClean ||
        treatment == woundrules::Treatment::Stitch)
    {
        if (mWorld.hasComponent<Bleeding>(mPlayerEntity)) mWorld.removeComponent<Bleeding>(mPlayerEntity);
    }
    if (item->illnessRelief > 0.0f && mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        needs.illness = std::max(0.0f, needs.illness - item->illnessRelief);
    }
    if (mWorld.hasComponent<Health>(mPlayerEntity))
    {
        Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        syncHealthWithWounds(health, wounds);
        if (item->healthRestore > 0.0f) health.current = std::min(health.maximum, health.current + item->healthRestore);
    }

    const std::string itemName = item->name;
    // Used up: a Bottle of Spirits by the quarter; Antibiotics by the half
    // for a practised hand (Medicine 4); the rest one at a time.
    if (treatment == woundrules::Treatment::Disinfect)
    {
        slot.condition -= 1.0f / woundrules::kSpiritsUsesPerBottle;
        if (slot.condition <= 0.01f) InventorySystem::consumeOne(inv, mItemDatabase, slotIndex);
    }
    else if (treatment == woundrules::Treatment::CureInfection && skillrules::antibioticsHalfDose(medicine) &&
             slot.condition > 0.75f)
    {
        slot.condition = 0.5f;
    }
    else
    {
        InventorySystem::consumeOne(inv, mItemDatabase, slotIndex);
    }

    const bool skilled = treatment == woundrules::Treatment::Stitch || treatment == woundrules::Treatment::Splint;
    awardXp(Skill::Medicine, skilled ? skillrules::kXpSplintOrStitch : skillrules::kXpTreat);
    std::string message;
    if (treatment == woundrules::Treatment::CureInfection)
    {
        message = "You take the " + itemName + ". The fever should break.";
    }
    else
    {
        message = "You " + std::string(woundrules::treatmentVerb(treatment)) + " the " + what + ".";
        if (treated > 1) message += " And a second.";
    }
    // A level gained keeps the line; otherwise say what was done.
    if (mInteractionMessage.empty() || mInteractionMessage.find(skillrules::name(Skill::Medicine)) != 0)
    {
        mInteractionMessage = message;
    }
    mInteractionMessageTimer = 3.0f;
}

// ---------------------------------------------------------------------------
// Comfort

bool Game::useComfortItem(int slotIndex)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity) || slotIndex < 0 ||
        slotIndex >= Inventory::kMaxSlots || !mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        return false;
    }
    Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    InventorySlot& slot = inv.slots[static_cast<std::size_t>(slotIndex)];
    const ItemDef* item = slot.itemId >= 0 && slot.count > 0 ? mItemDatabase.find(slot.itemId) : nullptr;
    if (item == nullptr) return false;
    MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
    const float comfort = mindrules::comfortScale(mindrules::depressionBand(mental.depression));

    if (item->id == mItems.book)
    {
        // Each copy once per character: `loaded` counts the ones read.
        if (slot.loaded >= slot.count)
        {
            mInteractionMessage = "You have read this one.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
        const glm::ivec2 me = playerTile();
        if (!mTileMap.isIndoors(me.x, me.y))
        {
            mInteractionMessage = "Not out here. Somewhere with a roof.";
            mInteractionMessageTimer = 2.0f;
            return true;
        }
        mBaseAction = BaseAction{};
        mBaseAction.kind = BaseAction::Kind::Read;
        mBaseAction.slot = slotIndex;
        mBaseAction.total = mindrules::kBookGameMinutes; // a game minute is a real second
        mBaseAction.label = "Reading";
        mBaseAction.startPos = playerWorldCenter();
        mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
        mShowInventory = false;
        return true;
    }
    if (item->id == mItems.keepsake)
    {
        if (mental.keepsakeUsed)
        {
            mInteractionMessage = "You turn it over in your hands. It does not do what it did.";
            mInteractionMessageTimer = 3.0f;
            return true;
        }
        mental.keepsakeUsed = true;
        mental.depression = std::max(0.0f, mental.depression - mindrules::kKeepsakeRelief * comfort);
        mInteractionMessage = "You hold it a long while. Someone loved this.";
        mInteractionMessageTimer = 3.5f;
        return true;
    }
    return false;
}

void Game::finishReading()
{
    const int slotIndex = mBaseAction.slot;
    mBaseAction = BaseAction{};
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity) || slotIndex < 0 ||
        slotIndex >= Inventory::kMaxSlots || !mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        return;
    }
    InventorySlot& slot = mWorld.getComponent<Inventory>(mPlayerEntity).slots[static_cast<std::size_t>(slotIndex)];
    if (slot.itemId != mItems.book || slot.count <= 0 || slot.loaded >= slot.count) return;
    MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
    const float comfort = mindrules::comfortScale(mindrules::depressionBand(mental.depression));
    mental.depression = std::max(0.0f, mental.depression - mindrules::kBookRelief * comfort);
    ++slot.loaded;
    mInteractionMessage = "You lose yourself in it for a while.";
    mInteractionMessageTimer = 3.0f;
}

// ---------------------------------------------------------------------------
// Boarding: a held action now (Carpentry makes it quicker and stronger)

void Game::startBoarding(const glm::ivec2& tile, bool window)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    if (mBaseAction.kind != BaseAction::Kind::None) return; // R again does not start over
    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    const int boards = window ? mWindows.boards(tile) : mDoors.boards(tile);
    const int maxBoards = window ? Windows::kMaxBoards : Doors::kMaxBoards;
    const bool ready = boards < maxBoards && InventorySystem::countTagged(inv, mItemDatabase, "tool:hammer") > 0 &&
                       InventorySystem::countItem(inv, mItems.woodPlanks) > 0 &&
                       InventorySystem::countItem(inv, mItems.nails) >= 2;
    if (!ready)
    {
        // The board-up itself says what is missing.
        if (window) boardUpWindow(tile);
        else boardUpDoor(tile);
        return;
    }
    mBaseAction = BaseAction{};
    mBaseAction.kind = BaseAction::Kind::Board;
    mBaseAction.tile = tile;
    mBaseAction.window = window;
    mBaseAction.total = skillrules::boardSeconds(skillLevel(Skill::Carpentry));
    mBaseAction.label = window ? "Boarding the window" : "Boarding the door";
    mBaseAction.startPos = playerWorldCenter();
    mBaseAction.startHp = mWorld.hasComponent<Health>(mPlayerEntity) ? mWorld.getComponent<Health>(mPlayerEntity).current : 0.0f;
}

void Game::finishBoarding()
{
    const glm::ivec2 tile = mBaseAction.tile;
    const bool window = mBaseAction.window;
    mBaseAction = BaseAction{};
    const int before = window ? mWindows.boards(tile) : mDoors.boards(tile);
    if (window) boardUpWindow(tile);
    else boardUpDoor(tile);
    const int after = window ? mWindows.boards(tile) : mDoors.boards(tile);
    if (after > before) awardXp(Skill::Carpentry, skillrules::kXpBoard);
    if (after > before && before == 0) noteJournal(journal::Kind::Boarded, window ? "window" : "door");
}

// ---------------------------------------------------------------------------
// Things that are not there

std::optional<glm::vec2> Game::placeFalseSign(FalseSign::Kind kind, float a, float b)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity)) return std::nullopt;
    const glm::vec2 here = playerWorldCenter();
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    float nearTiles = 6.0f;
    float farTiles = 9.0f; // the edge of the view
    if (kind == FalseSign::Kind::Knock || kind == FalseSign::Kind::Groan)
    {
        nearTiles = 4.0f;
        farTiles = 9.0f;
    }
    else if (kind == FalseSign::Kind::MinimapDot)
    {
        nearTiles = 14.0f;
        farTiles = 28.0f;
    }
    const float angle = a * kTwoPi;
    const float distance = (nearTiles + (farTiles - nearTiles) * b) * tile;
    const glm::vec2 at = here + glm::vec2(std::cos(angle), std::sin(angle)) * distance;
    const int tx = static_cast<int>(std::floor(at.x / tile));
    const int ty = static_cast<int>(std::floor(at.y / static_cast<float>(std::max(1, mTileMap.tileHeight()))));
    if (tx < 0 || ty < 0 || tx >= mTileMap.width() || ty >= mTileMap.height()) return std::nullopt;
    if (kind == FalseSign::Kind::Silhouette)
    {
        if (mTileMap.isSolid(tx, ty)) return std::nullopt;
        // Never inside the building you are in.
        const glm::ivec2 me = playerTile();
        const int mine = mTileMap.getBuildingId(me.x, me.y);
        if (mine > 0 && mTileMap.getBuildingId(tx, ty) == mine) return std::nullopt;
    }
    return at;
}

// ---------------------------------------------------------------------------
// Drawing: the narrowing view, closing eyes, the figures

void Game::renderBodyEffects()
{
    if (mRunState != RunState::Playing || mPlayerEntity == kInvalidEntity) return;
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    const float w = static_cast<float>(mWindowWidth);
    const float h = static_cast<float>(mWindowHeight);

    // Figures at the edge of things (drawn as a dark shape of one of the dead).
    const float zoom = mCamera.zoom();
    for (const FalseSign& sign : mHallucinations.signs())
    {
        if (sign.kind != FalseSign::Kind::Silhouette || sign.alpha <= 0.01f) continue;
        const SpriteRegion* region = mSpriteSheet.find("zombie0_walk_s");
        if (region == nullptr) continue;
        const glm::vec2 feet = mCamera.worldToScreen(sign.world);
        const glm::vec2 size = region->pixelSize * zoom;
        const glm::vec4 uv = mSpriteSheet.frameUv("zombie0_walk_s", 0);
        mSpriteBatch.draw(glm::round(feet - glm::vec2(size.x * 0.5f, size.y * 0.92f)), size, uv,
                          glm::vec4(0.02f, 0.02f, 0.03f, 0.62f * sign.alpha));
    }

    // The view narrows (Shaken, a head wound) and its edges soften (Tired).
    float narrow = 0.0f;
    float soften = 0.0f;
    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        narrow += mindrules::vignette(mindrules::panicBand(mWorld.getComponent<MentalState>(mPlayerEntity).panic));
    }
    if (mWorld.hasComponent<Wounds>(mPlayerEntity) && woundrules::effects(mWorld.getComponent<Wounds>(mPlayerEntity).active).headVignette)
    {
        narrow += 0.10f;
    }
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        soften = sleeprules::viewSoften(sleeprules::band(sleeprules::shownDebt(needs.sleepDebt, needs.coffeeHours)));
    }
    const auto edges = [&](float share, float alpha, const glm::vec4& tint)
    {
        // Bands stepping in from each edge, darker at the rim.
        constexpr int kSteps = 6;
        for (int i = 0; i < kSteps; ++i)
        {
            const float t = static_cast<float>(i + 1) / static_cast<float>(kSteps);
            const float bandW = std::round(w * share * 0.5f * t);
            const float bandH = std::round(h * share * 0.5f * t);
            const glm::vec4 c(tint.r, tint.g, tint.b, alpha / static_cast<float>(kSteps));
            mSpriteBatch.draw(glm::vec2(0.0f), glm::vec2(w, bandH), white, c);
            mSpriteBatch.draw(glm::vec2(0.0f, h - bandH), glm::vec2(w, bandH), white, c);
            mSpriteBatch.draw(glm::vec2(0.0f, bandH), glm::vec2(bandW, h - bandH * 2.0f), white, c);
            mSpriteBatch.draw(glm::vec2(w - bandW, bandH), glm::vec2(bandW, h - bandH * 2.0f), white, c);
        }
    };
    if (narrow > 0.0f) edges(narrow * 2.0f, 0.85f, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    if (soften > 0.0f) edges(soften, 0.55f, glm::vec4(0.10f, 0.10f, 0.12f, 1.0f));

    // Eyes shut: a microsleep.
    if (mMicrosleepLeft > 0.0f)
    {
        const float t = mMicrosleepLeft / sleeprules::kMicrosleepSeconds; // 1 -> 0
        const float closed = std::clamp(std::min(1.0f - t, t) * 6.0f, 0.0f, 1.0f);
        mSpriteBatch.draw(glm::vec2(0.0f), glm::vec2(w, h), white, glm::vec4(0.0f, 0.0f, 0.0f, 0.97f * closed));
    }
}

// ---------------------------------------------------------------------------
// The BODY tab

void Game::renderBodyTab()
{
    if (mPlayerEntity == kInvalidEntity) return;
    const InventoryLayout layout = computeInventoryLayout(mWindowWidth, mWindowHeight);
    const glm::vec4 white = mSpriteSheet.uvRect("solid_white");
    renderDockedHeader(layout.panelPos, layout.panelSize.x, false);

    const float left = layout.panelPos.x + InventoryLayout::kPadding;
    const float right = layout.panelPos.x + layout.panelSize.x - InventoryLayout::kPadding;
    const float innerW = right - left;
    const float colGap = 24.0f;
    const float leftW = std::floor((innerW - colGap) * 0.55f);
    const float skillX = left + leftW + colGap;
    const float skillW = right - skillX;
    const auto rect = [&](const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color)
    {
        mSpriteBatch.draw(pos, size, white, color);
    };
    const auto text = [&](const glm::vec2& pos, const std::string& s, const glm::vec4& color, float scale,
                          TextAlign align = TextAlign::Left)
    {
        mFont.drawText(mSpriteBatch, pos, s, color, scale, align);
    };
    const auto fit = [&](const std::string& s, float width, float scale)
    {
        return truncateHudText(s, static_cast<std::size_t>(std::max(4.0f, width / (6.0f * scale))));
    };

    // Left: wounds and what each needs, then the body's state.
    float y = layout.panelPos.y + 84.0f;
    text(glm::vec2(left, y), "WOUNDS", kPanelDim, 1.0f);
    y += 18.0f;
    const bool seesInfection = skillrules::seesInfectionEarly(skillLevel(Skill::Medicine));
    const std::vector<Wound> wounds =
        mWorld.hasComponent<Wounds>(mPlayerEntity) ? mWorld.getComponent<Wounds>(mPlayerEntity).active : std::vector<Wound>{};
    const int cursor = wounds.empty() ? -1 : std::clamp(mBodyCursor, 0, static_cast<int>(wounds.size()) - 1);
    if (wounds.empty())
    {
        text(glm::vec2(left, y + 4.0f), mWorld.hasComponent<Bleeding>(mPlayerEntity) ? "Bleeding" : "No wounds.",
             mWorld.hasComponent<Bleeding>(mPlayerEntity) ? kPanelAmber : kPanelDim, 2.0f);
        y += 30.0f;
    }
    const Inventory* inv = mWorld.hasComponent<Inventory>(mPlayerEntity) ? &mWorld.getComponent<Inventory>(mPlayerEntity) : nullptr;
    for (int i = 0; i < static_cast<int>(wounds.size()) && y < layout.footerY - 160.0f; ++i)
    {
        const Wound& wound = wounds[static_cast<std::size_t>(i)];
        const bool chosen = i == cursor;
        const std::string need = woundrules::need(wound, seesInfection);
        const bool attention = need.rfind("needs", 0) == 0 || need.rfind("infected", 0) == 0 ||
                               need.rfind("dirty", 0) == 0 || need.rfind("dressing", 0) == 0 || need.rfind("turning", 0) == 0;
        if (chosen) rect(glm::vec2(left - 4.0f, y - 3.0f), glm::vec2(leftW + 8.0f, 40.0f), kPanelInk);
        text(glm::vec2(left, y), fit(woundrules::label(wound), leftW, 2.0f), chosen ? kPanelOnInk : kPanelInk, 2.0f);
        text(glm::vec2(left, y + 20.0f), fit(need, leftW, 1.0f), chosen ? kPanelOnInk : (attention ? kPanelAmber : kPanelDim), 1.0f);
        y += 44.0f;

        // Enter on a wound: what you carry that helps.
        if (chosen && mBodyChoosing && inv != nullptr)
        {
            const std::vector<int> slots = treatmentSlots(*inv, mItemDatabase, wound);
            const int choice = slots.empty() ? -1 : std::clamp(mBodyChoice, 0, static_cast<int>(slots.size()) - 1);
            if (slots.empty())
            {
                text(glm::vec2(left + 12.0f, y), "Nothing you carry helps.", kPanelDim, 1.0f);
                y += 18.0f;
            }
            for (int k = 0; k < static_cast<int>(slots.size()); ++k)
            {
                const InventorySlot& slot = inv->slots[static_cast<std::size_t>(slots[static_cast<std::size_t>(k)])];
                const ItemDef* def = mItemDatabase.find(slot.itemId);
                const bool on = k == choice;
                if (on) rect(glm::vec2(left + 8.0f, y - 2.0f), glm::vec2(leftW - 8.0f, 18.0f), kPanelInk);
                const std::string line = std::string(def != nullptr ? def->name : "?") + "  x" +
                                         std::to_string(InventorySystem::countItem(*inv, slot.itemId));
                text(glm::vec2(left + 12.0f, y), fit(line, leftW - 16.0f, 1.0f), on ? kPanelOnInk : kPanelInk, 1.0f);
                y += 20.0f;
            }
            y += 6.0f;
        }
    }

    // The body's state, in words.
    y += 8.0f;
    rect(glm::vec2(left, y), glm::vec2(leftW, 1.0f), kPanelRule);
    y += 12.0f;
    text(glm::vec2(left, y), "STATE", kPanelDim, 1.0f);
    y += 18.0f;
    const auto stateLine = [&](const std::string& label, const std::string& value, const glm::vec4& color)
    {
        text(glm::vec2(left, y), label, kPanelDim, 1.0f);
        text(glm::vec2(left + leftW, y), value, color, 1.0f, TextAlign::Right);
        y += 16.0f;
    };
    if (mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& health = mWorld.getComponent<Health>(mPlayerEntity);
        stateLine("Health", std::to_string(static_cast<int>(std::round(health.current))) + " / " +
                                std::to_string(static_cast<int>(std::round(health.maximum))),
                  health.current < health.maximum * 0.3f ? kPanelBad : kPanelInk);
    }
    if (mWorld.hasComponent<Needs>(mPlayerEntity))
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        const sleeprules::Band band = sleeprules::band(sleeprules::shownDebt(needs.sleepDebt, needs.coffeeHours));
        const std::string name = sleeprules::name(band);
        stateLine("Sleep", name.empty() ? (needs.restedHours > 0.0f ? "Rested" : "Fine") : name,
                  band >= sleeprules::Band::Exhausted ? kPanelBad : (name.empty() ? kPanelInk : kPanelAmber));
        std::ostringstream temp;
        temp << std::fixed << std::setprecision(1) << needs.bodyTemperature << " C";
        stateLine("Temperature", temp.str(), needs.bodyTemperature > 38.0f || needs.bodyTemperature < 35.5f ? kPanelAmber : kPanelInk);
    }
    if (mWorld.hasComponent<MentalState>(mPlayerEntity))
    {
        const MentalState& mental = mWorld.getComponent<MentalState>(mPlayerEntity);
        const std::string panic = mindrules::panicName(mindrules::panicBand(mental.panic));
        const std::string mood = mindrules::depressionName(mindrules::depressionBand(mental.depression));
        stateLine("Nerves", panic.empty() ? "Calm" : panic, panic.empty() ? kPanelInk : kPanelAmber);
        stateLine("Mood", mood.empty() ? "Holding up" : mood, mood.empty() ? kPanelInk : kPanelAmber);
    }
    {
        std::ostringstream carry;
        carry << std::fixed << std::setprecision(0) << playerCarryLimit() << " kg";
        stateLine("Carry", carry.str(), kPanelInk);
    }

    // Right: the seven skills, level pips and a thin line to the next.
    float sy = layout.panelPos.y + 84.0f;
    text(glm::vec2(skillX, sy), "SKILLS", kPanelDim, 1.0f);
    sy += 18.0f;
    const Skills* skills = mWorld.hasComponent<Skills>(mPlayerEntity) ? &mWorld.getComponent<Skills>(mPlayerEntity) : nullptr;
    for (std::size_t i = 0; i < kSkillCount; ++i)
    {
        const Skill skill = static_cast<Skill>(i);
        const int level = skills != nullptr ? skills->of(skill) : 0;
        const float progress = skills != nullptr ? skillrules::progress(skills->xp[i]) : 0.0f;
        text(glm::vec2(skillX, sy), fit(skillrules::name(skill), skillW, 1.0f), level > 0 ? kPanelInk : kPanelDim, 1.0f);
        sy += 14.0f;
        for (int p = 0; p < 5; ++p)
        {
            const glm::vec2 at(skillX + static_cast<float>(p) * 10.0f, sy);
            rect(at, glm::vec2(7.0f), p < level ? kPanelInk : kPanelTrack);
        }
        const float lineX = skillX + 56.0f;
        const float lineW = std::max(10.0f, skillW - 56.0f);
        rect(glm::vec2(lineX, sy + 3.0f), glm::vec2(lineW, 1.0f), kPanelTrack);
        if (level < 5) rect(glm::vec2(lineX, sy + 3.0f), glm::vec2(std::round(lineW * std::clamp(progress, 0.0f, 1.0f)), 1.0f), kPanelInk);
        sy += 20.0f;
    }

    // Traits under the skills.
    sy += 8.0f;
    rect(glm::vec2(skillX, sy), glm::vec2(skillW, 1.0f), kPanelRule);
    sy += 12.0f;
    text(glm::vec2(skillX, sy), "TRAITS", kPanelDim, 1.0f);
    sy += 18.0f;
    if (const Traits* traits = playerTraits())
    {
        for (PositiveTrait trait : traits->positives)
        {
            text(glm::vec2(skillX, sy), fit(traitName(trait), skillW, 1.0f), kPanelGood, 1.0f);
            sy += 16.0f;
        }
        for (NegativeTrait trait : traits->negatives)
        {
            text(glm::vec2(skillX, sy), fit(traitName(trait), skillW, 1.0f), kPanelBad, 1.0f);
            sy += 16.0f;
        }
        if (!traits->bloodlines.empty())
        {
            text(glm::vec2(skillX, sy), fit(std::string("Line: ") + traits->bloodlineNames(), skillW, 1.0f), kPanelDim, 1.0f);
            sy += 16.0f;
        }
    }

    text(glm::vec2(left, layout.footerY),
         fit(mBodyChoosing ? "W/S choose   Enter use   Esc back" : "W/S wound   Enter treat   Esc close", innerW, 2.0f),
         kPanelDim, 2.0f);
}

void Game::updateBodyTabInput()
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Inventory>(mPlayerEntity)) return;
    const std::vector<Wound> wounds =
        mWorld.hasComponent<Wounds>(mPlayerEntity) ? mWorld.getComponent<Wounds>(mPlayerEntity).active : std::vector<Wound>{};
    const int count = static_cast<int>(wounds.size());
    if (count == 0)
    {
        mBodyCursor = 0;
        mBodyChoosing = false;
        return;
    }
    mBodyCursor = std::clamp(mBodyCursor, 0, count - 1);
    const bool up = mInput.wasKeyPressed(SDL_SCANCODE_UP) || mInput.wasKeyPressed(SDL_SCANCODE_W);
    const bool down = mInput.wasKeyPressed(SDL_SCANCODE_DOWN) || mInput.wasKeyPressed(SDL_SCANCODE_S);
    const bool enter = mInput.wasKeyPressed(SDL_SCANCODE_RETURN) || mInput.wasKeyPressed(SDL_SCANCODE_F);
    const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
    if (!mBodyChoosing)
    {
        if (up) mBodyCursor = (mBodyCursor + count - 1) % count;
        if (down) mBodyCursor = (mBodyCursor + 1) % count;
        if (enter)
        {
            mBodyChoosing = true;
            mBodyChoice = 0;
            if (treatmentSlots(inv, mItemDatabase, wounds[static_cast<std::size_t>(mBodyCursor)]).empty())
            {
                mInteractionMessage = "You carry nothing that helps that.";
                mInteractionMessageTimer = 2.0f;
            }
        }
        return;
    }
    const std::vector<int> slots = treatmentSlots(inv, mItemDatabase, wounds[static_cast<std::size_t>(mBodyCursor)]);
    if (slots.empty())
    {
        if (enter) mBodyChoosing = false;
        return;
    }
    const int n = static_cast<int>(slots.size());
    mBodyChoice = std::clamp(mBodyChoice, 0, n - 1);
    if (up) mBodyChoice = (mBodyChoice + n - 1) % n;
    if (down) mBodyChoice = (mBodyChoice + 1) % n;
    if (enter)
    {
        mBodyChoosing = false;
        treatWithItem(slots[static_cast<std::size_t>(mBodyChoice)], mBodyCursor);
    }
}

// What the needs system is told of the world: heat nearby, the weather,
// what is worn.
NeedsEnvironment Game::buildNeedsEnvironment()
{
    NeedsEnvironment environment;
    environment.gameHours = mGameHours;
    environment.outdoorTemperature = mWeatherState.airTemperature;
    environment.tileMap = &mTileMap;
    // The weather, as the rules read it (phase 7).
    const WeatherCondition weather = mWeatherState.condition;
    environment.weatherShift = weatherrules::bodyShift(weather, false);
    environment.weatherShiftRaincoat = weatherrules::bodyShift(weather, true);
    environment.rainy = weatherrules::rainy(weather);
    environment.coldDrift = weatherrules::coldDriftScale(weather);
    environment.thirstOutdoors = weatherrules::thirstScale(weather, true, !nightNow());
    environment.heatwave = weatherrules::heatwave(weather);
    if (environment.heatwave && mPlayerEntity != kInvalidEntity)
    {
        std::vector<SceneryCanopy> canopies;
        mSceneryView.canopiesNear(playerWorldCenter(), 64.0f, canopies);
        for (const SceneryCanopy& canopy : canopies) environment.shade.push_back(canopy.position);
    }

    mWorld.forEach<Structure, Transform>(
        [&](Entity, Structure& structure, Transform& st)
        {
            if (structure.type != StructureType::Campfire || !structure.lit || structure.hp <= 0.0f) return;
            environment.heatSources.emplace_back(st.x + kSpriteSize * 0.5f, st.y + kSpriteSize * 0.5f);
        });
    // Burning tiles near the player warm (and dry) too.
    if (mPlayerEntity != kInvalidEntity)
    {
        for (const FireSystem::Burning& b : mFireSystem.burningNear(playerTile(), 3))
        {
            if (b.heat > 0.1f)
            {
                environment.heatSources.emplace_back((static_cast<float>(b.tile.x) + 0.5f) * static_cast<float>(mTileMap.tileWidth()),
                                                     (static_cast<float>(b.tile.y) + 0.5f) * static_cast<float>(mTileMap.tileHeight()));
            }
        }
    }

    // Clothing (phase 7): the warmest thing in the pack counts as worn, and
    // Rags wrapped round it add 2. A raincoat keeps the rain off.
    if (mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Needs>(mPlayerEntity) && mWorld.hasComponent<Inventory>(mPlayerEntity))
    {
        const Inventory& inv = mWorld.getComponent<Inventory>(mPlayerEntity);
        Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        const float best = InventorySystem::bestWarmth(inv, mItemDatabase);
        needs.clothingInsulation = weatherrules::clothingInsulation(best, best > 0.0f && InventorySystem::countItem(inv, mItems.rags) > 0);
        needs.raincoat = InventorySystem::countItem(inv, mItems.raincoat) > 0;
        needs.winterCoat = InventorySystem::countItem(inv, mItems.winterCoat) > 0;
    }
    return environment;
}
