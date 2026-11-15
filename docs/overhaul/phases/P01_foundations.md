# Phase 1: foundations

**Goal.** Fix what is broken, move content out of C++ into data files, let
the player put things into containers, and build a bot that can play the game
so later phases can be measured. Nothing here is glamorous; everything after
depends on it.

**The player notices:** weather that lasts hours, `Tough` that works, storing
things in cupboards, a molotov made from an empty bottle.

## Core

### 1. Housekeeping

- Create branch `overhaul` from `art-and-feel`.
- Add `/build-local/` and `/build*/` to `.gitignore`.
- Delete `tools/generate_items.py` (it would overwrite `items.json` with an
  old list). Remove its row from the top-level README.
- Remove `SpawnData.homeFurnitureSpawns` and `containerSpawns` and what
  generates and nudges them (`MapGenerator`, `nudgeSpawnsOffFurniture`), and
  the unreachable bulk-transfer branch in `InteractionSystem.cpp`. Check
  that the mapgen tests still pass and the town for seed 1234 is unchanged
  (the layout fingerprint test, or compare `world_tour` screenshots).

### 2. Defect fixes (see `02_current_state.md` for where)

| # | Fix |
|---|---|
| 1 | Weather: count `mConditionTimer` in game hours (`dt * kGameHoursPerRealSecond`). Give `WeatherSystem` an RNG seeded from the run seed (`reset(seed)`, called where `ThreatDirector.reset` is) and draw both the condition and its length (2 to 6 game hours) from it. Save `condition`, hours left and the RNG state in the checkpoint so a resume continues the same weather. |
| 2 | Max health: add `Health::baseMaximum` (default 100). `Tough` sets it to 120. `WoundSystem` and the bandage code in `GameShared.h` compute `maximum = max(20, baseMaximum - 5 * wounds)`. Checkpoints save `baseMaximum`. |
| 3 | Workbench: `hasNearbyWorkbench` is true within 3 tiles of a `FurnitureKind` workbench (or, after phase 2, a built one). Drop the supply-cache check. |
| 4 | Combat RNG: seed `CombatSystem` from `currentRunSeed() ^ 0xC0B7` at run start. Remove `std::random_device` from it. |
| 5 | Hit-stop: keep two deltas in `Game::update`: `dt` (true) and `feelDt` (`dt * 0.06` during hit-stop). Use `feelDt` only for combat, zombie AI, movement, collision, projectiles, animation and particles. The clock, needs, wounds, mind, weather, fire, survivors' needs and timers use `dt`. |
| 6 | Projectile: mark destroyed once; a bullet stops at the first zombie it hits (the shotgun's pellets are separate projectiles). |
| 7 | Survivor melee hits the nearest zombie in reach only. |
| 8 | `mTargetSurvivor` defaults to `kInvalidEntity`. |
| 10 | District infection uses `tileMap.tileWidth()` and counts walkable tiles of the district as its area (compute once in `initialize`). |
| 11 | Campaign deadline no longer kills. When it passes, show "The far side of town has gone quiet." once and leave the objective in place. (Phase 9 replaces all of this.) |

Each fix gets a unit test where the rule is testable without the window:
weather lasts between 2 and 6 game hours and repeats for a seed; `Tough`
survives a wound and a bandage; the workbench check; one bullet, one body.

### 3. Items and recipes by key

- Add `"key"` to all 30 items in `items.json` (keys in
  `03_content_tables.md` section 1.1) and the new optional fields of section
  1.3 to the loader (`ItemDatabase`). `ItemDatabase` gains
  `findByKey(std::string_view)` and `idOf(key)` (returns -1 and logs once if
  missing).
- New header `src/items/ItemIds.h`: a struct `ItemIds` with one `int` per key
  the code uses (`cannedFood`, `waterBottle`, `emptyBottle`, `woodPlanks`,
  `hammer`, `fuelCan`, `molotov`, `matches`, `notebook`, ...), filled by
  `ItemIds::resolve(const ItemDatabase&)` after load. `Game` holds one
  (`mItems`). Replace every literal item id in C++ with it. Find them with:
  `grep -nE "itemId == [0-9]+|Item\(.*, [0-9]+,|kWaterItem|kPlankItem|emplace_back\([0-9]+, " src -r`
  and by reading `GameFirstDays.cpp`, `GameFire.cpp`, `GameVehicles.cpp`,
  `GameScene.cpp`, `GameInventory.cpp`, `InteractionSystem.cpp`,
  `BuildSystem.cpp`, `GameShared.h`.
- `leaves`: using up a Water Bottle leaves an Empty Bottle through the data
  field, not a special case.
- Recipes: `recipes.json` names inputs, outputs and tools by key; add
  `station`, `countA`, `countB`, optional `inputB`. `requiresWorkbench: true`
  is read as `station: "workbench"`. Tools may be an item key or a tag
  (`tool:hammer`); an Axe carries `tool:hammer` (it already boards doors).
- Molotov becomes Empty Bottle + Soaked Rag. Migration: when loading known
  recipes (notebooks, session discoveries) a pair (3, 25) is read as (29, 25).
- Give `Pistol Ammo` no source yet (phase 3 does); rename item 21 to
  "Raw Meat" (key `rat_meat` stays).
- Test: every key in `ItemIds` resolves; every recipe's keys exist; every
  sprite named in `items.json` exists in `spritesheet.json`.

### 4. Loot tables in data

- New `assets/data/loot.json` and `src/items/LootTables.h/.cpp`:
  `struct LootEntry {int itemId; float weight; int min, max;}`,
  `struct LootClass {float stockedChance; int minRolls, maxRolls;
  std::vector<LootEntry> entries;}`, `LootTables::load(path, db)`,
  `roll(classKey, rng, suppliesScale, std::vector<std::pair<int,int>>& out)`.
- Port the nine tables from `GameScene.cpp` (the `lootTable` lambda) with the
  rule in `03_content_tables.md` section 5, using only items that exist
  today. `FurnitureLoot` maps to a class key by name (`fridge`, `kitchen`,
  ...). Vehicle trunks use class `trunk`. Medical and supply caches keep
  their code for now but take ids from `ItemIds`.
- The second roll happens with chance 0.25, as today. Result for seed 1234
  will differ from before; that is accepted. Tone's `suppliesScale` still
  scales `stockedChance` (clamped to 0.9).
- Test: a class with one entry always yields it; weights hold over 10,000
  rolls within 3 %; unknown item keys fail the load with a clear message.

### 5. Tuning file

- New `assets/data/tuning.json` and `src/core/Tuning.h/.cpp`: a flat map of
  named numbers with defaults in code (`Tuning::get("night.callers.base",
  2.0f)`). Loaded once at start; a missing file means all defaults.
- Move only numbers that phases 2 to 14 will tune: night callers, board
  health, threat director intervals and growth, needs decay rates, zombie
  damage and hit chance, loot `stockedChance` scale. Do not move everything.
- Harness command `tune KEY VALUE` overrides a value for the run.

### 6. Putting things into containers

- In the container panel, the right half lists the container (as today) and
  the left half lists your pack. Left/Right (or A/D) switches side; E or
  Enter moves one of the item under the cursor to the other side; Shift+E
  moves the whole stack. Mouse: click moves one, shift-click the stack.
- A container holds 12 slots and 30 kg by default (`Inventory` gets
  `maxSlots` and `maxWeight` fields; the player's stay 20 and 25). Fridges
  hold 8 slots; wardrobes and shop shelves 12; crates (phase 2) 16.
- What a container holds is world memory. Replace "looted positions" with a
  per-container record `{tileX, tileY, items[(id, count, condition)]}` saved
  in the memory file whenever a container is changed and at run end. A
  container with a record is filled from the record, not rolled. Old saves:
  a looted position becomes an empty record.
- Food in a fridge spoils at half speed while the power is on (phase 6 turns
  the power off; until then, always on).
- Panel follows the bare-hands style: one docked panel 700 px wide with two
  columns, the active column's cursor row inverted. Header: the container's
  name and "8 of 12". Footer hint: `E move   Shift+E move all   A/D side
  Esc close`.
- The day-1 plan step "Search the house" still counts opened containers.
- Tests: move in, move out, weight and slot limits, save round trip of
  container records, migration of looted positions.
- Harness: `stash ITEM [COUNT]` puts an item from the pack into the open
  container; `container` prints the open container's contents.

### 7. Save version 2

- Raise `kCurrentSaveVersion` to 2. Loading version 1: accept it, fill new
  fields with defaults, apply the migrations above (looted positions,
  molotov recipe), write back as version 2 with the old file kept as
  `.v1.backup`. Round-trip test with a version-1 fixture built in the test.

### 8. The bot (`src/dev/Autoplayer.h/.cpp`)

A scripted player for soak runs, driven by the harness command
`bot PROFILE [DAYS]`. It reads the world the way the player could (what is on
screen, the HUD state, the day plan) and issues the same inputs the keyboard
would, through `Input`. It is not clever; it is consistent.

- Priorities, top first: treat bleeding; flee if 3 or more of the dead are
  within 5 tiles; fight if 1 or 2 are within 2 tiles (swing when in reach,
  step back during a wind-up); drink if thirsty and carrying water; eat if
  hungry; sleep in the home bed between 22:00 and 06:00; follow the day plan
  steps (search home containers, use the tap, board home doors); otherwise
  walk to the nearest unsearched container within 40 tiles and search it,
  taking food, water, medicine, planks, nails and one better weapon; go home
  at 18:00.
- Movement uses `Pathfinder` to the target tile and presses WASD along it.
- Profiles: `careful` (flees at 2, never goes beyond 25 tiles from home),
  `normal`, `bold` (fights up to 3, ranges 60 tiles).
- `metrics INTERVAL` gains columns: day, hour, cause of death, kills, items
  held by category, containers searched, boards up, callers tonight, zombies
  within 10 tiles, noise events per game hour.
- New script `tools/harness/bot_week.txt`: `newrun`, `bot normal 7`,
  `metrics 600`, write `metrics.csv`. New tool `tools/soak.py`: runs the
  script for seeds 1 to 10 in fresh directories and prints a table (days
  survived, cause, kills, food and water at death). It is how phase 14 and
  every later phase read balance.
- Record the baseline table in `docs/revival_plan.md` under this phase.

## Later (only if time remains)

- Move the medical and supply caches into `loot.json` as classes.

## Files

New: `src/items/ItemIds.h`, `src/items/LootTables.h/.cpp`,
`src/core/Tuning.h/.cpp`, `src/dev/Autoplayer.h/.cpp`,
`assets/data/loot.json`, `assets/data/tuning.json`, `tools/soak.py`,
`tools/harness/storage.txt`, `tools/harness/bot_week.txt`,
`tests/test_content.cpp`, `tests/test_weather.cpp`.
Changed: most of `src/core/Game*.cpp`, `ItemDatabase`, `CraftingSystem`,
`WeatherSystem`, `WoundSystem`, `CombatSystem`, `ProjectileSystem`,
`SurvivorAISystem`, `DistrictInfectionSystem`, `SaveManager`, `DevHarness`,
`MapGenerator`, `CMakeLists.txt`, `.gitignore`, README.

## Acceptance

- `tools/harness/storage.txt`: open a cupboard at home, move three items in,
  `save`, `resume`, they are there; `kill`, `newrun`, go home, they are
  there. Screenshot `storage_panel` shows two columns that fit.
- `atmosphere.txt` still passes; a new check in it holds one condition across
  60 real seconds without it changing.
- `set` a wound on a `Tough` character (harness `set wounds 1` if missing,
  add it): max health reads 115.
- `bot_week` runs for seeds 1 to 10 without a crash or a stuck bot (a bot
  that does not change tile for 3 game hours while awake prints `FAIL`).
- No literal item id remains in `src/` outside `ItemIds` (the grep above
  returns nothing).
- All 20 older harness scripts pass.
