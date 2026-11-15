# Current state (2026-11-13, branch `art-and-feel`, commit `1d89abf`)

A map of what exists, so each phase knows what it builds on. Line numbers
drift; treat them as pointers and re-read the code before changing it.
`docs/revival_plan.md` has the history of how it got here.

## Shape of the code

- C++20, SDL2, OpenGL 3.3, a small ECS (`src/ecs`). About 25,000 lines in
  `src/`, 86 unit tests in `tests/`, 20 harness scripts in `tools/harness/`.
- `Game` is one class split across `src/core/Game*.cpp` by concern. The
  update order is in `GameUpdate.cpp` (`Game::update`): input, clock, needs,
  bleeding, wounds, spoilage, mind, weather, fire, survivors, infection,
  build mode, panels, movement modifiers, sleep, first days, death check,
  interaction, combat, kills and bodies, threat director, zombie AI, doors,
  movement and collision, vehicles, campaign check, legacy, animation,
  particles, noise.
- Fixed step: 1/60 s. 1 real second = 1 game minute.
- Town: 150x100 tiles, generated once per save from the world seed
  (`MapGenerator`), then fixed. Districts: Wilderness, Residential,
  Commercial, Industrial. A building's kind comes only from its width
  (9 or less: house; 11 or less: shop; wider: industrial).
- Saves (`src/save/SaveManager`): one folder `save/`, files per slot: meta,
  legacy (graves, retirements), structures, layout, checkpoint, memory
  (world events, looted positions, broken and boarded doors, burned tiles),
  survivors. `kCurrentSaveVersion` is 1.

## What each system does today

| System | Today | Shallow spots |
|---|---|---|
| Needs (`NeedsSystem`) | Hunger, thirst, fatigue, sleep debt, body temperature, illness; stamina ceiling; slow death when a bar is empty | Clothing is "a jacket in the pack"; no cooking; water is never dirty |
| Wounds (`WoundSystem`, `BleedingSystem`) | Cut, bruise, bite; bites can infect; each wound costs 5 max HP | `Wound.severity` unused; no fractures, no stitches; infected wounds never heal on their own |
| Mind (`MentalStateSystem`) | Panic from the dead in sight; depression from isolation and unmet needs; crisis for 24 h forces retirement | Panic only wobbles aim and jitters movement; desensitisation never fades; few ways to feel better |
| The dead (`ZombieAISystem`, `ThreatDirectorSystem`) | One kind. Wander, investigate noise, chase, telegraphed lunge. Batter doors. Director sends wanderers toward the player and grows the population | No kinds; decomposition only scales speed and damage; they target the player only; hearing passes through walls; an A* over the whole map per chasing zombie every 0.45 s |
| Night callers (`FirstDays`) | 2, 4, 6, then +1 a night, in up to three groups | Nothing changes after day 3 |
| Combat (`CombatSystem`) | Swing with wind-up 0.20 s, active 0.10 s, recovery 0.30 s; five weapon classes; wear; grab and execute (right click); knockback, stagger, hit-stop | No shove; the dead never grab; classes differ only in numbers; no gun item exists (`Pistol Ammo` has no gun and no source); thrown things are limited to the molotov on the L key |
| Noise (`NoiseModel`) | Five tiers with radius and decay; footsteps by surface and stance | No walls, doors or weather in propagation; survivors and the dead make none |
| Weather (`WeatherSystem`, `Atmosphere`) | Seasons of 15 days; eight conditions; drawn rain, fog, cloud, lightning, wind | **Bug:** conditions last 2 to 6 real seconds. Visibility is computed and never used; snow is drawn as rain |
| District infection | Rises daily; over 75 is "overwhelmed"; strengthens the dead there | Never falls; shown only in a debug view and one HUD word |
| Survivors (`SurvivorAISystem`, `SurvivorInteractionSystem`) | Wander, flee, fight, "scavenge" (walks in a circle); observe, approach, give an item | Never eat, sleep or loot; the dead ignore them; hostile ones never attack; a trade gives the player nothing |
| Items and crafting | 30 items in `assets/data/items.json`, 5 recipes in `recipes.json`; discover by holding both parts | Ids are hard-coded across C++; loot tables are in `GameScene.cpp`; `requiresWorkbench` checks for a supply cache, not a workbench |
| Containers | Furniture holds loot; taken items are remembered across lives | Take-only: nothing can be put in |
| Build mode (`BuildSystem`, B key) | Places six flat 32 px props from before the revival | Out of style; overlaps with boarding doors |
| Fire (`GameFire.cpp`) | Molotov, spread by surface, rain and wind; burn scars remembered | O(fires x entities) each tick; kills credit the fire |
| Vehicles (`GameVehicles.cpp`) | One sedan: fuel, wear, running the dead over, a trunk rolled at spawn | The car is an overlay on the player's position; no key, no types |
| Legacy (`LegacyRules`, `GameLegacy.cpp`, `GameRunEnd.cpp`) | Graves, packs, the risen, retired survivors, run-start card, notebook, town map; broken and boarded doors, burns and emptied cupboards persist | No journals; bloodline is one of four fixed traits; bases do not carry over in the new style |
| Campaign | An objective tile in the farthest district with an infection deadline; touching it "retires" the character; missing the deadline kills them | A placeholder |
| Traits | One positive and one negative, picked by the seed | No choice; `Tough` does nothing (see below) |
| Audio | 10 effects and one wind loop, generated; pan only | No distance falloff, no ambience by place or weather, no music |
| Interface | Bare-hands HUD, docked pack and make panels, restyled menus | No settings screen, no key bindings, no character creation |

