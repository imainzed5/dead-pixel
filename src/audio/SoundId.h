#pragma once

// Every sound the game plays (phase 12). The keys are the ids in
// assets/data/sounds.json, which tools/generate_sounds.py writes with the
// WAVs; tests/test_sounds.cpp keeps this list and the table in step.

// One-shots: played once, from a place in the world or straight to the ear.
#define DP_SOUND_LIST(X) \
    X(FootGrassWalk, "foot_grass_walk") \
    X(FootGrassRun, "foot_grass_run") \
    X(FootGrassCrouch, "foot_grass_crouch") \
    X(FootDirtWalk, "foot_dirt_walk") \
    X(FootDirtRun, "foot_dirt_run") \
    X(FootDirtCrouch, "foot_dirt_crouch") \
    X(FootWoodWalk, "foot_wood_walk") \
    X(FootWoodRun, "foot_wood_run") \
    X(FootWoodCrouch, "foot_wood_crouch") \
    X(FootTileWalk, "foot_tile_walk") \
    X(FootTileRun, "foot_tile_run") \
    X(FootTileCrouch, "foot_tile_crouch") \
    X(FootCarpetWalk, "foot_carpet_walk") \
    X(FootCarpetRun, "foot_carpet_run") \
    X(FootCarpetCrouch, "foot_carpet_crouch") \
    X(FootAsphaltWalk, "foot_asphalt_walk") \
    X(FootAsphaltRun, "foot_asphalt_run") \
    X(FootAsphaltCrouch, "foot_asphalt_crouch") \
    X(FootGravelWalk, "foot_gravel_walk") \
    X(FootGravelRun, "foot_gravel_run") \
    X(FootGravelCrouch, "foot_gravel_crouch") \
    X(FootMetalWalk, "foot_metal_walk") \
    X(FootMetalRun, "foot_metal_run") \
    X(FootMetalCrouch, "foot_metal_crouch") \
    X(FootSnowWalk, "foot_snow_walk") \
    X(FootSnowRun, "foot_snow_run") \
    X(FootSnowCrouch, "foot_snow_crouch") \
    X(FootMudWalk, "foot_mud_walk") \
    X(FootMudRun, "foot_mud_run") \
    X(FootMudCrouch, "foot_mud_crouch") \
    X(BodyHeartbeat, "body_heartbeat") \
    X(BodyEat, "body_eat") \
    X(BodyDrink, "body_drink") \
    X(BodyCough, "body_cough") \
    X(BodyBreathOut, "body_breath_out") \
    X(BodyGruntSmall, "body_grunt_small") \
    X(BodyGruntBig, "body_grunt_big") \
    X(BodyScream, "body_scream") \
    X(BodyClimb, "body_climb") \
    X(DoorOpen, "door_open") \
    X(DoorClose, "door_close") \
    X(DoorLocked, "door_locked") \
    X(DoorCreak, "door_creak") \
    X(OpenCupboard, "open_cupboard") \
    X(OpenFridge, "open_fridge") \
    X(OpenDrawer, "open_drawer") \
    X(OpenLocker, "open_locker") \
    X(Hammer, "hammer") \
    X(BoardPry, "board_pry") \
    X(PlankSplit, "plank_split") \
    X(DragLight, "drag_light") \
    X(DragHeavy, "drag_heavy") \
    X(FurnitureBreak, "furniture_break") \
    X(WindowBreak, "window_break") \
    X(GlassStep, "glass_step") \
    X(Curtain, "curtain") \
    X(CanAlarm, "can_alarm") \
    X(SpikeBoard, "spike_board") \
    X(Match, "match") \
    X(FireIgnite, "fire_ignite") \
    X(WaterPour, "water_pour") \
    X(TapRun, "tap_run") \
    X(TapSputter, "tap_sputter") \
    X(TapDry, "tap_dry") \
    X(BottleFill, "bottle_fill") \
    X(PackOpen, "pack_open") \
    X(PackClose, "pack_close") \
    X(PickupLight, "pickup_light") \
    X(PickupHeavy, "pickup_heavy") \
    X(DropLight, "drop_light") \
    X(DropHeavy, "drop_heavy") \
    X(CraftDone, "craft_done") \
    X(Bandage, "bandage") \
    X(Splint, "splint") \
    X(Stitches, "stitches") \
    X(Reload, "reload") \
    X(DryClick, "dry_click") \
    X(Jam, "jam") \
    X(SwingLight, "swing_light") \
    X(SwingHeavy, "swing_heavy") \
    X(HitBlunt, "hit_blunt") \
    X(HitBlade, "hit_blade") \
    X(HitHeavy, "hit_heavy") \
    X(HitPan, "hit_pan") \
    X(HitWall, "hit_wall") \
    X(StickPull, "stick_pull") \
    X(Shove, "shove") \
    X(Stomp, "stomp") \
    X(WeaponBreak, "weapon_break") \
    X(Pistol, "pistol") \
    X(Shotgun, "shotgun") \
    X(Casing, "casing") \
    X(ThrowWhoosh, "throw_whoosh") \
    X(RockLand, "rock_land") \
    X(BottleSmash, "bottle_smash") \
    X(ClockRing, "clock_ring") \
    X(FlareHiss, "flare_hiss") \
    X(ShamblerIdle, "shambler_idle") \
    X(ShamblerAlert, "shambler_alert") \
    X(ShamblerChase, "shambler_chase") \
    X(RunnerShriek, "runner_shriek") \
    X(RunnerBreath, "runner_breath") \
    X(BruteGrowl, "brute_growl") \
    X(BruteStep, "brute_step") \
    X(CrawlerScrape, "crawler_scrape") \
    X(CrawlerDrag, "crawler_drag") \
    X(ScreamerScream, "screamer_scream") \
    X(DeadLunge, "dead_lunge") \
    X(DeadGrab, "dead_grab") \
    X(DeadBite, "dead_bite") \
    X(DeadFall, "dead_fall") \
    X(DeadDoorBang, "dead_door_bang") \
    X(DeadBoardCrack, "dead_board_crack") \
    X(DeadWindowBang, "dead_window_bang") \
    X(PersonCough, "person_cough") \
    X(PersonHey, "person_hey") \
    X(PersonDoorSlam, "person_door_slam") \
    X(ThunderNear, "thunder_near") \
    X(ThunderFar, "thunder_far") \
    X(DogFar, "dog_far") \
    X(PowerDown, "power_down") \
    X(CarCrank, "car_crank") \
    X(CarCrankFail, "car_crank_fail") \
    X(CarDoor, "car_door") \
    X(CarAlarm, "car_alarm") \
    X(ShopAlarm, "shop_alarm") \
    X(CarImpact, "car_impact") \
    X(BicycleTick, "bicycle_tick") \
    X(GateGrind, "gate_grind") \
    X(GateOpen, "gate_open") \
    X(StingDusk, "sting_dusk") \
    X(StingCaller, "sting_caller") \
    X(StingDawn, "sting_dawn") \
    X(DayCard, "day_card") \
    X(DeathSting, "death_sting") \
    X(UiMove, "ui_move") \
    X(UiSelect, "ui_select") \
    X(UiBack, "ui_back") \
    X(UiError, "ui_error") \
    X(UiTab, "ui_tab")

