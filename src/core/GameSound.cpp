// Sound (phase 12): the listener, the sounds noises make, the body, the
// dead heard about you, ambience, music and stings. The mixer only reads the
// world: nothing here makes an entity, touches the simulation's random
// numbers or changes what the dead hear.
#include "core/GameShared.h"
#include "world/WeatherRules.h"

#include <fstream>

#include <nlohmann/json.hpp>

using namespace gameshared;

namespace
{
constexpr float kDeadSoundSeconds = 0.25f;  // the dead about you are listened for this often
constexpr float kDeadHeardTiles = 26.0f;
constexpr float kListenSeconds = 1.0f;     // crouched and still by a door this long: listening
constexpr float kFliesAfterSeconds = 1440.0f; // a body older than a game day
constexpr float kFliesTiles = 2.0f;
constexpr float kDogMaxInfection = 25.0f;

glm::vec2 centreOfT(const Transform& t)
{
    return glm::vec2(t.x + kSpriteSize * 0.5f, t.y + kSpriteSize * 0.5f);
}

glm::vec2 tileMiddle(const TileMap& map, const glm::ivec2& tile)
{
    return glm::vec2((static_cast<float>(tile.x) + 0.5f) * static_cast<float>(map.tileWidth()),
                     (static_cast<float>(tile.y) + 0.5f) * static_cast<float>(map.tileHeight()));
}

bool isFootstep(SoundId id)
{
    return static_cast<int>(id) >= static_cast<int>(SoundId::FootGrassWalk) &&
           static_cast<int>(id) <= static_cast<int>(SoundId::FootMudCrouch);
}
} // namespace

void Game::setupSound(const std::filesystem::path& assetsRoot)
{
    mSoundReady = mAudioManager.loadTable(assetsRoot.string());
    // Harness runs are silent (phase 1), and so is --mute; the mixer still
    // works out what would play.
    if (!mLaunchOptions.harnessEnabled() && !mLaunchOptions.mute && !mAudioManager.openDevice(assetsRoot.string()))
    {
        std::cerr << "Warning: Audio init failed, continuing without sound.\n";
    }
    mAudioManager.setProjection([this](const glm::vec2& world) { return mCamera.worldToScreen(world).x; });
    mAudioManager.setShellResolver([this](const glm::vec2& world) { return soundShellAt(world); });

    // The slot-independent settings (the screen for them is phase 13's).
    mScreenEffects = mLaunchOptions.harnessEnabled() ? 0.0f : 1.0f;
    std::ifstream file("save/settings.json");
    if (file)
    {
        try
        {
            nlohmann::json json;
            file >> json;
            if (json.contains("volume") && json["volume"].is_object())
            {
                const nlohmann::json& v = json["volume"];
                mAudioManager.setVolume(AudioManager::Bus::Master, v.value("master", 1.0f));
                mAudioManager.setVolume(AudioManager::Bus::Effects, v.value("effects", 1.0f));
                mAudioManager.setVolume(AudioManager::Bus::Ambience, v.value("ambience", 1.0f));
                mAudioManager.setVolume(AudioManager::Bus::Music, v.value("music", 1.0f));
            }
            const std::string fx = json.value("screenEffects", std::string("full"));
            if (!mLaunchOptions.harnessEnabled()) mScreenEffects = fx == "off" ? 0.0f : (fx == "reduced" ? 0.5f : 1.0f);
        }
        catch (const std::exception& e)
        {
            std::cerr << "save/settings.json: " << e.what() << "\n";
        }
    }
}

audiorules::Shell Game::soundShellAt(const glm::vec2& world) const
{
    const glm::ivec2 tile = worldToTile(mTileMap, world);
    if (!mTileMap.inBounds(tile.x, tile.y)) return {};
    const int building = mTileMap.getBuildingId(tile.x, tile.y);
    if (building <= 0) return {};
    return audiorules::Shell{building, buildingSealedNow(building)};
}

music::Tone Game::musicTone() const
{
    switch (saveTonePresetFromIndex(mTitleToneIndex))
    {
    case SaveTonePreset::Hopeless: return music::Tone::Hopeless;
    case SaveTonePreset::BrutalArcade: return music::Tone::BrutalArcade;
    default: return music::Tone::GrimHuman;
    }
}