## Defects found while planning (fix in phase 1)

1. **Weather flickers.** `WeatherSystem.cpp` counts `mConditionTimer` down in
   real seconds but sets it to "2 to 6", meant as game hours. Conditions
   change every few seconds. The next condition is also picked from
   `gameHours * 100`, so it depends on the exact tick.
2. **`Tough` is a no-op.** `WoundSystem` sets `health.maximum =
   max(20, 100 - 5 * wounds)` every tick, overwriting the +20.
   `GameShared.h` does the same when a bandage is used.
3. **Workbench check is wrong.** `hasNearbyWorkbench` (`GameShared.h`) looks
   for `StructureType::SupplyCache`, not the workbench furniture.
4. **Combat RNG is unseeded** (`std::random_device`) unless `--world-seed` is
   given, so exhaustion misses and panic wobble are not reproducible.
5. **Hit-stop slows the clock.** The 0.06 scale is applied to `dt` before the
   game clock, needs and weather advance.
6. **Projectile may be destroyed twice** when it passes through two zombies in
   one tick (`ProjectileSystem.cpp` pushes it to the destroy list per hit).
7. **Survivor melee hits every zombie in reach** each swing; the comment says
   "nearest".
8. **`SurvivorInteractionSystem::mTargetSurvivor` defaults to 0**, a valid
   entity id.
9. **Dead data:** `Pistol Ammo` and `Rat Meat` have no source;
   `SpawnData.homeFurnitureSpawns` and `containerSpawns` are generated and
   unused; `tools/generate_items.py` would overwrite `items.json` with an old
   list; the unreachable bulk-transfer branch in `InteractionSystem.cpp`.
10. **District infection uses 32 px tiles hard-coded** and the bounding box of
    a district as its area.
11. **Campaign deadline kills the player outright** with no warning in the
    world. Phase 9 replaces the campaign; phase 1 only removes the kill.
12. A `build-local/` folder with build output and saves is untracked in the
    repository root and not ignored.

## Things that work well and must be kept

- The fixed town and everything remembered about it.
- The first three days: home kit, taps, day plans, night callers.
- Telegraphed lunges and the blow that breaks one off.
- Doors: open, close, batter, board, pry; all remembered.
- Furniture placement rules (rooms stay walkable, doorways stay clear).
- The harness. Every phase adds to it.
- The bare-hands interface and its palette.