// Loops: ambience, the body, machines and music, each faded in and out by
// the mixer (AudioManager::setLayer).
#define DP_LAYER_LIST(X) \
    X(Wind, "loop_wind") \
    X(WindGust, "loop_wind_gust") \
    X(WindTrees, "loop_wind_trees") \
    X(RainLight, "loop_rain_light") \
    X(RainHeavy, "loop_rain_heavy") \
    X(RainRoof, "loop_rain_roof") \
    X(Snow, "loop_snow") \
    X(Blizzard, "loop_blizzard") \
    X(FogDrip, "loop_fog_drip") \
    X(Birds, "loop_birds") \
    X(Insects, "loop_insects") \
    X(Crows, "loop_crows") \
    X(Hum, "loop_hum") \
    X(Room, "loop_room") \
    X(Crowd, "loop_crowd") \
    X(FireSmall, "loop_fire_small") \
    X(FireLarge, "loop_fire_large") \
    X(Tinnitus, "loop_tinnitus") \
    X(Breath, "loop_breath") \
    X(Rummage, "loop_rummage") \
    X(Flies, "loop_flies") \
    X(EngineIdle, "loop_engine_idle") \
    X(EngineDrive, "loop_engine_drive") \
    X(Generator, "loop_generator") \
    X(CarAlarm, "loop_car_alarm") \
    X(Radio, "loop_radio") \
    X(PadA, "loop_pad_a") \
    X(PadB, "loop_pad_b") \
    X(PadC, "loop_pad_c") \
    X(Pulse, "loop_pulse")

enum class SoundId
{
#define DP_SOUND_ENUM(name, key) name,
    DP_SOUND_LIST(DP_SOUND_ENUM)
#undef DP_SOUND_ENUM
    Count,
    None = Count
};

enum class LayerId
{
#define DP_LAYER_ENUM(name, key) name,
    DP_LAYER_LIST(DP_LAYER_ENUM)
#undef DP_LAYER_ENUM
    Count
};

inline constexpr int kSoundCount = static_cast<int>(SoundId::Count);
inline constexpr int kLayerCount = static_cast<int>(LayerId::Count);

// The table key of each id ("foot_grass_walk", "loop_wind").
[[nodiscard]] const char* soundKey(SoundId id);
[[nodiscard]] const char* layerKey(LayerId id);
// By key; SoundId::None / LayerId::Count when unknown.
[[nodiscard]] SoundId soundByKey(const char* key);
[[nodiscard]] LayerId layerByKey(const char* key);