void Game::makeNoise(const glm::vec2& at, NoiseTier tier, float seconds, SoundId sound, Entity source, NoiseSource kind,
                     float reach)
{
    mNoiseModel.emitNoise(at, tier, seconds, source, kind, reach, sound);
}

void Game::playAt(SoundId sound, const glm::vec2& at, float volume)
{
    mAudioManager.playAt(sound, at, volume, soundShellAt(at));
}

void Game::playUi(SoundId sound)
{
    mAudioManager.play(sound);
}

SoundId Game::footstepSound(const glm::ivec2& tile, int stance) const
{
    // grass, dirt, wood, tile, carpet, asphalt, gravel, metal, snow, mud
    int surface = 5;
    if (mTileMap.inBounds(tile.x, tile.y))
    {
        const int base = ((std::max(1, mTileMap.groundGid(tile.x, tile.y)) - 1) % TileGid::kPerRow) + 1;
        const bool outdoors = !mTileMap.isIndoors(tile.x, tile.y);
        switch (base)
        {
        case TileGid::kGrass:
        case TileGid::kGrass2: surface = 0; break;
        case TileGid::kDirt: surface = 1; break;
        case TileGid::kWood: surface = 2; break;
        case TileGid::kTileFloor: surface = 3; break;
        case TileGid::kCarpet: surface = 4; break;
        case TileGid::kGravel: surface = 6; break;
        case TileGid::kMetal: surface = 7; break;
        case TileGid::kWater: surface = 9; break;
        default: surface = 5; break; // asphalt, sidewalk, concrete floor
        }
        if (outdoors && mWeatherState.snowCover >= weatherrules::kSnowDeepEnough && surface != 7) surface = 8;
        else if (outdoors && surface == 1 && (weatherrules::rainy(mWeatherState.condition) || mHoursSinceRain < 1.0f))
        {
            surface = 9; // a dirt path after rain
        }
    }
    const int column = stance >= 2 ? 1 : (stance <= 0 ? 2 : 0); // walk, run, crouch in the table
    return static_cast<SoundId>(static_cast<int>(SoundId::FootGrassWalk) + surface * 3 + column);
}

void Game::placeLayer(LayerId layer, const glm::vec2& at, float rangeTiles, float volume)
{
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    const glm::vec2 here = mPlayerEntity != kInvalidEntity ? playerWorldCenter() : at;
    float gain = audiorules::distanceGain(glm::length(at - here) / tile, rangeTiles) * volume;
    const audiorules::Shell from = soundShellAt(at);
    const audiorules::Shell me = soundShellAt(here);
    if (audiorules::occluded(from, me, mListening)) gain *= audiorules::kOcclusionGain;
    const float pan = audiorules::panFromScreen(mCamera.worldToScreen(at).x, mCamera.worldToScreen(here).x,
                                                static_cast<float>(mWindowWidth) * 0.5f);
    mAudioManager.setLayer(layer, gain, pan);
}

void Game::startTinnitus()
{
    mTinnitusLeft = ambience::kTinnitusSeconds;
}

void Game::soundOfDoor(const glm::ivec2& tile, bool opening, bool gently)
{
    const glm::vec2 at = tileMiddle(mTileMap, tile);
    if (gently) playAt(SoundId::DoorCreak, at, 0.8f);
    else playAt(opening ? SoundId::DoorOpen : SoundId::DoorClose, at);
}

void Game::soundOfHit(int weaponCategory, bool heavy, bool killed, const glm::vec2& at)
{
    SoundId id = SoundId::HitBlunt;
    if (heavy) id = SoundId::HitHeavy;
    else if (weaponCategory == static_cast<int>(WeaponCategory::Bladed)) id = SoundId::HitBlade;
    playAt(id, at, killed ? 1.0f : 0.85f);
    if (killed) playAt(SoundId::DeadFall, at, 0.8f);
}

// ---------------------------------------------------------------------------

void Game::updateSoundscape(float dtSeconds)
{
    if (!mSoundReady) return;
    const bool alive = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity);

    AudioManager::Listener listener;
    listener.world = alive ? playerWorldCenter() : mCamera.screenToWorld(mCamera.viewportSize() * 0.5f);
    listener.shell = soundShellAt(listener.world);
    listener.listening = mListening;
    listener.tileSize = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    listener.screenX = mCamera.worldToScreen(listener.world).x;
    listener.halfViewport = static_cast<float>(mWindowWidth) * 0.5f;
    listener.rangeScale = weatherrules::noiseScale(mWeatherState.condition);
    mAudioManager.setListener(listener);

    // The sounds that went with this tick's noises. Others' feet are softer.
    for (const NoiseModel::HeardSound& heard : mNoiseModel.takeSounds())
    {
        const float volume = isFootstep(heard.sound) && heard.event.sourceEntity != mPlayerEntity ? 0.6f : 1.0f;
        mAudioManager.playAt(heard.sound, heard.event.worldPosition, volume,
                             audiorules::Shell{heard.event.building, heard.event.sealed});
    }

    if (mRunState == RunState::Playing)
    {
        updateListening(dtSeconds);
        updateBodySounds(dtSeconds);
        mDeadSoundTimer -= dtSeconds;
        if (mDeadSoundTimer <= 0.0f)
        {
            mDeadSoundTimer += kDeadSoundSeconds;
            updateDeadSounds();
        }
        updateStings();
    }
    else
    {
        mListening = false;
        mListenStill = 0.0f;
    }

    mTinnitusLeft = std::max(0.0f, mTinnitusLeft - dtSeconds);
    mAudioManager.setDuck(ambience::duck(mTinnitusLeft));

    mAmbienceTimer -= dtSeconds;
    if (mAmbienceTimer <= 0.0f)
    {
        mAmbienceTimer += ambience::kUpdateSeconds;
        updateAmbience();
    }
    mAudioManager.update(dtSeconds);
}

void Game::updateListening(float dtSeconds)
{
    bool ear = false;
    if (mPlayerEntity != kInvalidEntity && mActiveVehicleEntity == kInvalidEntity &&
        mWorld.hasComponent<Facing>(mPlayerEntity) && mWorld.hasComponent<Velocity>(mPlayerEntity) &&
        mWorld.getComponent<Facing>(mPlayerEntity).crouching)
    {
        const Velocity& v = mWorld.getComponent<Velocity>(mPlayerEntity);
        if (v.dx * v.dx + v.dy * v.dy < 4.0f)
        {
            // A shut door within a tile.
            const glm::ivec2 here = playerTile();
            for (const Doors::Door& door : mDoors.all())
            {
                if (std::abs(door.tile.x - here.x) > 1 || std::abs(door.tile.y - here.y) > 1) continue;
                if (mTileMap.doorState(door.tile.x, door.tile.y) == DoorState::Closed)
                {
                    ear = true;
                    break;
                }
            }
        }
    }
    mListenStill = ear ? mListenStill + dtSeconds : 0.0f;
    mListening = mListenStill >= kListenSeconds;
}

void Game::updateBodySounds(float dtSeconds)
{
    if (mPlayerEntity == kInvalidEntity || !mWorld.hasComponent<Transform>(mPlayerEntity)) return;
    const bool asleep = mWorld.hasComponent<Sleeping>(mPlayerEntity);

    // The heart, by panic (60 to 140 a minute), and under a fifth of health.
    float panic = 0.0f;
    if (mWorld.hasComponent<MentalState>(mPlayerEntity)) panic = mWorld.getComponent<MentalState>(mPlayerEntity).panic;
    float health01 = 1.0f;
    if (mWorld.hasComponent<Health>(mPlayerEntity))
    {
        const Health& h = mWorld.getComponent<Health>(mPlayerEntity);
        health01 = h.maximum > 0.0f ? h.current / h.maximum : 1.0f;
    }
    const float drive = std::max(std::clamp((panic - 35.0f) / 65.0f, 0.0f, 1.0f), health01 < 0.2f ? 0.7f : 0.0f);
    mHeartbeatTimer -= dtSeconds;
    if (drive > 0.0f && !asleep && mHeartbeatTimer <= 0.0f)
    {
        const float bpm = 60.0f + 80.0f * std::clamp(panic / 100.0f, 0.0f, 1.0f);
        mHeartbeatTimer = 60.0f / std::max(60.0f, health01 < 0.2f ? std::max(bpm, 100.0f) : bpm);
        mAudioManager.play(SoundId::BodyHeartbeat, 0.35f + 0.65f * drive);
    }
    else if (drive <= 0.0f)
    {
        mHeartbeatTimer = std::min(mHeartbeatTimer, 0.2f);
    }

    // Out of breath: a loop that rises as stamina falls under 30 %.
    float breath = 0.0f;
    if (mWorld.hasComponent<Stamina>(mPlayerEntity))
    {
        const Stamina& s = mWorld.getComponent<Stamina>(mPlayerEntity);
        const float share = s.maximum > 0.0f ? s.current / s.maximum : 1.0f;
        if (share < 0.3f && !asleep) breath = 0.25f + 0.75f * (0.3f - share) / 0.3f;
    }
    mAudioManager.setLayer(LayerId::Breath, breath);

    // Held by the dead: the grab.
    const bool held = playerHeld();
    if (held && !mWasHeld) playAt(SoundId::DeadGrab, playerWorldCenter());
    mWasHeld = held;

    // A cough in smoke, or when ill (now and then).
    if (mWorld.hasComponent<Needs>(mPlayerEntity) && !asleep)
    {
        const Needs& needs = mWorld.getComponent<Needs>(mPlayerEntity);
        const bool smoke = needs.smokeSeconds >= weatherrules::kCoughAfterSeconds;
        const float chancePerSecond = smoke ? 0.3f : (needs.illness >= 50.0f ? 0.02f : 0.0f);
        if (chancePerSecond > 0.0f && std::uniform_real_distribution<float>(0.0f, 1.0f)(mSoundRng) < chancePerSecond * dtSeconds)
        {
            mAudioManager.play(SoundId::BodyCough, 0.8f);
        }
    }
}

void Game::updateDeadSounds()
{
    if (mPlayerEntity == kInvalidEntity) return;
    const glm::vec2 here = playerWorldCenter();
    const float tile = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::unordered_map<Entity, std::uint8_t> now;
    now.reserve(mHeardDead.size() + 8);

    mWorld.forEach<ZombieAI, Transform, Health>(
        [&](Entity e, ZombieAI& ai, Transform& t, Health& h)
        {
            if (h.current <= 0.0f || ai.dormant) return;
            const glm::vec2 at = centreOfT(t);
            const float d = glm::length(at - here) / tile;
            if (d > kDeadHeardTiles) return;
            const std::uint8_t state = static_cast<std::uint8_t>(static_cast<int>(ai.state) | (ai.attackWindup > 0.0f ? 8 : 0));
            now[e] = state;
            const auto was = mHeardDead.find(e);
            const ZombieState before = was == mHeardDead.end() ? ai.state : static_cast<ZombieState>(was->second & 7);
            const bool lungedBefore = was != mHeardDead.end() && (was->second & 8) != 0;
            const bool moving = std::abs(t.x - t.prevX) + std::abs(t.y - t.prevY) > 0.05f;

            // It has noticed something.
            if (before == ZombieState::Wander && (ai.state == ZombieState::Investigate || ai.state == ZombieState::Chase))
            {
                switch (ai.kind)
                {
                case ZombieKind::Brute: playAt(SoundId::BruteGrowl, at); break;
                case ZombieKind::Crawler: playAt(SoundId::CrawlerScrape, at); break;
                case ZombieKind::Runner: break; // its shriek is the StartRun event
                default: playAt(SoundId::ShamblerAlert, at); break;
                }
            }
            // The lunge, telegraphed by the snarl as much as the pose.
            if (ai.attackWindup > 0.0f && !lungedBefore) playAt(SoundId::DeadLunge, at);

            // The low sounds of the dead going about: the tell before they are seen.
            const float r = unit(mSoundRng);
            switch (ai.kind)
            {
            case ZombieKind::Brute:
                if (moving && r < 0.45f) playAt(SoundId::BruteStep, at);
                else if (ai.state == ZombieState::Wander && r > 0.985f) playAt(SoundId::BruteGrowl, at, 0.7f);
                break;
            case ZombieKind::Crawler:
                if (moving && r < 0.2f) playAt(SoundId::CrawlerDrag, at);
                break;
            case ZombieKind::Runner:
                if (ai.state == ZombieState::Chase && r < 0.3f) playAt(SoundId::RunnerBreath, at);
                else if (ai.state == ZombieState::Wander && r < 0.02f) playAt(SoundId::ShamblerIdle, at, 0.7f);
                break;
            default:
                if (ai.state == ZombieState::Wander && r < 0.03f) playAt(SoundId::ShamblerIdle, at);
                break;
            }
        });
    mHeardDead.swap(now);
}

void Game::updateStings()
{
    const bool night = weatherrules::isNight(mCurrentDay, mGameHours);
    const int was = mNightKnown;
    mNightKnown = night ? 1 : 0;
    if (was < 0 || was == mNightKnown) return;
    if (!music::stingsAllowed(musicTone())) return;
    // The sun is going; or the night is over and you are still here.
    mAudioManager.play(night ? SoundId::StingDusk : SoundId::StingDawn);
}

void Game::updateAmbience()
{
    ambience::Facts f;
    const bool playing = mRunState == RunState::Playing || mRunState == RunState::Paused;
    f.playing = playing;
    f.out = mRunState == RunState::Out;
    const glm::vec2 here = mPlayerEntity != kInvalidEntity && mWorld.hasComponent<Transform>(mPlayerEntity)
                               ? playerWorldCenter()
                               : mCamera.screenToWorld(mCamera.viewportSize() * 0.5f);
    const glm::ivec2 tile = worldToTile(mTileMap, here);
    const float tilePx = static_cast<float>(std::max(1, mTileMap.tileWidth()));
    f.day = !weatherrules::isNight(mCurrentDay, mGameHours);
    f.condition = mWeatherState.condition;
    f.windStrength = mWeatherState.windStrength;
    f.temperature = mWeatherState.airTemperature;

    music::Facts mf;
    mf.tone = musicTone();
    mf.playing = mRunState == RunState::Playing && mPlayerEntity != kInvalidEntity;
    mf.hours = static_cast<double>(mCurrentDay) * 24.0 + mGameHours;
    const double dawn = weatherrules::dawnHour(mCurrentDay);
    const double dusk = weatherrules::duskHour(mCurrentDay);
    mf.dawnOrDusk = std::abs(mGameHours - dawn) < 1.0 || std::abs(mGameHours - dusk) < 1.0;

    if (playing && mTileMap.inBounds(tile.x, tile.y))
    {
        const int building = mTileMap.getBuildingId(tile.x, tile.y);
        f.indoors = mTileMap.isIndoors(tile.x, tile.y) || building > 0;
        if (building > 0)
        {
            const bool sealed = buildingSealedNow(building);
            f.openToOutside = !sealed;
            bool windows = false;
            for (const windows::Spot& spot : mWindowSpots)
            {
                if (spot.building == building)
                {
                    windows = true;
                    break;
                }
            }
            f.closedIn = sealed && !windows;
        }
        const int district = mTileMap.getDistrictId(tile.x, tile.y);
        f.infection = district > 0 ? mDistrictInfectionSystem.getInfection(district) : 0.0f;
        f.ruin = ruinLevel(district);

        // The dead about you, and where the crowd of them is.
        glm::vec2 crowdSum(0.0f);
        int fight = 0;
        mWorld.forEach<ZombieAI, Transform, Health>(
            [&](Entity, ZombieAI& ai, Transform& t, Health& h)
            {
                if (h.current <= 0.0f) return;
                const glm::vec2 at = centreOfT(t);
                const float d = glm::length(at - here) / tilePx;
                if (d <= ambience::kQuietTiles) ++f.deadNear;
                if (d <= music::kCalmTiles) mf.calm = false;
                if (d <= music::kFightTiles && (ai.state == ZombieState::Chase || ai.state == ZombieState::Attack)) ++fight;
                if (d <= ambience::kCrowdTiles)
                {
                    ++f.deadAround;
                    crowdSum += at;
                }
            });
        mf.fight = fight;
        mf.indoors = f.indoors;

        // Wind in the trees close by.
        for (const glm::ivec2& tree : mSceneryView.treeTiles())
        {
            if (std::abs(tree.x - tile.x) <= 5 && std::abs(tree.y - tile.y) <= 5)
            {
                f.treesNear = true;
                break;
            }
        }
        // The town's hum: a lamp or a fridge within 6 tiles, while the power lasts.
        const bool power = powerOn();
        if (power)
        {
            const int r = static_cast<int>(ambience::kHumTiles);
            for (const glm::ivec2& lamp : mLitLamps)
            {
                if (std::abs(lamp.x - tile.x) <= r && std::abs(lamp.y - tile.y) <= r) f.hum = true;
            }
            if (!f.hum)
            {
                for (const FurniturePiece& piece : mFurniture)
                {
                    if (piece.kind != FurnitureKind::Fridge && piece.kind != FurnitureKind::FloorLamp) continue;
                    if (std::abs(piece.tile.x - tile.x) <= r && std::abs(piece.tile.y - tile.y) <= r)
                    {
                        f.hum = true;
                        break;
                    }
                }
            }
        }
        // The power failing: the hum stops, once.
        if (mWasPowerOn && !power) playAt(SoundId::PowerDown, here);
        mWasPowerOn = power;
        f.burningNear = static_cast<int>(mFireSystem.burningNear(tile, static_cast<int>(ambience::kFireTiles)).size());
        // A lit campfire crackles too (it is a structure, not the fire grid).
        mWorld.forEach<Structure, Transform>(
            [&](Entity, Structure& s, Transform& t)
            {
                if (s.type != StructureType::Campfire || !s.lit) return;
                if (glm::length(glm::vec2(t.x + 16.0f, t.y + 16.0f) - here) < ambience::kFireTiles * tilePx)
                {
                    ++f.burningNear;
                }
            });
        f.tinnitusSeconds = mTinnitusLeft;

        const ambience::Mix mix = ambience::mix(f);
        float crowdPan = 0.0f;
        if (f.deadAround > 0)
        {
            const glm::vec2 centre = crowdSum / static_cast<float>(f.deadAround);
            crowdPan = audiorules::panFromScreen(mCamera.worldToScreen(centre).x, mCamera.worldToScreen(here).x,
                                                 static_cast<float>(mWindowWidth) * 0.5f);
        }
        for (int i = 0; i < kLayerCount; ++i)
        {
            const LayerId layer = static_cast<LayerId>(i);
            if (!ambience::isAmbience(layer) && layer != LayerId::Tinnitus) continue;
            mAudioManager.setLayer(layer, mix[static_cast<std::size_t>(i)], layer == LayerId::Crowd ? crowdPan : 0.0f);
        }
    }
    else
    {
        // The title, a death, the way out: wind (and only wind).
        const ambience::Mix mix = ambience::mix(f);
        for (int i = 0; i < kLayerCount; ++i)
        {
            const LayerId layer = static_cast<LayerId>(i);
            if (ambience::isAmbience(layer) || layer == LayerId::Tinnitus) mAudioManager.setLayer(layer, mix[static_cast<std::size_t>(i)]);
        }
        if (!f.out) mAudioManager.setLayer(LayerId::Wind, mRunState == RunState::TitleScreen ? 0.5f : 0.3f);
    }

    // Music: almost none.
    mMusic.update(mf);
    for (const LayerId pad : {LayerId::PadA, LayerId::PadB, LayerId::PadC})
    {
        mAudioManager.setLayer(pad, mMusic.pad() == pad ? mMusic.padTarget() : 0.0f);
    }
    mAudioManager.setLayer(LayerId::Pulse, mMusic.pulseTarget());

    if (!playing || mPlayerEntity == kInvalidEntity)
    {
        for (const LayerId layer : {LayerId::Radio, LayerId::EngineIdle, LayerId::EngineDrive, LayerId::Generator, LayerId::CarAlarm,
                                    LayerId::Flies, LayerId::Rummage, LayerId::Breath})
        {
            mAudioManager.setLayer(layer, 0.0f);
        }
        return;
    }

    // The radio's thin tune, close by (it is in your pack).
    mAudioManager.setLayer(LayerId::Radio, mRadioOn ? (f.indoors ? 0.7f : 0.45f) : 0.0f);

    // The engine you sit behind.
    float idle = 0.0f;
    float drive = 0.0f;
    if (mActiveVehicleEntity != kInvalidEntity && mWorld.hasComponent<Vehicle>(mActiveVehicleEntity))
    {
        const Vehicle& v = mWorld.getComponent<Vehicle>(mActiveVehicleEntity);
        if (v.engineOn && v.kind != VehicleKind::Bicycle)
        {
            const float pace = std::clamp(std::abs(v.speed) / 300.0f, 0.0f, 1.0f);
            idle = 0.7f * (1.0f - pace);
            drive = 0.3f + 0.6f * pace;
            if (pace < 0.05f) drive = 0.0f;
        }
    }
    mAudioManager.setLayer(LayerId::EngineIdle, idle);
    mAudioManager.setLayer(LayerId::EngineDrive, drive);

    // A car alarm ringing somewhere: the nearest.
    {
        float best = 1.0e9f;
        glm::vec2 at(0.0f);
        mWorld.forEach<Vehicle, Transform>(
            [&](Entity, Vehicle& v, Transform& t)
            {
                if (v.alarmSeconds <= 0.0f) return;
                const float d = glm::length(centreOfT(t) - here);
                if (d < best)
                {
                    best = d;
                    at = centreOfT(t);
                }
            });
        if (best < 1.0e8f) placeLayer(LayerId::CarAlarm, at, audiorules::tierRangeTiles(4), 1.0f);
        else mAudioManager.setLayer(LayerId::CarAlarm, 0.0f);
    }

    // The gate's generator while the last stand runs.
    if (mStand.running)
    {
        placeLayer(LayerId::Generator, tileMiddle(mTileMap, mMapGenerator.layoutData().gate.generator),
                   audiorules::tierRangeTiles(5), 1.0f);
    }
    else
    {
        mAudioManager.setLayer(LayerId::Generator, 0.0f);
    }

    // Flies over a body older than a day, within 2 tiles.
    {
        float flies = 0.0f;
        mWorld.forEach<Corpse, Transform>(
            [&](Entity, Corpse& c, Transform& t)
            {
                if (c.age < kFliesAfterSeconds) return;
                const float d = glm::length(centreOfT(t) - here) / tilePx;
                if (d <= kFliesTiles) flies = std::max(flies, 1.0f - d / (kFliesTiles + 1.0f));
            });
        mAudioManager.setLayer(LayerId::Flies, flies);
    }

    // Rummaging through a container.
    mAudioManager.setLayer(LayerId::Rummage, mBaseAction.kind == BaseAction::Kind::Search ? 0.8f : 0.0f);

    // A dog far off now and then, by day, where the town is not yet lost.
    // Never an entity: a sound from somewhere off the screen.
    mDogTimer -= ambience::kUpdateSeconds;
    if (mDogTimer <= 0.0f)
    {
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        mDogTimer = 90.0f + 180.0f * unit(mSoundRng);
        if (f.day && !f.indoors && f.infection < kDogMaxInfection)
        {
            const float angle = unit(mSoundRng) * 6.2831853f;
            playAt(SoundId::DogFar, here + glm::vec2(std::cos(angle), std::sin(angle)) * 40.0f * tilePx);
        }
    }

    // A storm coming: thunder far off before it arrives (phase 7's warning).
    const bool stormComing = mWeatherState.nextCondition == WeatherCondition::Thunderstorm &&
                             mWeatherState.condition != WeatherCondition::Thunderstorm &&
                             mWeatherState.hoursToChange <= weatherrules::kWarningHours;
    if (stormComing || mWeatherState.condition == WeatherCondition::Thunderstorm)
    {
        mThunderFarTimer -= ambience::kUpdateSeconds;
        if (mThunderFarTimer <= 0.0f)
        {
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            mThunderFarTimer = 25.0f + 50.0f * unit(mSoundRng);
            mAudioManager.play(SoundId::ThunderFar, stormComing ? 0.6f : 0.9f, unit(mSoundRng) * 1.6f - 0.8f);
        }
    }
}
